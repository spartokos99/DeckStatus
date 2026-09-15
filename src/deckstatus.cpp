#include "injector.h"
#include "language.h"
#include "artwork.h"
#include "server.h"
#include "master_history.h"
#include "prolink.h"
#include "network.h"
#include "portal.h"
#include <nlohmann/json.hpp>
#include <atomic>
#include <array>
#include <charconv>
#include <chrono>
#include <iostream>
#include <mutex>
#include <thread>

namespace {
std::atomic_bool stopping{};
BOOL WINAPI on_console(DWORD signal) {
    if (signal == CTRL_C_EVENT || signal == CTRL_BREAK_EVENT || signal == CTRL_CLOSE_EVENT) {
        stopping = true;
        return TRUE;
    }
    return FALSE;
}

std::filesystem::path executable_directory() {
    wchar_t path[32768]{};
    if (!GetModuleFileNameW(nullptr, path, static_cast<DWORD>(std::size(path))))
        throw std::runtime_error("Programmpfad nicht ermittelbar.");
    return std::filesystem::path(path).parent_path();
}

unsigned number(const std::wstring& input, unsigned max) {
    if (input.empty() || input.find_first_not_of(L"0123456789") != std::wstring::npos)
        throw std::runtime_error("Ungueltige numerische Option.");
    const auto value = std::stoull(input);
    if (!value || value > max) throw std::runtime_error("Numerische Option ausserhalb des gueltigen Bereichs.");
    return static_cast<unsigned>(value);
}

template<std::size_t N> std::string safe_string(const char (&input)[N]) {
    const std::string text(input, strnlen_s(input, N));
    // Round-trip with replacement ensures malformed/truncated native UTF-8 cannot break JSON.
    const auto encoded = nlohmann::json(text).dump(-1, ' ', false, nlohmann::json::error_handler_t::replace);
    return nlohmann::json::parse(encoded).get<std::string>();
}

template<std::size_t N> nlohmann::json optional_string(const char (&input)[N], bool available) {
    if (!available || !input[0]) return nullptr;
    return safe_string(input);
}

const char* status_name(deckstatus::BridgeStatus status) {
    switch (status) {
    case deckstatus::BridgeStatus::starting: return "starting";
    case deckstatus::BridgeStatus::connected: return "connected";
    case deckstatus::BridgeStatus::unsupported: return "unsupported";
    case deckstatus::BridgeStatus::stopped: return "disconnected";
    default: return "error";
    }
}

nlohmann::json serialize(const deckstatus::SharedState& state, bool alive, bool demo, const std::string& artwork_status) {
    const auto now = GetTickCount64();
    const bool stale = state.sample_tick && now - state.sample_tick > 3000;
    std::string status = demo ? "demo" : status_name(state.status);
    std::string message = safe_string(state.message);
    if (!alive) { status = "disconnected"; message = "Rekordbox wurde beendet."; }
    else if (stale && status == "connected") { status = "stale"; message = "Seit mehr als 3 Sekunden keine Deckdaten empfangen."; }
    nlohmann::json result = {
        {"schemaVersion", 1}, {"mode", "rekordbox"}, {"status", status}, {"message", deckstatus::tr(message)},
        {"version", deckstatus::tr(safe_string(state.rekordbox_version))}, {"demo", demo},
        {"updatedAt", nullptr}, {"sampleAgeMs", nullptr}, {"artworkStatus", deckstatus::tr(artwork_status)},
        {"decks", nlohmann::json::array()}
    };
    if (state.sample_tick) {
        const auto age = now - state.sample_tick;
        const auto unix_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        result["updatedAt"] = unix_ms - static_cast<std::int64_t>(age);
        result["sampleAgeMs"] = age;
    }
    // Don't present old track data as current after disconnection or a failed sample.
    const bool valid = status == "connected" || status == "demo";
    const auto master = valid && state.master_deck >= 1 && state.master_deck <= 4 ? state.master_deck : 0;
    result["masterDeckId"] = master ? nlohmann::json(master) : nlohmann::json(nullptr);
    for (unsigned i = 0; i < 4; ++i) {
        const auto& d = state.decks[i];
        const bool loaded = valid && d.track_id;
        const bool metadata = loaded && d.metadata_available;
        nlohmann::json deck = {
            {"id", i + 1}, {"trackId", loaded ? nlohmann::json(d.track_id) : nlohmann::json(nullptr)},
            {"loaded", loaded}, {"metadataAvailable", metadata},
            {"isMaster", master ? nlohmann::json(master == i + 1) : nlohmann::json(nullptr)},
            {"positionMs", loaded && d.timeline_available ? nlohmann::json(d.position_ms) : nlohmann::json(nullptr)},
            {"durationMs", loaded && d.timeline_available ? nlohmann::json(d.duration_ms) : nlohmann::json(nullptr)},
            {"title", optional_string(d.title, metadata)},
            {"artist", optional_string(d.artist, metadata)},
            {"album", optional_string(d.album, metadata)},
            {"key", optional_string(d.key, metadata)},
            {"genre", optional_string(d.genre, metadata)},
            {"label", optional_string(d.label, metadata)},
            {"bpm", loaded && d.bpm_x100 ? nlohmann::json(d.bpm_x100 / 100.0) : nlohmann::json(nullptr)},
            {"originalBpm", metadata && d.original_bpm_x100 ? nlohmann::json(d.original_bpm_x100 / 100.0) : nlohmann::json(nullptr)},
            {"coverUrl", loaded ? nlohmann::json("/api/decks/" + std::to_string(i + 1) +
                "/cover?trackId=" + std::to_string(d.track_id)) : nlohmann::json(nullptr)}
        };
        result["decks"].push_back(std::move(deck));
    }
    return result;
}

deckstatus::SharedState demo_state() {
    deckstatus::SharedState state;
    state.status = deckstatus::BridgeStatus::connected;
    state.sample_tick = GetTickCount64();
    static const auto started = GetTickCount64();
    state.master_deck = 1 + static_cast<std::uint32_t>((state.sample_tick - started) / 8000 % 2);
    strcpy_s(state.rekordbox_version, "Demo (synthetische Daten)");
    strcpy_s(state.message, "Demo-Modus: keine Verbindung zu Rekordbox.");
    for (unsigned i = 0; i < 4; ++i) state.decks[i].id = i + 1;
    auto& one = state.decks[0];
    one.track_id = 1001;
    one.metadata_available = 1;
    one.bpm_x100 = 12800;
    one.original_bpm_x100 = 12600;
    one.timeline_available = 1;
    one.duration_ms = 240000;
    one.position_ms = static_cast<std::int32_t>((state.sample_tick - started) % one.duration_ms);
    strcpy_s(one.title, "Night Drive \"Live\"");
    strcpy_s(one.artist, "Orbit & Friends");
    strcpy_s(one.album, "After Hours");
    strcpy_s(one.key, "8A");
    strcpy_s(one.genre, "House");
    auto& two = state.decks[1];
    two = one;
    two.id = 2;
    two.track_id = 1002;
    two.bpm_x100 = 12750;
    two.duration_ms = 210000;
    two.position_ms = static_cast<std::int32_t>((state.sample_tick - started + 63000) % two.duration_ms);
    strcpy_s(two.title, "First Light");
    strcpy_s(two.artist, "Studio North");
    strcpy_s(two.key, "9A");
    return state;
}

const std::string& demo_cover() {
    // A tiny code-generated vinyl illustration, encoded as an uncompressed BMP.
    static const std::string bitmap = [] {
        constexpr unsigned width = 240, stride = width * 3, size = 54 + stride * width;
        std::string data(size, '\0');
        const auto word = [&](unsigned offset, unsigned value, unsigned count) {
            for (unsigned i = 0; i < count; ++i) data[offset + i] = static_cast<char>(value >> (8 * i));
        };
        data[0] = 'B'; data[1] = 'M';
        word(2, size, 4); word(10, 54, 4); word(14, 40, 4);
        word(18, width, 4); word(22, width, 4); word(26, 1, 2); word(28, 24, 2);
        word(34, stride * width, 4);
        for (unsigned y = 0; y < width; ++y) for (unsigned x = 0; x < width; ++x) {
            const int dx = static_cast<int>(x) - 120, dy = static_cast<int>(y) - 120;
            const int radius = dx * dx + dy * dy;
            const bool green = (radius < 10000 && radius > 4000) || radius < 64;
            const auto at = 54 + y * stride + x * 3;
            data[at] = static_cast<char>(green ? 172 : 54);
            data[at + 1] = static_cast<char>(green ? 224 : 44);
            data[at + 2] = static_cast<char>(green ? 82 : 18);
        }
        return data;
    }();
    return bitmap;
}
}

int wmain(int argc, wchar_t** argv) {
    try {
        SetConsoleOutputCP(CP_UTF8);
        std::string language = "en";
        for (int i = 1; i < argc; ++i) if (std::wstring_view(argv[i]) == L"--lang" && i + 1 < argc) {
            language = std::wstring_view(argv[i + 1]) == L"de" ? "de" : "en";
        }
        deckstatus::load_language(executable_directory() / "web" / "locales", language);
        bool demo = false;
        bool prolink_mode = false;
        int port = 18740;
        std::optional<int> port_override;
        std::optional<std::string> bind_override;
        std::optional<bool> remote_control_override;
        auto network_file = executable_directory() / "DeckStatus.network.json";
        auto data_directory = executable_directory() / "DeckStatus.data";
        DWORD pid = 0;
        std::filesystem::path database;
        for (int i = 1; i < argc; ++i) {
            const std::wstring option = argv[i];
            if (option == L"--help" || option == L"-h") {
                std::cout << deckstatus::tr("cliHelp") << deckstatus::tr("cliNetworkHelp");
                return 0;
            }
            if (option == L"--lang") {
                if (++i >= argc) throw std::runtime_error("Wert fuer Option fehlt.");
                if (std::wstring_view(argv[i]) != L"en" && std::wstring_view(argv[i]) != L"de")
                    throw std::runtime_error("Language must be en or de.");
                continue;
            }
            if (option == L"--prolink") { prolink_mode = true; continue; }
            if (option == L"--mode") {
                if (++i >= argc || (std::wstring_view(argv[i]) != L"rekordbox" && std::wstring_view(argv[i]) != L"prolink"))
                    throw std::runtime_error("Mode must be rekordbox or prolink.");
                prolink_mode = std::wstring_view(argv[i]) == L"prolink";
                continue;
            }
            if (option == L"--demo") { demo = true; continue; }
            if (option == L"--allow-remote-control") { remote_control_override = true; continue; }
            if (option == L"--data-dir") { if (++i >= argc) throw std::runtime_error("Wert fuer Option fehlt."); data_directory = argv[i]; continue; }
            if (option == L"--bind" || option == L"--network-config") {
                if (++i >= argc) throw std::runtime_error("Wert fuer Option fehlt.");
                if (option == L"--network-config") network_file = argv[i];
                else {
                    const std::wstring value = argv[i];
                    if (value.find_first_not_of(L"0123456789.") != std::wstring::npos) throw std::runtime_error("networkInvalidSettings");
                    std::string ascii;
                    for (const wchar_t digit : value) ascii.push_back(static_cast<char>(digit));
                    bind_override = std::move(ascii);
                }
                continue;
            }
            if (option != L"--port" && option != L"--pid" && option != L"--database")
                throw std::runtime_error("Unbekannte Option; siehe --help.");
            if (++i >= argc) throw std::runtime_error("Wert fuer Option fehlt.");
            if (option == L"--port") port_override = static_cast<int>(number(argv[i], 65535));
            else if (option == L"--pid") pid = number(argv[i], MAXDWORD);
            else database = argv[i];
        }
        if (demo && (pid || !database.empty())) throw std::runtime_error("--demo ist nicht mit --pid/--database kombinierbar.");
        SetConsoleCtrlHandler(on_console, TRUE);
        const auto directory = executable_directory();
        deckstatus::NetworkConfig network(network_file, bind_override, port_override, remote_control_override);
        port = network.active().port;
        const auto host = network.active().bind;
        deckstatus::ServerFeatures features;
        features.network = &network;
        deckstatus::Portal portal(data_directory);
        features.portal = &portal;
        const auto initial_password = portal.initial_password();
        if (!initial_password.empty()) std::cout << deckstatus::tr("authInitialConsole") << '\n' << deckstatus::tr("authTemporaryConsole") << initial_password << '\n' << deckstatus::tr("authChangeConsole") << '\n' << std::flush;
        if (prolink_mode) {
            if (demo || pid || !database.empty()) throw std::runtime_error("ProLink mode cannot be combined with --demo, --pid or --database.");
            deckstatus::ProLink link(directory);
            deckstatus::MasterHistory history([&](std::uint32_t id) { return link.cover(id); });
            features.mode = "prolink";
            features.prolink_setup = [&] { return link.setup(); };
            features.prolink_control = [&](const nlohmann::json& command) { return link.control(command); };
            std::jthread sampler([&](std::stop_token token) {
                while (!token.stop_requested() && !stopping) {
                    const auto state = link.snapshot();
                    history.update(state);
                    for (const auto& deck : state["decks"]) history.enrich(deck);
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                }
            });
            std::cout << deckstatus::tr("prolinkStartup") << '\n';
            return deckstatus::run_server(host, port, directory / "web", [&] { return link.snapshot(); },
                [&](int id) -> std::pair<std::string, std::string> {
                    const auto state = link.snapshot();
                    for (const auto& deck : state["decks"]) if (deck["id"] == id && deck.value("loaded", false))
                        return link.cover(deck["trackId"].get<std::uint32_t>());
                    return {};
                }, stopping, &history, &features);
        }
        std::unique_ptr<deckstatus::Injection> injection;
        std::unique_ptr<deckstatus::ArtworkResolver> artwork;
        deckstatus::SharedState current = demo ? demo_state() : deckstatus::SharedState{};
        if (!demo) {
            const auto target = deckstatus::find_target(pid);
            std::wcout << L"Rekordbox " << target.version << L", PID " << target.pid << L"\n";
            artwork = std::make_unique<deckstatus::ArtworkResolver>(target.executable, database);
            injection = std::make_unique<deckstatus::Injection>(target, directory / "DeckStatusBridge.dll");
            current = injection->read();
            std::cout << deckstatus::tr(safe_string(current.message)) << "\n";
        }
        std::mutex mutex;
        bool alive = true;
        std::array<deckstatus::DeckData, 4> metadata_cache{};
        deckstatus::MasterHistory master_history([&](std::uint32_t track_id) -> deckstatus::MasterHistory::Cover {
            if (demo) return {"image/bmp", demo_cover()};
            return artwork->get(track_id);
        });
        // Heartbeats are independent of HTTP clients. jthread always joins before IPC destruction.
        std::jthread sampler([&](std::stop_token token) {
            while (!token.stop_requested() && !stopping) {
                auto next = demo ? demo_state() : injection->read();
                const bool next_alive = demo || injection->alive();
                {
                    std::lock_guard lock(mutex);
                    current = next;
                    alive = next_alive;
                    if (!demo) for (unsigned i = 0; i < 4; ++i) {
                        auto& deck = next.decks[i];
                        if (deck.track_id && metadata_cache[i].track_id == deck.track_id) {
                            const auto live_bpm = deck.bpm_x100;
                            const auto position = deck.position_ms;
                            const auto duration = deck.duration_ms;
                            const auto timeline = deck.timeline_available;
                            deck = metadata_cache[i];
                            deck.bpm_x100 = live_bpm;
                            deck.position_ms = position;
                            deck.duration_ms = duration;
                            deck.timeline_available = timeline;
                        }
                    }
                }
                // Capture even with no browser connected. No SQL or disk I/O on this thread.
                master_history.update(serialize(next, next_alive, demo, ""));
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        });
        // Database work is outside the injected process and cannot delay its heartbeat.
        std::jthread metadata_worker([&](std::stop_token token) {
            if (demo) return;
            std::array<ULONGLONG, 4> refreshed{};
            ULONGLONG history_refreshed{};
            while (!token.stop_requested() && !stopping) {
                deckstatus::SharedState state;
                { std::lock_guard lock(mutex); state = current; }
                if (state.status == deckstatus::BridgeStatus::connected &&
                    GetTickCount64() - state.sample_tick <= 3000) {
                    for (unsigned i = 0; i < 4 && !token.stop_requested() && !stopping; ++i) {
                        auto deck = state.decks[i];
                        std::uint32_t cached_id{};
                        { std::lock_guard lock(mutex); cached_id = metadata_cache[i].track_id; }
                        if (cached_id != deck.track_id || GetTickCount64() - refreshed[i] >= 2000) {
                            if (deck.track_id) artwork->enrich(deck);
                            { std::lock_guard lock(mutex); metadata_cache[i] = deck; }
                            refreshed[i] = GetTickCount64();
                        }
                    }
                }
                // A very short master appearance can end before deck metadata arrives.
                // Resolve those history IDs too, preserving the BPM at the transition.
                if (GetTickCount64() - history_refreshed >= 2000) {
                    for (auto id : master_history.pending_metadata()) {
                        if (token.stop_requested() || stopping) break;
                        deckstatus::SharedState metadata{};
                        metadata.status = deckstatus::BridgeStatus::connected;
                        metadata.sample_tick = GetTickCount64();
                        metadata.decks[0].id = 1;
                        metadata.decks[0].track_id = id;
                        if (artwork->enrich(metadata.decks[0]))
                            master_history.enrich(serialize(metadata, true, false, "")["decks"][0]);
                    }
                    history_refreshed = GetTickCount64();
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        });
        auto snapshot = [&]() {
            deckstatus::SharedState copy;
            bool connected;
            {
                std::lock_guard lock(mutex);
                copy = current;
                connected = alive;
                if (!demo) for (unsigned i = 0; i < 4; ++i) {
                    auto& deck = copy.decks[i];
                    if (deck.track_id && metadata_cache[i].track_id == deck.track_id) {
                        const auto live_bpm = deck.bpm_x100;
                        const auto position = deck.position_ms;
                        const auto duration = deck.duration_ms;
                        const auto timeline = deck.timeline_available;
                        deck = metadata_cache[i];
                        deck.bpm_x100 = live_bpm;
                        deck.position_ms = position;
                        deck.duration_ms = duration;
                        deck.timeline_available = timeline;
                    }
                }
            }
            return serialize(copy, connected, demo, artwork ? artwork->diagnostic() : "Demo-Cover");
        };
        auto cover = [&](int deck) -> std::pair<std::string, std::string> {
            if (deck < 1 || deck > 4) return {};
            std::uint32_t track_id{};
            {
                std::lock_guard lock(mutex);
                if (!alive || current.status != deckstatus::BridgeStatus::connected ||
                    GetTickCount64() - current.sample_tick > 3000) return {};
                track_id = current.decks[deck - 1].track_id;
            }
            if (!track_id) return {};
            if (demo) return {"image/bmp", demo_cover()};
            return artwork->get(track_id);
        };
        const auto base = deckstatus::network_url(host == "0.0.0.0" ? "127.0.0.1" : host, port);
        std::cout << "Dashboard: " << base << "/\n"
                  << "JSON:      " << base << "/api/state\n"
                  << "OBS:       " << base << "/overlay?deck=1\n"
                  << "Decks:     " << base << "/overlay/settings\n"
                  << "Master:    " << base << "/master-overlay/settings\n"
                  << "Waveform:  " << base << "/waveform/settings\n"
                  << "Network:   " << base << "/network/settings\n"
                  << deckstatus::tr("cliStop") << "\n" << std::flush;
        const int result = deckstatus::run_server(host, port, directory / "web", snapshot, cover, stopping, &master_history, &features);
        stopping = true;
        return result;
    } catch (const std::exception& error) {
        stopping = true;
        std::cerr << deckstatus::tr("cliError") << deckstatus::tr(error.what()) << "\n";
        return 1;
    }
}
