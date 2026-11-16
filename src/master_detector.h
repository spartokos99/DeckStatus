#pragma once

#include <nlohmann/json.hpp>
#include <array>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <mutex>
#include <stdexcept>
#include <string>

namespace deckstatus {

// One now-playing processor for both sources. The timing/interrupt/end rules
// follow ProlinkTools 0.1.0-beta.5; tempo-master flags are not detection inputs.
class MasterDetector {
public:
    using Json = nlohmann::json;
    using Clock = std::chrono::steady_clock;
    static constexpr int recovery_ms = 10000;
    static Json defaults() {
        return {{"detectionBeats",128},{"interruptBeats",16},{"onlyAfterEnd",false},{"useOnAir",true}};
    }
    static bool valid_settings(const Json& value, bool legacy = false) {
        if (!value.is_object() || value.empty()) return false;
        for (const auto& [key, item] : value.items()) {
            if (key == "detectionBeats" || key == "interruptBeats") {
                if (!item.is_number_integer() || item < (key == "detectionBeats" ? 1 : 0) || item > 1024) return false;
            } else if (key == "onlyAfterEnd" || key == "useOnAir") {
                if (!item.is_boolean()) return false;
            } else if (legacy && key == "holdMs") {
                if (!item.is_number_integer() || item < 0 || item > 30000) return false;
            } else if (legacy && key == "prolinkMethod") {
                if (item != "tempo" && item != "smart") return false;
            } else return false;
        }
        return true;
    }
    static Json migrate(const Json& value) {
        if (!valid_settings(value, true)) throw std::invalid_argument("Invalid master detection settings");
        auto result = defaults();
        for (auto& [key, item] : result.items()) if (value.contains(key)) item = value[key];
        return result;
    }
    explicit MasterDetector(const Json& settings = defaults()) { configure(settings); }
    void configure(const Json& settings) {
        if (!valid_settings(settings)) throw std::invalid_argument("Invalid master detection settings");
        std::lock_guard lock(mutex_);
        settings_.update(settings);
        // Changes affect timing immediately, but never re-arm already reported tracks.
    }
    Json describe() const { std::lock_guard lock(mutex_); return settings_; }

    // Only the sampler advances the processor. HTTP readers use snapshot().
    void apply(Json& state, Clock::time_point now = Clock::now()) {
        std::lock_guard lock(mutex_);
        if (initialized_ && now < sampled_) throw std::invalid_argument("Detection clock moved backwards");
        const auto interval = initialized_ ? now - sampled_ : Clock::duration::zero();
        const bool continuous = initialized_ && interval <= std::chrono::seconds(1);
        initialized_ = true;
        sampled_ = now;
        const auto mode = state.value("mode", std::string("rekordbox"));
        if (!mode_.empty() && mode_ != mode) { players_ = {}; current_ = 0; }
        mode_ = mode;
        const auto status = state.value("status", std::string{});
        bool connected = status == "connected" || status == "demo";
        const bool use_on_air = mode == "prolink" && settings_["useOnAir"].get<bool>();
        bool current_ended = false;
        std::array<bool,4> seen{};
        if (!state.contains("decks") || !state["decks"].is_array()) {
            connected = false;
            state["status"] = "error";
            state["message"] = "Invalid detection source snapshot";
            state["decks"] = Json::array();
        }
        for (auto& deck : state["decks"]) {
            const int id = deck.value("id", 0);
            if (id < 1 || id > 4) continue;
            seen[id - 1] = true;
            auto& p = players_[id - 1];
            const bool was_available = p.available;
            p.available = connected && deck.value("statusAvailable", true);
            if (!p.available) { p.motion_sample = false; continue; }
            const auto track = deck.value("loaded", false) && deck.contains("trackId") &&
                deck["trackId"].is_number_integer() && deck["trackId"] > 0 && deck["trackId"] <= UINT32_MAX
                ? deck["trackId"].get<std::uint32_t>() : 0;
            if (!track || track != p.track) {
                if (current_ == id) { current_ended = true; current_ = 0; }
                p = {};
                p.track = track;
                p.available = true;
            }
            if (!track) { p.seen = now; continue; }
            const double bpm = number(deck, "bpm");
            const bool tempo = bpm >= 20 && bpm <= 400;
            if (tempo) p.bpm = bpm;
            metadata(deck, p);
            if (mode == "rekordbox" && !state.value("demo", false)) playback(deck, p, state, now);
            const auto playing = deck.value("playing", Json(nullptr));
            const auto on_air = deck.value("onAir", Json(nullptr));
            if (deck.value("stopped", Json(nullptr)) == true) {
                p.seen = now;
                if (current_ == id) { current_ended = true; current_ = 0; }
                end(p);
                continue;
            }
            const bool known = playing.is_boolean() && (!use_on_air || on_air.is_boolean());
            if (!known) { p.available = false; continue; }
            p.seen = now;
            const bool eligible = playing == true && (!use_on_air || on_air == true);
            p.eligible = eligible;
            // Missing samples freeze both timers. A seek never contributes its position delta.
            const auto elapsed = continuous && was_available && p.started && p.tempo && tempo ? interval : Clock::duration::zero();
            p.tempo = tempo;
            if (p.started) p.elapsed += elapsed;
            // Playback starts the smart timer; mixer gating controls reporting. A
            // short pause/cut preserves the start, as in ProlinkTools' elapsed timer.
            if (playing == true && !p.started) {
                p.started = true;
                p.order = ++order_;
            }
            const bool interrupted = playing == false || (p.reported && !eligible);
            if (interrupted && p.started) {
                p.interrupted += p.interrupting ? elapsed : Clock::duration::zero();
                p.interrupting = true;
                if (settings_["interruptBeats"] == 0 ||
                    (p.bpm > 0 && std::chrono::duration<double>(p.interrupted).count() * p.bpm / 60 >= settings_["interruptBeats"].get<int>())) {
                    if (current_ == id) { current_ended = true; current_ = 0; }
                    end(p);
                }
            } else { p.interrupted = {}; p.interrupting = false; }
        }
        for (int i = 0; i < 4; ++i) if (!seen[i]) {
            players_[i].available = false;
            players_[i].motion_sample = false;
        }
        // An unavailable outgoing player is not a stopped one. Protect brief gaps,
        // then allow a new, unreported player without re-arming the missing track.
        const bool current_missing = current_ && !players_[current_ - 1].available;
        const bool recovering = current_missing && now - players_[current_ - 1].seen <= std::chrono::milliseconds(recovery_ms);
        const bool next_after_end = current_ended || !current_ || (current_missing && !recovering);
        int candidate = 0;
        if (connected && !recovering) for (int i = 0; i < 4; ++i) {
            const auto& p = players_[i];
            if (!p.track || !p.available || !p.eligible || !p.started || p.reported) continue;
            const bool timed = !settings_["onlyAfterEnd"].get<bool>() &&
                p.tempo && p.bpm > 0 && std::chrono::duration<double>(p.elapsed).count() * p.bpm / 60 >= settings_["detectionBeats"].get<int>();
            if (!next_after_end && !timed) continue;
            if (!candidate || p.order < players_[candidate - 1].order) candidate = i + 1;
        }
        if (candidate) { current_ = candidate; players_[candidate - 1].reported = true; }
        const bool available = !current_ || players_[current_ - 1].available;
        const int master = connected && available ? current_ : 0;
        state["masterDeckId"] = master ? Json(master) : Json(nullptr);
        state["masterAvailable"] = connected && available;
        for (auto& deck : state["decks"]) {
            deck["isMaster"] = master ? Json(deck.value("id",0) == master) : Json(nullptr);
            deck["sourceSession"] = source_session_;
        }
        published_ = state;
    }
    Json snapshot() const {
        std::lock_guard lock(mutex_);
        auto state = published_;
        if (initialized_ && Clock::now() - sampled_ > std::chrono::seconds(3)) {
            state["status"] = "stale";
            state["masterAvailable"] = false;
            state["masterDeckId"] = nullptr;
            for (auto& deck : state["decks"]) { deck["statusAvailable"] = false; deck["isMaster"] = nullptr; }
        }
        return state;
    }

private:
    struct Player {
        std::uint32_t track{};
        std::uint64_t order{};
        Clock::duration elapsed{}, interrupted{};
        double bpm{};
        bool started{}, reported{}, eligible{}, available{}, tempo{}, interrupting{};
        Clock::time_point seen{};
        Json metadata = Json::object();
        bool motion_sample{}, moving{};
        int advances{};
        double position{}, sample_stamp{};
        Clock::time_point position_time{}, motion_time{};
    };
    static double number(const Json& value, const char* key) {
        if (!value.contains(key) || !value[key].is_number()) return NAN;
        const double result = value[key].get<double>();
        return std::isfinite(result) ? result : NAN;
    }
    static void end(Player& p) {
        p.started = p.reported = p.eligible = p.interrupting = false;
        p.elapsed = p.interrupted = {};
        p.order = 0;
    }
    static void metadata(Json& deck, Player& p) {
        bool cached = false;
        for (const auto* field : {"title","artist","album","key","genre","label","originalBpm","durationMs"}) {
            const auto value = deck.value(field, Json(nullptr));
            if (!value.is_null() && !(value.is_string() && value.get_ref<const std::string&>().empty()))
                p.metadata[field] = value;
            else if (p.metadata.contains(field)) { deck[field] = p.metadata[field]; cached = true; }
        }
        if (deck.value("metadataAvailable", false)) p.metadata["metadataAvailable"] = true;
        else if (p.metadata.value("metadataAvailable", false)) deck["metadataAvailable"] = true;
        deck["metadataCached"] = cached;
    }
    static void playback(Json& deck, Player& p, const Json& state, Clock::time_point now) {
        deck["playing"] = nullptr;
        deck["stopped"] = nullptr;
        deck["onAir"] = nullptr;
        deck["playbackSource"] = "position";
        const double position = number(deck, "positionMs");
        const double stamp = number(state, state.contains("sampleTick") ? "sampleTick" : "updatedAt");
        if (!std::isfinite(position)) { p.motion_sample = false; p.advances = 0; return; }
        if (!p.motion_sample) {
            p.position = position; p.position_time = p.motion_time = now; p.sample_stamp = stamp;
            p.motion_sample = true; p.moving = false; p.advances = 0;
            return;
        }
        // A repeated bridge sample is not evidence of a stationary player.
        if (!std::isfinite(stamp) || stamp == p.sample_stamp) return;
        {
            const double dt = std::chrono::duration<double>(now - p.position_time).count();
            const double delta = position - p.position;
            if (dt > 0 && dt <= 1 && delta > 0 && delta <= dt * 4000 + 100) {
                if (++p.advances >= 2) p.moving = true;
                p.motion_time = now;
            } else {
                p.advances = 0;
                // A looping/seek jump is not an end or a fresh playback episode.
                if (delta != 0 && p.moving) p.motion_time = now;
            }
            p.position = position; p.position_time = now; p.sample_stamp = stamp;
        }
        if (now - p.motion_time >= std::chrono::milliseconds(500)) p.moving = false;
        if (p.moving || now - p.motion_time >= std::chrono::milliseconds(500)) {
            deck["playing"] = p.moving;
            const double duration = number(deck, "durationMs");
            deck["stopped"] = !p.moving && (position <= 0 || (std::isfinite(duration) && position >= duration));
        }
    }
    mutable std::mutex mutex_;
    Json settings_ = defaults();
    Json published_ = {{"status","starting"},{"masterDeckId",nullptr},{"masterAvailable",false},{"decks",Json::array()}};
    std::array<Player,4> players_{};
    std::string mode_;
    const std::string source_session_ = std::to_string(Clock::now().time_since_epoch().count());
    Clock::time_point sampled_{};
    bool initialized_{};
    int current_{};
    std::uint64_t order_{};
};
}
