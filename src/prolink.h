#pragma once
#include <filesystem>
#include <memory>
#include <nlohmann/json.hpp>

namespace deckstatus {
// The optional network backend owns its helper process. Construction opens no network sockets.
class ProLink {
public:
    explicit ProLink(std::filesystem::path directory);
    ~ProLink();
    ProLink(const ProLink&) = delete;
    ProLink& operator=(const ProLink&) = delete;
    nlohmann::json snapshot();
    nlohmann::json setup();
    nlohmann::json control(const nlohmann::json& command);
    std::pair<std::string, std::string> cover(std::uint32_t track_id);
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
nlohmann::json prolink_empty_state(const std::string& message);
bool valid_prolink_command(const nlohmann::json& command);
}
