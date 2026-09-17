#pragma once

#include <atomic>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <utility>

#include <nlohmann/json.hpp>

namespace deckstatus {
class MasterHistory;
class NetworkConfig;
class Portal;
struct TwitchTransport;
struct ServerFeatures {
    std::string mode = "rekordbox";
    std::function<nlohmann::json()> prolink_setup;
    std::function<nlohmann::json(const nlohmann::json&)> prolink_control;
    NetworkConfig* network = nullptr;
    Portal* portal = nullptr;
    // Optional protocol fixture; production leaves this empty.
    std::shared_ptr<TwitchTransport> twitch_transport;
};

// cover returns { MIME type, binary image data }; empty data means no cover.
// Both callbacks can run concurrently and must return a consistent snapshot.
int run_server(const std::string& host, int port,
               const std::filesystem::path& web_root,
               std::function<nlohmann::json()> snapshot,
               std::function<std::pair<std::string, std::string>(int)> cover,
               std::atomic_bool& stop, MasterHistory* master = nullptr, ServerFeatures* features = nullptr);

} // namespace deckstatus
