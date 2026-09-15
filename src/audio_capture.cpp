#include "audio_capture.h"
#include "audio_samples.h"
#include <Windows.h>
#include <audioclient.h>
#include <mmdeviceapi.h>
#include <functiondiscoverykeys_devpkey.h>
#include <ksmedia.h>
#include <wrl/client.h>
#include <chrono>
#include <mutex>
#include <thread>
#include <vector>

namespace deckstatus {
namespace {
using Microsoft::WRL::ComPtr;
using Json = nlohmann::json;
using Clock = std::chrono::steady_clock;
struct AudioError { HRESULT code; };
void check(HRESULT hr) { if (FAILED(hr)) throw AudioError{hr}; }
struct ComSession {
    HRESULT result = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    ComSession() { if (result != RPC_E_CHANGED_MODE) check(result); }
    ~ComSession() { if (SUCCEEDED(result)) CoUninitialize(); }
};
struct TaskMemory { void* p{}; ~TaskMemory() { CoTaskMemFree(p); } };
struct Property {
    PROPVARIANT value{};
    ~Property() { PropVariantClear(&value); }
};
std::string utf8(const wchar_t* input) {
    if (!input || !*input) return {};
    const int length = WideCharToMultiByte(CP_UTF8, 0, input, -1, nullptr, 0, nullptr, nullptr);
    if (length <= 1) return {};
    std::string result(length, '\0');
    WideCharToMultiByte(CP_UTF8, 0, input, -1, result.data(), length, nullptr, nullptr);
    result.pop_back(); return result;
}
ComPtr<IMMDeviceEnumerator> enumerator() {
    ComPtr<IMMDeviceEnumerator> result;
    check(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, IID_PPV_ARGS(&result)));
    return result;
}
struct Device { std::wstring native_id; std::string id, name; bool loopback{}; };
std::vector<Device> enumerate() {
    ComSession session;
    const auto provider = enumerator();
    std::vector<Device> result;
    for (auto flow : {eCapture, eRender}) {
        ComPtr<IMMDeviceCollection> devices;
        check(provider->EnumAudioEndpoints(flow, DEVICE_STATE_ACTIVE, &devices));
        UINT count{}; check(devices->GetCount(&count));
        for (UINT i = 0; i < count; ++i) {
            ComPtr<IMMDevice> device;
            if (FAILED(devices->Item(i, &device))) continue;
            TaskMemory id;
            if (FAILED(device->GetId(reinterpret_cast<LPWSTR*>(&id.p)))) continue;
            ComPtr<IPropertyStore> properties;
            Property name;
            if (FAILED(device->OpenPropertyStore(STGM_READ, &properties)) ||
                FAILED(properties->GetValue(PKEY_Device_FriendlyName, &name.value)) || name.value.vt != VT_LPWSTR) continue;
            const auto* native = static_cast<LPCWSTR>(id.p);
            result.push_back({native, utf8(native), utf8(name.value.pwszVal), flow == eRender});
        }
    }
    return result;
}
const char* error_key(HRESULT hr) {
    if (hr == E_ACCESSDENIED) return "audioPermission";
    if (hr == AUDCLNT_E_DEVICE_INVALIDATED || hr == HRESULT_FROM_WIN32(ERROR_NOT_FOUND)) return "audioDeviceLost";
    if (hr == AUDCLNT_E_UNSUPPORTED_FORMAT) return "audioFormat";
    return "audioError";
}
}
struct AudioCapture::Impl {
    std::mutex command;
    mutable std::mutex mutex;
    audio::Samples samples;
    std::string status = "stopped", error, id, name;
    std::uint32_t sample_rate{};
    std::uint64_t sequence{};
    Clock::time_point updated{};
    std::jthread worker;

    void capture(std::stop_token stop, Device device) noexcept {
        try {
            ComSession session;
            const auto provider = enumerator();
            ComPtr<IMMDevice> endpoint;
            check(provider->GetDevice(device.native_id.c_str(), &endpoint));
            ComPtr<IAudioClient> client;
            check(endpoint->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, &client));
            TaskMemory format_memory;
            check(client->GetMixFormat(reinterpret_cast<WAVEFORMATEX**>(&format_memory.p)));
            const auto* wave = static_cast<WAVEFORMATEX*>(format_memory.p);
            bool floating = wave->wFormatTag == WAVE_FORMAT_IEEE_FLOAT;
            bool pcm = wave->wFormatTag == WAVE_FORMAT_PCM;
            if (wave->wFormatTag == WAVE_FORMAT_EXTENSIBLE && wave->cbSize >= sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX)) {
                const auto* ext = reinterpret_cast<const WAVEFORMATEXTENSIBLE*>(wave);
                floating = ext->SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT;
                pcm = ext->SubFormat == KSDATAFORMAT_SUBTYPE_PCM;
            }
            const audio::Format format{wave->wBitsPerSample, wave->nChannels, wave->nBlockAlign, floating};
            if ((!pcm && !floating) || !audio::supported(format) || wave->nSamplesPerSec == 0)
                throw AudioError{AUDCLNT_E_UNSUPPORTED_FORMAT};
            check(client->Initialize(AUDCLNT_SHAREMODE_SHARED, device.loopback ? AUDCLNT_STREAMFLAGS_LOOPBACK : 0,
                                     1000000, 0, wave, nullptr));
            ComPtr<IAudioCaptureClient> capture_client;
            check(client->GetService(IID_PPV_ARGS(&capture_client)));
            check(client->Start());
            struct StopClient { IAudioClient* p; ~StopClient() { p->Stop(); } } stop_client{client.Get()};
            {
                std::lock_guard lock(mutex);
                status = "capturing"; sample_rate = wave->nSamplesPerSec;
            }
            while (!stop.stop_requested()) {
                UINT32 packet{}; check(capture_client->GetNextPacketSize(&packet));
                while (packet && !stop.stop_requested()) {
                    BYTE* buffer{}; UINT32 frames{}; DWORD flags{};
                    check(capture_client->GetBuffer(&buffer, &frames, &flags, nullptr, nullptr));
                    const bool silent = (flags & AUDCLNT_BUFFERFLAGS_SILENT) != 0;
                    {
                        std::lock_guard lock(mutex);
                        if ((flags & AUDCLNT_BUFFERFLAGS_DATA_DISCONTINUITY) ||
                            Clock::now() - updated > std::chrono::milliseconds(250)) samples.clear();
                        const auto bytes = silent ? std::span<const std::byte>{} :
                            std::span<const std::byte>{reinterpret_cast<const std::byte*>(buffer), static_cast<std::size_t>(frames) * format.block_align};
                        samples.append(bytes, frames, format, silent);
                        updated = Clock::now(); ++sequence;
                    }
                    check(capture_client->ReleaseBuffer(frames));
                    check(capture_client->GetNextPacketSize(&packet));
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(8));
            }
        } catch (const AudioError& failure) {
            std::lock_guard lock(mutex);
            samples.clear(); updated = {}; status = "error"; error = error_key(failure.code);
        } catch (...) {
            std::lock_guard lock(mutex);
            samples.clear(); updated = {}; status = "error"; error = "audioError";
        }
    }
};
AudioCapture::AudioCapture() : impl_(std::make_unique<Impl>()) {}
AudioCapture::~AudioCapture() { select(""); }
Json AudioCapture::devices() const {
    Json result = {{"devices", Json::array()}, {"error", nullptr}};
    try {
        for (const auto& device : enumerate()) result["devices"].push_back({{"id", device.id}, {"name", device.name}, {"kind", device.loopback ? "loopback" : "input"}});
    } catch (const AudioError& error) { result["error"] = error_key(error.code); }
    return result;
}
bool AudioCapture::select(const std::string& id) {
    std::lock_guard command(impl_->command);
    Device selected;
    if (!id.empty()) {
        try {
            const auto devices = enumerate();
            const auto found = std::find_if(devices.begin(), devices.end(), [&](const auto& d) { return d.id == id; });
            if (found == devices.end()) return false;
            selected = *found;
        } catch (const AudioError&) { return false; }
    }
    if (impl_->worker.joinable()) { impl_->worker.request_stop(); impl_->worker.join(); }
    {
        std::lock_guard lock(impl_->mutex);
        impl_->samples.clear(); impl_->updated = {}; impl_->sample_rate = 0; ++impl_->sequence;
        impl_->id = id; impl_->name = selected.name; impl_->error.clear();
        impl_->status = id.empty() ? "stopped" : "starting";
    }
    if (!id.empty()) impl_->worker = std::jthread([this, selected](std::stop_token stop) { impl_->capture(stop, selected); });
    return true;
}
Json AudioCapture::state() const {
    std::lock_guard lock(impl_->mutex);
    const auto age = impl_->updated == Clock::time_point{} ? -1LL :
        std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - impl_->updated).count();
    const bool fresh = impl_->status == "capturing" && age >= 0 && age <= 250;
    const auto samples = fresh ? impl_->samples.snapshot() : audio::Samples{}.snapshot();
    return {{"status", impl_->status}, {"error", impl_->error.empty() ? Json(nullptr) : Json(impl_->error)},
        {"deviceId", impl_->id}, {"deviceName", impl_->name}, {"sampleRate", impl_->sample_rate},
        {"sequence", impl_->sequence}, {"sampleAgeMs", age < 0 ? Json(nullptr) : Json(age)}, {"fresh", fresh},
        {"left", samples[0]}, {"right", samples[1]}};
}
}
