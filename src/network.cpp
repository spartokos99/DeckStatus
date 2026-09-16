#include "network.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <Windows.h>
#include <array>
#include <charconv>
#include <fstream>
#include <set>
#include <stdexcept>
#include <vector>

namespace deckstatus {
namespace {
using Json = nlohmann::json;
Json encode(const NetworkOptions& options) {
    return {{"bind", options.bind}, {"port", options.port}, {"allowRemoteControl", options.allow_remote_control},
        {"publicDomain", options.public_domain}};
}
std::string public_domain(const Json& value) {
    if (!value.is_string()) throw std::runtime_error("networkInvalidDomain");
    auto domain = value.get<std::string>();
    if (domain.empty()) return domain;
    if (domain.size() > 253 || domain.find('.') == std::string::npos) throw std::runtime_error("networkInvalidDomain");
    std::size_t label = 0;
    bool letter = false;
    char previous = '.';
    for (auto& c : domain) {
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c + ('a' - 'A'));
        if (c == '.') {
            if (!label || previous == '-') throw std::runtime_error("networkInvalidDomain");
            label = 0; letter = false;
        } else {
            const bool alpha = c >= 'a' && c <= 'z';
            if ((!alpha && !(c >= '0' && c <= '9') && c != '-') || (!label && c == '-') || ++label > 63)
                throw std::runtime_error("networkInvalidDomain");
            letter = letter || alpha;
        }
        previous = c;
    }
    // DNS names only: no IP literals, URL schemes, ports, paths or wildcard hosts.
    if (!label || previous == '-' || !letter) throw std::runtime_error("networkInvalidDomain");
    return domain;
}
std::optional<std::array<unsigned, 4>> ipv4(const std::string& address) {
    std::array<unsigned, 4> parts{};
    std::size_t begin = 0;
    for (std::size_t i = 0; i < parts.size(); ++i) {
        const auto end = i == 3 ? address.size() : address.find('.', begin);
        if (end == std::string::npos || end <= begin || end - begin > 3 ||
            (end - begin > 1 && address[begin] == '0')) return {};
        const auto result = std::from_chars(address.data() + begin, address.data() + end, parts[i]);
        if (result.ec != std::errc{} || result.ptr != address.data() + end || parts[i] > 255) return {};
        begin = end + 1;
    }
    return parts;
}
std::string utf8(const wchar_t* text) {
    if (!text) return {};
    const int length = WideCharToMultiByte(CP_UTF8, 0, text, -1, nullptr, 0, nullptr, nullptr);
    if (length < 1) return {};
    std::string result(length, '\0');
    WideCharToMultiByte(CP_UTF8, 0, text, -1, result.data(), length, nullptr, nullptr);
    result.pop_back(); return result;
}
}
bool valid_bind_address(const std::string& address) {
    if (address == "0.0.0.0") return true;
    const auto parts = ipv4(address);
    return parts && (*parts)[0] > 0 && (*parts)[0] < 224;
}
bool loopback_peer(const std::string& address) {
    if (address == "::1") return true;
    const auto parts = ipv4(address.starts_with("::ffff:") ? address.substr(7) : address);
    return parts && (*parts)[0] == 127;
}
bool may_control_network(const std::string& peer, bool allow_remote_control) {
    return loopback_peer(peer) || (allow_remote_control && !peer.empty());
}
bool local_network_peer(const std::string& peer, const std::string& destination) {
    return loopback_peer(peer) || (!peer.empty() && peer == destination);
}
NetworkOptions parse_network_options(const Json& value) {
    if (!value.is_object() || value.size() < 3 || value.size() > 4 ||
        (value.size() == 4 && !value.contains("publicDomain")) || !value.contains("bind") || !value["bind"].is_string() ||
        !value.contains("port") || !value["port"].is_number_integer() || value["port"] < 1 || value["port"] > 65535 ||
        !value.contains("allowRemoteControl") || !value["allowRemoteControl"].is_boolean()) throw std::runtime_error("networkInvalidSettings");
    NetworkOptions result{value["bind"].get<std::string>(), value["port"].get<int>(), value["allowRemoteControl"].get<bool>()};
    if (!valid_bind_address(result.bind)) throw std::runtime_error("networkInvalidSettings");
    if (value.contains("publicDomain")) result.public_domain = public_domain(value["publicDomain"]);
    return result;
}
std::string network_url(const std::string& address, int port) {
    return "http://" + address + (port == 80 ? "" : ":" + std::to_string(port));
}
Json network_interfaces() {
    ULONG size = 16384;
    std::vector<unsigned char> buffer(size);
    ULONG result = ERROR_BUFFER_OVERFLOW;
    for (int attempt = 0; attempt < 3 && result == ERROR_BUFFER_OVERFLOW; ++attempt) {
        buffer.resize(size);
        result = GetAdaptersAddresses(AF_INET, GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER,
            nullptr, reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buffer.data()), &size);
    }
    if (result != NO_ERROR) return Json::array();
    Json adapters = Json::array(); std::set<std::string> seen;
    for (auto* adapter = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buffer.data()); adapter; adapter = adapter->Next) {
        if (adapter->OperStatus != IfOperStatusUp || adapter->IfType == IF_TYPE_SOFTWARE_LOOPBACK) continue;
        for (auto* unicast = adapter->FirstUnicastAddress; unicast; unicast = unicast->Next) {
            if (!unicast->Address.lpSockaddr || unicast->Address.lpSockaddr->sa_family != AF_INET) continue;
            const auto* address = reinterpret_cast<sockaddr_in*>(unicast->Address.lpSockaddr);
            char text[INET_ADDRSTRLEN]{};
            if (!InetNtopA(AF_INET, &address->sin_addr, text, sizeof(text)) || !valid_bind_address(text) ||
                loopback_peer(text) || std::string(text) == "0.0.0.0" || !seen.insert(text).second) continue;
            adapters.push_back({{"address", text}, {"name", utf8(adapter->FriendlyName)}});
        }
    }
    return adapters;
}
NetworkConfig::NetworkConfig(std::filesystem::path file, std::optional<std::string> bind,
                             std::optional<int> port, std::optional<bool> remote_control) : file_(std::move(file)) {
    if (std::filesystem::exists(file_)) {
        if (!std::filesystem::is_regular_file(file_) || std::filesystem::file_size(file_) > 4096) throw std::runtime_error("networkInvalidFile");
        std::ifstream input(file_, std::ios::binary);
        if (!input) throw std::runtime_error("networkInvalidFile");
        const auto document = Json::parse(input, nullptr, false);
        try { saved_ = parse_network_options(document); }
        catch (...) { throw std::runtime_error("networkInvalidFile"); }
    }
    active_ = saved_;
    if (bind) active_.bind = *bind;
    if (port) active_.port = *port;
    if (remote_control) active_.allow_remote_control = *remote_control;
    active_ = parse_network_options(encode(active_));
    overridden_ = bind.has_value() || port.has_value() || remote_control.has_value();
}
Json NetworkConfig::describe(bool can_configure) const {
    std::lock_guard lock(mutex_);
    auto interfaces = network_interfaces();
    Json urls = Json::array({network_url("127.0.0.1", active_.port)});
    if (!active_.public_domain.empty()) urls.push_back("https://" + active_.public_domain);
    if (active_.bind == "0.0.0.0") {
        for (const auto& adapter : interfaces) urls.push_back(network_url(adapter["address"], active_.port));
    } else if (active_.bind != "127.0.0.1") urls.push_back(network_url(active_.bind, active_.port));
    return {{"active", encode(active_)}, {"saved", encode(saved_)}, {"restartRequired", active_ != saved_},
        {"commandLineOverrides", overridden_}, {"canConfigure", can_configure}, {"interfaces", std::move(interfaces)}, {"urls", std::move(urls)}};
}
void NetworkConfig::save(const Json& value) {
    const auto options = parse_network_options(value);
    const auto content = encode(options).dump(2) + '\n';
    std::lock_guard lock(mutex_);
    // Same-directory replacement preserves the previous file if writing fails.
    auto temporary = file_; temporary += L".tmp-" + std::to_wstring(GetCurrentProcessId());
    HANDLE output = CreateFileW(temporary.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (output == INVALID_HANDLE_VALUE) throw std::runtime_error("networkSaveFailed");
    DWORD written{};
    const bool complete = WriteFile(output, content.data(), static_cast<DWORD>(content.size()), &written, nullptr) &&
        written == content.size() && FlushFileBuffers(output);
    CloseHandle(output);
    if (!complete || !MoveFileExW(temporary.c_str(), file_.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        DeleteFileW(temporary.c_str()); throw std::runtime_error("networkSaveFailed");
    }
    saved_ = options;
}
} // namespace deckstatus
