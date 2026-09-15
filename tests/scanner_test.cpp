#include "scanner.h"
#include <iostream>
#include <stdexcept>

void check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
struct Memory {
    unsigned char* bytes;
    explicit Memory(std::size_t size) : bytes(static_cast<unsigned char*>(
        VirtualAlloc(nullptr, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE))) {
        if (!bytes) throw std::runtime_error("VirtualAlloc failed");
    }
    ~Memory() { VirtualFree(bytes, 0, MEM_RELEASE); }
    std::uintptr_t address(std::size_t offset = 0) const { return reinterpret_cast<std::uintptr_t>(bytes + offset); }
};

int main() {
    try {
        Memory strings(8192);
        char output[8]{};
        check(deckstatus::memory::utf8_string(0, output, sizeof(output)) && !output[0], "Null string must be empty");
        std::memcpy(strings.bytes, "abc\xf0\x9f\x8e\xb5long", 12);
        check(deckstatus::memory::utf8_string(strings.address(), output, 6) && std::string(output) == "abc",
              "Truncation must discard partial UTF-8 codepoint");
        check(deckstatus::memory::utf8_string(strings.address(), output, 8) && std::string(output) == "abc\xf0\x9f\x8e\xb5",
              "Truncation must preserve complete UTF-8 codepoint");
        std::memcpy(strings.bytes, "\xed\xa0\x80", 4);
        check(!deckstatus::memory::utf8_string(strings.address(), output, sizeof(output)), "Surrogate must fail UTF-8 validation");
        std::memcpy(strings.bytes + 4093, "xy", 3);
        DWORD old{};
        check(VirtualProtect(strings.bytes + 4096, 4096, PAGE_NOACCESS, &old), "Cannot protect test page");
        check(deckstatus::memory::utf8_string(strings.address(4093), output, sizeof(output)) && std::string(output) == "xy",
              "String at readable page end must succeed");
        check(!deckstatus::memory::utf8_string(strings.address(4096), output, sizeof(output)), "Unreadable memory must fail");

        constexpr DWORD image_size = 0x104000, section_start = 0x1000, section_size = image_size - section_start;
        Memory image(image_size);
        auto dos = reinterpret_cast<IMAGE_DOS_HEADER*>(image.bytes);
        dos->e_magic = IMAGE_DOS_SIGNATURE;
        dos->e_lfanew = 0x100;
        auto nt = reinterpret_cast<IMAGE_NT_HEADERS64*>(image.bytes + 0x100);
        nt->Signature = IMAGE_NT_SIGNATURE;
        nt->FileHeader.Machine = IMAGE_FILE_MACHINE_AMD64;
        nt->FileHeader.NumberOfSections = 1;
        nt->FileHeader.SizeOfOptionalHeader = sizeof(IMAGE_OPTIONAL_HEADER64);
        nt->OptionalHeader.Magic = IMAGE_NT_OPTIONAL_HDR64_MAGIC;
        nt->OptionalHeader.SizeOfImage = image_size;
        auto section = IMAGE_FIRST_SECTION(nt);
        section->VirtualAddress = section_start;
        section->Misc.VirtualSize = section_size;
        section->Characteristics = IMAGE_SCN_MEM_EXECUTE;
        const unsigned char pattern[] = {0xAB, 0xDC, 0x89, 0xCD};
        constexpr auto crossing = section_start + 1024 * 1024 - 2;
        std::memcpy(image.bytes + crossing, pattern, sizeof(pattern));
        deckstatus::memory::ImageScanner scanner;
        check(scanner.initialize(reinterpret_cast<HMODULE>(image.bytes)), "Valid image rejected");
        bool ambiguous{};
        check(scanner.unique("AB ?? 89 CD", ambiguous) == image.address(crossing) && !ambiguous,
              "Signature crossing scan block boundary must be found once");
        std::memcpy(image.bytes + image_size - 4, pattern, sizeof(pattern));
        check(!scanner.unique("AB ?? 89 CD", ambiguous) && ambiguous, "Duplicate signature must be rejected");
        std::memset(image.bytes + crossing, 0, sizeof(pattern));
        check(scanner.unique("AB DC 89 CD", ambiguous) == image.address(image_size - 4),
              "Last valid signature start must be included");
        check(!scanner.unique("ZZ", ambiguous) && !ambiguous, "Malformed signature must fail cleanly");
        section->Misc.VirtualSize = image_size;
        check(!scanner.initialize(reinterpret_cast<HMODULE>(image.bytes)), "Out-of-image section must be rejected");
        check(!scanner.unique("AB DC 89 CD", ambiguous), "Failed initialization must clear previous sections");
        std::cout << "UTF-8, inaccessible pages, signature boundaries and ambiguity checks passed.\n";
        return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
