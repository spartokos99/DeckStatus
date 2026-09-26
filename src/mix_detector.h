#pragma once
#include <nlohmann/json.hpp>
#include <array>
#include <algorithm>
#include <chrono>
#include <cmath>

namespace deckstatus {
// Playback-time detection inspired by prolink-connect's SmartTiming. Position and
// beat-number jumps are deliberately not counted as time spent playing a track.
class MixDetector {
public:
    using Clock = std::chrono::steady_clock;
    int apply(const nlohmann::json& decks, int detection_beats, int interrupt_beats,
              bool use_on_air, Clock::time_point now) {
        if(initialized_&&now<last_)return master_;
        const double seconds = initialized_ ? std::clamp(std::chrono::duration<double>(now - last_).count(), 0.0, 1.0) : 0;
        initialized_ = true; last_ = now;
        std::array<bool, 4> seen{};
        for (const auto& deck : decks) {
            const int id = deck.value("id", 0);
            if (id < 1 || id > 4) continue;
            auto& p = players_[id - 1]; seen[id - 1] = true;
            const auto track = deck.value("loaded", false) && deck.contains("trackId") && deck["trackId"].is_number_integer()
                ? deck["trackId"].get<std::uint64_t>() : 0;
            if (track != p.track || !track) { p = {}; p.track = track; if (master_ == id) master_ = 0; }
            if (!track) continue;
            const bool eligible = deck.value("playing", nlohmann::json()) == true &&
                (!use_on_air || deck.value("onAir", nlohmann::json()) == true);
            double bpm = 0;
            if (deck.contains("bpm") && deck["bpm"].is_number()) bpm = deck["bpm"].get<double>();
            const bool valid_bpm = std::isfinite(bpm) && bpm >= 20 && bpm <= 400;
            // Credit only intervals whose endpoints are eligible. A short interruption
            // preserves progress, but cannot earn detection beats while paused/off air.
            if (eligible && p.eligible && p.bpm > 0) p.beats += seconds * p.bpm / 60;
            if (!eligible) {
                p.interrupt += seconds * (p.bpm > 0 ? p.bpm : 120) / 60;
                if (p.interrupt >= interrupt_beats || deck.value("stopped", nlohmann::json()) == true) {
                    p.beats = 0; p.order = 0; p.promoted = false;
                    if (master_ == id) master_ = 0;
                }
            } else {
                p.interrupt = 0;
                if (!p.order) p.order = ++order_;
            }
            p.eligible = eligible;
            // Unknown current tempo cannot accrue beats. Keep the last known tempo
            // separately for interruption tolerance; it is never exposed as metadata.
            if (valid_bpm) p.bpm = bpm;
            else if (eligible) p.bpm = 0;
        }
        for (int i = 0; i < 4; ++i) if (!seen[i]) { players_[i] = {}; if (master_ == i + 1) master_ = 0; }
        int candidate = 0;
        for (int i = 0; i < 4; ++i) {
            const auto& p = players_[i];
            if (!p.eligible || (master_ && (p.promoted || p.beats < detection_beats))) continue;
            if (!candidate || p.order < players_[candidate - 1].order) candidate = i + 1;
        }
        if (candidate) { master_ = candidate; players_[candidate - 1].promoted = true; }
        return master_;
    }
private:
    struct Player { std::uint64_t track{}, order{}; double beats{}, interrupt{}, bpm{}; bool eligible{}, promoted{}; };
    std::array<Player, 4> players_{};
    Clock::time_point last_{};
    bool initialized_{};
    int master_{};
    std::uint64_t order_{};
};
}
