#pragma once
#include <nlohmann/json.hpp>
#include <chrono>
#include <filesystem>
#include <map>
#include <mutex>
#include <shared_mutex>
#include <stdexcept>
#include <string>

namespace deckstatus {
struct PortalError : std::runtime_error {
    int status;
    PortalError(int code, const char* key) : std::runtime_error(key), status(code) {}
};
// Persistent users, ratings, component presets and scenes. Sessions expire on restart.
// Uploaded media lives in a "media" directory beside the store, addressed by content
// hash, so saving a vote or a scene never rewrites megabytes of image data.
class Portal {
public:
    using Json = nlohmann::json;
    explicit Portal(const std::filesystem::path& directory);
    ~Portal();
    Portal(const Portal&) = delete;
    Portal& operator=(const Portal&) = delete;
    std::string initial_password() const;
    Json login(const std::string& username, const std::string& password, const std::string& peer);
    Json identity(const std::string& session);
    void logout(const std::string& session);
    void change_password(const std::string& session, const Json& command);
    Json users() const;
    Json edit_user(const std::string& actor, const Json& command);
    Json presets() const;
    Json edit_preset(const Json& command);
    Json media() const;
    Json audio_settings() const;
    Json save_audio_settings(const Json& settings);
    // Server-wide master hold time in milliseconds; see src/master_gate.h.
    Json master_settings() const;
    Json save_master_settings(const Json& settings);
    // Native integration only: secrets are DPAPI-encrypted at rest, never HTTP output.
    Json twitch_settings() const;
    void save_twitch_settings(const Json& settings);
    Json twitch_credentials() const;
    void save_twitch_credentials(const Json& credentials);
    Json edit_media(const Json& command);
    std::pair<std::string, std::string> media_file(const std::string& id) const;
    Json scenes() const;
    Json edit_scene(const Json& command);
    Json scene(const std::string& id, bool include_secret = false) const;
    bool broadcast_access(const std::string& key, const std::string& path, const std::string& scene_id) const;
    Json overlay_keys(bool rotate);
    Json public_history(Json history, const std::string& voter);
    Json rate(const std::string& voter, const std::string& peer, const Json& command);
    Json ratings() const;
    Json rate_twitch(const Json& viewer,const std::string& peer,const Json& command);
    Json rating_viewers(const std::string& track) const;
    std::string visitor(const std::string& cookie) const;
    static std::string random_token();
private:
    struct Session { std::string user; std::chrono::steady_clock::time_point expires; };
    struct Attempts { int count = 0; std::chrono::steady_clock::time_point until; };
    std::filesystem::path file_, media_dir_;
    void* lock_file_ = nullptr;
    Json data_, catalog_ = Json::object();
    mutable std::shared_mutex mutex_;
    std::map<std::string, Session> sessions_;
    std::map<std::string, Attempts> attempts_;
    // Readers share the lock; every writer holds it exclusively. Expensive work
    // (key derivation, media file I/O) happens before or after the critical section.
    void commit(Json&& next);
    void invalidate(const std::string& user);
    void throttle(const std::string& key, int limit);
    Json user_identity(const std::string& session) const;
    std::filesystem::path asset_path(const std::string& id) const;
    std::string read_asset(const std::string& id) const;
    void write_asset(const std::string& id, const std::string& bytes) const;
    void remove_asset(const std::string& id) const;
    void prune_assets() const;
};
}
