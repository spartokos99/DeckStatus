#pragma once
#include "protocol.h"
#include <filesystem>
#include <memory>

namespace rb {
struct Target { DWORD pid; std::filesystem::path executable; std::wstring version; };
Target find_target(DWORD requested_pid = 0);

class Injection {
public:
    Injection(const Target& target, const std::filesystem::path& dll);
    ~Injection();
    Injection(const Injection&) = delete;
    Injection& operator=(const Injection&) = delete;
    SharedState read(); // Also renews the host heartbeat.
    bool alive() const;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
