#pragma once

#include <nlohmann/json.hpp>
#include <chrono>
#include <cstdint>
#include <deque>
#include <functional>
#include <mutex>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace rb {

// One server session, independent of browser clients. Repeated tracks have distinct
// entry IDs; switching decks with the same track does not count as a track change.
class MasterHistory {
public:
    using Json = nlohmann::json;
    using Cover = std::pair<std::string, std::string>;
    static constexpr std::size_t limit = 50;

    explicit MasterHistory(std::function<Cover(std::uint32_t)> loader = {}) : loader_(std::move(loader)) {}

    void update(const Json& state) {
        std::lock_guard lock(mutex_);
        sampled_ = std::chrono::steady_clock::now();
        status_ = state.value("status", std::string("disconnected"));
        demo_ = state.value("demo", false);
        active_ = false;
        if (status_ != "connected" && status_ != "demo") return;
        const auto master = state.value("masterDeckId", Json(nullptr));
        if (!master.is_number_integer() || !state.contains("decks")) return;
        for (auto deck : state["decks"]) {
            if (deck["id"] != master || !deck.value("loaded", false) ||
                !deck["trackId"].is_number_integer() || deck["trackId"].get<std::uint32_t>() == 0) continue;
            const bool same = !current_.is_null() && current_["trackId"] == deck["trackId"];
            if (!same && !current_.is_null()) {
                current_["endedAt"] = state.value("updatedAt", Json(nullptr));
                current_["isMaster"] = false;
                history_.push_front(current_);
                if (history_.size() > limit) history_.pop_back();
            }
            // Keep successful metadata through a temporary database miss.
            if (same && !deck.value("metadataAvailable", false) && current_.value("metadataAvailable", false))
                copy_metadata(deck, current_);
            deck["entryId"] = same ? current_["entryId"] : Json(++sequence_);
            deck["startedAt"] = same ? current_["startedAt"] : state.value("updatedAt", Json(nullptr));
            deck["endedAt"] = nullptr;
            deck["isMaster"] = true;
            deck["coverUrl"] = "/api/master/covers/" + std::to_string(deck["trackId"].get<std::uint32_t>());
            current_ = std::move(deck);
            active_ = true;
            break;
        }
    }

    Json snapshot() const {
        std::lock_guard lock(mutex_);
        const bool fresh = std::chrono::steady_clock::now() - sampled_ <= std::chrono::seconds(3);
        return {{"schemaVersion", 1}, {"status", fresh ? status_ : "stale"}, {"demo", demo_},
                {"current", active_ && fresh ? current_ : Json(nullptr)},
                {"history", history_}, {"historyLimit", limit}};
    }

    std::vector<std::uint32_t> pending_metadata() const {
        std::lock_guard lock(mutex_);
        std::set<std::uint32_t> ids;
        const auto add = [&](const Json& item) {
            if (!item.is_null() && !item.value("metadataAvailable", false)) ids.insert(item["trackId"].get<std::uint32_t>());
        };
        add(current_);
        for (const auto& item : history_) add(item);
        return {ids.begin(), ids.end()};
    }

    void enrich(const Json& metadata) {
        if (!metadata.value("metadataAvailable", false)) return;
        std::lock_guard lock(mutex_);
        const auto apply = [&](Json& item) {
            if (!item.is_null() && !item.value("metadataAvailable", false) && item["trackId"] == metadata["trackId"])
                copy_metadata(item, metadata);
        };
        apply(current_);
        for (auto& item : history_) apply(item);
    }

    Cover cover(std::uint32_t id) const {
        { std::lock_guard lock(mutex_); if (!contains(id) || !loader_) return {}; }
        // No history lock during disk/database work. Recheck after eviction races.
        auto result = loader_(id);
        { std::lock_guard lock(mutex_); if (!contains(id)) return {}; }
        return result;
    }

private:
    static void copy_metadata(Json& target, const Json& source) {
        for (const auto* field : {"metadataAvailable", "title", "artist", "album", "key", "genre", "label", "originalBpm"})
            target[field] = source.value(field, Json(nullptr));
    }
    bool contains(std::uint32_t id) const {
        if (!current_.is_null() && current_["trackId"] == id) return true;
        for (const auto& item : history_) if (item["trackId"] == id) return true;
        return false;
    }
    mutable std::mutex mutex_;
    std::function<Cover(std::uint32_t)> loader_;
    Json current_ = nullptr;
    std::deque<Json> history_;
    std::uint64_t sequence_{};
    bool active_{}, demo_{};
    std::string status_{"starting"};
    std::chrono::steady_clock::time_point sampled_ = std::chrono::steady_clock::now();
};
}
