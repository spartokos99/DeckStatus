#pragma once
#include <atomic>
#include <filesystem>
#include <memory>
#include <string_view>
#include <vector>
#include <nlohmann/json.hpp>

namespace deckstatus {
class Updater {
public:
    Updater(const std::filesystem::path& directory, const std::filesystem::path& data,
            const std::filesystem::path& network, const std::vector<std::wstring>& arguments,
            std::atomic_bool& stopping, bool automatic_check);
    ~Updater();
    nlohmann::json describe() const;
    nlohmann::json summary() const;
    nlohmann::json command(const nlohmann::json& value);
    nlohmann::json upload(std::string_view id, std::uint64_t offset, std::string_view bytes);
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
