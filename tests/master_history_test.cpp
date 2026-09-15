#include "master_history.h"
#include <iostream>
#include <stdexcept>
#include <thread>

using Json = nlohmann::json;
void require(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }

Json sample(unsigned track, unsigned deck = 1, bool metadata = true) {
    return {{"status", "connected"}, {"demo", false}, {"updatedAt", 1234}, {"masterDeckId", deck},
        {"decks", Json::array({{{"id", deck}, {"trackId", track}, {"loaded", track != 0},
            {"metadataAvailable", metadata}, {"title", metadata ? Json("Track " + std::to_string(track)) : Json(nullptr)},
            {"artist", "Artist"}, {"album", "Album"}, {"key", "8A"}, {"bpm", 128.5}, {"originalBpm", 126}}})}};
}

int main() {
    try {
        int calls = 0;
        deckstatus::MasterHistory feed([&](std::uint32_t id) { ++calls; return deckstatus::MasterHistory::Cover{"image/png", std::to_string(id)}; });
        require(feed.snapshot()["current"].is_null(), "Empty session must have no master");
        feed.update(sample(11));
        const auto first = feed.snapshot()["current"]["entryId"];
        auto changed = sample(11, 3);
        changed["decks"][0]["bpm"] = 132.25;
        feed.update(changed);
        require(feed.snapshot()["current"]["entryId"] == first && feed.snapshot()["history"].empty(), "Same track on another deck duplicated history");
        require(feed.snapshot()["current"]["bpm"] == 132.25, "Live BPM did not update");
        auto unknown = changed;
        unknown["masterDeckId"] = nullptr;
        feed.update(unknown);
        require(feed.snapshot()["current"].is_null(), "Unknown master must hide current track");
        feed.update(changed);
        require(feed.snapshot()["current"]["entryId"] == first && feed.snapshot()["history"].empty(), "Unknown gap generated fake history");
        feed.update(sample(12, 2, false));
        auto state = feed.snapshot();
        require(state["history"].size() == 1 && state["history"][0]["trackId"] == 11, "Previous master was not archived");
        require(state["current"]["isMaster"] == true && state["history"][0]["isMaster"] == false, "Historical entry still marked as current master");
        require(state["history"][0]["bpm"] == 132.25, "History BPM must freeze at departure");
        require(state["history"][0]["coverUrl"] == "/api/master/covers/11", "History needs a stable cover URL");
        require(feed.cover(11).second == "11", "Departed track lost its cover");
        require(feed.cover(999).second.empty() && calls == 1, "Unseen library ID triggered a lookup");
        feed.update(sample(13));
        require(feed.pending_metadata() == std::vector<std::uint32_t>{12}, "Missing departed metadata not queued");
        feed.enrich(sample(12)["decks"][0]);
        require(feed.snapshot()["history"][0]["title"] == "Track 12", "Late metadata did not enrich history");
        require(feed.snapshot()["history"][0]["bpm"] == 128.5 && feed.pending_metadata().empty(), "Metadata modified captured BPM");
        feed.update(sample(11));
        require(feed.snapshot()["current"]["entryId"] != first, "Replayed track reused old occurrence");
        require(feed.snapshot()["history"].size() == 3, "Replay should append a transition");
        feed.update(sample(11, 1, false));
        require(feed.snapshot()["current"]["title"] == "Track 11", "Temporary metadata failure erased good metadata");
        auto disconnected = sample(11);
        disconnected["status"] = "disconnected";
        feed.update(disconnected);
        require(feed.snapshot()["current"].is_null() && feed.snapshot()["history"].size() == 3, "Disconnect must hide current and retain history");
        feed.update(sample(0));
        require(feed.snapshot()["current"].is_null(), "Empty master deck must stay hidden");
        for (unsigned id = 100; id < 170; ++id) feed.update(sample(id));
        state = feed.snapshot();
        require(state["history"].size() == 50 && state["history"][0]["trackId"] == 168 && state["history"][49]["trackId"] == 119,
                "History must retain the last 50 in reverse chronological order");
        require(feed.cover(11).second.empty() && calls == 1, "Evicted cover remained accessible");
        require(feed.snapshot() == state, "Reading history changed the session");
        const auto full = feed.full_snapshot();
        require(full["total"] == 74 && full["entries"].size() == 74, "Full history lost tracks past the overlay limit");
        require(full["entries"].back()["trackId"] == 11 && full["entries"].back()["bpm"] == 132.25,
                "Full history lost the first track or captured BPM");
        require(feed.history_cover(11).second == "11" && feed.history_cover(999).second.empty(), "Full history cover allowlist failed");
        require(feed.full_snapshot(1)["entries"].empty(), "Cursor before first entry should be empty");
        auto page = feed.full_snapshot(0, 10);
        require(page["entries"].size() == 10 && page["entries"][0]["trackId"] == 169, "Full history page order or bounds");
        const auto cursor = page["nextBefore"].get<std::uint64_t>();
        feed.update(sample(170));
        auto older = feed.full_snapshot(cursor, 10);
        require(older["entries"][0]["entryId"].get<std::uint64_t>() == cursor - 1, "New tracks shifted the older-page cursor");
        std::set<std::uint64_t> seen;
        std::uint64_t before = 0;
        do {
            page = feed.full_snapshot(before, 13);
            for (const auto& entry : page["entries"]) require(seen.insert(entry["entryId"].get<std::uint64_t>()).second, "Duplicate full-history entry across pages");
            before = page["nextBefore"].is_null() ? 0 : page["nextBefore"].get<std::uint64_t>();
        } while (before);
        require(seen.size() == 75 && seen.contains(1) && seen.contains(75), "Paged history omitted an occurrence");
        deckstatus::MasterHistory late;
        late.update(sample(777, 1, false));
        for (unsigned id = 800; id < 860; ++id) late.update(sample(id));
        require(late.pending_metadata() == std::vector<std::uint32_t>{777}, "Old full-history metadata not queued");
        late.enrich(sample(777)["decks"][0]);
        require(late.full_snapshot(2)["entries"][0]["title"] == "Track 777", "Old full-history metadata not enriched");
        // Deterministically evict during an artwork lookup; the result must be rejected.
        deckstatus::MasterHistory* racing_feed = nullptr;
        deckstatus::MasterHistory racing([&](std::uint32_t) {
            for (unsigned id = 2; id <= 53; ++id) racing_feed->update(sample(id));
            return deckstatus::MasterHistory::Cover{"image/png", "old bytes"};
        });
        racing_feed = &racing;
        racing.update(sample(1));
        require(racing.cover(1).second.empty(), "Eviction during cover lookup leaked removed artwork");
        std::this_thread::sleep_for(std::chrono::milliseconds(3050));
        require(feed.snapshot()["current"].is_null() && feed.snapshot()["status"] == "stale", "Stopped sampler kept a stale current track");
        require(feed.snapshot()["history"].size() == 50, "Staleness erased session history");
        require(feed.full_snapshot()["total"] == 75 && feed.full_snapshot()["currentEntryId"].is_null(), "Staleness erased full history or retained a live badge");
        require(feed.full_snapshot()["entries"][0]["isMaster"] == false, "Stale history marked its last track live");
        std::cout << "Master transitions, gaps, replay, late metadata, frozen history, covers, eviction and staleness passed.\n";
        return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
