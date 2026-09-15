#include "protocol.h"
#include "scanner.h"

#include <array>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <exception>
#include <string>
#include <vector>

#pragma comment(lib, "Version.lib")

namespace {

using rb::memory::read;
using rb::memory::read_bytes;

// Exact 7.2.18.0 Windows x64 profile. Every field and code guard is documented
// in docs/rekordbox-7.2.18.md. Other versions/builds fail closed.
constexpr std::uint32_t supported_timestamp = 0x6A672BEA;
constexpr std::uint32_t supported_image_size = 0x06291000;
constexpr std::uintptr_t main_global_rva = 0x05D1F260;
constexpr std::uintptr_t player_vtable_rva = 0x03BB7E70;
constexpr std::uintptr_t bpm_vtable_rva = 0x03B85620;
constexpr std::size_t manager_offset = 0x490;
constexpr std::size_t players_offset = 0x50;
constexpr std::size_t player_index_offset = 0x478;
constexpr std::size_t track_id_offset = 0x580;
constexpr std::size_t bpm_device_offset = 0xCE8;
constexpr std::size_t bpm_value_offset = 0x9C;
constexpr std::size_t master_device_offset = 0x958;

template<class T> bool member(std::uintptr_t object, std::size_t offset, T& value) {
    return object && object <= UINTPTR_MAX - offset && read(object + offset, value);
}

template<std::size_t N>
bool code_matches(std::uintptr_t base, std::size_t rva, const unsigned char (&expected)[N]) {
    unsigned char actual[N]{};
    return rva < supported_image_size && N <= supported_image_size - rva &&
           read_bytes(base + rva, actual, N) && std::memcmp(actual, expected, N) == 0;
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
        mapping = OpenFileMappingW(FILE_MAP_READ | FILE_MAP_WRITE, FALSE, rb::object_name(pid, L"State").c_str());
        mutex = OpenMutexW(SYNCHRONIZE | MUTEX_MODIFY_STATE, FALSE, rb::object_name(pid, L"Lock").c_str());
        stop = OpenEventW(SYNCHRONIZE, FALSE, rb::object_name(pid, L"Stop").c_str());
        ready = OpenEventW(EVENT_MODIFY_STATE, FALSE, rb::object_name(pid, L"Ready").c_str());
        stopped = OpenEventW(EVENT_MODIFY_STATE, FALSE, rb::object_name(pid, L"Stopped").c_str());
        if (!mapping || !mutex || !stop || !ready || !stopped) return false;
        state = static_cast<rb::SharedState*>(MapViewOfFile(mapping, FILE_MAP_READ | FILE_MAP_WRITE,
                                                          0, 0, sizeof(rb::SharedState)));
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
    bool publish(const rb::SharedState& snapshot) const {
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
    rb::SharedState* state{};

private:
    bool valid_protocol() const {
        return state && state->magic == rb::protocol_magic &&
               state->version == rb::protocol_version && state->size == sizeof(rb::SharedState);
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
    return major == 7 && minor == 2 && patch == 18 && build == 0;
}

bool bpm_of_player(std::uintptr_t base, std::uintptr_t player, std::uint32_t& bpm) {
    std::uintptr_t device = 0, vtable = 0, name = 0, verified_device = 0;
    char label[5]{};
    // +0x9C is the DeviceComponent's cached integer, written before its
    // downstream UI value. This avoids depending on the skin object's layout.
    return member(player, bpm_device_offset, device) && member(device, 0, vtable) &&
           vtable == base + bpm_vtable_rva && member(device, 0x10, name) &&
           read_bytes(name, label, sizeof(label)) && std::memcmp(label, "@BPM", 5) == 0 &&
           member(device, bpm_value_offset, bpm) && bpm <= 100000 &&
           member(player, bpm_device_offset, verified_device) && verified_device == device;
}

bool resolve(std::uintptr_t base, char* message, std::size_t capacity) {
    IMAGE_DOS_HEADER dos{};
    IMAGE_NT_HEADERS64 nt{};
    if (!read(base, dos) || dos.e_lfanew <= 0 || dos.e_lfanew > 0x100000 ||
        !member(base, static_cast<std::size_t>(dos.e_lfanew), nt) ||
        nt.FileHeader.TimeDateStamp != supported_timestamp ||
        nt.OptionalHeader.SizeOfImage != supported_image_size) {
        snprintf(message, capacity, "Unsupported Rekordbox executable fingerprint; expected the documented 7.2.18.0 x64 build.");
        return false;
    }
    // All guards are instruction bytes containing RIP-relative or object-member
    // displacements, so ASLR does not change them. No native functions are called.
    if (!code_matches(base, 0x1729D41, {0x4C,0x89,0x3D,0x18,0x55,0x5F,0x04}) ||
        !code_matches(base, 0x175579D, {0x48,0x89,0xAE,0x90,0x04,0x00,0x00}) ||
        !code_matches(base, 0x249EB4E, {0x8B,0x42,0x08,0x89,0x87,0x80,0x05,0x00,0x00}) ||
        !code_matches(base, 0x2473EC8, {0x48,0x8D,0x15,0x5D,0x80,0x71,0x01}) ||
        !code_matches(base, 0x2473EF8, {0x48,0x89,0x87,0xE8,0x0C,0x00,0x00}) ||
        !code_matches(base, 0x22ABF46, {0x44,0x89,0x89,0x9C,0x00,0x00,0x00}) ||
        !code_matches(base, 0x2470049, {0x48,0x8D,0x15,0x9C,0xBD,0x71,0x01}) ||
        !code_matches(base, 0x2470079, {0x48,0x89,0x87,0x58,0x09,0x00,0x00}) ||
        !code_matches(base, 0x22ABD3F, {0x83,0xF0,0x01,0x48,0x8B,0xD9,0x89,0x81,0x94,0x00,0x00,0x00}) ||
        !code_matches(base, 0x2473DDE, {0x48,0x8D,0x15,0x1B,0x81,0x71,0x01}) ||
        !code_matches(base, 0x2473E0E, {0x48,0x89,0x87,0xD8,0x0C,0x00,0x00}) ||
        !code_matches(base, 0x2473E53, {0x48,0x8D,0x15,0xB6,0x80,0x71,0x01}) ||
        !code_matches(base, 0x2473E83, {0x48,0x89,0x87,0xE0,0x0C,0x00,0x00}) ||
        !code_matches(base, 0x24784D1, {0x85,0xC0,0x79,0x06,0xF7,0xD8,0x0F,0xBA,0xE8,0x1F})) {
        snprintf(message, capacity, "Rekordbox 7.2.18 code guards do not match; this executable is unsupported.");
        return false;
    }
    return true;
}

bool master_of_player(std::uintptr_t base, std::uintptr_t player, std::uint32_t& value) {
    std::uintptr_t device = 0, vtable = 0, name = 0, verified_device = 0;
    char label[7]{};
    // Boolean setter at RVA 0x22ABD20 stores !is_master as uint32 at +0x94.
    // This is the Master indicator's own cached state, independent of its skin.
    return member(player, master_device_offset, device) && member(device, 0, vtable) &&
           vtable == base + bpm_vtable_rva && member(device, 0x10, name) &&
           read_bytes(name, label, sizeof(label)) && std::memcmp(label, "Master", 7) == 0 &&
           member(device, 0x94, value) && value <= 1 &&
           member(player, master_device_offset, verified_device) && verified_device == device;
}

template<std::size_t N>
bool time_device(std::uintptr_t base, std::uintptr_t player, std::size_t offset,
                 const char (&expected)[N], std::uint32_t& value) {
    std::uintptr_t device = 0, vtable = 0, name = 0, verified = 0;
    char label[N]{};
    return member(player, offset, device) && member(device, 0, vtable) &&
           vtable == base + bpm_vtable_rva && member(device, 0x10, name) &&
           read_bytes(name, label, N) && std::memcmp(label, expected, N) == 0 &&
           member(device, 0x9C, value) && member(player, offset, verified) && verified == device;
}

void timeline_of_player(std::uintptr_t base, std::uintptr_t player, rb::DeckData& deck) {
    std::uint32_t position{}, duration{}, verified_id{};
    if (!deck.track_id || !time_device(base, player, 0xCD8, "@CurrentTime", position) ||
        !time_device(base, player, 0xCE0, "@TotalTime", duration) || !duration || duration > 86400000 ||
        (position & 0x7FFFFFFFu) > 86400000 ||
        !member(player, track_id_offset, verified_id) || verified_id != deck.track_id) return;
    // Rekordbox encodes negative preroll positions as sign + magnitude.
    deck.position_ms = static_cast<std::int32_t>(position & 0x7FFFFFFFu) * (position & 0x80000000u ? -1 : 1);
    deck.duration_ms = duration;
    deck.timeline_available = 1;
}

void sample_loop(Channel& channel, rb::SharedState& snapshot, std::uintptr_t base) {
    const auto main_global = base + main_global_rva;
    while (channel.alive()) {
        std::uintptr_t component = 0, manager = 0;
        const bool ui_ready = read(main_global, component) && component &&
                              member(component, manager_offset, manager) && manager;
        unsigned valid_players = 0;
        unsigned master_count = 0, valid_master_states = 0;
        std::array<std::uintptr_t, 4> players{};
        std::array<std::uint32_t, 4> master_states{};
        snapshot.master_deck = 0;
        for (unsigned index = 0; index < 4; ++index) {
            rb::DeckData current{};
            current.id = index + 1;
            std::uintptr_t player = 0, vtable = 0, verified_player = 0;
            std::uint32_t player_index = 0, track_id = 0, verified_id = 0, bpm = 0;
            const auto slot = players_offset + index * sizeof(player);
            if (ui_ready && member(manager, slot, player) && member(player, 0, vtable) &&
                vtable == base + player_vtable_rva &&
                member(player, player_index_offset, player_index) && player_index == index &&
                member(player, track_id_offset, track_id) && bpm_of_player(base, player, bpm) &&
                member(player, track_id_offset, verified_id) && verified_id == track_id &&
                member(manager, slot, verified_player) && verified_player == player) {
                ++valid_players;
                current.track_id = track_id;
                current.bpm_x100 = track_id ? bpm : 0;
                timeline_of_player(base, player, current);
                players[index] = player;
                if (master_of_player(base, player, master_states[index])) {
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
            if (!master_of_player(base, players[index], verified) || verified != master_states[index])
                valid_master_states = 0;
        }
        if (valid_master_states != 4 || master_count != 1) snapshot.master_deck = 0;
        std::uintptr_t verified_component = 0, verified_manager = 0;
        if (!read(main_global, verified_component) || verified_component != component ||
            !member(component, manager_offset, verified_manager) || verified_manager != manager) {
            valid_players = 0;
            snapshot.master_deck = 0;
            for (unsigned index = 0; index < 4; ++index) {
                snapshot.decks[index] = {};
                snapshot.decks[index].id = index + 1;
            }
        }
        snapshot.sample_tick = GetTickCount64();
        snapshot.status = valid_players ? rb::BridgeStatus::connected : rb::BridgeStatus::starting;
        text(snapshot.message, valid_players ? "Connected to Rekordbox 7.2.18; sampling live deck IDs, BPM and Master." :
             "Waiting for Rekordbox deck UI and BPM devices.");
        if (!channel.publish(snapshot)) return;
        if (WaitForSingleObject(channel.stop, 100) != WAIT_TIMEOUT) break;
    }
    snapshot.sample_tick = GetTickCount64();
    snapshot.status = rb::BridgeStatus::stopped;
    text(snapshot.message, "Bridge stopped.");
    channel.publish(snapshot);
}

void run() {
    Channel channel;
    if (!channel.open()) { channel.finish(); return; }
    rb::SharedState snapshot{};
    for (unsigned index = 0; index < 4; ++index) snapshot.decks[index].id = index + 1;
    try {
        if (!channel.alive()) { channel.finish(); return; }
        if (!get_version(snapshot.rekordbox_version, sizeof(snapshot.rekordbox_version))) {
            snapshot.status = rb::BridgeStatus::unsupported;
            text(snapshot.message, "This bridge supports the documented rekordbox.exe 7.2.18.0 x64 build only.");
        } else {
            rb::memory::ImageScanner scanner;
            const auto module = GetModuleHandleW(nullptr);
            const auto base = reinterpret_cast<std::uintptr_t>(module);
            if (!scanner.initialize(module)) {
                snapshot.status = rb::BridgeStatus::unsupported;
                text(snapshot.message, "Cannot validate executable image sections.");
            } else if (!resolve(base, snapshot.message, sizeof(snapshot.message))) {
                snapshot.status = rb::BridgeStatus::unsupported;
            } else {
                sample_loop(channel, snapshot, base);
                channel.finish();
                return;
            }
        }
    } catch (const std::exception& exception) {
        snapshot.status = rb::BridgeStatus::error;
        snprintf(snapshot.message, sizeof(snapshot.message), "Bridge failed: %.450s", exception.what());
    } catch (...) {
        snapshot.status = rb::BridgeStatus::error;
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
