#include "rekordbox_profile.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace rb = deckstatus::rekordbox;
void check(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }

struct Image {
    unsigned char* bytes = static_cast<unsigned char*>(VirtualAlloc(nullptr, rb::profiles[1].image_size,
                                                                   MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
    Image() { check(bytes != nullptr, "Cannot allocate synthetic image"); }
    ~Image() { VirtualFree(bytes, 0, MEM_RELEASE); }
    std::uintptr_t base() const { return reinterpret_cast<std::uintptr_t>(bytes); }
    IMAGE_NT_HEADERS64& set(const rb::Profile& profile) {
        std::memset(bytes, 0, 4096);
        auto& dos = *reinterpret_cast<IMAGE_DOS_HEADER*>(bytes);
        dos.e_magic = IMAGE_DOS_SIGNATURE;
        dos.e_lfanew = 0x100;
        auto& nt = *reinterpret_cast<IMAGE_NT_HEADERS64*>(bytes + dos.e_lfanew);
        nt.Signature = IMAGE_NT_SIGNATURE;
        nt.FileHeader.Machine = IMAGE_FILE_MACHINE_AMD64;
        nt.FileHeader.NumberOfSections = profile.sections;
        nt.FileHeader.SizeOfOptionalHeader = sizeof(IMAGE_OPTIONAL_HEADER64);
        nt.FileHeader.TimeDateStamp = profile.timestamp;
        nt.OptionalHeader.Magic = IMAGE_NT_OPTIONAL_HDR64_MAGIC;
        nt.OptionalHeader.SizeOfImage = profile.image_size;
        nt.OptionalHeader.AddressOfEntryPoint = profile.entry_point;
        auto section = IMAGE_FIRST_SECTION(&nt);
        section->VirtualAddress = 4096;
        section->Misc.VirtualSize = profile.image_size - 4096;
        section->Characteristics = IMAGE_SCN_MEM_EXECUTE;
        for (const auto& guard : rb::guards)
            std::memcpy(bytes + guard.rva, guard.bytes.data(), guard.bytes.size());
        return nt;
    }
};

struct TempFile {
    std::filesystem::path path;
    TempFile() {
        wchar_t directory[MAX_PATH + 1]{}, file[MAX_PATH + 1]{};
        check(GetTempPathW(MAX_PATH, directory) && GetTempFileNameW(directory, L"dsp", 0, file), "Cannot create temporary test file");
        path = file;
    }
    ~TempFile() { DeleteFileW(path.c_str()); }
};

// Optional, read-only check of an installed file. Never executes it or attaches to a process.
void verify_file(const wchar_t* path) {
    struct Mapping {
        HANDLE file = INVALID_HANDLE_VALUE, mapping{};
        void* view{};
        ~Mapping() {
            if (view) UnmapViewOfFile(view);
            if (mapping) CloseHandle(mapping);
            if (file != INVALID_HANDLE_VALUE) CloseHandle(file);
        }
    } mapped;
    mapped.file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
    check(mapped.file != INVALID_HANDLE_VALUE, "Cannot open profile file");
    mapped.mapping = CreateFileMappingW(mapped.file, nullptr, PAGE_READONLY | SEC_IMAGE_NO_EXECUTE, 0, 0, nullptr);
    check(mapped.mapping != nullptr, "Cannot map PE image without execution");
    mapped.view = MapViewOfFile(mapped.mapping, FILE_MAP_READ, 0, 0, 0);
    check(mapped.view != nullptr, "Cannot read PE mapping");
    char message[512]{};
    const auto profile = rb::validate(reinterpret_cast<std::uintptr_t>(mapped.view), path, message, sizeof(message));
    check(profile != nullptr, message);
    std::cout << "Verified " << profile->name << ": PE profile, full-file SHA-256 and all 14 mapped code guards.\n";
}

int wmain(int argc, wchar_t** argv) {
    try {
        if (argc > 1) {
            check(argc >= 3 && std::wstring_view(argv[1]) == L"--verify", "Usage: rekordbox_profile_test [--verify <exe> ...]");
            for (int i = 2; i < argc; ++i) verify_file(argv[i]);
            return 0;
        }
        Image image;
        for (const auto& profile : rb::profiles) {
            auto& nt = image.set(profile);
            DWORD failed = 0;
            check(rb::identify(image.base()) == &profile, "Known PE variant rejected");
            check(rb::code_matches(image.base(), profile, failed) && !failed, "Known code guards rejected");
            for (const auto& guard : rb::guards) {
                image.bytes[guard.rva] ^= 1;
                check(!rb::code_matches(image.base(), profile, failed) && failed == guard.rva, "Modified code guard accepted or wrong diagnostic RVA");
                image.bytes[guard.rva] ^= 1;
            }
            ++nt.FileHeader.TimeDateStamp;
            check(!rb::identify(image.base()), "Unknown timestamp accepted");
            image.set(profile).OptionalHeader.SizeOfImage += 4096;
            check(!rb::identify(image.base()), "Unknown image size accepted");
            image.set(profile).OptionalHeader.AddressOfEntryPoint++;
            check(!rb::identify(image.base()), "Unknown entry point accepted");
            image.set(profile).FileHeader.NumberOfSections++;
            check(!rb::identify(image.base()), "Unknown section count accepted");
            image.set(profile).FileHeader.Machine = IMAGE_FILE_MACHINE_I386;
            check(!rb::identify(image.base()), "Wrong architecture accepted");
            image.set(profile).Signature = 0;
            check(!rb::identify(image.base()), "Invalid PE accepted");
            image.set(profile);
            DWORD old = 0;
            check(VirtualProtect(image.bytes + rb::guards[0].rva, 1, PAGE_NOACCESS, &old), "Cannot protect guard page");
            check(!rb::code_matches(image.base(), profile, failed) && failed == rb::guards[0].rva, "Unreadable code guard accepted");
            check(VirtualProtect(image.bytes + rb::guards[0].rva, 1, old, &old), "Cannot restore guard page");
        }
        check(!rb::identify(0), "Null image accepted");
        TempFile file;
        { std::ofstream output(file.path, std::ios::binary); output << "abc"; }
        constexpr auto abc_hash = "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad";
        check(rb::file_matches(file.path.c_str(), 3, abc_hash), "SHA-256 known-answer test failed");
        check(!rb::file_matches(file.path.c_str(), 4, abc_hash), "Wrong file size accepted");
        check(!rb::file_matches(file.path.c_str(), 3, rb::profiles[0].sha256), "Unknown SHA-256 accepted");
        char message[512]{};
        for (const auto& profile : rb::profiles) {
            image.set(profile);
            check(!rb::validate(image.base(), file.path.c_str(), message, sizeof(message)), "PE/code match bypassed disk identity");
            check(std::string_view(message).find("SHA-256") != std::string_view::npos, "Missing hash diagnostic");
        }
        { std::ofstream output(file.path, std::ios::binary); output << "abd"; }
        check(!rb::file_matches(file.path.c_str(), 3, abc_hash), "Same-size file mutation accepted");
        check(DeleteFileW(file.path.c_str()), "Cannot remove owned test file");
        check(!rb::file_matches(file.path.c_str(), 3, abc_hash), "Missing file accepted");
        std::cout << "Both PE profiles, all 14 guards, rejection boundaries and SHA-256 file identity passed.\n";
        return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
