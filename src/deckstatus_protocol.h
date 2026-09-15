#pragma once

#include <Windows.h>
#include <cstdint>
#include <string>
#include <type_traits>

namespace deckstatus {
inline constexpr std::uint32_t protocol_magic = 0x44535453;
inline constexpr std::uint32_t protocol_version = 3;
enum class BridgeStatus : std::uint32_t { starting, connected, unsupported, error, stopped };

// Fixed-size UTF-8 strings. A named mutex protects the entire shared mapping.
struct DeckData {
    std::uint32_t id{};
    std::uint32_t track_id{};
    std::uint32_t bpm_x100{};
    std::uint32_t original_bpm_x100{};
    std::uint32_t metadata_available{};
    std::uint32_t reserved{};
    char title[1024]{};
    char artist[1024]{};
    char album[1024]{};
    char key[128]{};
    char genre[256]{};
    char label[256]{};
    char image_path[2048]{};
    char file_path[2048]{};
    std::int32_t position_ms{};
    std::uint32_t duration_ms{};
    std::uint32_t timeline_available{};
};

struct SharedState {
    std::uint32_t magic{protocol_magic};
    std::uint32_t version{protocol_version};
    std::uint32_t size{sizeof(SharedState)};
    std::uint32_t host_pid{};
    std::uint64_t host_heartbeat{}; // GetTickCount64()
    std::uint64_t sample_tick{};
    BridgeStatus status{BridgeStatus::starting};
    std::uint32_t reserved{};
    char rekordbox_version[64]{};
    char message[512]{};
    DeckData decks[4]{};
    std::uint32_t master_deck{}; // 1..4; zero = unknown, none or ambiguous
};
static_assert(std::is_trivially_copyable_v<SharedState>);

inline std::wstring object_name(DWORD pid, const wchar_t* suffix) {
    return L"Local\\DeckStatus.Bridge." + std::to_wstring(pid) + L"." + suffix;
}
// Objects: State=file mapping, Lock=mutex, Stop=manual-reset event,
// Ready=first snapshot event, Stopped=worker finished event.
}
