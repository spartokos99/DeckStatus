#include "injector.h"
#include <iostream>
#include <stdexcept>

void require(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }

struct Fixture {
    HANDLE stop{};
    PROCESS_INFORMATION process{};
    ~Fixture() {
        if (stop) SetEvent(stop);
        if (process.hProcess) { WaitForSingleObject(process.hProcess, 3000); CloseHandle(process.hProcess); }
        if (process.hThread) CloseHandle(process.hThread);
        if (stop) CloseHandle(stop);
    }
};

int wmain(int argc, wchar_t** argv) {
    if (argc != 3) return 2;
    try {
        Fixture fixture;
        const auto event_name = L"Local\\RBBridgeFixture." + std::to_wstring(GetCurrentProcessId());
        fixture.stop = CreateEventW(nullptr, TRUE, FALSE, event_name.c_str());
        require(fixture.stop != nullptr, "Cannot create fixture event");
        std::wstring command = L"\"" + std::wstring(argv[1]) + L"\" " + event_name;
        STARTUPINFOW startup{};
        startup.cb = sizeof(startup);
        require(CreateProcessW(argv[1], command.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW,
                               nullptr, nullptr, &startup, &fixture.process), "Cannot launch fixture");
        const auto target = deckstatus::find_target(fixture.process.dwProcessId);
        {
            deckstatus::Injection injected(target, argv[2]);
            const auto state = injected.read();
            require(state.status == deckstatus::BridgeStatus::unsupported, "Wrong executable fingerprint must be rejected");
            require(std::string(state.message).find("fingerprint") != std::string::npos,
                    "Diagnostic must identify an unsupported executable fingerprint");
            require(injected.alive(), "Rejected injection must leave target alive");
            require(state.host_pid == GetCurrentProcessId(), "Bridge must preserve host-owned IPC fields");
            bool duplicate_rejected = false;
            try { deckstatus::Injection duplicate(target, argv[2]); }
            catch (const std::exception&) { duplicate_rejected = true; }
            require(duplicate_rejected, "Duplicate bridge must be rejected");
        }
        Sleep(100);
        {
            deckstatus::Injection again(target, argv[2]);
            require(again.read().status == deckstatus::BridgeStatus::unsupported, "Reattachment failed");
            require(again.alive(), "Target died after reattachment");
        }
        std::cout << "Injection, IPC, fingerprint rejection, duplicate detection and reattachment passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
