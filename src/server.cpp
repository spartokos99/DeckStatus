#include "server.h"
#include "audio_capture.h"
#include "network.h"
#include "portal_http.h"
#include "language.h"
#include <map>
#include "master_history.h"

#include <httplib/httplib.h>

#include <algorithm>
#include <chrono>
#include <charconv>
#include <cctype>
#include <cstdint>
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
    struct Asset { std::string body; std::string mime; };
    std::map<std::string, Asset> assets;
    for (const auto& [url, file] : std::initializer_list<std::pair<const char*, const char*>>{
        {"/", "index.html"}, {"/index.html", "index.html"},
        {"/overlay", "overlay.html"}, {"/overlay.html", "overlay.html"},
        {"/master-overlay", "master-overlay.html"},
        {"/master-overlay/settings", "master-settings.html"}, {"/overlay/settings", "master-settings.html"},
        {"/master-overlay.js", "master-overlay.js"}, {"/master-options.js", "master-options.js"},
        {"/deck-overlay.js", "deck-overlay.js"}, {"/overlay-shared.js", "overlay-shared.js"},
        {"/overlay.css", "overlay.css"}, {"/settings.css", "settings.css"},
        {"/settings.js", "settings.js"}, {"/i18n.js", "i18n.js"}, {"/storage.js", "storage.js"},
        {"/waveform", "waveform.html"}, {"/waveform/settings", "waveform-settings.html"},
        {"/waveform.js", "waveform.js"}, {"/waveform-options.js", "waveform-options.js"},
        {"/waveform-renderer.js", "waveform-renderer.js"}, {"/waveform-settings.js", "waveform-settings.js"},
        {"/icon.svg", "icon.svg"},
        {"/history", "history.html"}, {"/history.js", "history.js"}, {"/history.css", "history.css"},
        {"/navigation.js", "navigation.js"}, {"/navigation.css", "navigation.css"},
        {"/prolink/settings", "prolink-settings.html"}, {"/prolink-settings.js", "prolink-settings.js"},
        {"/connection.css", "connection.css"}, {"/rekordbox/settings", "rekordbox-settings.html"}, {"/rekordbox-settings.js", "rekordbox-settings.js"},
        {"/network/settings", "network-settings.html"}, {"/network-settings.js", "network-settings.js"}, {"/network-settings.css", "network-settings.css"},
        {"/login", "login.html"}, {"/account/password", "password.html"}, {"/auth.js", "auth.js"}, {"/portal.css", "portal.css"},
        {"/admin", "admin.html"}, {"/admin.js", "admin.js"}, {"/broadcast.js", "broadcast.js"},
        {"/scenes", "scene-editor.html"}, {"/scene-editor.js", "scene-editor.js"}, {"/scene-editor.css", "scene-editor.css"},
        {"/component-presets.js", "component-presets.js"}, {"/component-presets.css", "component-presets.css"},
        {"/scene", "scene.html"}, {"/scene.js", "scene.js"}, {"/scene-shared.js", "scene-shared.js"},
        {"/locales/en.json", "locales/en.json"}, {"/locales/de.json", "locales/de.json"}
    }) {
        const auto body = read_page(web_root / file);
        if (body.empty()) { std::cerr << tr("Web assets missing in: ") << web_root << '\n'; return 1; }
        const auto extension = std::filesystem::path(file).extension();
        assets.emplace(url, Asset{body, extension == ".js" ? "text/javascript; charset=utf-8" :
            extension == ".svg" ? "image/svg+xml" : extension == ".css" ? "text/css; charset=utf-8" : extension == ".json" ? "application/json; charset=utf-8" : "text/html; charset=utf-8"});
    }

    AudioCapture audio;
    PortalServer server;
    auto* portal = features ? features->portal : nullptr;
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
    server.set_payload_max_length(65536);
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

    const auto allowed = allowed_authorities(host, port);
    server.set_pre_routing_handler([allowed, host, port](const httplib::Request& request,
                                             httplib::Response& response) {
        // Accept the socket's destination IP on a wildcard listener, not arbitrary
        // Host names or forwarded headers supplied by a client.
        const auto requested = lower_ascii(request.get_header_value("Host"));
        const bool destination = host == "0.0.0.0" && valid_bind_address(request.local_addr) &&
            (requested == authority(request.local_addr, port) || (port == 80 && requested == request.local_addr + ":80"));
        if (request.get_header_value_count("Host") != 1 ||
            (!allowed.contains(requested) && !destination)) {
            json_response(response, {{"error", "Host is not allowed"}}, 403);
            return httplib::Server::HandlerResponse::Handled;
        }
        if (request.get_header_value_count("Origin") > 1) {
            json_response(response, {{"error", "Origin is not allowed"}}, 403);
            return httplib::Server::HandlerResponse::Handled;
        }
        if (request.has_header("Origin")) {
            const auto origin = lower_ascii(request.get_header_value("Origin"));
            const auto expected = "http://" + lower_ascii(request.get_header_value("Host"));
            if (origin != expected) {
                json_response(response, {{"error", "Cross-origin requests are not allowed"}}, 403);
                return httplib::Server::HandlerResponse::Handled;
            }
        }
        const bool public_navigation = (request.method == "GET" || request.method == "HEAD") &&
            (request.path == "/history" || request.path == "/login") &&
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
    server.authorize = [portal, &assets](const httplib::Request& request, auto& response) {
        if (!portal) return true; // Isolated native fixtures may omit the application store.
        const auto& path = request.path;
        const bool read = request.method == "GET" || request.method == "HEAD";
        const auto asset = assets.find(path);
        if (read && asset != assets.end() && !asset->second.mime.starts_with("text/html")) return true;
        if (read && (path == "/login" || path == "/history" || path == "/api/history" || path.starts_with("/api/history/covers/") || path == "/api/auth/me")) return true;
        if (request.method == "POST" && (path == "/api/auth/login" || path == "/api/auth/logout" || path == "/api/public/rating")) return true;
        if (read && raw_parameter_count(request,"key") == 1 && raw_parameter_count(request,"scene") <= 1 && portal->broadcast_access(request.get_param_value("key"),path,request.get_param_value("scene"))) return true;
        const auto user = portal->identity(portal_session(request));
        if (user.is_null()) {
            if (read && asset != assets.end()) { response.set_redirect("/login",303); return false; }
            throw PortalError(401,"authRequired");
        }
        if (user["mustChangePassword"].get<bool>() && path != "/account/password" && path != "/api/auth/password") {
            if (read && asset != assets.end()) { response.set_redirect("/account/password",303); return false; }
            throw PortalError(403,"authPasswordRequired");
        }
        if ((path == "/admin" || path.starts_with("/api/admin/") || path == "/network/settings" || path == "/api/network") && user["role"] != "admin") throw PortalError(403,"authAdminRequired");
        return true;
    };
    if (portal) {
        server.Get("/api/auth/me",[portal](const auto& request,auto& response){json_response(response,{{"user",portal->identity(portal_session(request))}});});
        server.Post("/api/auth/login",[portal](const auto& request,auto& response){
            const auto body=portal_body(request);if(!body.contains("username")||!body["username"].is_string()||!body.contains("password")||!body["password"].is_string())throw PortalError(400,"portalInvalid");
            auto result=portal->login(body["username"],body["password"],request.remote_addr);portal->logout(portal_session(request));session_cookie(response,result["session"]);result.erase("session");json_response(response,result);
        });
        server.Post("/api/auth/logout",[portal](const auto& request,auto& response){portal_body(request);portal->logout(portal_session(request));session_cookie(response,"");json_response(response,{{"ok",true}});});
        server.Post("/api/auth/password",[portal](const auto& request,auto& response){portal->change_password(portal_session(request),portal_body(request));session_cookie(response,"");json_response(response,{{"ok",true}});});
        server.Get("/api/admin/users",[portal](const auto&,auto& response){json_response(response,{{"users",portal->users()}});});
        server.Post("/api/admin/users",[portal](const auto& request,auto& response){const auto user=portal->identity(portal_session(request));json_response(response,{{"users",portal->edit_user(user["id"],portal_body(request))}});});
        server.Get("/api/admin/ratings",[portal](const auto&,auto& response){json_response(response,{{"tracks",portal->ratings()}});});
        server.Get("/api/broadcast",[portal](const auto&,auto& response){json_response(response,portal->overlay_keys(false));});
        server.Post("/api/admin/broadcast",[portal](const auto& request,auto& response){portal_body(request);json_response(response,portal->overlay_keys(true));});
        server.Get("/api/scenes",[portal](const auto&,auto& response){json_response(response,{{"scenes",portal->scenes()}});});
        server.Get("/api/presets",[portal](const auto&,auto& response){json_response(response,{{"presets",portal->presets()}});});
        server.Post("/api/presets",[portal](const auto& request,auto& response){json_response(response,portal->edit_preset(portal_body(request)));});
        server.Post("/api/scenes",[portal](const auto& request,auto& response){json_response(response,portal->edit_scene(portal_body(request)));});
        server.Get("/api/scene",[portal](const httplib::Request& request,auto& response){if(raw_parameter_count(request,"scene")!=1)throw PortalError(400,"sceneInvalid");json_response(response,portal->scene(request.get_param_value("scene")));});
        server.Post("/api/public/rating",[portal](const auto& request,auto& response){
            const auto old=portal_cookie(request,"deckstatus_voter"),voter=portal->visitor(old);
            if(old!=voter)throw PortalError(400,"ratingVisitHistory");
            json_response(response,portal->rate(voter,request.remote_addr,portal_body(request)));
        });
    }
    server.Get("/api/audio/devices", [&](const auto&, auto& response) { json_response(response, audio.devices()); });
    server.Get("/api/audio/state", [&](const auto&, auto& response) { json_response(response, audio.state()); });
    const auto* network = features ? features->network : nullptr;
    const bool remote_control = network && network->active().allow_remote_control;
    const auto can_control = [remote_control](const httplib::Request& request) {
        return local_network_peer(request.remote_addr, request.local_addr) || may_control_network(request.remote_addr, remote_control);
    };
    server.Post("/api/audio/source", [&audio, can_control](const auto& request, auto& response) {
        if (!can_control(request)) { json_response(response, {{"error", "networkReadOnly"}}, 403); return; }
        const auto type = lower_ascii(request.get_header_value("Content-Type"));
        if (type != "application/json" && type != "application/json; charset=utf-8") {
            json_response(response, {{"error", "JSON content type required"}}, 415); return;
        }
        const auto body = Json::parse(request.body, nullptr, false);
        if (!body.is_object() || body.size() != 1 || !body.contains("deviceId") || !body["deviceId"].is_string()) {
            json_response(response, {{"error", "Expected a deviceId string; empty string stops capture"}}, 400); return;
        }
        if (!audio.select(body["deviceId"].template get<std::string>())) {
            json_response(response, {{"error", "audioDeviceLost"}}, 400); return;
        }
        json_response(response, audio.state());
    });
    const bool prolink = features && features->mode == "prolink";
    server.Get("/api/app", [prolink, network, portal, can_control](const auto& request, auto& response) {
        const auto user=portal?portal->identity(portal_session(request)):Json(nullptr);
        const bool admin=!portal||(!user.is_null()&&user["role"]=="admin");
        json_response(response, {{"version", "2.0.1"}, {"mode", prolink ? "prolink" : "rekordbox"},
            {"user",user},
            {"canControl", can_control(request)},
            {"capabilities", {{"dashboard", true}, {"history", true}, {"deckOverlays", true}, {"masterOverlay", true},
                {"networkSettings", network != nullptr && admin}, {"scenes",portal!=nullptr}, {"admin",portal!=nullptr && admin},
                {"audioWaveform", true}, {"rekordboxSetup", !prolink}, {"prolinkSetup", prolink},
                {"playbackStatus", prolink}, {"onAir", prolink}, {"mixerControls", false}, {"trackWaveform", false}}}});
    });
    server.Get("/api/prolink/devices", [features, prolink](const auto&, auto& response) {
        if (!prolink || !features->prolink_setup) { json_response(response, {{"error", "modeUnavailable"}}, 409); return; }
        json_response(response, features->prolink_setup());
    });
    server.Get("/api/rekordbox/status", [prolink, &snapshot](const auto&, auto& response) {
        if (prolink) { json_response(response, {{"error", "modeUnavailable"}}, 409); return; }
        json_response(response, snapshot());
    });
    server.Post("/api/prolink/control", [features, prolink, can_control](const auto& request, auto& response) {
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
    server.Get("/api/network", [network](const auto& request, auto& response) {
        if (!network) { json_response(response, {{"error", "networkUnavailable"}}, 503); return; }
        json_response(response, network->describe(local_network_peer(request.remote_addr, request.local_addr)));
    });
    server.Post("/api/network", [features](const auto& request, auto& response) {
        if (!local_network_peer(request.remote_addr, request.local_addr)) { json_response(response, {{"error", "networkLocalOnly"}}, 403); return; }
        if (!features || !features->network) { json_response(response, {{"error", "networkUnavailable"}}, 503); return; }
        const auto type = lower_ascii(request.get_header_value("Content-Type"));
        if (type != "application/json" && type != "application/json; charset=utf-8") { json_response(response, {{"error", "JSON content type required"}}, 415); return; }
        try {
            features->network->save(Json::parse(request.body, nullptr, false));
            json_response(response, features->network->describe(true));
        } catch (const std::exception& error) {
            json_response(response, {{"error", error.what()}}, std::string(error.what()) == "networkInvalidSettings" ? 400 : 500);
        }
    });
    server.Post(R"(/.*)", reject_method);
    server.Put(R"(/.*)", reject_method);
    server.Patch(R"(/.*)", reject_method);
    server.Delete(R"(/.*)", reject_method);
    server.Options(R"(/.*)", reject_method);
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
        server.Get(pattern, [asset](const auto&, auto& response) { response.set_content(asset.body, asset.mime); });
    }
    server.Get("/api/state", [&snapshot](const auto&, auto& response) { json_response(response, snapshot()); });
    server.Get("/api/master", [master](const auto&, auto& response) {
        if (!master) { json_response(response, {{"error", "Master feed is not available"}}, 503); return; }
        json_response(response, master->snapshot());
    });
    server.Get("/api/history", [master,portal](const httplib::Request& request, auto& response) {
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
        if(portal){const auto old=portal_cookie(request,"deckstatus_voter"),voter=portal->visitor(old);if(old!=voter)response.set_header("Set-Cookie","deckstatus_voter="+voter+"; Path=/; HttpOnly; SameSite=Strict; Max-Age=31536000");result=portal->public_history(std::move(result),voter);}
        json_response(response,result);
    });
    server.Get(R"(/api/(master|history)/covers/([1-9][0-9]{0,9}))", [master](const httplib::Request& request, auto& response) {
        const auto input = request.matches[2].str();
        std::uint32_t id{};
        const auto [end, error] = std::from_chars(input.data(), input.data() + input.size(), id);
        if (!master || error != std::errc{} || end != input.data() + input.size()) {
            json_response(response, {{"error", "Master cover is not available"}}, 404); return;
        }
        auto [mime, bytes] = request.matches[1].str() == "history" ? master->history_cover(id) : master->cover(id);
        if (bytes.empty()) { json_response(response, {{"error", "Master cover is not available"}}, 404); return; }
        if (mime != "image/jpeg" && mime != "image/png" && mime != "image/webp" && mime != "image/gif" && mime != "image/bmp") {
            json_response(response, {{"error", "Cover format is not supported"}}, 415); return;
        }
        response.set_content(std::move(bytes), mime);
    });
    server.Get("/api/decks", [&snapshot](const auto&, auto& response) {
        json_response(response, decks_from(snapshot()));
    });
    server.Get(R"(/api/decks/([1-4]))", [&snapshot](const httplib::Request& request,
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
    server.Get(R"(/api/decks/([1-4])/cover)", [&cover, &snapshot](const httplib::Request& request,
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
    server.Get("/api/health", [&snapshot](const auto&, auto& response) {
        const auto state = snapshot();
        const auto status = state.value("status", std::string("disconnected"));
        const bool healthy = status == "connected" || status == "demo";
        json_response(response, {
            {"ok", healthy}, {"status", status},
            {"message", state.value("message", std::string{})},
            {"version", state.value("version", Json(nullptr))}
        }, healthy ? 200 : 503);
    });

    if (!server.bind_to_port(host, port)) {
        std::cerr << tr("Could not bind web server to ") << host << ':' << port << '\n';
        return 1;
    }

    // Wait for the listener before stopping it: httplib::stop() is a no-op before
    // listen_after_bind() starts. Keep the monitor alive until the listener exits.
    std::jthread monitor([&](std::stop_token token) {
        while (!token.stop_requested()) {
            if (stop.load() && server.is_running()) {
                server.stop();
                return;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    });
    std::cout << "Dashboard: " << network_url(host == "0.0.0.0" ? "127.0.0.1" : host, port) << "/\n";
    if (network) {
        const auto config = network->describe(true);
        for (const auto& url : config["urls"]) std::cout << "Server: " << url.get<std::string>() << "/\n";
    }
    const bool listened = server.listen_after_bind();
    monitor.request_stop();
    return listened ? 0 : 1;
}

} // namespace deckstatus
