#include "server.h"
#include "audio_capture.h"
#include "audio_control.h"
#include "twitch.h"
#include "network.h"
#include "portal_http.h"
#include "language.h"
#include "updater.h"
#include <map>
#include "master_gate.h"
#include "master_history.h"
#include "deckstatus_version.h"

#include <httplib/httplib.h>

#include <algorithm>
#include <chrono>
#include <charconv>
#include <cctype>
#include <cstdint>
#include <functional>
#include <fstream>
#include <iostream>
#include <iterator>
#include <optional>
#include <set>
#include <stdexcept>
#include <thread>

namespace deckstatus {
namespace {

using Json = nlohmann::json;

std::string lower_ascii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

std::string authority(const std::string& host, int port) {
    const auto name = host.find(':') == std::string::npos || host.front() == '['
        ? host : '[' + host + ']';
    return port == 80 ? name : name + ':' + std::to_string(port);
}

std::set<std::string> allowed_authorities(const std::string& host, int port) {
    std::set<std::string> result;
    // Explicit Host validation also protects loopback listeners from DNS rebinding.
    for (const std::string name : {"127.0.0.1", "localhost", "[::1]"}) {
        result.insert(authority(name, port));
        if (port == 80) result.insert(name + ":80");
    }
    const auto normalized = lower_ascii(host);
    if (normalized != "0.0.0.0" && normalized != "::" && !normalized.empty()) {
        result.insert(authority(normalized, port));
        if (port == 80) result.insert(authority(normalized, port) + ":80");
    }
    return result;
}

void json_response(httplib::Response& response, const Json& body, int status = 200) {
    response.status = status;
    response.set_content(body.dump(-1, ' ', false, Json::error_handler_t::replace),
                         "application/json; charset=utf-8");
}

Json decks_from(const Json& state) {
    if (!state.is_object() || !state.contains("decks") || !state["decks"].is_array()) {
        return Json::array();
    }
    return state["decks"];
}

std::optional<std::uint64_t> loaded_track(const Json& state, int deck_id) {
    for (const auto& deck : decks_from(state)) {
        if (!deck.is_object() || deck.value("id", 0) != deck_id ||
            !deck.value("loaded", false) || !deck.contains("trackId")) continue;
        const auto& id = deck["trackId"];
        if (id.is_number_unsigned() && id.get<std::uint64_t>() > 0)
            return id.get<std::uint64_t>();
        if (id.is_number_integer() && id.get<std::int64_t>() > 0)
            return static_cast<std::uint64_t>(id.get<std::int64_t>());
    }
    return std::nullopt;
}

std::string read_page(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) return {};
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

std::size_t raw_parameter_count(const httplib::Request& request, const char* name) {
    // cpp-httplib deduplicates identical raw fields when parsing a whole query.
    // Parse each field independently so duplicates cannot evade validation.
    const auto separator = request.target.find('?');
    if (separator == std::string::npos) return 0;
    std::size_t count = 0;
    for (auto begin = separator + 1; begin < request.target.size();) {
        auto end = request.target.find('&', begin);
        if (end == std::string::npos) end = request.target.size();
        httplib::Params field;
        httplib::detail::parse_query_text(request.target.data() + begin, end - begin, field);
        count += field.count(name);
        begin = end + 1;
    }
    return count;
}

} // namespace

int run_server(const std::string& host, int port,
               const std::filesystem::path& web_root,
               std::function<Json()> snapshot,
               std::function<std::pair<std::string, std::string>(int)> cover,
               std::atomic_bool& stop, MasterHistory* master, ServerFeatures* features) {
    if (host.empty() || port < 1 || port > 65535 || !snapshot || !cover) {
        std::cerr << tr("Invalid web server configuration.") << '\n';
        return 1;
    }
    if (stop.load()) return 0;

    // Fixed allowlist only: request paths never reach the filesystem.
    struct Asset { std::string body; std::string mime; std::string etag; Access access; };
    // Documents are grouped by who may open them; every other asset is a public
    // script, style, icon or translation that carries no application data.
    const auto asset_access = [](const std::string& url, const std::string& mime) {
        if (!mime.starts_with("text/html")) return Access::Public;
        static const std::set<std::string> public_pages = {"/login", "/history"};
        static const std::set<std::string> renderers = {"/overlay", "/overlay.html", "/master-overlay",
            "/waveform", "/scene", "/component/text", "/component/image", "/component/fx"};
        static const std::set<std::string> admin_pages = {"/admin", "/automations", "/network/settings"};
        if (public_pages.contains(url)) return Access::Public;
        if (renderers.contains(url)) return Access::KeyedPage;
        if (admin_pages.contains(url)) return Access::AdminPage;
        if (url == "/account/password") return Access::PasswordPage;
        return Access::Page;
    };
    std::map<std::string, Asset> assets;
    for (const auto& [url, file] : std::initializer_list<std::pair<const char*, const char*>>{
        {"/", "index.html"}, {"/index.html", "index.html"},
        {"/overlay", "overlay.html"}, {"/overlay.html", "overlay.html"},
        {"/master-overlay", "master-overlay.html"},
        {"/master-overlay/settings", "master-settings.html"}, {"/overlay/settings", "master-settings.html"},
        {"/master-overlay.js", "master-overlay.js"}, {"/master-options.js", "master-options.js"},
        {"/deck-overlay.js", "deck-overlay.js"}, {"/overlay-shared.js", "overlay-shared.js"},
        {"/overlay.css", "overlay.css"}, {"/settings.css", "settings.css"},
        {"/settings.js", "settings.js"}, {"/track-controls.js", "track-controls.js"}, {"/i18n.js", "i18n.js"}, {"/storage.js", "storage.js"},
        {"/theme.css", "theme.css"}, {"/poll.js", "poll.js"},
        {"/waveform", "waveform.html"}, {"/waveform/settings", "waveform-settings.html"},
        {"/waveform.js", "waveform.js"}, {"/waveform-options.js", "waveform-options.js"},
        {"/waveform-renderer.js", "waveform-renderer.js"}, {"/waveform-settings.js", "waveform-settings.js"},
        {"/icon.svg", "icon.svg"},
        {"/history", "history.html"}, {"/history.js", "history.js"}, {"/history.css", "history.css"},
        {"/navigation.js", "navigation.js"}, {"/navigation.css", "navigation.css"},
        {"/prolink/settings", "prolink-settings.html"}, {"/prolink-settings.js", "prolink-settings.js"},
        {"/connection.css", "connection.css"}, {"/rekordbox/settings", "rekordbox-settings.html"}, {"/rekordbox-settings.js", "rekordbox-settings.js"},
        {"/network/settings", "network-settings.html"}, {"/network-settings.js", "network-settings.js"},
        {"/login", "login.html"}, {"/account/password", "password.html"}, {"/auth.js", "auth.js"}, {"/portal.css", "portal.css"},
        {"/admin", "admin.html"}, {"/admin.js", "admin.js"}, {"/broadcast.js", "broadcast.js"},
        {"/scenes", "scene-editor.html"}, {"/scene-editor.js", "scene-editor.js"}, {"/scene-editor.css", "scene-editor.css"},
        {"/component-presets.js", "component-presets.js"}, {"/component-presets.css", "component-presets.css"},
        {"/scene", "scene.html"}, {"/scene.js", "scene.js"}, {"/scene-shared.js", "scene-shared.js"},
        {"/components/text", "creative-settings.html"}, {"/components/image", "creative-settings.html"}, {"/components/fx", "creative-settings.html"},
        {"/component/text", "creative.html"}, {"/component/image", "creative.html"}, {"/component/fx", "creative.html"},
        {"/creative-settings.js", "creative-settings.js"}, {"/creative.js", "creative.js"}, {"/creative-options.js", "creative-options.js"},
        {"/creative-renderer.js", "creative-renderer.js"}, {"/audio-reactivity.js", "audio-reactivity.js"}, {"/creative.css", "creative.css"},
        {"/media-library.js", "media-library.js"},
        {"/admin-audio.js", "admin-audio.js"}, {"/admin-master.js", "admin-master.js"},
        {"/admin-twitch.js", "admin-twitch.js"}, {"/admin-updater.js", "admin-updater.js"},
        {"/automations", "automations.html"}, {"/automations.js", "automations.js"},
        {"/locales/en.json", "locales/en.json"}, {"/locales/de.json", "locales/de.json"}
    }) {
        const auto body = read_page(web_root / file);
        if (body.empty()) { std::cerr << tr("Web assets missing in: ") << web_root << '\n'; return 1; }
        const auto extension = std::filesystem::path(file).extension();
        const std::string mime = extension == ".js" ? "text/javascript; charset=utf-8" :
            extension == ".svg" ? "image/svg+xml" : extension == ".css" ? "text/css; charset=utf-8" : extension == ".json" ? "application/json; charset=utf-8" : "text/html; charset=utf-8";
        // Documents stay uncacheable. Public scripts, styles, icons and translations get a
        // validator so a repeat visit costs a 304 instead of the whole file; the embedded
        // bodies never change while the process runs.
        const std::string etag = mime.starts_with("text/html") ? std::string{}
            : '"' + std::to_string(std::hash<std::string>{}(body)) + '-' + std::to_string(body.size()) + '"';
        assets.emplace(url, Asset{body, mime, etag, asset_access(url, mime)});
    }

    AudioCapture audio;
    auto* portal = features ? features->portal : nullptr;
    const auto* network = features ? features->network : nullptr;
    AudioControl audio_control(portal,[&]{return audio.devices();},[&]{return audio.state();},[&](const auto& id){return audio.select(id);});
    TwitchIntegration twitch(portal,features?features->twitch_transport:nullptr);
    // Both listeners share audio, track state, accounts and scenes.
    const auto configure = [&](PortalServer& server) {
        // Windows SO_REUSEADDR permits multiple listeners on the same address.
        // Use exclusive ownership so a second instance fails at startup.
        server.set_socket_options([](socket_t socket) {
            if (!httplib::detail::set_socket_opt(socket, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, 1))
                throw std::runtime_error("Could not reserve the HTTP socket exclusively.");
        });
        server.new_task_queue = [] { return new httplib::ThreadPool(4, 64); };
        server.set_read_timeout(3);
        server.set_write_timeout(3);
        server.set_keep_alive_timeout(2);
        server.set_keep_alive_max_count(50);
        server.set_payload_max_length(12*1024*1024); // Authenticated media/update uploads may exceed the normal 64 KiB limit.
        server.set_default_headers({
            {"Cache-Control", "no-store"},
            {"X-Content-Type-Options", "nosniff"},
            {"X-Frame-Options", "SAMEORIGIN"},
            {"Referrer-Policy", "no-referrer"},
            {"Content-Security-Policy", "default-src 'self'; script-src 'self' 'unsafe-inline'; "
                                        "style-src 'self' 'unsafe-inline'; img-src 'self'; "
                                        "connect-src 'self'; object-src 'none'; base-uri 'none'; "
                                        "frame-ancestors 'self'; form-action 'none'"}
        });

        const auto domain = network ? network->active().public_domain : std::string{};
        const auto public_request = [domain](const httplib::Request& request) {
            const auto requested = lower_ascii(request.get_header_value("Host"));
            return !domain.empty() && (requested == domain || requested == domain + ":443");
        };
        const auto local_request = [public_request](const httplib::Request& request) {
            // A proxy on this PC must not turn a domain visitor into a local operator.
            return !public_request(request) && local_network_peer(request.remote_addr, request.local_addr);
        };
        server.set_post_routing_handler([public_request](const auto& request, auto& response) {
            if (public_request(request)) {
                const auto cookies = response.headers.equal_range("Set-Cookie");
                for (auto it = cookies.first; it != cookies.second; ++it) it->second += "; Secure";
            }
        });
        const auto allowed = allowed_authorities(host, port);
        const auto validate_request = [allowed, host, port, domain, public_request, portal](const httplib::Request& request,
                                                 httplib::Response& response) {
            // Accept the socket's destination IP on a wildcard listener, not arbitrary
            // Host names or forwarded headers supplied by a client.
            const auto requested = lower_ascii(request.get_header_value("Host"));
            const bool destination = host == "0.0.0.0" && valid_bind_address(request.local_addr) &&
                (requested == authority(request.local_addr, port) || (port == 80 && requested == request.local_addr + ":80"));
            if (request.get_header_value_count("Host") != 1 ||
                (!allowed.contains(requested) && !destination && !public_request(request))) {
                json_response(response, {{"error", "Host is not allowed"}}, 403);
                return httplib::Server::HandlerResponse::Handled;
            }
            if (request.get_header_value_count("Origin") > 1) {
                json_response(response, {{"error", "Origin is not allowed"}}, 403);
                return httplib::Server::HandlerResponse::Handled;
            }
            if (request.has_header("Origin")) {
                const auto origin = lower_ascii(request.get_header_value("Origin"));
                const bool external = public_request(request);
                const auto expected = external ? "https://" + domain : "http://" + lower_ascii(request.get_header_value("Host"));
                if (origin != expected && !(external && origin == expected + ":443")) {
                    json_response(response, {{"error", "Cross-origin requests are not allowed"}}, 403);
                    return httplib::Server::HandlerResponse::Handled;
                }
            }
            // A local OBS link opened from the public domain is a cross-site navigation.
            // Only these renderer documents with their matching read key may cross that boundary.
            const bool renderer = request.path == "/overlay" || request.path == "/overlay.html" ||
                request.path == "/master-overlay" || request.path == "/waveform" || request.path == "/scene" ||
                request.path == "/component/text" || request.path == "/component/image" || request.path == "/component/fx";
            const bool keyed_renderer = renderer && portal && raw_parameter_count(request,"key") == 1 &&
                raw_parameter_count(request,"scene") <= 1 &&
                portal->broadcast_access(request.get_param_value("key"),request.path,request.get_param_value("scene"));
            const bool public_navigation = (request.method == "GET" || request.method == "HEAD") &&
                (request.path == "/history" || request.path == "/login" || keyed_renderer) &&
                request.get_header_value("Sec-Fetch-Mode") == "navigate" && request.get_header_value("Sec-Fetch-Dest") == "document";
            if (request.has_header("Sec-Fetch-Site") &&
                request.get_header_value("Sec-Fetch-Site") == "cross-site" && !public_navigation) {
                json_response(response, {{"error", "Cross-site requests are not allowed"}}, 403);
                return httplib::Server::HandlerResponse::Handled;
            }
            if (request.method != "GET" && request.method != "HEAD" &&
                request.method != "POST" && request.method != "PUT" && request.method != "PATCH" &&
                request.method != "DELETE" && request.method != "OPTIONS") {
                response.set_header("Allow", "GET, HEAD");
                json_response(response, {{"error", "Method is not allowed"}}, 405);
                return httplib::Server::HandlerResponse::Handled;
            }
            return httplib::Server::HandlerResponse::Unhandled;
        };
        server.set_pre_routing_handler([validate_request](const auto& request, auto& response) {
            // Consume bounded mutation bodies before sending a denial. Closing a Windows
            // socket with unread bytes can reset it before the browser receives the 403.
            if (request.method == "POST" || request.method == "PUT" || request.method == "PATCH" ||
                request.method == "DELETE" || request.method == "OPTIONS")
                return httplib::Server::HandlerResponse::Unhandled;
            return validate_request(request, response);
        });
        server.set_exception_handler([](const auto&, auto& response, std::exception_ptr) {
            json_response(response, {{"error", "Could not read the current state"}}, 500);
        });
        // Normal handlers run after httplib consumes the bounded request body.
        // Rejecting a POST in pre-routing can close a Windows socket with unread
        // bytes and reset the connection before the client receives the 405.
        const auto reject_method = [](const auto&, auto& response) {
            response.set_header("Allow", "GET, HEAD");
            json_response(response, {{"error", "Method is not allowed"}}, 405);
        };
        server.authorize = [portal, &twitch, validate_request](const httplib::Request& request, auto& response, Access access) {
            if (validate_request(request, response) == httplib::Server::HandlerResponse::Handled) return false;
            if(request.body.size()>65536&&request.path!="/api/media"&&request.path!="/api/admin/updater/upload")throw PortalError(413,"portalCapacity");
            if (!portal) return true; // Isolated native fixtures may omit the application store.
            if (access == Access::Public) return true;
            const bool read = request.method == "GET" || request.method == "HEAD";
            // A scoped read key opens only the renderer routes that carry its own content.
            if (keyed_access(access) && read && raw_parameter_count(request,"key") == 1 && raw_parameter_count(request,"scene") <= 1 &&
                (portal->broadcast_access(request.get_param_value("key"),request.path,request.get_param_value("scene")) ||
                 (raw_parameter_count(request,"scene") == 1 && twitch.broadcast_access(request.get_param_value("key"),request.path,request.get_param_value("scene"))))) return true;
            const auto user = portal->identity(portal_session(request));
            if (user.is_null()) {
                if (page_access(access)) { response.set_redirect("/login",303); return false; }
                throw PortalError(401,"authRequired");
            }
            if (user["mustChangePassword"].get<bool>() && !password_access(access)) {
                if (page_access(access)) { response.set_redirect("/account/password",303); return false; }
                throw PortalError(403,"authPasswordRequired");
            }
            if (admin_access(access) && user["role"] != "admin") throw PortalError(403,"authAdminRequired");
            return true;
        };
        if (portal) {
            server.Get("/api/auth/me", Access::Public, [portal](const auto& request,auto& response){json_response(response,{{"user",portal->identity(portal_session(request))}});});
            server.Post("/api/auth/login", Access::Public, [portal](const auto& request,auto& response){
                const auto body=portal_body(request);if(!body.contains("username")||!body["username"].is_string()||!body.contains("password")||!body["password"].is_string())throw PortalError(400,"portalInvalid");
                auto result=portal->login(body["username"],body["password"],request.remote_addr);portal->logout(portal_session(request));session_cookie(response,result["session"]);result.erase("session");json_response(response,result);
            });
            server.Post("/api/auth/logout", Access::Public, [portal](const auto& request,auto& response){portal_body(request);portal->logout(portal_session(request));session_cookie(response,"");json_response(response,{{"ok",true}});});
            server.Post("/api/auth/password", Access::Password, [portal](const auto& request,auto& response){portal->change_password(portal_session(request),portal_body(request));session_cookie(response,"");json_response(response,{{"ok",true}});});
            server.Get("/api/admin/users", Access::Admin, [portal](const auto&,auto& response){json_response(response,{{"users",portal->users()}});});
            server.Post("/api/admin/users", Access::Admin, [portal](const auto& request,auto& response){const auto user=portal->identity(portal_session(request));json_response(response,{{"users",portal->edit_user(user["id"],portal_body(request))}});});
            server.Get("/api/admin/ratings", Access::Admin, [portal](const auto&,auto& response){json_response(response,{{"tracks",portal->ratings()}});});
            server.Get(R"(/api/admin/ratings/([a-f0-9]{64})/viewers)", Access::Admin, [portal](const auto& request,auto& response){json_response(response,portal->rating_viewers(request.matches[1].str()));});
            server.Get("/api/public/twitch", Access::Public, [&twitch](const auto& request,auto& response){json_response(response,twitch.viewer_status(portal_cookie(request,"deckstatus_viewer")));});
            server.Post("/api/public/twitch", Access::Public, [&twitch](const auto& request,auto& response){
                const auto cmd=portal_body(request);auto result=twitch.viewer_command(portal_cookie(request,"deckstatus_viewer"),request.remote_addr,cmd);
                if(result.contains("session")){response.set_header("Set-Cookie","deckstatus_viewer="+result["session"].template get<std::string>()+"; Path=/; HttpOnly; SameSite=Strict; Max-Age=3600");result.erase("session");}
                if(cmd.value("action",std::string{})=="logout")response.set_header("Set-Cookie","deckstatus_viewer=; Path=/; HttpOnly; SameSite=Strict; Max-Age=0");json_response(response,result);
            });
            server.Get("/api/broadcast", Access::User, [portal](const auto&,auto& response){json_response(response,portal->overlay_keys(false));});
            server.Post("/api/admin/broadcast", Access::Admin, [portal](const auto& request,auto& response){portal_body(request);json_response(response,portal->overlay_keys(true));});
            server.Get("/api/scenes", Access::User, [portal](const auto&,auto& response){json_response(response,{{"scenes",portal->scenes()}});});
            server.Get("/api/presets", Access::User, [portal](const auto&,auto& response){json_response(response,{{"presets",portal->presets()}});});
            server.Post("/api/presets", Access::User, [portal](const auto& request,auto& response){json_response(response,portal->edit_preset(portal_body(request)));});
            server.Get("/api/media", Access::User, [portal](const auto&,auto& response){json_response(response,{{"media",portal->media()}});});
            server.Post("/api/media", Access::User, [portal](const auto& request,auto& response){json_response(response,portal->edit_media(portal_body(request)));});
            server.Get("/api/media/([a-f0-9]{64})", Access::Keyed, [portal](const auto& request,auto& response){const auto [mime,bytes]=portal->media_file(request.matches[1].str());response.set_content(bytes,mime);});
            server.Post("/api/scenes", Access::User, [portal](const auto& request,auto& response){json_response(response,portal->edit_scene(portal_body(request)));});
            server.Get("/api/scene", Access::Keyed, [portal,&twitch](const httplib::Request& request,auto& response){if(raw_parameter_count(request,"scene")!=1)throw PortalError(400,"sceneInvalid");json_response(response,twitch.render(portal->scene(request.get_param_value("scene"))));});
            server.Post("/api/public/rating", Access::Public, [portal,&twitch](const auto& request,auto& response){
                const auto viewer=twitch.viewer_identity(portal_cookie(request,"deckstatus_viewer"),true);
                json_response(response,portal->rate_twitch(viewer,request.remote_addr,portal_body(request)));
            });
        }
        server.Get("/api/audio/devices", Access::User, [&](const auto&, auto& response) { json_response(response, audio.devices()); });
        server.Get("/api/audio/state", Access::Keyed, [&](const auto&, auto& response) { json_response(response, audio.state()); });
        const bool remote_control = network && network->active().allow_remote_control;
        const auto can_control = [remote_control, public_request](const httplib::Request& request) {
            if (public_request(request)) return remote_control;
            return local_network_peer(request.remote_addr, request.local_addr) || may_control_network(request.remote_addr, remote_control);
        };
        const auto audio_description=[&audio_control,can_control](const auto& request){auto result=audio_control.describe();result["canControl"]=can_control(request);return result;};
        server.Get("/api/admin/twitch", Access::Admin, [&twitch,can_control](const auto& request,auto& response){auto result=twitch.describe();result["canControl"]=can_control(request);json_response(response,result);});
        server.Get("/api/admin/automations", Access::Admin, [&twitch,can_control](const auto& request,auto& response){auto result=twitch.automation_description();result["canControl"]=can_control(request);json_response(response,result);});
        server.Post("/api/admin/automations", Access::Admin, [&twitch,can_control](const auto& request,auto& response){if(!can_control(request))throw PortalError(403,"networkReadOnly");auto result=twitch.automation_command(portal_body(request));result["canControl"]=true;json_response(response,result);});
        server.Post("/api/admin/twitch", Access::Admin, [&twitch,can_control](const auto& request,auto& response){if(!can_control(request))throw PortalError(403,"networkReadOnly");auto result=twitch.command(portal_body(request));result["canControl"]=true;json_response(response,result);});
        server.Get("/api/admin/audio", Access::Admin, [audio_description](const auto& request,auto& response){json_response(response,audio_description(request));});
        server.Post("/api/admin/audio", Access::Admin, [&audio_control,can_control,audio_description](const auto& request,auto& response){
            if(!can_control(request))throw PortalError(403,"networkReadOnly");
            const auto body=portal_body(request);
            if(!body.is_object()||!body.contains("action")||!body["action"].is_string())throw PortalError(400,"audioSettingsInvalid");
            const auto action=body["action"].template get<std::string>();
            if(action=="save"&&body.size()==3){auto settings=body;settings.erase("action");audio_control.save(settings);}
            else if(action=="start"&&body.size()==1)audio_control.start_saved();
            else if(action=="stop"&&body.size()==1)audio_control.stop();
            else throw PortalError(400,"audioSettingsInvalid");
            json_response(response,audio_description(request));
        });
        server.Post("/api/audio/source", Access::Admin, [&audio, &audio_control, can_control](const auto& request, auto& response) {
            if (!can_control(request)) { json_response(response, {{"error", "networkReadOnly"}}, 403); return; }
            const auto type = lower_ascii(request.get_header_value("Content-Type"));
            if (type != "application/json" && type != "application/json; charset=utf-8") {
                json_response(response, {{"error", "JSON content type required"}}, 415); return;
            }
            const auto body = Json::parse(request.body, nullptr, false);
            if (!body.is_object() || body.size() != 1 || !body.contains("deviceId") || !body["deviceId"].is_string()) {
                json_response(response, {{"error", "Expected a deviceId string; empty string stops capture"}}, 400); return;
            }
            audio_control.select(body["deviceId"].template get<std::string>());
            json_response(response, audio.state());
        });
        // The hold filter is server-wide: every overlay, the dashboard and Full History
        // observe the same confirmed master, so it is configured once in Admin.
        auto* master_gate = features ? features->master_gate : nullptr;
        const auto master_description=[master_gate,can_control](const httplib::Request& request){
            Json result=master_gate?master_gate->describe():Json{{"holdMs",MasterGate::default_hold_ms},{"defaultHoldMs",MasterGate::default_hold_ms},{"maxHoldMs",MasterGate::max_hold_ms}};
            result["available"]=master_gate!=nullptr;result["canControl"]=can_control(request);return result;
        };
        server.Get("/api/admin/master", Access::Admin, [master_description](const auto& request,auto& response){json_response(response,master_description(request));});
        server.Post("/api/admin/master", Access::Admin, [master_gate,portal,can_control,master_description](const auto& request,auto& response){
            if(!can_control(request))throw PortalError(403,"networkReadOnly");
            if(!master_gate)throw PortalError(503,"masterSettingsUnavailable");
            const auto body=portal_body(request);
            if(body.size()!=1||!body.contains("holdMs")||!body["holdMs"].is_number_integer())throw PortalError(400,"masterSettingsInvalid");
            const auto hold=body["holdMs"].template get<std::int64_t>();
            if(!MasterGate::valid_hold(hold))throw PortalError(400,"masterSettingsInvalid");
            // Persist first: a rejected write must not leave the running gate ahead of the store.
            if(portal)portal->save_master_settings({{"holdMs",hold}});
            master_gate->set_hold_ms(hold);
            json_response(response,master_description(request));
        });
        auto* updater=features?features->updater:nullptr;
        server.Get("/api/admin/updater",Access::Admin,[updater,can_control](const auto& request,auto& response){
            if(!updater)throw PortalError(409,"updateUnavailable");auto result=updater->describe();result["canControl"]=can_control(request);json_response(response,result);
        });
        server.Post("/api/admin/updater",Access::Admin,[updater,can_control](const auto& request,auto& response){
            if(!updater)throw PortalError(409,"updateUnavailable");if(!can_control(request))throw PortalError(403,"networkReadOnly");json_response(response,updater->command(portal_body(request)),202);
        });
        server.Post("/api/admin/updater/upload",Access::Admin,[updater,can_control](const auto& request,auto& response){
            if(!updater)throw PortalError(409,"updateUnavailable");if(!can_control(request))throw PortalError(403,"networkReadOnly");
            if(request.get_header_value("Content-Type")!="application/octet-stream"||raw_parameter_count(request,"id")!=1||raw_parameter_count(request,"offset")!=1)throw PortalError(400,"portalInvalid");
            const auto position=request.get_param_value("offset");std::uint64_t offset{};const auto parsed=std::from_chars(position.data(),position.data()+position.size(),offset);
            if(parsed.ec!=std::errc{}||parsed.ptr!=position.data()+position.size())throw PortalError(400,"portalInvalid");
            json_response(response,updater->upload(request.get_param_value("id"),offset,request.body));
        });
        const bool prolink = features && features->mode == "prolink";
        server.Get("/api/app", Access::User, [prolink, network, portal, can_control, port, updater](const auto& request, auto& response) {
            const auto user=portal?portal->identity(portal_session(request)):Json(nullptr);
            const bool admin=!portal||(!user.is_null()&&user["role"]=="admin");
            json_response(response, {{"version", DECKSTATUS_VERSION}, {"mode", prolink ? "prolink" : "rekordbox"},
                {"obsBaseUrl", network_url("127.0.0.1", port)},
                {"user",user},
                {"update",updater?updater->summary():Json{{"available",false}}},
                {"canControl", can_control(request)},
                {"capabilities", {{"dashboard", true}, {"history", true}, {"deckOverlays", true}, {"masterOverlay", true},
                    {"networkSettings", network != nullptr && admin}, {"scenes",portal!=nullptr}, {"admin",portal!=nullptr && admin},
                    {"audioWaveform", true}, {"rekordboxSetup", !prolink}, {"prolinkSetup", prolink},
                    {"playbackStatus", prolink}, {"onAir", prolink}, {"mixerControls", false}, {"trackWaveform", false}}}});
        });
        server.Get("/api/prolink/devices", Access::User, [features, prolink](const auto&, auto& response) {
            if (!prolink || !features->prolink_setup) { json_response(response, {{"error", "modeUnavailable"}}, 409); return; }
            json_response(response, features->prolink_setup());
        });
        server.Get("/api/prolink/settings", Access::User, [prolink,portal](const auto&,auto& response) {
            if(!prolink||!portal){json_response(response,{{"error","modeUnavailable"}},409);return;}
            json_response(response,portal->prolink_settings());
        });
        server.Post("/api/prolink/settings", Access::User, [prolink,portal,features,can_control](const auto& request,auto& response) {
            if(!prolink||!portal||!features->prolink_configure){json_response(response,{{"error","modeUnavailable"}},409);return;}
            if(!can_control(request)){json_response(response,{{"error","networkReadOnly"}},403);return;}
            const auto saved=portal->save_prolink_settings(portal_body(request));features->prolink_configure(saved);json_response(response,saved);
        });
        server.Get("/api/rekordbox/status", Access::User, [prolink, &snapshot](const auto&, auto& response) {
            if (prolink) { json_response(response, {{"error", "modeUnavailable"}}, 409); return; }
            json_response(response, snapshot());
        });
        server.Post("/api/prolink/control", Access::User, [features, prolink, can_control](const auto& request, auto& response) {
            if (!prolink || !features->prolink_control) { json_response(response, {{"error", "modeUnavailable"}}, 409); return; }
            if (!can_control(request)) { json_response(response, {{"error", "networkReadOnly"}}, 403); return; }
            const auto type = lower_ascii(request.get_header_value("Content-Type"));
            if (type != "application/json" && type != "application/json; charset=utf-8") {
                json_response(response, {{"error", "JSON content type required"}}, 415); return;
            }
            const auto body = Json::parse(request.body, nullptr, false);
            if (!body.is_object()) { json_response(response, {{"error", "prolinkInvalidCommand"}}, 400); return; }
            const auto result = features->prolink_control(body);
            json_response(response, result, result.contains("error") ? 400 : 202);
        });
        server.Get("/api/network", Access::Admin, [network, local_request](const auto& request, auto& response) {
            if (!network) { json_response(response, {{"error", "networkUnavailable"}}, 503); return; }
            json_response(response, network->describe(local_request(request)));
        });
        server.Post("/api/network", Access::Admin, [features, local_request](const auto& request, auto& response) {
            if (!local_request(request)) { json_response(response, {{"error", "networkLocalOnly"}}, 403); return; }
            if (!features || !features->network) { json_response(response, {{"error", "networkUnavailable"}}, 503); return; }
            const auto type = lower_ascii(request.get_header_value("Content-Type"));
            if (type != "application/json" && type != "application/json; charset=utf-8") { json_response(response, {{"error", "JSON content type required"}}, 415); return; }
            try {
                features->network->save(Json::parse(request.body, nullptr, false));
                json_response(response, features->network->describe(true));
            } catch (const std::exception& error) {
                const std::string message = error.what();
                json_response(response, {{"error", message}}, message == "networkInvalidSettings" || message == "networkInvalidDomain" ? 400 : 500);
            }
        });
        server.Post(R"(/.*)", Access::User, reject_method);
        server.Put(R"(/.*)", Access::User, reject_method);
        server.Patch(R"(/.*)", Access::User, reject_method);
        server.Delete(R"(/.*)", Access::User, reject_method);
        server.Options(R"(/.*)", Access::User, reject_method);
        server.set_error_handler([](const auto&, auto& response) {
            if (response.body.empty()) {
                json_response(response, {{"error", response.status == 404 ? "Not found" : "Request failed"}},
                              response.status);
            }
        });

        // Catch only these literal paths (dots escaped for httplib's regex routes).
        for (const auto& [url, asset] : assets) {
            std::string pattern;
            for (char c : url) { if (c == '.') pattern += '\\'; pattern += c; }
            server.Get(pattern, asset.access, [asset,url](const httplib::Request& request, httplib::Response& response) {
                if(url=="/components/image")response.set_header("Content-Security-Policy","default-src 'self'; script-src 'self'; style-src 'self' 'unsafe-inline'; img-src 'self' https://upload.wikimedia.org; connect-src 'self' https://commons.wikimedia.org https://upload.wikimedia.org; object-src 'none'; base-uri 'none'; frame-ancestors 'self'; form-action 'none'");
                if (!asset.etag.empty()) {
                    // Replace, not append: the default headers are already in the response.
                    response.headers.erase("Cache-Control");
                    response.set_header("Cache-Control", "no-cache");
                    response.set_header("ETag", asset.etag);
                    if (request.get_header_value("If-None-Match") == asset.etag) { response.status = 304; return; }
                }
                response.set_content(asset.body, asset.mime);
            });
        }
        server.Get("/api/state", Access::Keyed, [&snapshot](const auto&, auto& response) { json_response(response, snapshot()); });
        server.Get("/api/master", Access::Keyed, [master](const auto&, auto& response) {
            if (!master) { json_response(response, {{"error", "Master feed is not available"}}, 503); return; }
            json_response(response, master->snapshot());
        });
        server.Get("/api/history", Access::Public, [master,portal,&twitch](const httplib::Request& request, auto& response) {
            if (!master) { json_response(response, {{"error", "History is not available"}}, 503); return; }
            std::uint64_t before = 0, page_size = 100;
            for (const auto* name : {"before", "limit"}) {
                if (!request.has_param(name)) continue;
                const auto input = request.get_param_value(name);
                auto& value = std::string_view(name) == "before" ? before : page_size;
                const auto [end, error] = std::from_chars(input.data(), input.data() + input.size(), value);
                if (raw_parameter_count(request, name) != 1 || input.empty() || error != std::errc{} ||
                    end != input.data() + input.size() || value == 0 || (std::string_view(name) == "limit" && value > 100)) {
                    json_response(response, {{"error", "Invalid history pagination"}}, 400); return;
                }
            }
            auto result=master->full_snapshot(before, static_cast<std::size_t>(page_size));
            if(portal){const auto viewer=twitch.viewer_identity(portal_cookie(request,"deckstatus_viewer"));result=portal->public_history(std::move(result),viewer.is_null()?std::string{}:"twitch:"+viewer["id"].get<std::string>());result["viewer"]=viewer;const auto admin=portal->identity(portal_session(request));result["canViewRatings"]=!admin.is_null()&&admin["role"]=="admin"&&!admin["mustChangePassword"].get<bool>();}
            json_response(response,result);
        });
        const auto cover_route = [master](bool history) {
            return [master,history](const httplib::Request& request, httplib::Response& response) {
                const auto input = request.matches[1].str();
                std::uint32_t id{};
                const auto [end, error] = std::from_chars(input.data(), input.data() + input.size(), id);
                if (!master || error != std::errc{} || end != input.data() + input.size()) {
                    json_response(response, {{"error", "Cover is not available"}}, 404); return;
                }
                auto [mime, bytes] = history ? master->history_cover(id) : master->cover(id);
                if (bytes.empty()) { json_response(response, {{"error", "Cover is not available"}}, 404); return; }
                if (mime != "image/jpeg" && mime != "image/png" && mime != "image/webp" && mime != "image/gif" && mime != "image/bmp") {
                    json_response(response, {{"error", "Cover format is not supported"}}, 415); return;
                }
                response.set_content(std::move(bytes), mime);
            };
        };
        server.Get(R"(/api/master/covers/([1-9][0-9]{0,9}))", Access::Keyed, cover_route(false));
        server.Get(R"(/api/history/covers/([1-9][0-9]{0,9}))", Access::Public, cover_route(true));
        server.Get("/api/decks", Access::User, [&snapshot](const auto&, auto& response) {
            json_response(response, decks_from(snapshot()));
        });
        server.Get(R"(/api/decks/([1-4]))", Access::User, [&snapshot](const httplib::Request& request,
                                                       httplib::Response& response) {
            const int deck_id = request.matches[1].str()[0] - '0';
            for (const auto& deck : decks_from(snapshot())) {
                if (deck.is_object() && deck.value("id", 0) == deck_id) {
                    json_response(response, deck);
                    return;
                }
            }
            json_response(response, {{"error", "Deck is not available"}}, 404);
        });
        server.Get(R"(/api/decks/([1-4])/cover)", Access::Keyed, [&cover, &snapshot](const httplib::Request& request,
                                                         httplib::Response& response) {
            const int deck_id = request.matches[1].str()[0] - '0';
            const auto before = loaded_track(snapshot(), deck_id);
            if (request.has_param("trackId")) {
                const auto input = request.get_param_value("trackId");
                std::uint64_t requested_id{};
                const auto [end, error] = std::from_chars(input.data(), input.data() + input.size(), requested_id);
                if (raw_parameter_count(request, "trackId") != 1 || input.empty() ||
                    error != std::errc{} || end != input.data() + input.size() || requested_id == 0) {
                    json_response(response, {{"error", "Invalid trackId"}}, 400);
                    return;
                }
                if (before != requested_id) {
                    json_response(response, {{"error", "Track is no longer loaded"}}, 404);
                    return;
                }
            }
            if (!before) {
                json_response(response, {{"error", "Deck is not loaded"}}, 404);
                return;
            }
            auto [mime_type, bytes] = cover(deck_id);
            // A database lookup may overlap a track change. Never label the next
            // track's artwork with the previous track's URL (or the reverse).
            if (loaded_track(snapshot(), deck_id) != before) {
                json_response(response, {{"error", "Track changed while loading its cover"}}, 404);
                return;
            }
            if (bytes.empty()) {
                json_response(response, {{"error", "Cover is not available"}}, 404);
                return;
            }
            if (mime_type != "image/jpeg" && mime_type != "image/png" &&
                mime_type != "image/webp" && mime_type != "image/gif" &&
                mime_type != "image/bmp") {
                json_response(response, {{"error", "Cover format is not supported"}}, 415);
                return;
            }
            response.set_content(std::move(bytes), mime_type);
        });
        server.Get("/api/health", Access::User, [&snapshot](const auto&, auto& response) {
            const auto state = snapshot();
            const auto status = state.value("status", std::string("disconnected"));
            const bool healthy = status == "connected" || status == "demo";
            json_response(response, {
                {"ok", healthy}, {"status", status},
                {"message", state.value("message", std::string{})},
                {"version", state.value("version", Json(nullptr))}
            }, healthy ? 200 : 503);
        });

    };
    PortalServer server;
    configure(server);
    std::unique_ptr<PortalServer> loopback;
    if (host != "0.0.0.0" && host != "127.0.0.1" && host != "::") {
        loopback = std::make_unique<PortalServer>();
        configure(*loopback);
    }
    if (!server.bind_to_port(host, port)) {
        std::cerr << tr("Could not bind web server to ") << host << ':' << port << '\n';
        return 1;
    }
    if (loopback && !loopback->bind_to_port("127.0.0.1", port)) {
        std::cerr << tr("Could not bind web server to ") << "127.0.0.1:" << port << '\n';
        return 1;
    }

    // Persisted opt-in is evaluated once, after both sockets bind successfully.
    if(!stop.load())audio_control.start_on_launch();
    if(!stop.load())twitch.start();
    // Wait for the listener before stopping it: httplib::stop() is a no-op before
    // listen_after_bind() starts. Keep the monitor alive until the listener exits.
    std::atomic_bool shutting_down{};
    std::jthread monitor([&](std::stop_token token) {
        while (!token.stop_requested()) {
            if (stop.load() || shutting_down.load()) {
                if (server.is_running()) server.stop();
                if (loopback && loopback->is_running()) loopback->stop();
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    });
    bool loopback_listened = true;
    std::jthread loopback_thread;
    if (loopback) loopback_thread = std::jthread([&] {
        loopback_listened = loopback->listen_after_bind();
        shutting_down = true;
    });
    std::cout << "Dashboard: " << network_url(host == "0.0.0.0" ? "127.0.0.1" : host, port) << "/\n";
    if (network) {
        const auto config = network->describe(true);
        for (const auto& url : config["urls"]) std::cout << "Server: " << url.get<std::string>() << "/\n";
    }
    const bool listened = server.listen_after_bind();
    shutting_down = true;
    if (loopback_thread.joinable()) loopback_thread.join();
    monitor.request_stop();
    return listened && loopback_listened ? 0 : 1;
}

} // namespace deckstatus
