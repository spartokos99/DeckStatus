#include "server.h"
#include "master_history.h"

#include <httplib/httplib.h>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <thread>

namespace {
using Json = nlohmann::json;

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

void expect_status(const httplib::Result& result, int status, const char* message) {
    if (!result) throw std::runtime_error(std::string(message) + ": transport error " + httplib::to_string(result.error()));
    if (result->status != status) throw std::runtime_error(std::string(message) + ": expected " +
        std::to_string(status) + ", received " + std::to_string(result->status));
}

int available_port() {
    // Reserve a port from the OS, then close the reservation before our server binds.
    // This httplib version closes its socket in stop() only after listening starts.
    httplib::Server reservation;
    reservation.new_task_queue = [] { return new httplib::ThreadPool(1); };
    const int port = reservation.bind_to_any_port("127.0.0.1");
    require(port > 0, "Could not reserve a test port");
    std::jthread listener([&] { reservation.listen_after_bind(); });
    reservation.wait_until_ready();
    reservation.stop();
    listener.join();
    return port;
}

struct TestServer {
    std::atomic_bool stop{};
    std::atomic_bool done{};
    std::atomic_int result{-1};
    deckstatus::MasterHistory master;
    std::jthread thread;

    TestServer(int port, const std::filesystem::path& root,
               std::function<Json()> snapshot,
               std::function<std::pair<std::string, std::string>(int)> cover)
        : master([cover](std::uint32_t id) { return id == 11 ? cover(1) : deckstatus::MasterHistory::Cover{}; }),
          thread([this, port, root, snapshot, cover] {
            result = deckstatus::run_server("127.0.0.1", port, root, snapshot, cover, stop, &master);
            done = true;
        }) {}

    ~TestServer() { stop = true; }
};
}

int main(int argc, char** argv) {
    try {
        require(argc == 2, "Usage: server_test WEB_DIRECTORY");
        const std::filesystem::path web_root = argv[1];
        std::mutex mutex;
        const std::string title = "Quote \" / slash \\ / newline\n / <script>alert(1)</script> / M\xc3\xb6" "bius";
        Json state = {
            {"schemaVersion", 1}, {"status", "connected"}, {"message", "ready"},
            {"version", "7.0.8"}, {"demo", false}, {"updatedAt", 1726000000000LL},
            {"sampleAgeMs", 5}, {"artworkStatus", "ready"},
            {"decks", Json::array({{
                {"id", 1}, {"trackId", 11}, {"loaded", true}, {"metadataAvailable", true},
                {"title", title}, {"artist", std::string("invalid: ") + '\xff'},
                {"album", nullptr}, {"bpm", 128.5}, {"originalBpm", nullptr},
                {"key", nullptr}, {"genre", nullptr}, {"label", nullptr},
                {"coverUrl", "/api/decks/1/cover?trackId=11"}
            }})}
        };
        std::string cover_type = "image/png";
        const std::string binary("\x89PNG\0\x01\x02", 7);
        std::string cover_data = binary;
        bool change_track_during_cover = false;
        bool fail_snapshot = false;
        int cover_calls = 0;
        auto snapshot = [&] {
            std::lock_guard lock(mutex);
            if (fail_snapshot) throw std::runtime_error("Synthetic snapshot failure");
            return state;
        };
        auto cover = [&](int deck) -> std::pair<std::string, std::string> {
            std::lock_guard lock(mutex);
            ++cover_calls;
            if (change_track_during_cover) state["decks"][0]["trackId"] = 12;
            return deck == 1 ? std::pair{cover_type, cover_data} : std::pair<std::string, std::string>{};
        };
        const int port = available_port();
        TestServer server(port, web_root, snapshot, cover);
        httplib::Client client("127.0.0.1", port);
        client.set_connection_timeout(0, 100000);
        client.set_read_timeout(2);
        bool ready = false;
        for (int attempt = 0; attempt < 200 && !server.done; ++attempt) {
            if (const auto response = client.Get("/api/health"); response && response->status == 200) {
                ready = true;
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        require(ready, "Server did not start");

        const auto audio = client.Get("/api/audio/state");
        expect_status(audio, 200, "Audio state missing");
        require(Json::parse(audio->body)["status"] == "stopped", "Audio must not start on server launch");
        expect_status(client.Get("/api/audio/devices"), 200, "Audio device enumeration missing");
        expect_status(client.Post("/api/audio/source", R"({"deviceId":""})", "application/json"), 200, "Explicit audio stop failed");
        expect_status(client.Post("/api/audio/source", R"({"deviceId":"missing-test-device"})", "application/json"), 400, "Unknown source accepted");
        expect_status(client.Post("/api/audio/source", R"({"deviceId":42})", "application/json"), 400, "Invalid source schema accepted");
        expect_status(client.Post("/api/audio/source", "{", "application/json"), 400, "Malformed audio JSON accepted");
        expect_status(client.Post("/api/audio/source", R"({"deviceId":""})", "text/plain"), 415, "Simple cross-origin form type accepted");
        expect_status(client.Get("/api/audio/source?deviceId=anything"), 404, "GET must not control audio capture");
        const httplib::Headers other_origin{{"Origin", "http://example.com"}};
        expect_status(client.Post("/api/audio/source", other_origin, "", "application/json"), 403, "Cross-origin audio control accepted");
        require(Json::parse(client.Get("/api/audio/state")->body)["status"] == "stopped", "Invalid requests changed capture state");

        const auto response = client.Get("/api/state");
        expect_status(response, 200, "State should be available");
        const auto parsed = Json::parse(response->body);
        require(parsed["decks"][0]["title"] == title, "Track title was not escaped losslessly");
        require(parsed["decks"][0]["artist"] == "invalid: \xef\xbf\xbd", "Invalid UTF-8 must be replaced");
        require(parsed["decks"][0]["album"].is_null(), "Missing metadata must remain null");
        require(parsed["decks"][0]["bpm"] == 128.5, "BPM must remain numeric");
        require(response->get_header_value("Content-Type").starts_with("application/json"), "JSON MIME missing");
        require(response->get_header_value("Cache-Control") == "no-store", "State must not be cached");
        require(response->get_header_value("X-Content-Type-Options") == "nosniff", "MIME protection missing");

        const auto decks = client.Get("/api/decks");
        expect_status(decks, 200, "Deck list missing");
        require(Json::parse(decks->body).is_array(), "Deck list should be an array");
        const auto deck = client.Get("/api/decks/1");
        expect_status(deck, 200, "Individual deck missing");
        require(Json::parse(deck->body)["trackId"] == 11, "Wrong individual deck");
        for (const char* path : {"/api/decks/0", "/api/decks/2", "/api/decks/5", "/api/decks/one", "/src/deckstatus.cpp", "/missing"})
            expect_status(client.Get(path), 404, "Unknown route or absent deck should return 404");

        for (const char* path : {"/", "/index.html", "/overlay", "/overlay.html", "/overlay?deck=4",
                                 "/master-overlay?history=3&fields=title,cover", "/master-overlay/settings", "/overlay/settings", "/waveform", "/waveform/settings"}) {
            const auto page = client.Get(path);
            expect_status(page, 200, "Web page missing");
            require(page->get_header_value("Content-Type").starts_with("text/html"), "HTML MIME missing");
            require(page->body.find("<!doctype html>") != std::string::npos, "HTML document missing");
        }
        for (const char* path : {"/master-overlay.js", "/master-options.js", "/deck-overlay.js",
                                 "/overlay-shared.js", "/settings.js", "/i18n.js", "/storage.js", "/waveform.js", "/waveform-settings.js", "/waveform-options.js", "/waveform-renderer.js"}) {
            const auto script = client.Get(path);
            expect_status(script, 200, "Master script missing");
            require(script->get_header_value("Content-Type").starts_with("text/javascript"), "Module MIME missing");
        }
        for (const char* path : {"/overlay.css", "/settings.css"}) {
            const auto sheet = client.Get(path);
            expect_status(sheet, 200, "Stylesheet missing");
            require(sheet->get_header_value("Content-Type").starts_with("text/css"), "Stylesheet MIME missing");
        }
        for (const char* path : {"/locales/en.json", "/locales/de.json"}) {
            const auto language = client.Get(path);
            expect_status(language, 200, "Language file missing");
            require(language->get_header_value("Content-Type").starts_with("application/json"), "Language MIME missing");
            require(Json::parse(language->body).contains("timeline"), "Language dictionary incomplete");
        }
        expect_status(client.Get("/locales/fr.json"), 404, "Unlisted language route accepted");
        expect_status(client.Get("/overlayXcss"), 404, "Asset dot treated as regex wildcard");
        auto master_state = state;
        master_state["masterDeckId"] = 1;
        server.master.update(master_state);
        master_state["decks"][0]["trackId"] = 12;
        server.master.update(master_state);
        const auto master_response = client.Get("/api/master");
        expect_status(master_response, 200, "Master API missing");
        require(Json::parse(master_response->body)["history"][0]["trackId"] == 11, "Master history not returned");
        expect_status(client.Get("/api/master/covers/11"), 200, "Historical cover missing");
        for (const char* id : {"0", "-1", "4294967296", "999", "11x", "0011"})
            expect_status(client.Get(std::string("/api/master/covers/") + id), 404, "Invalid or unseen master cover should be rejected");
        { std::lock_guard lock(mutex); cover_type = "text/html"; }
        expect_status(client.Get("/api/master/covers/11"), 415, "Active content accepted as history cover");
        { std::lock_guard lock(mutex); cover_type = "image/png"; }
        const auto head = client.Head("/api/state");
        expect_status(head, 200, "HEAD should work");
        require(head->body.empty(), "HEAD must not return a body");
        expect_status(client.Post("/api/state", "{}", "application/json"), 405, "POST should be rejected");
        expect_status(client.Put("/api/state", "{}", "application/json"), 405, "PUT should be rejected");
        expect_status(client.Patch("/api/state", "{}", "application/json"), 405, "PATCH should be rejected");
        expect_status(client.Delete("/api/state", "{}", "application/json"), 405, "DELETE should be rejected");
        expect_status(client.Options("/api/state"), 405, "OPTIONS should be rejected");
        httplib::Client persistent("127.0.0.1", port);
        persistent.set_keep_alive(true);
        persistent.set_read_timeout(2);
        for (int i = 0; i < 20; ++i) {
            const auto rejected = persistent.Post("/api/state", "{\"ignored\":true}", "application/json");
            expect_status(rejected, 405, "POST body should be consumed before rejection");
            require(rejected->get_header_value("Allow") == "GET, HEAD", "Allowed methods missing");
            expect_status(persistent.Get("/api/state"), 200, "Rejected body corrupted the next request");
        }
        persistent.stop();

        const auto image = client.Get("/api/decks/1/cover?trackId=11");
        expect_status(image, 200, "Loaded track cover missing");
        require(image->body == binary, "Cover bytes were changed");
        require(image->get_header_value("Content-Type") == "image/png", "Cover MIME incorrect");
        expect_status(client.Get("/api/decks/1/cover"), 200, "Cover without trackId should use current track");
        expect_status(client.Get("/api/decks/2/cover"), 404, "Empty deck must not return a cover");
        int calls_before;
        { std::lock_guard lock(mutex); calls_before = cover_calls; }
        expect_status(client.Get("/api/decks/1/cover?trackId=12"), 404, "Old cover URL should be rejected");
        for (const char* query : {"", "0", "-1", "1x", "18446744073709551616", "11&trackId=11",
                                 "11&trackId=12", "11&track%49d=11"}) {
            expect_status(client.Get(std::string("/api/decks/1/cover?trackId=") + query), 400,
                          ("Malformed trackId should be rejected: " + std::string(query)).c_str());
        }
        { std::lock_guard lock(mutex); require(cover_calls == calls_before, "Invalid trackId performed a cover lookup"); cover_data.clear(); }
        expect_status(client.Get("/api/decks/1/cover?trackId=11"), 404, "Missing cover should return 404");
        { std::lock_guard lock(mutex); cover_data = binary; cover_type = "text/html"; }
        expect_status(client.Get("/api/decks/1/cover?trackId=11"), 415, "Active content must not be served as artwork");
        { std::lock_guard lock(mutex); cover_type = "image/png"; change_track_during_cover = true; }
        expect_status(client.Get("/api/decks/1/cover?trackId=11"), 404, "Track change during lookup returned mismatched cover");
        { std::lock_guard lock(mutex); change_track_during_cover = false; state["decks"][0]["trackId"] = 11; }

        const auto host = "127.0.0.1:" + std::to_string(port);
        expect_status(client.Get("/api/state", {{"Host", "evil.example:" + std::to_string(port)}}), 403, "Unknown Host should be rejected");
        expect_status(client.Get("/api/state", {{"Host", host}, {"Host", host}}), 403, "Duplicate Host should be rejected");
        expect_status(client.Get("/api/state", {{"Origin", "https://evil.example"}}), 403, "External origin should be rejected");
        expect_status(client.Get("/api/state", {{"Origin", "null"}}), 403, "Opaque origin should be rejected");
        expect_status(client.Get("/api/state", {{"Origin", "http://" + host}, {"Origin", "http://" + host}}), 403, "Duplicate origin should be rejected");
        expect_status(client.Get("/api/state", {{"Origin", "http://" + host}}), 200, "Matching origin should be accepted");
        expect_status(client.Get("/api/state", {{"Host", "localhost:" + std::to_string(port)}}), 200, "Localhost alias should be accepted");
        expect_status(client.Get("/api/state", {{"Sec-Fetch-Site", "cross-site"}}), 403, "Cross-site fetch should be rejected");

        { std::lock_guard lock(mutex); state["status"] = "disconnected"; }
        const auto unhealthy = client.Get("/api/health");
        expect_status(unhealthy, 503, "Disconnected bridge should be unhealthy");
        require(Json::parse(unhealthy->body)["ok"] == false, "Health should report false");
        expect_status(client.Get("/api/state"), 200, "Disconnected state should remain inspectable");
        { std::lock_guard lock(mutex); state["status"] = "demo"; }
        expect_status(client.Get("/api/health"), 200, "Demo should serve healthy HTTP");
        { std::lock_guard lock(mutex); fail_snapshot = true; }
        expect_status(client.Get("/api/state"), 500, "Snapshot exception should become a JSON error");
        { std::lock_guard lock(mutex); fail_snapshot = false; }
        expect_status(client.Get("/api/state"), 200, "Server should survive a snapshot exception");

        std::atomic_bool already_stopped{true};
        require(deckstatus::run_server("127.0.0.1", port, web_root, snapshot, cover, already_stopped) == 0,
                "Server should accept cancellation before startup");
        std::atomic_bool not_stopped{};
        require(deckstatus::run_server("127.0.0.1", 0, web_root, snapshot, cover, not_stopped) == 1,
                "Invalid port should be rejected");
        require(deckstatus::run_server("127.0.0.1", port, web_root / "missing-assets", snapshot, cover, not_stopped) == 1,
                "Missing assets should fail startup");
        require(deckstatus::run_server("127.0.0.1", port, web_root, snapshot, cover, not_stopped) == 1,
                "Occupied port should fail startup");

        server.stop = true;
        for (int attempt = 0; attempt < 300 && !server.done; ++attempt)
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        require(server.done, "Server did not shut down within 3 seconds");
        require(server.result == 0, "Normal shutdown should succeed");
        std::cout << "HTTP routing, JSON, artwork races, request origin, health and shutdown passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Server test failed: " << error.what() << '\n';
        return 1;
    }
}
