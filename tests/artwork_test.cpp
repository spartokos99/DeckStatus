#include "artwork.h"

#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string_view>

namespace {
using Path = std::filesystem::path;
struct sqlite3;

void check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

std::string utf8(const Path& path) {
    const auto bytes = path.u8string();
    return {reinterpret_cast<const char*>(bytes.data()), bytes.size()};
}

struct Fixture {
    Path root;
    HMODULE library = nullptr;
    sqlite3* db = nullptr;
    int (__cdecl* close)(sqlite3*) = nullptr;
    int (__cdecl* execute)(sqlite3*, const char*, void*, void*, char**) = nullptr;

    ~Fixture() {
        if (db && close) close(db);
        if (library) FreeLibrary(library);
        // Delete only files this fixture creates; no recursive cleanup.
        if (!root.empty()) {
            std::error_code error;
            for (const auto* file : {L"collection/share/ARTWORK/cover.png", L"outside.png",
                                     L"collection/master.db", L"collection/master.db-journal"})
                std::filesystem::remove(root / file, error);
            for (const auto* directory : {L"collection/share/ARTWORK", L"collection/share", L"collection"})
                std::filesystem::remove(root / directory, error);
            std::filesystem::remove(root, error);
        }
    }

    void sql(const std::string& query) {
        check(execute(db, query.c_str(), nullptr, nullptr, nullptr) == 0, "Fixture SQL failed");
    }

    void initialize(const Path& executable) {
        library = LoadLibraryExW((executable.parent_path() / L"sqlite3.dll").c_str(), nullptr,
                                  LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
        check(library != nullptr, "Cannot load the installed Rekordbox sqlite3.dll");
        using Open = int (__cdecl*)(const char*, sqlite3**, int, const char*);
        const auto open = reinterpret_cast<Open>(GetProcAddress(library, "sqlite3_open_v2"));
        close = reinterpret_cast<decltype(close)>(GetProcAddress(library, "sqlite3_close_v2"));
        execute = reinterpret_cast<decltype(execute)>(GetProcAddress(library, "sqlite3_exec"));
        check(open && close && execute, "Installed sqlite3.dll is missing test functions");
        const auto candidate = std::filesystem::temp_directory_path() /
            (L"rb_artwork_test_" + std::to_wstring(GetCurrentProcessId()) + L"_" +
             std::to_wstring(GetTickCount64()));
        check(std::filesystem::create_directory(candidate), "Cannot create a unique fixture directory");
        root = candidate;
        std::filesystem::create_directories(root / L"collection/share/ARTWORK");
        const auto db_name = utf8(root / L"collection/master.db");
        check(open(db_name.c_str(), &db, 0x00000002 | 0x00000004, nullptr) == 0,
              "Cannot create fixture database");
        sql("CREATE TABLE djmdContent(ID TEXT PRIMARY KEY,Title TEXT,ArtistID TEXT,AlbumID TEXT,"
            "GenreID TEXT,KeyID TEXT,LabelID TEXT,BPM INTEGER,ImagePath TEXT);"
            "CREATE TABLE djmdArtist(ID TEXT PRIMARY KEY,Name TEXT);"
            "CREATE TABLE djmdAlbum(ID TEXT PRIMARY KEY,Name TEXT);"
            "CREATE TABLE djmdGenre(ID TEXT PRIMARY KEY,Name TEXT);"
            "CREATE TABLE djmdKey(ID TEXT PRIMARY KEY,ScaleName TEXT);"
            "CREATE TABLE djmdLabel(ID TEXT PRIMARY KEY,Name TEXT);"
            "INSERT INTO djmdArtist VALUES('1','Bj\xc3\xb6rk');"
            "INSERT INTO djmdAlbum VALUES('2','\xe6\x9d\xb1\xe4\xba\xac');"
            "INSERT INTO djmdGenre VALUES('3','House');"
            "INSERT INTO djmdKey VALUES('4','Am');"
            "INSERT INTO djmdLabel VALUES('5','Test Label');"
            "INSERT INTO djmdContent VALUES('123','Track \xf0\x9f\x8e\xb5','1','2','3','4','5',12850,'/ARTWORK/cover.png');"
            "INSERT INTO djmdContent(ID,Title,BPM) VALUES('124',NULL,NULL);"
            "INSERT INTO djmdContent(ID,Title,ArtistID,AlbumID,GenreID,KeyID,LabelID,BPM) "
            "VALUES('125','Missing relations','missing','missing','missing','missing','missing',-1);"
            "INSERT INTO djmdContent(ID,Title) VALUES('126','" +
            std::string(1022, 'a') + "\xf0\x9f\x8e\xb5');"
            "INSERT INTO djmdContent(ID,Title) VALUES('127',CAST(X'EDA080' AS TEXT));"
            "INSERT INTO djmdContent(ID,Title) VALUES('128','" +
            std::string(1019, 'a') + "\xf0\x9f\x8e\xb5suffix');"
            "INSERT INTO djmdContent(ID,ImagePath) VALUES('130','../outside.png');"
            "INSERT INTO djmdContent(ID,ImagePath) VALUES('131','//server/share/cover.png');"
            "INSERT INTO djmdContent(ID,ImagePath) VALUES('132','share/ARTWORK/cover.png:secret');"
            "INSERT INTO djmdContent(ID,ImagePath) VALUES('133','share/ARTWORK/cover.png');");
        check(close(db) == 0, "Cannot finish fixture database");
        db = nullptr;
        // A complete one-pixel PNG; the resolver also checks its signature.
        constexpr unsigned char png[] = {
            0x89,0x50,0x4e,0x47,0x0d,0x0a,0x1a,0x0a,0,0,0,0x0d,0x49,0x48,0x44,0x52,
            0,0,0,1,0,0,0,1,8,6,0,0,0,0x1f,0x15,0xc4,0x89,
            0,0,0,0x0b,0x49,0x44,0x41,0x54,0x78,0x9c,0x63,0,1,0,0,5,0,1,0x0d,0x0a,0x2d,0xb4,
            0,0,0,0,0x49,0x45,0x4e,0x44,0xae,0x42,0x60,0x82};
        for (const auto* file : {L"collection/share/ARTWORK/cover.png", L"outside.png"}) {
            std::ofstream output(root / file, std::ios::binary);
            output.write(reinterpret_cast<const char*>(png), sizeof(png));
            check(output.good(), "Cannot create fixture cover");
        }
    }
};

void check_live(const rb::DeckData& deck) {
    check(deck.id == 2 && deck.bpm_x100 == 13025 && std::string_view(deck.file_path) == "untouched",
          "Enrichment must preserve live BPM, deck ID and path fields");
}

void check_empty(const rb::DeckData& deck) {
    check(!deck.metadata_available && !deck.original_bpm_x100 && !deck.title[0] && !deck.artist[0] &&
          !deck.album[0] && !deck.key[0] && !deck.genre[0] && !deck.label[0],
          "Missing content must clear stale metadata");
    check_live(deck);
}
}

int wmain(int argc, wchar_t** argv) {
    if (argc != 2) {
        std::cout << "Skipped: pass the installed rekordbox.exe path to test its sqlite3.dll.\n";
        return 77;
    }
    try {
        const auto executable = std::filesystem::canonical(argv[1]);
        Fixture fixture;
        fixture.initialize(executable);
        const auto db_path = fixture.root / L"collection/master.db";
        const auto before = std::filesystem::last_write_time(db_path);
        {
            rb::ArtworkResolver resolver(executable, db_path);
            rb::DeckData deck{};
            deck.id = 2;
            deck.track_id = 123;
            deck.bpm_x100 = 13025;
            std::memcpy(deck.file_path, "untouched", 10);
            check(resolver.enrich(deck) && deck.metadata_available == 1, "Metadata lookup failed");
            check(deck.track_id == 123 && deck.original_bpm_x100 == 12850, "Track ID or original BPM changed");
            check(std::string_view(deck.title) == "Track \xf0\x9f\x8e\xb5" &&
                  std::string_view(deck.artist) == "Bj\xc3\xb6rk" &&
                  std::string_view(deck.album) == "\xe6\x9d\xb1\xe4\xba\xac" &&
                  std::string_view(deck.key) == "Am" && std::string_view(deck.genre) == "House" &&
                  std::string_view(deck.label) == "Test Label", "Joined metadata or UTF-8 did not match");
            check_live(deck);
            const auto image = resolver.get(123);
            check(image.first == "image/png" && image.second.size() > 8, "Collection-relative cover failed");
            check(resolver.get(133) == image, "share-relative cover failed");
            check(resolver.get(130).second.empty(), "Traversal must be rejected");
            check(resolver.get(131).second.empty(), "Network paths must be rejected");
            check(resolver.get(132).second.empty(), "Alternate data streams must be rejected");
            deck.track_id = 124;
            check(resolver.enrich(deck) && deck.metadata_available && !deck.title[0] && !deck.artist[0] &&
                  !deck.album[0] && !deck.key[0] && !deck.original_bpm_x100, "NULL columns must be empty");
            deck.track_id = 125;
            check(resolver.enrich(deck) && std::string_view(deck.title) == "Missing relations" &&
                  !deck.original_bpm_x100, "LEFT JOINs or invalid BPM handling failed");
            deck.track_id = 126;
            check(resolver.enrich(deck) && std::string_view(deck.title) == std::string(1022, 'a'),
                  "Truncated metadata must end on a complete UTF-8 codepoint");
            deck.track_id = 127;
            check(resolver.enrich(deck) && !deck.title[0], "Invalid UTF-8 must not escape the resolver");
            deck.track_id = 128;
            check(resolver.enrich(deck) && std::string_view(deck.title) ==
                  std::string(1019, 'a') + "\xf0\x9f\x8e\xb5",
                  "A complete UTF-8 codepoint at buffer capacity must be preserved");
            deck.track_id = 123;
            check(resolver.enrich(deck), "Cannot reload fixture track");
            deck.track_id = 999;
            check(!resolver.enrich(deck) && deck.track_id == 999, "Missing content must return false");
            check_empty(deck);
            deck.track_id = 123;
            check(resolver.enrich(deck), "Cannot reload fixture track before unload");
            deck.track_id = 0;
            check(!resolver.enrich(deck), "Unloaded deck must return false");
            check_empty(deck);
        }
        check(std::filesystem::last_write_time(db_path) == before, "Resolver modified the collection database");
        std::cout << "Read-only metadata joins, NULLs, UTF-8, missing tracks, covers and traversal checks passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
