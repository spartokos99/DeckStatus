#include "audio_capture.h"
#include "audio_samples.h"
#include <bit>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>
#include <thread>
#include <chrono>

void require(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
int main(int argc, char** argv) {
    try {
        using namespace deckstatus::audio;
        const auto near = [](float a, float b) { return std::abs(a-b) < .00001f; };
        for (unsigned bits : {8u,16u,24u,32u}) {
            Format f{bits,1,bits/8,false};
            std::array<std::byte,4> minimum{}, maximum{};
            if (bits != 8) minimum[bits/8-1] = std::byte{0x80};
            maximum.fill(std::byte{0xff});
            if (bits != 8) maximum[bits/8-1] = std::byte{0x7f};
            require(near(decode(minimum.data(),f),-1), "PCM signed minimum conversion");
            require(decode(maximum.data(),f) > .99f, "PCM maximum conversion");
        }
        Samples samples;
        const Format stereo{32,2,8,true};
        const float raw[] = {.5f,-.25f,2.f,-2.f,std::numeric_limits<float>::quiet_NaN(),.75f};
        require(samples.append(std::as_bytes(std::span{raw}),3,stereo), "Float stereo format rejected");
        auto frame = samples.snapshot();
        require(near(frame[0][1021],.5f) && near(frame[1][1021],-.25f), "Stereo channel mapping");
        require(frame[0][1022] == 1 && frame[1][1022] == -1, "Float clipping");
        require(frame[0][1023] == 0 && frame[1][1023] == .75f, "NaN must not reach JSON");
        require(!samples.append({},1,stereo), "Truncated packet accepted");
        require(!supported({64,2,16,true}) && !supported({16,2,2,false}), "Invalid format accepted");
        require(samples.append({},1024,stereo,true), "Silent packets may have a null buffer");
        for (auto& channel : samples.snapshot()) for (float sample : channel) require(sample == 0, "Silent packet retains audio");
        std::vector<float> mono(2050,.25f); mono.back() = -.75f;
        require(samples.append(std::as_bytes(std::span{mono}),mono.size(),{32,1,4,true}), "Mono packet rejected");
        frame = samples.snapshot();
        require(frame[0] == frame[1] && frame[0].front() == .25f && frame[0].back() == -.75f, "Window order / mono duplication");
        samples.clear(); require(samples.snapshot()[0].back() == 0, "Clear retained audio");
        deckstatus::AudioCapture capture;
        auto state = capture.state();
        require(state["status"] == "stopped" && !state["fresh"] && state["sampleAgeMs"].is_null(), "Capture must default off");
        require(state["left"].size() == sample_count, "Sample window size");
        const auto devices = capture.devices();
        require(devices["devices"].is_array(), "Device enumeration schema");
        for (auto& device : devices["devices"]) require(device["id"].is_string() && device["name"].is_string(), "Device schema");
        require(!capture.select("missing-test-endpoint"), "Invalid endpoint accepted");
        require(capture.select("") && capture.state()["status"] == "stopped", "Idempotent capture stop");
        std::cout << "Audio conversion, ring buffer, off-by-default lifecycle, and device enumeration passed. No audio source opened.\n";
        // Opt-in hardware smoke test. Never selects a recording/microphone endpoint.
        if (argc == 2 && std::string(argv[1]) == "--loopback-smoke") {
            std::string loopback;
            for (const auto& device : devices["devices"]) if (device["kind"] == "loopback") { loopback = device["id"]; break; }
            require(!loopback.empty(), "No output endpoint available for optional loopback test");
            for (int cycle = 0; cycle < 2; ++cycle) {
                require(capture.select(loopback), "Loopback selection failed");
                for (int attempt=0; attempt<100 && capture.state()["status"] == "starting"; ++attempt)
                    std::this_thread::sleep_for(std::chrono::milliseconds(20));
                require(capture.state()["status"] == "capturing", "WASAPI loopback stream did not start");
                std::this_thread::sleep_for(std::chrono::milliseconds(300));
                require(capture.state()["sampleRate"].get<unsigned>() > 0, "WASAPI mix format missing");
                require(capture.select("") && capture.state()["status"] == "stopped", "WASAPI stream did not stop");
            }
            std::cout << "Optional live WASAPI loopback open / format / stop / restart passed. No audio files saved.\n";
        }
        return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
