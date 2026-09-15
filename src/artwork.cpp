#include "artwork.h"

#include <windows.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstring>
#include <fstream>
#include <limits>
#include <mutex>
#include <optional>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace deckstatus {
namespace {

using Path = std::filesystem::path;
using Image = std::pair<std::string, std::string>;
using Clock = std::chrono::steady_clock;
constexpr std::size_t max_image_size = 8 * 1024 * 1024;
constexpr std::size_t max_cache_size = 32 * 1024 * 1024;
constexpr std::size_t max_cache_entries = 16;
struct sqlite3;
struct sqlite3_stmt;

struct Module {
    HMODULE value = nullptr;
    ~Module() { if (value) FreeLibrary(value); }
};

struct FileHandle {
    HANDLE value = INVALID_HANDLE_VALUE;
    ~FileHandle() { if (value != INVALID_HANDLE_VALUE) CloseHandle(value); }
};

std::optional<Path> canonical_local(const Path& path) {
    std::error_code error;
    const auto absolute = std::filesystem::absolute(path, error);
    if (error) return {};
    const auto spelling = absolute.native();
    if (spelling.size() < 3 || spelling[1] != L':' || spelling.find(L':', 2) != std::wstring::npos)
        return {}; // Exclude UNC/device paths and alternate data streams.
    const auto root = absolute.root_path().native();
    const auto drive = GetDriveTypeW(root.c_str());
    if (drive != DRIVE_FIXED && drive != DRIVE_REMOVABLE && drive != DRIVE_RAMDISK) return {};
    auto canonical = std::filesystem::canonical(absolute, error);
    if (error) return {};
    const auto real_root = canonical.root_path().native();
    const auto real_drive = GetDriveTypeW(real_root.c_str());
    if (real_drive != DRIVE_FIXED && real_drive != DRIVE_REMOVABLE && real_drive != DRIVE_RAMDISK)
        return {};
    return canonical;
}

bool under(const Path& path, const Path& root) {
    auto part = path.begin();
    for (auto base = root.begin(); base != root.end(); ++base, ++part) {
        if (part == path.end() || _wcsicmp(part->c_str(), base->c_str()) != 0) return false;
    }
    return part != path.end();
}

std::optional<Path> handle_path(HANDLE handle) {
    std::wstring buffer(32768, L'\0');
    const auto length = GetFinalPathNameByHandleW(handle, buffer.data(),
                                               static_cast<DWORD>(buffer.size()), FILE_NAME_NORMALIZED);
    if (!length || length >= buffer.size()) return {};
    buffer.resize(length);
    if (buffer.starts_with(L"\\\\?\\")) buffer.erase(0, 4);
    if (buffer.size() < 3 || buffer[1] != L':') return {};
    return Path(buffer);
}

HMODULE local_library(const Path& directory, const wchar_t* filename) {
    const auto library = canonical_local(directory / filename);
    if (!library || library->parent_path() != directory) return nullptr;
    return LoadLibraryExW(library->c_str(), nullptr,
                          LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
}

std::string utf8(const Path& path) {
    const auto result = path.u8string();
    return {reinterpret_cast<const char*>(result.data()), result.size()};
}

Path from_utf8(std::string_view value) {
    return Path(std::u8string_view(reinterpret_cast<const char8_t*>(value.data()), value.size()));
}

template<std::size_t Capacity>
void copy_utf8(char (&destination)[Capacity], std::string_view source) {
    std::memset(destination, 0, Capacity);
    if (const auto nul = source.find('\0'); nul != std::string_view::npos)
        source = source.substr(0, nul);
    auto count = std::min(source.size(), Capacity - 1);
    // Drop a partial final code point before validating the retained prefix.
    while (count > 0 && count < source.size() &&
           (static_cast<unsigned char>(source[count]) & 0xc0) == 0x80) --count;
    if (count && MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, source.data(),
                                    static_cast<int>(count), nullptr, 0) > 0)
        std::memcpy(destination, source.data(), count);
}

std::string read_small(const Path& path, std::size_t limit) {
    std::error_code error;
    const auto size = std::filesystem::file_size(path, error);
    if (error || size == 0 || size > limit) return {};
    std::ifstream file(path, std::ios::binary);
    std::string result(static_cast<std::size_t>(size), '\0');
    if (!file.read(result.data(), static_cast<std::streamsize>(result.size()))) return {};
    return result;
}

Path database_path(const Path& override_path) {
    if (!override_path.empty()) return override_path;
    std::wstring appdata(32768, L'\0');
    const auto count = GetEnvironmentVariableW(L"APPDATA", appdata.data(),
                                              static_cast<DWORD>(appdata.size()));
    if (!count || count >= appdata.size()) return {};
    appdata.resize(count);
    const auto pioneer = Path(appdata) / L"Pioneer";
    const auto raw = read_small(pioneer / L"rekordboxAgent/storage/options.json", 2 * 1024 * 1024);
    if (!raw.empty()) {
        const auto json = nlohmann::json::parse(raw, nullptr, false);
        if (json.is_object() && json.contains("options") && json["options"].is_array()) {
            for (const auto& option : json["options"]) {
                if (option.is_array() && option.size() == 2 && option[0] == "db-path" &&
                    option[1].is_string()) {
                    const auto value = option[1].get<std::string>();
                    if (!value.empty() && value.find('\0') == std::string::npos)
                        return from_utf8(value);
                }
            }
        }
    }
    return pioneer / L"rekordbox/master.db";
}

// Public compatibility constants and algorithm from pyrekordbox (MIT).
// See docs/artwork-sources.md. Never print the decoded database key.
std::string database_key(const Path& library_directory) {
    constexpr std::string_view alphabet =
        "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz!#$%&()*+-;<=>?@^_`{|}~";
    constexpr std::string_view blob =
        "PN_Pq^*N>(JYe*u^8;Yg76HuZ<mR13S?=>)b9;DpoTXV(6ItkU`}8*m6tx_I{Solh_N#dfe{v=";
    constexpr std::string_view xor_key = "657f48f84c437cc1";
    std::vector<unsigned char> compressed;
    for (std::size_t offset = 0; offset < blob.size(); offset += 5) {
        const auto count = std::min<std::size_t>(5, blob.size() - offset);
        std::uint64_t word = 0;
        for (std::size_t index = 0; index < 5; ++index) {
            const auto digit = index < count ? alphabet.find(blob[offset + index]) : 84;
            if (digit == std::string_view::npos) return {};
            word = word * 85 + digit;
        }
        if (word > std::numeric_limits<std::uint32_t>::max()) return {};
        for (std::size_t index = 0; index < count - 1; ++index)
            compressed.push_back(static_cast<unsigned char>(word >> (24 - 8 * index)));
    }
    for (std::size_t index = 0; index < compressed.size(); ++index)
        compressed[index] ^= static_cast<unsigned char>(xor_key[index % xor_key.size()]);
    Module zlib;
    zlib.value = local_library(library_directory, L"zlib.dll");
    if (!zlib.value) zlib.value = local_library(library_directory, L"zlib1.dll");
    if (!zlib.value) return {};
    using Uncompress = int (__cdecl*)(unsigned char*, unsigned long*, const unsigned char*, unsigned long);
    const auto uncompress = reinterpret_cast<Uncompress>(GetProcAddress(zlib.value, "uncompress"));
    if (!uncompress) return {};
    std::array<unsigned char, 256> output{};
    unsigned long length = static_cast<unsigned long>(output.size());
    const auto status = uncompress(output.data(), &length, compressed.data(),
                                   static_cast<unsigned long>(compressed.size()));
    std::string result;
    if (status == 0 && length > 0 && length < output.size())
        result.assign(reinterpret_cast<const char*>(output.data()), length);
    SecureZeroMemory(output.data(), output.size());
    return result;
}

std::string mime_type(std::string_view bytes) {
    if (bytes.size() >= 3 && static_cast<unsigned char>(bytes[0]) == 0xff &&
        static_cast<unsigned char>(bytes[1]) == 0xd8 && static_cast<unsigned char>(bytes[2]) == 0xff)
        return "image/jpeg";
    if (bytes.starts_with(std::string_view("\x89PNG\r\n\x1a\n", 8))) return "image/png";
    if (bytes.starts_with("GIF87a") || bytes.starts_with("GIF89a")) return "image/gif";
    if (bytes.size() >= 12 && bytes.starts_with("RIFF") && bytes.substr(8, 4) == "WEBP")
        return "image/webp";
    if (bytes.size() >= 14 && bytes.starts_with("BM")) return "image/bmp";
    return {};
}

Image read_image(const Path& candidate, const Path& root) {
    const auto resolved = canonical_local(candidate);
    if (!resolved || !under(*resolved, root)) return {};
    FileHandle file;
    file.value = CreateFileW(resolved->c_str(), GENERIC_READ,
                             FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                             nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN,
                             nullptr);
    if (file.value == INVALID_HANDLE_VALUE || GetFileType(file.value) != FILE_TYPE_DISK) return {};
    // Check the actual opened handle too, so junction/symlink changes cannot escape root.
    const auto final_path = handle_path(file.value);
    if (!final_path || !under(*final_path, root)) return {};
    LARGE_INTEGER length{};
    if (!GetFileSizeEx(file.value, &length) || length.QuadPart <= 0 || length.QuadPart > max_image_size)
        return {};
    std::string bytes(static_cast<std::size_t>(length.QuadPart), '\0');
    DWORD received = 0;
    if (!ReadFile(file.value, bytes.data(), static_cast<DWORD>(bytes.size()), &received, nullptr) ||
        received != bytes.size()) return {};
    auto mime = mime_type(bytes);
    if (mime.empty()) return {};
    return {std::move(mime), std::move(bytes)};
}

Image find_image(std::string value, const Path& root) {
    if (value.empty() || value.size() > 32760 || value.find('\0') != std::string::npos) return {};
    std::replace(value.begin(), value.end(), '\\', '/');
    if (value.starts_with("//")) return {};
    auto relative = from_utf8(value);
    for (const auto& part : relative) if (part == L"..") return {};
    if (relative.is_absolute()) return read_image(relative, root);
    if (relative.has_root_name() || value.find(':') != std::string::npos) return {};
    // Rekordbox stores both /ARTWORK/... (relative to share) and share/ARTWORK/...
    // variants. A leading slash is a collection-relative path, never a drive root.
    if (value.starts_with('/')) value.erase(0, 1);
    relative = from_utf8(value);
    auto result = read_image(root / relative, root);
    if (!result.second.empty()) return result;
    return read_image(root / L"share" / relative, root);
}

} // namespace

struct ArtworkResolver::Impl {
    mutable std::mutex mutex;
    Module sqlite;
    sqlite3* db = nullptr;
    Path root;
    std::string status = "Rekordbox-Datenbank nicht verfuegbar";
    int (__cdecl* open_v2)(const char*, sqlite3**, int, const char*) = nullptr;
    int (__cdecl* close_v2)(sqlite3*) = nullptr;
    int (__cdecl* key)(sqlite3*, const void*, int) = nullptr;
    int (__cdecl* prepare)(sqlite3*, const char*, int, sqlite3_stmt**, const char**) = nullptr;
    int (__cdecl* bind_int64)(sqlite3_stmt*, int, std::int64_t) = nullptr;
    int (__cdecl* step)(sqlite3_stmt*) = nullptr;
    int (__cdecl* finalize)(sqlite3_stmt*) = nullptr;
    const unsigned char* (__cdecl* column_text)(sqlite3_stmt*, int) = nullptr;
    int (__cdecl* column_bytes)(sqlite3_stmt*, int) = nullptr;
    std::int64_t (__cdecl* column_int64)(sqlite3_stmt*, int) = nullptr;
    int (__cdecl* busy_timeout)(sqlite3*, int) = nullptr;
    struct Entry { Image image; Clock::time_point expires; std::uint64_t use; };
    std::unordered_map<std::uint32_t, Entry> cache;
    std::size_t cache_bytes = 0;
    std::uint64_t cache_use = 0;

    ~Impl() { if (db && close_v2) close_v2(db); }

    bool initialize(const Path& executable, const Path& override_path) {
        const auto real_exe = canonical_local(executable);
        if (!real_exe) { status = "Rekordbox-Programmpfad nicht verfuegbar"; return false; }
        const auto directory = real_exe->parent_path();
        sqlite.value = local_library(directory, L"sqlite3.dll");
        if (!sqlite.value) { status = "Rekordbox sqlite3.dll konnte nicht geladen werden"; return false; }
#define RB_SQLITE_LOAD(field, symbol) \
        field = reinterpret_cast<decltype(field)>(GetProcAddress(sqlite.value, symbol)); \
        if (!field) { status = "Rekordbox sqlite3.dll besitzt nicht alle benoetigten Funktionen"; return false; }
        RB_SQLITE_LOAD(open_v2, "sqlite3_open_v2")
        RB_SQLITE_LOAD(close_v2, "sqlite3_close_v2")
        RB_SQLITE_LOAD(prepare, "sqlite3_prepare_v2")
        RB_SQLITE_LOAD(bind_int64, "sqlite3_bind_int64")
        RB_SQLITE_LOAD(step, "sqlite3_step")
        RB_SQLITE_LOAD(finalize, "sqlite3_finalize")
        RB_SQLITE_LOAD(column_text, "sqlite3_column_text")
        RB_SQLITE_LOAD(column_bytes, "sqlite3_column_bytes")
        RB_SQLITE_LOAD(column_int64, "sqlite3_column_int64")
        RB_SQLITE_LOAD(busy_timeout, "sqlite3_busy_timeout")
#undef RB_SQLITE_LOAD
        key = reinterpret_cast<decltype(key)>(GetProcAddress(sqlite.value, "sqlite3_key"));
        const auto configured = database_path(override_path);
        const auto path = configured.empty() ? std::optional<Path>{} : canonical_local(configured);
        if (!path) { status = "master.db nicht gefunden; --database PFAD verwenden"; return false; }
        root = path->parent_path();
        constexpr int readonly_fullmutex = 0x00000001 | 0x00010000;
        if (open_v2(utf8(*path).c_str(), &db, readonly_fullmutex, nullptr) != 0) {
            status = "Rekordbox-Datenbank konnte nicht lesend geoeffnet werden";
            return false;
        }
        busy_timeout(db, 250);
        std::array<char, 16> header{};
        std::ifstream header_file(*path, std::ios::binary);
        header_file.read(header.data(), static_cast<std::streamsize>(header.size()));
        const bool plaintext = std::memcmp(header.data(), "SQLite format 3\0", header.size()) == 0;
        if (!plaintext) {
            auto secret = database_key(directory);
            if (!key || secret.empty()) {
                if (!secret.empty()) SecureZeroMemory(secret.data(), secret.size());
                status = "SQLCipher oder Rekordbox zlib.dll nicht verfuegbar";
                return false;
            }
            const auto result = key(db, secret.data(), static_cast<int>(secret.size()));
            SecureZeroMemory(secret.data(), secret.size());
            if (result != 0) { status = "Rekordbox-Datenbank konnte nicht entsperrt werden"; return false; }
        }
        sqlite3_stmt* statement = nullptr;
        const auto prepared = prepare(db, "SELECT ImagePath FROM djmdContent LIMIT 1", -1,
                                      &statement, nullptr);
        int result = prepared;
        if (prepared == 0 && statement) result = step(statement);
        if (statement) finalize(statement);
        if (result != 100 && result != 101) {
            status = "Rekordbox-Datenbank inkompatibel oder momentan gesperrt";
            return false;
        }
        status = "Rekordbox-Datenbank bereit (nur lesend)";
        return true;
    }

    Image lookup(std::uint32_t content_id) {
        sqlite3_stmt* statement = nullptr;
        if (prepare(db, "SELECT ImagePath FROM djmdContent WHERE ID=? LIMIT 1", -1,
                    &statement, nullptr) != 0 || !statement) {
            if (statement) finalize(statement);
            status = "Cover-Abfrage momentan nicht verfuegbar";
            return {};
        }
        std::string path;
        const auto bound = bind_int64(statement, 1, content_id);
        const auto result = bound == 0 ? step(statement) : bound;
        if (result == 100) {
            const auto value = column_text(statement, 0);
            const auto count = column_bytes(statement, 0);
            if (value && count > 0 && count <= 32760)
                path.assign(reinterpret_cast<const char*>(value), static_cast<std::size_t>(count));
        }
        finalize(statement);
        if (result != 100 && result != 101) {
            status = "Cover-Abfrage momentan nicht verfuegbar";
            return {};
        }
        status = "Rekordbox-Datenbank bereit (nur lesend)";
        return find_image(std::move(path), root);
    }

    bool metadata(DeckData& deck) {
        constexpr auto query =
            "SELECT c.Title,a.Name,album.Name,k.ScaleName,g.Name,l.Name,c.BPM "
            "FROM djmdContent c "
            "LEFT JOIN djmdArtist a ON c.ArtistID=a.ID "
            "LEFT JOIN djmdAlbum album ON c.AlbumID=album.ID "
            "LEFT JOIN djmdKey k ON c.KeyID=k.ID "
            "LEFT JOIN djmdGenre g ON c.GenreID=g.ID "
            "LEFT JOIN djmdLabel l ON c.LabelID=l.ID "
            "WHERE c.ID=? LIMIT 1";
        sqlite3_stmt* raw = nullptr;
        const auto prepared = prepare(db, query, -1, &raw, nullptr);
        std::unique_ptr<sqlite3_stmt, decltype(finalize)> statement(raw, finalize);
        if (prepared != 0 || !statement) {
            status = "Metadaten-Abfrage momentan nicht verfuegbar";
            return false;
        }
        const auto bound = bind_int64(statement.get(), 1, deck.track_id);
        const auto result = bound == 0 ? step(statement.get()) : bound;
        if (result != 100) {
            status = result == 101 ? "Rekordbox-Datenbank bereit (nur lesend)" :
                                    "Metadaten-Abfrage momentan nicht verfuegbar";
            return false;
        }
        const auto copy_column = [&](auto& target, int index) {
            const auto bytes = column_text(statement.get(), index);
            const auto count = column_bytes(statement.get(), index);
            if (bytes && count > 0)
                copy_utf8(target, {reinterpret_cast<const char*>(bytes), static_cast<std::size_t>(count)});
        };
        copy_column(deck.title, 0);
        copy_column(deck.artist, 1);
        copy_column(deck.album, 2);
        copy_column(deck.key, 3);
        copy_column(deck.genre, 4);
        copy_column(deck.label, 5);
        const auto bpm = column_int64(statement.get(), 6);
        if (bpm > 0 && bpm <= std::numeric_limits<std::uint32_t>::max())
            deck.original_bpm_x100 = static_cast<std::uint32_t>(bpm);
        deck.metadata_available = 1;
        status = "Rekordbox-Datenbank bereit (nur lesend)";
        return true;
    }
};

ArtworkResolver::ArtworkResolver(Path rekordbox_exe, Path database_override)
    : impl_(std::make_unique<Impl>()) {
    try {
        if (impl_->initialize(rekordbox_exe, database_override)) return;
    } catch (const std::exception&) {
        impl_->status = "Rekordbox-Datenbank konnte nicht initialisiert werden";
    }
    if (impl_->db && impl_->close_v2) impl_->close_v2(impl_->db);
    impl_->db = nullptr;
}

ArtworkResolver::~ArtworkResolver() = default;

Image ArtworkResolver::get(std::uint32_t content_id) {
    std::lock_guard lock(impl_->mutex);
    if (!impl_->db || !content_id) return {};
    const auto now = Clock::now();
    if (const auto entry = impl_->cache.find(content_id); entry != impl_->cache.end()) {
        if (now < entry->second.expires) {
            entry->second.use = ++impl_->cache_use;
            return entry->second.image;
        }
        impl_->cache_bytes -= entry->second.image.second.size();
        impl_->cache.erase(entry);
    }
    Image result;
    try { result = impl_->lookup(content_id); }
    catch (const std::exception&) { impl_->status = "Cover-Datei konnte nicht gelesen werden"; }
    while (!impl_->cache.empty() && (impl_->cache.size() >= max_cache_entries ||
           impl_->cache_bytes + result.second.size() > max_cache_size)) {
        const auto oldest = std::min_element(impl_->cache.begin(), impl_->cache.end(),
            [](const auto& lhs, const auto& rhs) { return lhs.second.use < rhs.second.use; });
        impl_->cache_bytes -= oldest->second.image.second.size();
        impl_->cache.erase(oldest);
    }
    impl_->cache_bytes += result.second.size();
    const auto ttl = result.second.empty() ? std::chrono::seconds(2) : std::chrono::seconds(10);
    impl_->cache.emplace(content_id, Impl::Entry{result, now + ttl, ++impl_->cache_use});
    return result;
}

bool ArtworkResolver::enrich(DeckData& deck) {
    std::lock_guard lock(impl_->mutex);
    deck.metadata_available = 0;
    deck.original_bpm_x100 = 0;
    std::memset(deck.title, 0, sizeof(deck.title));
    std::memset(deck.artist, 0, sizeof(deck.artist));
    std::memset(deck.album, 0, sizeof(deck.album));
    std::memset(deck.key, 0, sizeof(deck.key));
    std::memset(deck.genre, 0, sizeof(deck.genre));
    std::memset(deck.label, 0, sizeof(deck.label));
    if (!impl_->db || !deck.track_id) return false;
    return impl_->metadata(deck);
}

std::string ArtworkResolver::diagnostic() const {
    std::lock_guard lock(impl_->mutex);
    return impl_->status;
}

} // namespace deckstatus
