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
        std::cout << "Master transitions, gaps, replay, late metadata, frozen history, covers, eviction and staleness passed.\n";
        return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
