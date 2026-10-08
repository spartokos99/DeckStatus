#include "deckstatus_protocol.h"
#include "rekordbox_profile.h"

#include <array>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <exception>
#include <string>
#include <vector>

#pragma comment(lib, "Version.lib")

namespace {

using deckstatus::memory::read;
using deckstatus::memory::read_bytes;

// Per-build layout, selected by the validated executable profile.
// Every field and code guard is documented in docs/rekordbox-7.2.18.md and docs/rekordbox-7.2.19.md.
using deckstatus::rekordbox::Layout;
constexpr std::size_t value_offset = 0x9C;      // DeviceComponent cached integer (all builds)
constexpr std::size_t master_flag_offset = 0x94; // DeviceComponent !is_master (all builds)

template<class T> bool member(std::uintptr_t object, std::size_t offset, T& value) {
    return object && object <= UINTPTR_MAX - offset && read(object + offset, value);
}

template<std::size_t N> void text(char (&destination)[N], const char* source) {
    strncpy_s(destination, N, source ? source : "", _TRUNCATE);
}

class Channel {
public:
    ~Channel() {
        if (state) UnmapViewOfFile(state);
        for (HANDLE handle : {mapping, mutex, stop, ready, stopped}) if (handle) CloseHandle(handle);
    }
    bool open() {
        const auto pid = GetCurrentProcessId();
        mapping = OpenFileMappingW(FILE_MAP_READ | FILE_MAP_WRITE, FALSE, deckstatus::object_name(pid, L"State").c_str());
        mutex = OpenMutexW(SYNCHRONIZE | MUTEX_MODIFY_STATE, FALSE, deckstatus::object_name(pid, L"Lock").c_str());
        stop = OpenEventW(SYNCHRONIZE, FALSE, deckstatus::object_name(pid, L"Stop").c_str());
        ready = OpenEventW(EVENT_MODIFY_STATE, FALSE, deckstatus::object_name(pid, L"Ready").c_str());
        stopped = OpenEventW(EVENT_MODIFY_STATE, FALSE, deckstatus::object_name(pid, L"Stopped").c_str());
        if (!mapping || !mutex || !stop || !ready || !stopped) return false;
        state = static_cast<deckstatus::SharedState*>(MapViewOfFile(mapping, FILE_MAP_READ | FILE_MAP_WRITE,
                                                          0, 0, sizeof(deckstatus::SharedState)));
        return state != nullptr;
    }
    bool acquire() const {
        const auto result = WaitForSingleObject(mutex, 100);
        return result == WAIT_OBJECT_0 || result == WAIT_ABANDONED;
    }
    bool alive() const {
        if (WaitForSingleObject(stop, 0) != WAIT_TIMEOUT) return false;
        if (!acquire()) return false;
        const auto now = GetTickCount64();
        const bool valid = valid_protocol() && state->host_pid && state->host_heartbeat &&
                           state->host_heartbeat <= now && now - state->host_heartbeat <= 5000;
        ReleaseMutex(mutex);
        return valid;
    }
    bool publish(const deckstatus::SharedState& snapshot) const {
        if (!acquire()) return false;
        if (!valid_protocol()) { ReleaseMutex(mutex); return false; }
        state->sample_tick = snapshot.sample_tick;
        state->status = snapshot.status;
        state->master_deck = snapshot.master_deck;
        std::memcpy(state->rekordbox_version, snapshot.rekordbox_version, sizeof(state->rekordbox_version));
        std::memcpy(state->message, snapshot.message, sizeof(state->message));
        std::memcpy(state->decks, snapshot.decks, sizeof(state->decks));
        ReleaseMutex(mutex);
        SetEvent(ready);
        return true;
    }
    void finish() const { if (stopped) SetEvent(stopped); }

    HANDLE mapping{}, mutex{}, stop{}, ready{}, stopped{};
    deckstatus::SharedState* state{};

private:
    bool valid_protocol() const {
        return state && state->magic == deckstatus::protocol_magic &&
               state->version == deckstatus::protocol_version && state->size == sizeof(deckstatus::SharedState);
    }
};

bool get_version(char* output, std::size_t capacity) {
    std::vector<wchar_t> path(32768);
    const auto length = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
    if (!length || length >= path.size()) return false;
    const auto name = wcsrchr(path.data(), L'\\');
    if (_wcsicmp(name ? name + 1 : path.data(), L"rekordbox.exe") != 0) return false;
    DWORD ignored = 0;
    const auto size = GetFileVersionInfoSizeW(path.data(), &ignored);
    if (!size) return false;
    std::vector<unsigned char> version(size);
    if (!GetFileVersionInfoW(path.data(), 0, size, version.data())) return false;
    VS_FIXEDFILEINFO* value = nullptr;
    UINT value_size = 0;
    if (!VerQueryValueW(version.data(), L"\\", reinterpret_cast<void**>(&value), &value_size) ||
        value_size < sizeof(VS_FIXEDFILEINFO) || value->dwSignature != 0xFEEF04BD) return false;
    const auto major = HIWORD(value->dwFileVersionMS);
    const auto minor = LOWORD(value->dwFileVersionMS);
    const auto patch = HIWORD(value->dwFileVersionLS);
    const auto build = LOWORD(value->dwFileVersionLS);
    snprintf(output, capacity, "%u.%u.%u.%u", major, minor, patch, build);
    return major == 7 && minor == 2 && (patch == 18 || patch == 19) && build == 0;
}

bool bpm_of_player(const Layout& L, std::uintptr_t base, std::uintptr_t player, std::uint32_t& bpm) {
    std::uintptr_t device = 0, vtable = 0, name = 0, verified_device = 0;
    char label[5]{};
    // +0x9C is the DeviceComponent's cached integer, written before its
    // downstream UI value. This avoids depending on the skin object's layout.
    return member(player, L.bpm_device_offset, device) && member(device, 0, vtable) &&
           vtable == base + L.device_vtable_rva && member(device, 0x10, name) &&
           read_bytes(name, label, sizeof(label)) && std::memcmp(label, "@BPM", 5) == 0 &&
           member(device, value_offset, bpm) && bpm <= 100000 &&
           member(player, L.bpm_device_offset, verified_device) && verified_device == device;
}

bool master_of_player(const Layout& L, std::uintptr_t base, std::uintptr_t player, std::uint32_t& value) {
    std::uintptr_t device = 0, vtable = 0, name = 0, verified_device = 0;
    char label[7]{};
    // Boolean setter at RVA 0x22ABD20 stores !is_master as uint32 at +0x94.
    // This is the Master indicator's own cached state, independent of its skin.
    return member(player, L.master_device_offset, device) && member(device, 0, vtable) &&
           vtable == base + L.device_vtable_rva && member(device, 0x10, name) &&
           read_bytes(name, label, sizeof(label)) && std::memcmp(label, "Master", 7) == 0 &&
           member(device, master_flag_offset, value) && value <= 1 &&
           member(player, L.master_device_offset, verified_device) && verified_device == device;
}

template<std::size_t N>
bool time_device(const Layout& L, std::uintptr_t base, std::uintptr_t player, std::size_t offset,
                 const char (&expected)[N], std::uint32_t& value) {
    std::uintptr_t device = 0, vtable = 0, name = 0, verified = 0;
    char label[N]{};
    return member(player, offset, device) && member(device, 0, vtable) &&
           vtable == base + L.device_vtable_rva && member(device, 0x10, name) &&
           read_bytes(name, label, N) && std::memcmp(label, expected, N) == 0 &&
           member(device, value_offset, value) && member(player, offset, verified) && verified == device;
}

void timeline_of_player(const Layout& L, std::uintptr_t base, std::uintptr_t player, deckstatus::DeckData& deck) {
    std::uint32_t position{}, duration{}, verified_id{};
    if (!deck.track_id || !time_device(L, base, player, L.current_time_offset, "@CurrentTime", position) ||
        !time_device(L, base, player, L.total_time_offset, "@TotalTime", duration) || !duration || duration > 86400000 ||
        (position & 0x7FFFFFFFu) > 86400000 ||
        !member(player, L.track_id_offset, verified_id) || verified_id != deck.track_id) return;
    // Rekordbox encodes negative preroll positions as sign + magnitude.
    deck.position_ms = static_cast<std::int32_t>(position & 0x7FFFFFFFu) * (position & 0x80000000u ? -1 : 1);
    deck.duration_ms = duration;
    deck.timeline_available = 1;
}

void sample_loop(Channel& channel, deckstatus::SharedState& snapshot, std::uintptr_t base,
                 const deckstatus::rekordbox::Profile& profile) {
    const Layout& L = *profile.layout;
    const auto main_global = base + L.main_global_rva;
    while (channel.alive()) {
        std::uintptr_t component = 0, manager = 0;
        const bool ui_ready = read(main_global, component) && component &&
                              member(component, L.manager_offset, manager) && manager;
        unsigned valid_players = 0;
        unsigned master_count = 0, valid_master_states = 0;
        std::array<std::uintptr_t, 4> players{};
        std::array<std::uint32_t, 4> master_states{};
        snapshot.master_deck = 0;
        for (unsigned index = 0; index < 4; ++index) {
            deckstatus::DeckData current{};
            current.id = index + 1;
            std::uintptr_t player = 0, vtable = 0, verified_player = 0;
            std::uint32_t player_index = 0, track_id = 0, verified_id = 0, bpm = 0;
            const auto slot = L.players_offset + index * sizeof(player);
            if (ui_ready && member(manager, slot, player) && member(player, 0, vtable) &&
                vtable == base + L.player_vtable_rva &&
                member(player, L.player_index_offset, player_index) && player_index == index &&
                member(player, L.track_id_offset, track_id) && bpm_of_player(L, base, player, bpm) &&
                member(player, L.track_id_offset, verified_id) && verified_id == track_id &&
                member(manager, slot, verified_player) && verified_player == player) {
                ++valid_players;
                current.track_id = track_id;
                current.bpm_x100 = track_id ? bpm : 0;
                timeline_of_player(L, base, player, current);
                players[index] = player;
                if (master_of_player(L, base, player, master_states[index])) {
                    ++valid_master_states;
                    if (master_states[index] == 0) {
                        ++master_count;
                        snapshot.master_deck = index + 1;
                    }
                }
            }
            // Metadata stays empty here. The host resolves this ID in its own
            // read-only database connection, outside Rekordbox's address space.
            snapshot.decks[index] = current;
        }
        // Recheck every flag: deck indicators are updated sequentially by the UI.
        // A transition with no master or two masters must never select a guessed deck.
        for (unsigned index = 0; index < 4 && valid_master_states == 4; ++index) {
            std::uint32_t verified = 0;
            if (!master_of_player(L, base, players[index], verified) || verified != master_states[index])
                valid_master_states = 0;
        }
        if (valid_master_states != 4 || master_count != 1) snapshot.master_deck = 0;
        std::uintptr_t verified_component = 0, verified_manager = 0;
        if (!read(main_global, verified_component) || verified_component != component ||
            !member(component, L.manager_offset, verified_manager) || verified_manager != manager) {
            valid_players = 0;
            snapshot.master_deck = 0;
            for (unsigned index = 0; index < 4; ++index) {
                snapshot.decks[index] = {};
                snapshot.decks[index].id = index + 1;
            }
        }
        snapshot.sample_tick = GetTickCount64();
        snapshot.status = valid_players ? deckstatus::BridgeStatus::connected : deckstatus::BridgeStatus::starting;
        if (valid_players)
            snprintf(snapshot.message, sizeof(snapshot.message), "Connected to Rekordbox %s; sampling live deck IDs, BPM and Master. [%s]", L.version, profile.name);
        else text(snapshot.message, "Waiting for Rekordbox deck UI and BPM devices.");
        if (!channel.publish(snapshot)) return;
        if (WaitForSingleObject(channel.stop, 100) != WAIT_TIMEOUT) break;
    }
    snapshot.sample_tick = GetTickCount64();
    snapshot.status = deckstatus::BridgeStatus::stopped;
    text(snapshot.message, "Bridge stopped.");
    channel.publish(snapshot);
}

void run() {
    Channel channel;
    if (!channel.open()) { channel.finish(); return; }
    deckstatus::SharedState snapshot{};
    for (unsigned index = 0; index < 4; ++index) snapshot.decks[index].id = index + 1;
    try {
        if (!channel.alive()) { channel.finish(); return; }
        if (!get_version(snapshot.rekordbox_version, sizeof(snapshot.rekordbox_version))) {
            snapshot.status = deckstatus::BridgeStatus::unsupported;
            text(snapshot.message, "This bridge supports the documented rekordbox.exe 7.2.18.0 and 7.2.19.0 x64 profiles only.");
        } else {
            const auto base = reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
            std::array<wchar_t, 32768> path{};
            const auto length = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
            if (!length || length >= path.size()) {
                snapshot.status = deckstatus::BridgeStatus::unsupported;
                text(snapshot.message, "Cannot determine executable path for profile verification.");
            } else if (const auto profile = deckstatus::rekordbox::validate(base, path.data(), snapshot.message, sizeof(snapshot.message))) {
                sample_loop(channel, snapshot, base, *profile);
                channel.finish();
                return;
            } else snapshot.status = deckstatus::BridgeStatus::unsupported;
        }
    } catch (const std::exception& exception) {
        snapshot.status = deckstatus::BridgeStatus::error;
        snprintf(snapshot.message, sizeof(snapshot.message), "Bridge failed: %.450s", exception.what());
    } catch (...) {
        snapshot.status = deckstatus::BridgeStatus::error;
        text(snapshot.message, "Unexpected bridge failure.");
    }
    snapshot.sample_tick = GetTickCount64();
    channel.publish(snapshot);
    channel.finish();
}

DWORD WINAPI worker(void* module) {
    run();
    FreeLibraryAndExitThread(static_cast<HMODULE>(module), 0);
}

} // namespace

BOOL WINAPI DllMain(HINSTANCE module, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        HANDLE thread = CreateThread(nullptr, 0, worker, module, 0, nullptr);
        if (!thread) return FALSE;
        CloseHandle(thread);
    }
    return TRUE;
}
