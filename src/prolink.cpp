#include "prolink.h"
#include "prolink_selection.h"
#include <Windows.h>
#include <wincrypt.h>
#include <chrono>
#include <map>
#include <mutex>
#include <set>
#include <thread>
#include <stdexcept>

namespace deckstatus {
using Json = nlohmann::json;
Json prolink_empty_state(const std::string& message) {
    Json decks = Json::array();
    for (int id = 1; id <= 4; ++id) {
        Json deck = {{"id", id}, {"loaded", false}, {"metadataAvailable", false}};
        for (const auto* key : {"trackId", "title", "artist", "album", "key", "genre", "label", "bpm", "originalBpm",
             "coverUrl", "positionMs", "durationMs", "isMaster", "playing", "onAir", "synced", "pitch", "playerNumber"}) deck[key] = nullptr;
        decks.push_back(std::move(deck));
    }
    return {{"schemaVersion", 1}, {"mode", "prolink"}, {"status", "disconnected"}, {"message", message},
        {"version", "PRO DJ LINK"}, {"demo", false}, {"updatedAt", nullptr}, {"sampleAgeMs", nullptr},
        {"artworkStatus", "prolinkMetadataHelp"}, {"masterDeckId", nullptr}, {"decks", std::move(decks)}};
}
bool valid_prolink_command(const Json& command) {
    if (!command.is_object() || !command.contains("action") || !command["action"].is_string()) return false;
    const auto action = command["action"].get<std::string>();
    if (action == "discover" || action == "disconnect") return command.size() == 1;
    if(action=="connect"&&command.size()==2&&command.contains("mapping"))return valid_prolink_mapping(command["mapping"]);
    if (action != "connect" || command.size() != 2 || !command.contains("players") || !command["players"].is_array() ||
        command["players"].empty() || command["players"].size() > 4) return false;
    std::set<int> players;
    for (const auto& player : command["players"]) {
        if (!player.is_number_integer() || player < 1 || player > 6 || !players.insert(player.get<int>()).second) return false;
    }
    return true;
}
struct ProLink::Impl {
    std::filesystem::path directory;
    std::mutex mutex, commands;
    HANDLE process{}, job{}, input{}, output{};
    std::jthread reader;
    Json state = prolink_empty_state("prolinkStopped");
    Json settings = {{"status", "stopped"}, {"message", "prolinkStopped"}, {"devices", Json::array()}, {"players", Json::array()}};
    ULONGLONG received{};
    std::map<std::uint32_t, std::pair<std::string, std::string>> covers;
    std::size_t cover_bytes{};
    unsigned generation{};
    Json preferences={{"autoConnect",false},{"devices",Json::array()}};
    bool suspended=false,awaiting_devices=false;
    ULONGLONG attempted{};

    explicit Impl(std::filesystem::path path) : directory(std::move(path)) {}
    void stop() {
        if (input) { CloseHandle(input); input = nullptr; }
        if (process && WaitForSingleObject(process, 3000) == WAIT_TIMEOUT && job) TerminateJobObject(job, 0);
        if (process) WaitForSingleObject(process, 3000);
        if (reader.joinable()) reader.join();
        for (HANDLE handle : {output, process, job}) if (handle) CloseHandle(handle);
        output = process = job = nullptr;
    }
    ~Impl() { stop(); }
    bool available() const {
        return std::filesystem::is_regular_file(directory / "prolink/runtime/bin/java.exe") &&
            std::filesystem::is_regular_file(directory / "prolink/DeckStatusProLink.jar");
    }
    void failure(const std::string& message) {
        std::lock_guard lock(mutex);
        state = prolink_empty_state(message);
        state["status"] = "error";
        settings["status"] = "error"; settings["message"] = message;
        received = 0;
    }
    bool start() {
        if (process && WaitForSingleObject(process, 0) == WAIT_TIMEOUT) return true;
        stop();
        if (!available()) { failure("prolinkRuntimeMissing"); return false; }
        SECURITY_ATTRIBUTES security{sizeof(security), nullptr, TRUE};
        HANDLE child_input{}, child_output{};
        if (!CreatePipe(&child_input, &input, &security, 0) || !CreatePipe(&output, &child_output, &security, 0)) {
            if (child_input) CloseHandle(child_input);
            if (child_output) CloseHandle(child_output);
            stop(); failure("prolinkHelperFailed"); return false;
        }
        SetHandleInformation(input, HANDLE_FLAG_INHERIT, 0);
        SetHandleInformation(output, HANDLE_FLAG_INHERIT, 0);
        HANDLE errors = CreateFileW(L"NUL", GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, &security, OPEN_EXISTING, 0, nullptr);
        job = CreateJobObjectW(nullptr, nullptr);
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
        limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        const bool job_ready = job && SetInformationJobObject(job, JobObjectExtendedLimitInformation, &limits, sizeof(limits));
        const auto java = directory / "prolink/runtime/bin/java.exe";
        if (++generation > 2000) { CloseHandle(child_input); CloseHandle(child_output); if (errors != INVALID_HANDLE_VALUE) CloseHandle(errors); stop(); failure("prolinkHelperFailed"); return false; }
        std::wstring command = L"\"" + java.wstring() + L"\" -Xmx256m -Djava.awt.headless=true -Dorg.slf4j.simpleLogger.defaultLogLevel=off -cp \"" +
            (directory / "prolink/DeckStatusProLink.jar").wstring() + L";" + (directory / "prolink/lib/*").wstring() + L"\" com.deckstatus.prolink.Main " + std::to_wstring(generation * 1000000);
        STARTUPINFOW startup{}; startup.cb = sizeof(startup); startup.dwFlags = STARTF_USESTDHANDLES;
        startup.hStdInput = child_input; startup.hStdOutput = child_output; startup.hStdError = errors;
        PROCESS_INFORMATION info{};
        const bool created = job_ready && errors != INVALID_HANDLE_VALUE && CreateProcessW(java.c_str(), command.data(), nullptr, nullptr,
            TRUE, CREATE_NO_WINDOW | CREATE_SUSPENDED, nullptr, directory.c_str(), &startup, &info);
        CloseHandle(child_input); CloseHandle(child_output);
        if (errors != INVALID_HANDLE_VALUE) CloseHandle(errors);
        if (!created) { stop(); failure("prolinkHelperFailed"); return false; }
        process = info.hProcess;
        if (!AssignProcessToJobObject(job, process)) {
            TerminateProcess(process, 1); CloseHandle(info.hThread); stop(); failure("prolinkHelperFailed"); return false;
        }
        ResumeThread(info.hThread); CloseHandle(info.hThread);
        reader = std::jthread([this] {
            std::string buffer; char bytes[16384]; DWORD count{};
            while (ReadFile(output, bytes, sizeof(bytes), &count, nullptr) && count) {
                buffer.append(bytes, count);
                if (buffer.size() > 4 * 1024 * 1024) { failure("prolinkInvalidData"); break; }
                for (auto end = buffer.find('\n'); end != std::string::npos; end = buffer.find('\n')) {
                    auto packet = Json::parse(buffer.substr(0, end), nullptr, false);
                    buffer.erase(0, end + 1);
                    if (packet.is_discarded() || !packet.is_object()) continue;
                    try { accept(packet); } catch (...) { failure("prolinkInvalidData"); }
                }
            }
            failure("prolinkHelperStopped");
        });
        return true;
    }
    void accept(const Json& packet) {
        const auto type = packet.value("type", std::string{});
        std::lock_guard lock(mutex);
        if (type == "snapshot" && packet.contains("decks") && packet["decks"].is_array() && packet["decks"].size() == 4 &&
            packet.contains("setup") && packet["setup"].is_object()) {
            state = packet; state.erase("type"); state.erase("setup"); settings = packet["setup"]; received = GetTickCount64();
        } else if (type == "cover" && packet.contains("trackId") && packet["trackId"].is_number_unsigned() && packet.contains("data") && packet["data"].is_string()) {
            const auto id = packet["trackId"].get<std::uint32_t>();
            const auto base64 = packet["data"].get<std::string>();
            if (!id || base64.size() > 2800000 || covers.contains(id)) return;
            DWORD size{};
            if (!CryptStringToBinaryA(base64.c_str(), static_cast<DWORD>(base64.size()), CRYPT_STRING_BASE64, nullptr, &size, nullptr, nullptr) || size > 2 * 1024 * 1024) return;
            std::string data(size, '\0');
            if (!CryptStringToBinaryA(base64.c_str(), static_cast<DWORD>(base64.size()), CRYPT_STRING_BASE64, reinterpret_cast<BYTE*>(data.data()), &size, nullptr, nullptr)) return;
            const auto mime = packet.value("mime", std::string{});
            if (mime != "image/png" && mime != "image/jpeg") return;
            while (!covers.empty() && cover_bytes + size > 64 * 1024 * 1024) { cover_bytes -= covers.begin()->second.second.size(); covers.erase(covers.begin()); }
            cover_bytes += size; covers.emplace(id, std::make_pair(mime, std::move(data)));
        }
    }
    Json send(const Json& command) {
        if(command["action"]=="disconnect"&&!process)return {{"accepted",true}};
        if(!start())return {{"error","prolinkHelperFailed"}};
        const auto line=command.dump()+'\n';DWORD written{};
        if(!WriteFile(input,line.data(),static_cast<DWORD>(line.size()),&written,nullptr)||written!=line.size()) {failure("prolinkHelperFailed");return {{"error","prolinkHelperFailed"}};}
        return {{"accepted",true}};
    }
};
ProLink::ProLink(std::filesystem::path directory) : impl_(std::make_unique<Impl>(std::move(directory))) {}
ProLink::~ProLink() = default;
Json ProLink::snapshot() {
    std::lock_guard lock(impl_->mutex);
    if (impl_->received && GetTickCount64() - impl_->received > 2000) return prolink_empty_state("prolinkHelperStale");
    return impl_->state;
}
Json ProLink::setup() {
    std::lock_guard lock(impl_->mutex);
    auto result = impl_->settings; result["runtimeAvailable"] = impl_->available();
    if (impl_->received && GetTickCount64() - impl_->received > 2000) { result["status"] = "error"; result["message"] = "prolinkHelperStale"; }
    return result;
}
Json ProLink::control(const Json& command) {
    if (!valid_prolink_command(command)) return {{"error", "prolinkInvalidCommand"}};
    std::lock_guard lock(impl_->commands);
    impl_->suspended=command["action"]!="connect";impl_->awaiting_devices=false;impl_->attempted=GetTickCount64();
    return impl_->send(command);
}
void ProLink::configure(const Json& settings,bool resume) {
    if(!valid_prolink_settings(settings))throw std::invalid_argument("Invalid ProLink settings");
    std::lock_guard lock(impl_->commands);impl_->preferences=settings;impl_->suspended=!resume;impl_->attempted=0;
}
void ProLink::maintain() {
    std::lock_guard lock(impl_->commands);
    if(impl_->suspended||!impl_->preferences["autoConnect"].get<bool>()||impl_->preferences["devices"].empty())return;
    const auto now=GetTickCount64();if(impl_->attempted&&now-impl_->attempted<5000&&!impl_->awaiting_devices)return;
    Json current;ULONGLONG received;
    {std::lock_guard state_lock(impl_->mutex);current=impl_->settings;received=impl_->received;}
    const bool fresh=received&&now-received<=2000;
    if(fresh&&(current.value("status","")=="connected"||current.value("status","")=="connecting"))return;
    if(!fresh||current.value("status","")!="discovering") {
        if(impl_->attempted&&now-impl_->attempted<5000)return;
        impl_->attempted=now;
        if(received&&now-received>6000)impl_->stop();
        impl_->awaiting_devices=true;impl_->send({{"action","discover"}});return;
    }
    Json mapping=Json::array();
    for(const auto& wanted:impl_->preferences["devices"]) {
        int matches=0;
        for(const auto& device:current.value("devices",Json::array()))if(device.value("number",0)==wanted["player"].get<int>()){
            // Duplicated numbers are ambiguous even when the other model is unsupported.
            if(!device.value("selectable",false)||device.value("name",std::string{})!=wanted["name"].get<std::string>())return;
            ++matches;
        }
        if(matches!=1)return;
        mapping.push_back({{"player",wanted["player"]},{"deck",wanted["deck"]}});
    }
    impl_->attempted=now;impl_->awaiting_devices=false;impl_->send({{"action","connect"},{"mapping",mapping}});
}
std::pair<std::string, std::string> ProLink::cover(std::uint32_t id) {
    std::lock_guard lock(impl_->mutex);
    const auto found = impl_->covers.find(id);
    return found == impl_->covers.end() ? std::pair<std::string, std::string>{} : found->second;
}
}
