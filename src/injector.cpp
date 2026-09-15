#include "injector.h"
#include "language.h"
#include <TlHelp32.h>
#include <algorithm>
#include <chrono>
#include <stdexcept>
#include <vector>

namespace rb {
namespace {
struct Handle {
    HANDLE value{};
    explicit Handle(HANDLE v = nullptr) : value(v) {}
    ~Handle() { if (value && value != INVALID_HANDLE_VALUE) CloseHandle(value); }
    Handle(const Handle&) = delete;
    Handle& operator=(const Handle&) = delete;
};

[[noreturn]] void fail(const char* what) {
    const auto error = GetLastError();
    throw std::runtime_error(tr(what) + " (" + tr("Windows error") + " " + std::to_string(error) + ")");
}

std::filesystem::path process_path(HANDLE process) {
    std::wstring path(32768, L'\0');
    DWORD length = static_cast<DWORD>(path.size());
    if (!QueryFullProcessImageNameW(process, 0, path.data(), &length)) fail("Prozesspfad nicht lesbar");
    path.resize(length);
    return path;
}

std::wstring file_version(const std::filesystem::path& path) {
    const DWORD size = GetFileVersionInfoSizeW(path.c_str(), nullptr);
    if (!size) return L"unbekannt";
    std::vector<std::byte> data(size);
    if (!GetFileVersionInfoW(path.c_str(), 0, size, data.data())) return L"unbekannt";
    VS_FIXEDFILEINFO* info{};
    UINT length{};
    if (!VerQueryValueW(data.data(), L"\\", reinterpret_cast<void**>(&info), &length) || length < sizeof(*info))
        return L"unbekannt";
    return std::to_wstring(HIWORD(info->dwFileVersionMS)) + L"." +
           std::to_wstring(LOWORD(info->dwFileVersionMS)) + L"." +
           std::to_wstring(HIWORD(info->dwFileVersionLS));
}

std::uintptr_t module_base(DWORD pid, const std::wstring& name) {
    for (int retry = 0; retry < 10; ++retry) {
        Handle snapshot(CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid));
        if (snapshot.value == INVALID_HANDLE_VALUE) {
            if (GetLastError() == ERROR_BAD_LENGTH) { Sleep(20); continue; }
            fail("Modulliste nicht lesbar");
        }
        MODULEENTRY32W entry{};
        entry.dwSize = sizeof(entry);
        if (Module32FirstW(snapshot.value, &entry)) {
            do {
                if (_wcsicmp(entry.szModule, name.c_str()) == 0)
                    return reinterpret_cast<std::uintptr_t>(entry.modBaseAddr);
            } while (Module32NextW(snapshot.value, &entry));
        }
        return 0;
    }
    throw std::runtime_error("Rekordbox-Modulliste ist noch nicht stabil; erneut versuchen.");
}

LPTHREAD_START_ROUTINE remote_load_library(DWORD pid) {
    // Resolve the actual owning module: Kernel32 exports may forward into KernelBase.
    const auto proc = GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "LoadLibraryW");
    if (!proc) fail("LoadLibraryW nicht gefunden");
    HMODULE owner{};
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCWSTR>(proc), &owner)) fail("LoadLibraryW-Modul nicht gefunden");
    wchar_t path[32768]{};
    if (!GetModuleFileNameW(owner, path, static_cast<DWORD>(std::size(path)))) fail("Systemmodulpfad nicht lesbar");
    const auto remote = module_base(pid, std::filesystem::path(path).filename().wstring());
    if (!remote) throw std::runtime_error("LoadLibraryW-Systemmodul fehlt im Zielprozess.");
    return reinterpret_cast<LPTHREAD_START_ROUTINE>(remote +
        reinterpret_cast<std::uintptr_t>(proc) - reinterpret_cast<std::uintptr_t>(owner));
}
}

Target find_target(DWORD requested_pid) {
    Handle processes(CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0));
    if (processes.value == INVALID_HANDLE_VALUE) fail("Prozessliste nicht lesbar");
    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    std::vector<DWORD> matches;
    if (Process32FirstW(processes.value, &entry)) {
        do {
            if (_wcsicmp(entry.szExeFile, L"rekordbox.exe") == 0 && (!requested_pid || requested_pid == entry.th32ProcessID))
                matches.push_back(entry.th32ProcessID);
        } while (Process32NextW(processes.value, &entry));
    }
    if (matches.empty()) throw std::runtime_error("Kein laufendes rekordbox.exe gefunden. Rekordbox 7 starten und erneut ausfuehren.");
    if (matches.size() != 1) throw std::runtime_error("Mehrere Rekordbox-Prozesse gefunden. Bitte mit --pid <PID> auswaehlen.");
    Handle process(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, matches.front()));
    if (!process.value) fail("Rekordbox nicht zugaenglich; beide Programme mit denselben Benutzerrechten starten");
    const auto path = process_path(process.value);
    return {matches.front(), path, file_version(path)};
}

struct Injection::Impl {
    Handle process, mapping, mutex, stop, ready, stopped;
    SharedState* state{};
    DWORD pid{};
    bool loaded{};
    ~Impl() {
        if (stop.value) SetEvent(stop.value);
        if (loaded && stopped.value) WaitForSingleObject(stopped.value, 3000);
        if (state) UnmapViewOfFile(state);
    }
};

Injection::Injection(const Target& target, const std::filesystem::path& dll) : impl_(std::make_unique<Impl>()) {
    auto& p = *impl_;
    p.pid = target.pid;
    if (!std::filesystem::is_regular_file(dll)) throw std::runtime_error("rb_bridge.dll fehlt neben rb_inj.exe.");
    p.process.value = OpenProcess(PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION | PROCESS_VM_OPERATION |
                                  PROCESS_VM_WRITE | PROCESS_VM_READ | SYNCHRONIZE, FALSE, target.pid);
    if (!p.process.value) fail("Injection-Zugriff verweigert; Rekordbox und Bridge mit denselben Benutzerrechten starten");
    // Verify identity again on the opened handle (PID may have been reused).
    if (_wcsicmp(process_path(p.process.value).c_str(), target.executable.c_str()) != 0)
        throw std::runtime_error("Zielprozess hat sich geaendert; erneut starten.");
    USHORT process_machine{}, native_machine{};
    if (!IsWow64Process2(p.process.value, &process_machine, &native_machine)) fail("Prozessarchitektur nicht erkennbar");
    if (process_machine != IMAGE_FILE_MACHINE_UNKNOWN || native_machine != IMAGE_FILE_MACHINE_AMD64)
        throw std::runtime_error("Nur Windows x64 mit Rekordbox x64 wird unterstuetzt.");
    if (module_base(target.pid, dll.filename().wstring()))
        throw std::runtime_error("rb_bridge.dll ist bereits geladen. Laufende Bridge zuerst beenden.");

    p.mapping.value = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, sizeof(SharedState),
                                        object_name(target.pid, L"State").c_str());
    const auto mapping_error = GetLastError();
    if (!p.mapping.value) fail("Shared Memory konnte nicht erstellt werden");
    if (mapping_error == ERROR_ALREADY_EXISTS) throw std::runtime_error("Eine Bridge ist bereits mit diesem Prozess verbunden.");
    p.mutex.value = CreateMutexW(nullptr, FALSE, object_name(target.pid, L"Lock").c_str());
    p.stop.value = CreateEventW(nullptr, TRUE, FALSE, object_name(target.pid, L"Stop").c_str());
    p.ready.value = CreateEventW(nullptr, TRUE, FALSE, object_name(target.pid, L"Ready").c_str());
    p.stopped.value = CreateEventW(nullptr, TRUE, FALSE, object_name(target.pid, L"Stopped").c_str());
    if (!p.mutex.value || !p.stop.value || !p.ready.value || !p.stopped.value) fail("IPC-Objekte konnten nicht erstellt werden");
    p.state = static_cast<SharedState*>(MapViewOfFile(p.mapping.value, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(SharedState)));
    if (!p.state) fail("Shared Memory konnte nicht geoeffnet werden");
    *p.state = SharedState{};
    p.state->host_pid = GetCurrentProcessId();
    p.state->host_heartbeat = GetTickCount64();
    for (unsigned i = 0; i < 4; ++i) p.state->decks[i].id = i + 1;

    const auto path = std::filesystem::absolute(dll).wstring();
    const auto bytes = (path.size() + 1) * sizeof(wchar_t);
    const auto start = remote_load_library(target.pid);
    void* remote_path = VirtualAllocEx(p.process.value, nullptr, bytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!remote_path) fail("DLL-Pfad konnte im Zielprozess nicht reserviert werden");
    SIZE_T written{};
    if (!WriteProcessMemory(p.process.value, remote_path, path.c_str(), bytes, &written) || written != bytes) {
        const auto error = GetLastError();
        VirtualFreeEx(p.process.value, remote_path, 0, MEM_RELEASE);
        SetLastError(error);
        fail("DLL-Pfad konnte nicht uebertragen werden");
    }
    Handle thread(CreateRemoteThread(p.process.value, nullptr, 0, start, remote_path, 0, nullptr));
    if (!thread.value) {
        const auto error = GetLastError();
        VirtualFreeEx(p.process.value, remote_path, 0, MEM_RELEASE);
        SetLastError(error);
        fail("DLL konnte nicht geladen werden");
    }
    const auto deadline = GetTickCount64() + 15000;
    DWORD thread_wait{};
    while ((thread_wait = WaitForSingleObject(thread.value, 100)) == WAIT_TIMEOUT) {
        read();
        if (GetTickCount64() >= deadline) {
            // Never free memory while the remote thread could still be reading it.
            throw std::runtime_error("LoadLibraryW antwortet nicht. Remote-Pfad bleibt bis zum Prozessende reserviert.");
        }
    }
    if (thread_wait != WAIT_OBJECT_0) {
        // Without a confirmed exit, the remote buffer must remain allocated.
        fail("Status des Injection-Threads konnte nicht abgefragt werden");
    }
    VirtualFreeEx(p.process.value, remote_path, 0, MEM_RELEASE);
    // GetExitCodeThread truncates HMODULE on x64, so verify the module list instead.
    p.loaded = module_base(target.pid, dll.filename().wstring()) != 0;
    // A rejected build can publish its diagnostic and unload before enumeration.
    if (!p.loaded && WaitForSingleObject(p.ready.value, 0) != WAIT_OBJECT_0)
        throw std::runtime_error("Windows hat rb_bridge.dll nicht geladen oder die DLL konnte IPC nicht oeffnen.");
    const auto ready_deadline = GetTickCount64() + 15000;
    while (WaitForSingleObject(p.ready.value, 100) == WAIT_TIMEOUT) {
        read();
        if (!alive() || GetTickCount64() >= ready_deadline)
            throw std::runtime_error("Die Rekordbox-Bridge hat keinen Startstatus geliefert.");
    }
}

Injection::~Injection() = default;

SharedState Injection::read() {
    auto& p = *impl_;
    SharedState copy{};
    const auto result = WaitForSingleObject(p.mutex.value, 100);
    if (result != WAIT_OBJECT_0 && result != WAIT_ABANDONED) {
        copy.status = BridgeStatus::error;
        strcpy_s(copy.message, "Shared-Memory-Sperre antwortet nicht.");
        return copy;
    }
    p.state->host_heartbeat = GetTickCount64();
    copy = *p.state;
    ReleaseMutex(p.mutex.value);
    if (copy.magic != protocol_magic || copy.version != protocol_version || copy.size != sizeof(SharedState)) {
        copy = SharedState{};
        copy.status = BridgeStatus::error;
        strcpy_s(copy.message, "Inkompatibles IPC-Protokoll.");
    }
    return copy;
}

bool Injection::alive() const { return WaitForSingleObject(impl_->process.value, 0) == WAIT_TIMEOUT; }
}
