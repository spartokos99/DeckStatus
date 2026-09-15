#pragma once

#include <nlohmann/json.hpp>
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <deque>
#include <functional>
#include <mutex>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace deckstatus {

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
                archive_.push_back(current_);
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
            full_track_ids_.insert(current_["trackId"].get<std::uint32_t>());
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

    // Complete session, newest first. A cursor keeps older pages stable as new
    // masters arrive. The overlay's 50-entry window remains a separate contract.
    Json full_snapshot(std::uint64_t before = 0, std::size_t page_size = 100) const {
        std::lock_guard lock(mutex_);
        const bool fresh = std::chrono::steady_clock::now() - sampled_ <= std::chrono::seconds(3);
        const std::size_t total = archive_.size() + (current_.is_null() ? 0 : 1);
        const auto end = before == 0 ? total : static_cast<std::size_t>(std::min<std::uint64_t>(total, before - 1));
        page_size = std::clamp<std::size_t>(page_size, 1, 100);
        const auto begin = end > page_size ? end - page_size : 0;
        Json entries = Json::array();
        for (auto index = end; index > begin; --index) {
            Json entry = index - 1 < archive_.size() ? archive_[index - 1] : current_;
            entry["isMaster"] = index == total && !current_.is_null() && active_ && fresh;
            entry["coverUrl"] = "/api/history/covers/" + std::to_string(entry["trackId"].get<std::uint32_t>());
            entries.push_back(std::move(entry));
        }
        return {{"schemaVersion", 1}, {"status", fresh ? status_ : "stale"}, {"demo", demo_},
            {"entries", entries}, {"total", total}, {"limit", page_size},
            {"nextBefore", begin > 0 ? Json(begin + 1) : Json(nullptr)},
            {"currentEntryId", active_ && fresh && !current_.is_null() ? current_["entryId"] : Json(nullptr)}};
    }

    std::vector<std::uint32_t> pending_metadata() const {
        std::lock_guard lock(mutex_);
        std::set<std::uint32_t> ids;
        const auto add = [&](const Json& item) {
            if (!item.is_null() && !item.value("metadataAvailable", false)) ids.insert(item["trackId"].get<std::uint32_t>());
        };
        add(current_);
        for (const auto& item : archive_) add(item);
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
        for (auto& item : archive_) apply(item);
    }

    Cover cover(std::uint32_t id) const {
        { std::lock_guard lock(mutex_); if (!contains(id) || !loader_) return {}; }
        // No history lock during disk/database work. Recheck after eviction races.
        auto result = loader_(id);
        { std::lock_guard lock(mutex_); if (!contains(id)) return {}; }
        return result;
    }

    Cover history_cover(std::uint32_t id) const {
        { std::lock_guard lock(mutex_); if (!full_track_ids_.contains(id) || !loader_) return {}; }
        // The full-session archive only grows; unknown library IDs remain blocked.
        return loader_(id);
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
    std::vector<Json> archive_;
    std::set<std::uint32_t> full_track_ids_;
    std::uint64_t sequence_{};
    bool active_{}, demo_{};
    std::string status_{"starting"};
    std::chrono::steady_clock::time_point sampled_ = std::chrono::steady_clock::now();
};
}
