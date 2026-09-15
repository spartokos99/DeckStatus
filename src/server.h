#pragma once

#include <atomic>
#include <filesystem>
#include <functional>
#include <string>
#include <utility>

#include <nlohmann/json.hpp>

namespace rb {
class MasterHistory;

// cover returns { MIME type, binary image data }; empty data means no cover.
// Both callbacks can run concurrently and must return a consistent snapshot.
int run_server(const std::string& host, int port,
               const std::filesystem::path& web_root,
               std::function<nlohmann::json()> snapshot,
               std::function<std::pair<std::string, std::string>(int)> cover,
               std::atomic_bool& stop, MasterHistory* master = nullptr);

} // namespace rb
