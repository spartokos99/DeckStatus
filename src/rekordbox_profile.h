#pragma once
#include "scanner.h"
#include <bcrypt.h>
#include <array>
#include <cstdio>
#include <string_view>

namespace deckstatus::rekordbox {

struct CodeGuard { DWORD rva; std::string_view bytes; };
using namespace std::string_view_literals;

// Memory layout of one Rekordbox build. Every field and code guard is documented in
// docs/rekordbox-7.2.18.md and docs/rekordbox-7.2.19.md.
struct Layout {
    const char* version;                 // "7.2.18", "7.2.19"
    std::uintptr_t main_global_rva, player_vtable_rva, device_vtable_rva;
    std::size_t manager_offset, players_offset, player_index_offset, track_id_offset;
    std::size_t current_time_offset, total_time_offset, bpm_device_offset, master_device_offset;
    const CodeGuard* guards;
    std::size_t guard_count;
};

// Exact instruction bytes, including embedded zeros; unchanged by ASLR.
inline constexpr CodeGuard guards_7_2_18[] = {
    {0x01729D41, "\x4C\x89\x3D\x18\x55\x5F\x04"sv},
    {0x0175579D, "\x48\x89\xAE\x90\x04\x00\x00"sv},
    {0x0249EB4E, "\x8B\x42\x08\x89\x87\x80\x05\x00\x00"sv},
    {0x02473EC8, "\x48\x8D\x15\x5D\x80\x71\x01"sv},
    {0x02473EF8, "\x48\x89\x87\xE8\x0C\x00\x00"sv},
    {0x022ABF46, "\x44\x89\x89\x9C\x00\x00\x00"sv},
    {0x02470049, "\x48\x8D\x15\x9C\xBD\x71\x01"sv},
    {0x02470079, "\x48\x89\x87\x58\x09\x00\x00"sv},
    {0x022ABD3F, "\x83\xF0\x01\x48\x8B\xD9\x89\x81\x94\x00\x00\x00"sv},
    {0x02473DDE, "\x48\x8D\x15\x1B\x81\x71\x01"sv},
    {0x02473E0E, "\x48\x89\x87\xD8\x0C\x00\x00"sv},
    {0x02473E53, "\x48\x8D\x15\xB6\x80\x71\x01"sv},
    {0x02473E83, "\x48\x89\x87\xE0\x0C\x00\x00"sv},
    {0x024784D1, "\x85\xC0\x79\x06\xF7\xD8\x0F\xBA\xE8\x1F"sv}
};
// 7.2.19.0: same classes, player/component fields shifted (see docs/rekordbox-7.2.19.md).
inline constexpr CodeGuard guards_7_2_19[] = {
    {0x01816554, "\x4C\x89\x3D\x45\xA9\x7C\x04"sv},             // global component pointer
    {0x0184209D, "\x48\x89\xAE\xC0\x04\x00\x00"sv},             // manager offset +0x4C0
    {0x0259C43E, "\x8B\x42\x08\x89\x87\x60\x05\x00\x00"sv},     // track ID +0x560
    {0x025717C8, "\x48\x8D\x15\x35\xAC\x83\x01"sv},             // @BPM name reference
    {0x025717F8, "\x48\x89\x87\xC8\x0C\x00\x00"sv},             // BPM device +0xCC8
    {0x023A95C6, "\x44\x89\x89\x9C\x00\x00\x00"sv},             // cached 32-bit value +0x9C
    {0x0256D949, "\x48\x8D\x15\x74\xE9\x83\x01"sv},             // Master name reference
    {0x0256D979, "\x48\x89\x87\x38\x09\x00\x00"sv},             // Master device +0x938
    {0x023A93BF, "\x83\xF0\x01\x48\x8B\xD9\x89\x81\x94\x00\x00\x00"sv}, // !is_master at +0x94
    {0x025716DE, "\x48\x8D\x15\xF3\xAC\x83\x01"sv},             // @CurrentTime name
    {0x0257170E, "\x48\x89\x87\xB8\x0C\x00\x00"sv},             // CurrentTime device +0xCB8
    {0x02571753, "\x48\x8D\x15\x8E\xAC\x83\x01"sv},             // @TotalTime name
    {0x02571783, "\x48\x89\x87\xC0\x0C\x00\x00"sv},             // TotalTime device +0xCC0
    {0x02575DD1, "\x85\xC0\x79\x06\xF7\xD8\x0F\xBA\xE8\x1F"sv}  // sign-magnitude position
};
inline constexpr Layout layout_7_2_18{"7.2.18", 0x05D1F260, 0x03BB7E70, 0x03B85620,
    0x490, 0x50, 0x478, 0x580, 0xCD8, 0xCE0, 0xCE8, 0x958, guards_7_2_18, std::size(guards_7_2_18)};
inline constexpr Layout layout_7_2_19{"7.2.19", 0x05FE0EA0, 0x03DDB150, 0x03DA4C18,
    0x4C0, 0x50, 0x458, 0x560, 0xCB8, 0xCC0, 0xCC8, 0x938, guards_7_2_19, std::size(guards_7_2_19)};

// Audited executable variants. Adding a hash alone is insufficient: each build needs its own layout.
struct Profile {
    const char* name;
    DWORD timestamp, image_size, entry_point;
    WORD sections;
    LONGLONG file_size;
    std::string_view sha256;
    const Layout* layout;
};
inline constexpr Profile profiles[] = {
    {"7.2.18.0 original", 0x6A672BEA, 0x06291000, 0x02711CCC, 8, 100561840,
     "a99896cf26d5998e6ad4177796a467b83df14bf8ae7207df21ed01251e402493", &layout_7_2_18},
    {"7.2.18.0 patched", 0x6A672BEA, 0x06481000, 0x06291000, 10, 102587392,
     "297ab491ae745191b09ee72612be6d4c61075788740fb62140f93b0628337a4f", &layout_7_2_18},
    {"7.2.19.0 original", 0x6A97B2E4, 0x06573000, 0x0280E96C, 8, 103540656,
     "c9ed23e974c51c75e2ec069fbd2679a3dbe606be9e79e9cbbd13d6418ab11c37", &layout_7_2_19}
};

inline const Profile* identify(std::uintptr_t base) {
    memory::ImageScanner scanner;
    if (!scanner.initialize(reinterpret_cast<HMODULE>(base))) return nullptr;
    IMAGE_DOS_HEADER dos{};
    IMAGE_NT_HEADERS64 nt{};
    if (!memory::read(base, dos) || dos.e_magic != IMAGE_DOS_SIGNATURE ||
        dos.e_lfanew <= 0 || dos.e_lfanew > 0x100000 ||
        base > UINTPTR_MAX - static_cast<std::uintptr_t>(dos.e_lfanew) ||
        !memory::read(base + dos.e_lfanew, nt)) return nullptr;
    for (const auto& profile : profiles) {
        if (nt.FileHeader.TimeDateStamp == profile.timestamp &&
            nt.OptionalHeader.SizeOfImage == profile.image_size &&
            nt.OptionalHeader.AddressOfEntryPoint == profile.entry_point &&
            nt.FileHeader.NumberOfSections == profile.sections) return &profile;
    }
    return nullptr;
}

inline bool file_matches(const wchar_t* path, LONGLONG expected_size, std::string_view expected_hash) {
    struct Resources {
        HANDLE file = INVALID_HANDLE_VALUE;
        BCRYPT_ALG_HANDLE algorithm{};
        BCRYPT_HASH_HANDLE hash{};
        ~Resources() {
            if (hash) BCryptDestroyHash(hash);
            if (algorithm) BCryptCloseAlgorithmProvider(algorithm, 0);
            if (file != INVALID_HANDLE_VALUE) CloseHandle(file);
        }
    } resources;
    // Deny writes/replacement while checking the file. Hash only once per attachment.
    resources.file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                                 FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
    LARGE_INTEGER size{};
    if (resources.file == INVALID_HANDLE_VALUE || !GetFileSizeEx(resources.file, &size) ||
        size.QuadPart != expected_size || expected_hash.size() != 64 ||
        BCryptOpenAlgorithmProvider(&resources.algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0 ||
        BCryptCreateHash(resources.algorithm, &resources.hash, nullptr, 0, nullptr, 0, 0) < 0) return false;
    std::array<unsigned char, 65536> buffer{};
    LONGLONG total = 0;
    for (;;) {
        DWORD count = 0;
        if (!ReadFile(resources.file, buffer.data(), static_cast<DWORD>(buffer.size()), &count, nullptr)) return false;
        if (!count) break;
        total += count;
        if (total > expected_size || BCryptHashData(resources.hash, buffer.data(), count, 0) < 0) return false;
    }
    std::array<unsigned char, 32> digest{};
    if (total != expected_size || BCryptFinishHash(resources.hash, digest.data(), static_cast<ULONG>(digest.size()), 0) < 0)
        return false;
    constexpr char hex[] = "0123456789abcdef";
    std::array<char, 64> actual{};
    for (std::size_t i = 0; i < digest.size(); ++i) {
        actual[2 * i] = hex[digest[i] >> 4];
        actual[2 * i + 1] = hex[digest[i] & 15];
    }
    return std::string_view(actual.data(), actual.size()) == expected_hash;
}

inline bool code_matches(std::uintptr_t base, const Profile& profile, DWORD& failed_rva) {
    for (std::size_t index = 0; index < profile.layout->guard_count; ++index) {
        const auto& guard = profile.layout->guards[index];
        std::array<char, 16> actual{};
        if (guard.bytes.size() > actual.size() || guard.rva >= profile.image_size ||
            guard.bytes.size() > profile.image_size - guard.rva || base > UINTPTR_MAX - guard.rva ||
            !memory::read_bytes(base + guard.rva, actual.data(), guard.bytes.size()) ||
            std::memcmp(actual.data(), guard.bytes.data(), guard.bytes.size()) != 0) {
            failed_rva = guard.rva;
            return false;
        }
    }
    failed_rva = 0;
    return true;
}

inline const Profile* validate(std::uintptr_t base, const wchar_t* path, char* message, std::size_t capacity) {
    const auto profile = identify(base);
    if (!profile) {
        snprintf(message, capacity, "Unsupported Rekordbox executable fingerprint; expected a documented 7.2.18.0 or 7.2.19.0 x64 profile.");
        return nullptr;
    }
    if (!file_matches(path, profile->file_size, profile->sha256)) {
        snprintf(message, capacity, "Rekordbox executable SHA-256/size verification failed or file is unreadable; this exact file is unsupported. [%s]", profile->name);
        return nullptr;
    }
    DWORD failed_rva = 0;
    if (!code_matches(base, *profile, failed_rva)) {
        snprintf(message, capacity, "Rekordbox %s code guards do not match; this executable is unsupported. [%s, RVA 0x%08lX]", profile->layout->version, profile->name, failed_rva);
        return nullptr;
    }
    return profile;
}

} // namespace deckstatus::rekordbox
