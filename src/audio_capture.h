#pragma once
#include <memory>
#include <string>
#include <nlohmann/json.hpp>

namespace deckstatus {
// One explicitly selected WASAPI source per server. No recording or audio output.
class AudioCapture {
public:
    AudioCapture();
    ~AudioCapture();
    AudioCapture(const AudioCapture&) = delete;
    AudioCapture& operator=(const AudioCapture&) = delete;
    nlohmann::json devices() const;
    nlohmann::json state() const;
    bool select(const std::string& id); // Empty ID stops; invalid IDs leave current capture unchanged.
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
