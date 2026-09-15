#pragma once

#include <Windows.h>
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <sstream>
#include <string>
#include <vector>

namespace deckstatus::memory {

inline bool read_bytes(std::uintptr_t address, void* destination, std::size_t count) {
    if (!address || !destination || !count || address > UINTPTR_MAX - count) return false;
    SIZE_T copied = 0;
    return ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<const void*>(address),
                             destination, count, &copied) && copied == count;
}

template<class T> bool read(std::uintptr_t address, T& destination) {
    return read_bytes(address, &destination, sizeof(destination));
}

// Validate complete code points and optionally discard a final partial code
// point at the output limit. Invalid UTF-8 inside the retained prefix is an error.
inline bool utf8_prefix(const char* bytes, std::size_t length, bool truncated,
                        std::size_t& prefix_length) {
    prefix_length = 0;
    while (prefix_length < length) {
        const auto lead = static_cast<unsigned char>(bytes[prefix_length]);
        const std::size_t width = lead < 0x80 ? 1 :
            lead >= 0xC2 && lead <= 0xDF ? 2 :
            lead >= 0xE0 && lead <= 0xEF ? 3 :
            lead >= 0xF0 && lead <= 0xF4 ? 4 : 0;
        if (!width) return false;
        const auto available = (std::min)(width, length - prefix_length);
        for (std::size_t index = 1; index < available; ++index) {
            const auto next = static_cast<unsigned char>(bytes[prefix_length + index]);
            if ((next & 0xC0) != 0x80) return false;
            if (index == 1 && ((lead == 0xE0 && next < 0xA0) ||
                              (lead == 0xED && next >= 0xA0) ||
                              (lead == 0xF0 && next < 0x90) ||
                              (lead == 0xF4 && next >= 0x90))) return false;
        }
        if (available < width) return truncated;
        prefix_length += width;
    }
    return true;
}

// A null native string is an absent optional field, represented by an empty
// string. An unreadable non-null pointer or malformed retained UTF-8 is an error.
// Long fields succeed with a bounded prefix ending on a code-point boundary.
inline bool utf8_string(std::uintptr_t address, char* destination, std::size_t capacity) {
    if (!destination || !capacity) return false;
    destination[0] = '\0';
    if (!address) return true;
    std::size_t used = 0;
    bool terminated = false;
    while (used < capacity - 1) {
        if (address > UINTPTR_MAX - used) return false;
        // Stay within the current page: a short string at a page boundary is valid.
        const auto page_left = 4096u - static_cast<unsigned>((address + used) & 4095u);
        const auto amount = (std::min)({std::size_t{128}, capacity - 1 - used,
                                      static_cast<std::size_t>(page_left)});
        char chunk[128];
        if (!read_bytes(address + used, chunk, amount)) {
            destination[0] = '\0';
            return false;
        }
        const auto end = static_cast<const char*>(std::memchr(chunk, 0, amount));
        const auto length = end ? static_cast<std::size_t>(end - chunk) : amount;
        std::memcpy(destination + used, chunk, length);
        used += length;
        destination[used] = '\0';
        if (end) { terminated = true; break; }
    }
    std::size_t prefix_length = 0;
    const bool valid = utf8_prefix(destination, used, !terminated, prefix_length);
    destination[valid ? prefix_length : 0] = '\0';
    return valid;
}

class ImageScanner {
public:
    bool initialize(HMODULE module) {
        sections_.clear();
        base_ = reinterpret_cast<std::uintptr_t>(module);
        IMAGE_DOS_HEADER dos{};
        if (!read(base_, dos) || dos.e_magic != IMAGE_DOS_SIGNATURE ||
            dos.e_lfanew <= 0 || dos.e_lfanew > 0x100000) return false;
        IMAGE_NT_HEADERS64 nt{};
        const auto nt_address = base_ + static_cast<std::uintptr_t>(dos.e_lfanew);
        if (!read(nt_address, nt) || nt.Signature != IMAGE_NT_SIGNATURE ||
            nt.FileHeader.Machine != IMAGE_FILE_MACHINE_AMD64 ||
            nt.OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC ||
            !nt.FileHeader.NumberOfSections || nt.FileHeader.NumberOfSections > 96 ||
            nt.FileHeader.SizeOfOptionalHeader < sizeof(IMAGE_OPTIONAL_HEADER64) ||
            !nt.OptionalHeader.SizeOfImage || base_ > UINTPTR_MAX - nt.OptionalHeader.SizeOfImage)
            return false;
        const auto section_address = nt_address + sizeof(DWORD) + sizeof(IMAGE_FILE_HEADER) +
                                     nt.FileHeader.SizeOfOptionalHeader;
        const auto image_end = base_ + nt.OptionalHeader.SizeOfImage;
        const auto table_size = nt.FileHeader.NumberOfSections * sizeof(IMAGE_SECTION_HEADER);
        if (section_address > image_end || table_size > image_end - section_address) return false;
        std::vector<Section> sections;
        for (unsigned index = 0; index < nt.FileHeader.NumberOfSections; ++index) {
            IMAGE_SECTION_HEADER section{};
            if (!read(section_address + index * sizeof(section), section)) return false;
            if (!(section.Characteristics & IMAGE_SCN_MEM_EXECUTE)) continue;
            if (section.VirtualAddress >= nt.OptionalHeader.SizeOfImage) return false;
            const auto size = section.Misc.VirtualSize;
            if (size > nt.OptionalHeader.SizeOfImage - section.VirtualAddress) return false;
            if (size) sections.push_back({base_ + section.VirtualAddress, size});
        }
        sections_ = std::move(sections);
        return !sections_.empty();
    }

    std::uintptr_t unique(const char* signature, bool& ambiguous) const {
        ambiguous = false;
        if (!signature) return 0;
        std::vector<int> pattern;
        std::istringstream tokens(signature);
        std::string token;
        const auto hex_digit = [](char value) {
            if (value >= '0' && value <= '9') return value - '0';
            if (value >= 'a' && value <= 'f') return value - 'a' + 10;
            if (value >= 'A' && value <= 'F') return value - 'A' + 10;
            return -1;
        };
        while (tokens >> token) {
            if (token == "?" || token == "??") { pattern.push_back(-1); continue; }
            if (token.size() != 2) return 0;
            const auto high = hex_digit(token[0]), low = hex_digit(token[1]);
            if (high < 0 || low < 0) return 0;
            pattern.push_back(high * 16 + low);
        }
        if (pattern.empty()) return 0;
        std::uintptr_t match = 0;
        constexpr std::size_t block_size = 1024 * 1024;
        for (const auto& section : sections_) {
            for (std::size_t offset = 0; offset < section.size;) {
                const auto count = (std::min)(block_size + pattern.size() - 1, section.size - offset);
                std::vector<unsigned char> bytes(count);
                if (!read_bytes(section.address + offset, bytes.data(), count)) return 0;
                if (count >= pattern.size()) {
                    const auto starts = (std::min)(block_size, count - pattern.size() + 1);
                    for (std::size_t at = 0; at < starts; ++at) {
                        bool equal = true;
                        for (std::size_t byte = 0; byte < pattern.size(); ++byte) {
                            if (pattern[byte] >= 0 && bytes[at + byte] != pattern[byte]) {
                                equal = false;
                                break;
                            }
                        }
                        if (!equal) continue;
                        if (match) { ambiguous = true; return 0; }
                        match = section.address + offset + at;
                    }
                }
                offset += (std::min)(block_size, section.size - offset);
            }
        }
        return match;
    }

private:
    struct Section { std::uintptr_t address; std::size_t size; };
    std::uintptr_t base_{};
    std::vector<Section> sections_;
};

} // namespace deckstatus::memory
