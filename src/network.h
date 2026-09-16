#pragma once
#include <filesystem>
#include <mutex>
#include <optional>
#include <string>
#include <nlohmann/json.hpp>

namespace deckstatus {
struct NetworkOptions {
    std::string bind = "127.0.0.1";
    int port = 18740;
    bool allow_remote_control = false;
    std::string public_domain;
    bool operator==(const NetworkOptions&) const = default;
};

bool valid_bind_address(const std::string& address);
bool loopback_peer(const std::string& address);
bool local_network_peer(const std::string& peer, const std::string& destination);
bool may_control_network(const std::string& peer, bool allow_remote_control);
NetworkOptions parse_network_options(const nlohmann::json& value);
nlohmann::json network_interfaces();
std::string network_url(const std::string& address, int port);

// The active listener is immutable. Saving changes affects the next app launch.
class NetworkConfig {
    std::filesystem::path file_;
    mutable std::mutex mutex_;
    NetworkOptions saved_, active_;
    bool overridden_{};
public:
    explicit NetworkConfig(std::filesystem::path file, std::optional<std::string> bind = {},
                           std::optional<int> port = {}, std::optional<bool> remote_control = {});
    const NetworkOptions& active() const { return active_; }
    nlohmann::json describe(bool can_configure) const;
    void save(const nlohmann::json& value);
};
} // namespace deckstatus
