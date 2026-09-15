#pragma once
#include <nlohmann/json.hpp>
#include <chrono>
#include <filesystem>
#include <map>
#include <mutex>
#include <stdexcept>
#include <string>

namespace deckstatus {
struct PortalError : std::runtime_error {
    int status;
    PortalError(int code, const char* key) : std::runtime_error(key), status(code) {}
};
// Persistent users, ratings, component presets and scenes. Sessions expire on restart.
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
    Json scenes() const;
    Json edit_scene(const Json& command);
    Json scene(const std::string& id, bool include_secret = false) const;
    bool broadcast_access(const std::string& key, const std::string& path, const std::string& scene_id) const;
    Json overlay_keys(bool rotate);
    Json public_history(Json history, const std::string& voter);
    Json rate(const std::string& voter, const std::string& peer, const Json& command);
    Json ratings() const;
    std::string visitor(const std::string& cookie) const;
    static std::string random_token();
private:
    struct Session { std::string user; std::chrono::steady_clock::time_point expires; };
    struct Attempts { int count = 0; std::chrono::steady_clock::time_point until; };
    std::filesystem::path file_;
    void* lock_file_ = nullptr;
    Json data_, catalog_ = Json::object();
    mutable std::mutex mutex_;
    std::map<std::string, Session> sessions_;
    std::map<std::string, Attempts> attempts_;
    void commit(const Json& next);
    void invalidate(const std::string& user);
    void throttle(const std::string& key, int limit);
    Json user_identity(const std::string& session);
};
}
