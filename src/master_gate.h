#pragma once

#include <nlohmann/json.hpp>
#include <chrono>
#include <cstdint>
#include <mutex>
#include <string>

namespace deckstatus {

// Rekordbox and PRO DJ LINK hand the tempo master to another deck as soon as that deck
// is started or nudged, so cueing the next track can flip the overlay for a moment.
// The gate promotes a new master deck only once it has held the role for the configured
// time. Until then the previously confirmed deck stays master for every consumer: the
// state API, the deck and master overlays and the session history.
//
// The hold only ever defends a master that is already on air. When there is none - at
// startup, after a reconnect or after an idle gap - the reported master is published at
// once, so the overlay is never blank and the running track is never missing from the
// history for the length of the hold. A track change on the already confirmed deck is an
// explicit action and passes without delay, as does a replacement for a deck that lost
// its track. This is an observation filter, not proof of audible playback.
class MasterGate {
public:
    using Json = nlohmann::json;
    using Clock = std::chrono::steady_clock;
    static constexpr int default_hold_ms = 4000;
    static constexpr int max_hold_ms = 30000;

    static bool valid_hold(std::int64_t value) { return value >= 0 && value <= max_hold_ms; }

    explicit MasterGate(std::int64_t hold_ms = default_hold_ms)
        : hold_(valid_hold(hold_ms) ? static_cast<int>(hold_ms) : default_hold_ms) {}

    int hold_ms() const { std::lock_guard lock(mutex_); return hold_; }

    // A shorter hold applies to the pending candidate as well; a longer one only
    // delays the next change. An invalid value falls back to the default.
    void set_hold_ms(std::int64_t value) {
        std::lock_guard lock(mutex_);
        hold_ = valid_hold(value) ? static_cast<int>(value) : default_hold_ms;
    }

    Json describe() const {
        std::lock_guard lock(mutex_);
        return {{"holdMs", hold_}, {"defaultHoldMs", default_hold_ms}, {"maxHoldMs", max_hold_ms}};
    }

    // Replaces masterDeckId and every deck's isMaster with the confirmed master.
    // Safe to call from the sampler and from request threads with the same state.
    void apply(Json& state, Clock::time_point now = Clock::now()) {
        if (!state.is_object() || !state.contains("decks") || !state["decks"].is_array()) return;
        const auto status = state.value("status", std::string{});
        const auto reported = state.contains("masterDeckId") && state["masterDeckId"].is_number_integer()
            ? state["masterDeckId"].get<int>() : 0;
        std::lock_guard lock(mutex_);
        if (status != "connected" && status != "demo") {
            // Nothing is observed while disconnected or stale; never carry a master across.
            confirmed_ = 0;
            candidate_ = 0;
        } else if (reported == confirmed_) {
            candidate_ = confirmed_;
        } else if (!confirmed_ || !loaded(state, confirmed_)) {
            // Nothing on air to protect: publish immediately rather than show nothing.
            confirmed_ = candidate_ = reported;
            since_ = now;
        } else {
            if (reported != candidate_) { candidate_ = reported; since_ = now; }
            if (now - since_ >= std::chrono::milliseconds(hold_)) confirmed_ = candidate_;
        }
        const auto master = confirmed_;
        state["masterDeckId"] = master ? Json(master) : Json(nullptr);
        for (auto& deck : state["decks"]) {
            if (!deck.is_object() || !deck.contains("id")) continue;
            deck["isMaster"] = master ? Json(deck["id"] == master) : Json(nullptr);
        }
    }

private:
    static bool loaded(const Json& state, int id) {
        for (const auto& deck : state["decks"]) {
            if (!deck.is_object() || deck.value("id", 0) != id) continue;
            return deck.value("loaded", false) && deck.contains("trackId") &&
                   deck["trackId"].is_number_integer() && deck["trackId"].get<std::uint64_t>() != 0;
        }
        return false;
    }
    mutable std::mutex mutex_;
    int hold_;
    int confirmed_{}, candidate_{};
    Clock::time_point since_{};
};

} // namespace deckstatus
