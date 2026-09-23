#include "prolink.h"
#include <Windows.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <thread>
#include <stdexcept>
using Json = nlohmann::json;
void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
template<class Test> void until(Test test, const char* message, unsigned count = 150) {
    for (unsigned i = 0; i < count; ++i) { if (test()) return; std::this_thread::sleep_for(std::chrono::milliseconds(20)); }
    throw std::runtime_error(message);
}
int main(int argc, char** argv) {
    try {
        require(argc == 2, "Missing fixture path");
        for (const auto& value : {Json(), Json::array(), Json{{"action", "play"}}, Json{{"action", "discover"}, {"extra", true}},
             Json{{"action", "connect"}, {"players", Json::array()}}, Json{{"action", "connect"}, {"players", {1, 1}}},
             Json{{"action", "connect"}, {"players", {0}}}, Json{{"action", "connect"}, {"players", {7}}},
             Json{{"action", "connect"}, {"players", {1.5}}}, Json{{"action", "connect"}, {"players", {1, 2, 3, 4, 5}}}})
            require(!deckstatus::valid_prolink_command(value), "Invalid command accepted");
        require(deckstatus::valid_prolink_command({{"action", "connect"}, {"players", {1, 2, 5, 6}}}), "Valid player mapping rejected");
        require(deckstatus::valid_prolink_command({{"action","connect"},{"mapping",Json::array({{{"player",3},{"deck",1}},{{"player",1},{"deck",4}}})}}),"Explicit deck assignment rejected");
        require(!deckstatus::valid_prolink_command({{"action","connect"},{"mapping",Json::array({{{"player",3},{"deck",1}},{{"player",1},{"deck",1}}})}}),"Duplicate deck assignment accepted");
        require(!deckstatus::valid_prolink_command({{"action","connect"},{"mapping",Json::array({{{"player",1},{"deck",5}}})}}),"Out-of-range deck accepted");
        const auto root = std::filesystem::absolute(std::filesystem::path(argv[1])).parent_path() / ("prolink-fixture-" + std::to_string(GetCurrentProcessId()));
        std::filesystem::create_directories(root / "prolink/runtime/bin");
        deckstatus::ProLink missing(root);
        require(!missing.setup()["runtimeAvailable"].get<bool>(), "Missing runtime not detected");
        require(missing.control({{"action", "discover"}}).contains("error"), "Missing helper silently succeeded");
        std::filesystem::copy_file(argv[1], root / "prolink/runtime/bin/java.exe", std::filesystem::copy_options::overwrite_existing);
        std::ofstream(root / "prolink/DeckStatusProLink.jar") << "fixture";
        HANDLE helper{};
        {
            deckstatus::ProLink link(root);
            require(link.snapshot()["status"] == "disconnected", "Backend started automatically");
            require(link.control({{"action", "play"}}).contains("error"), "Playback control accepted");
            require(link.control({{"action", "discover"}}).value("accepted", false), "Discovery command failed");
            until([&] { return link.setup().contains("helperPid"); }, "Fixture did not start");
            helper = OpenProcess(SYNCHRONIZE, FALSE, link.setup()["helperPid"].get<DWORD>());
            require(helper != nullptr, "Could not observe owned helper");
            link.control({{"action", "connect"}, {"players", {1}}});
            until([&] { return link.snapshot()["status"] == "connected"; }, "Fixture did not connect");
            const auto id = link.snapshot()["decks"][0]["trackId"].get<unsigned>();
            require(link.cover(id).first == "image/png" && link.cover(id).second.size() == 7, "Artwork transfer failed");
            require(link.cover(id + 1).second.empty(), "Unknown artwork leaked");
            link.control({{"action", "connect"}, {"players", {3}}});
            until([&] { return link.snapshot()["message"] == "prolinkHelperStale"; }, "Hung helper stayed live", 180);
            require(link.snapshot()["decks"][0]["loaded"] == false, "Stale helper retained current track");
            until([&] { return link.snapshot()["status"] == "connected"; }, "Helper failed to recover", 150);
            link.control({{"action", "connect"}, {"players", {2}}});
            until([&] { return link.snapshot()["message"] == "prolinkHelperStopped"; }, "Helper exit not reported");
            require(WaitForSingleObject(helper, 1000) == WAIT_OBJECT_0, "Crashed helper still running");
            CloseHandle(helper); helper = nullptr;
            link.control({{"action", "discover"}});
            until([&] { return link.setup()["status"] == "discovering"; }, "Helper restart failed");
            helper = OpenProcess(SYNCHRONIZE, FALSE, link.setup()["helperPid"].get<DWORD>());
            link.control({{"action", "connect"}, {"players", {1}}});
            until([&] { return link.snapshot()["status"] == "connected"; }, "Restarted helper did not connect");
            require(link.snapshot()["decks"][0]["trackId"].get<unsigned>() != id, "Restart reused track identities");
        }
        require(helper && WaitForSingleObject(helper, 1000) == WAIT_OBJECT_0, "Owned helper survived backend destruction");
        CloseHandle(helper);
        {
            deckstatus::ProLink automatic(root);
            automatic.configure({{"autoConnect",true},{"devices",Json::array({{{"player",1},{"deck",4},{"name","CDJ-3000"}}})}},true);
            until([&]{automatic.maintain();return automatic.setup().value("status","")=="connected";},"Saved devices did not connect when discovered",400);
            require(automatic.snapshot()["decks"][3]["loaded"]==true,"Automatic connection ignored saved deck assignment");
            automatic.control({{"action","disconnect"}});
            until([&]{return automatic.setup().value("status","")=="stopped";},"Automatic fixture did not stop");
            automatic.maintain();require(automatic.setup()["status"]=="stopped","Manual disconnect restarted automatic connection");
        }
        std::cout << "ProLink commands, runtime detection, IPC/artwork, stale/exit/restart identities and owned-process cleanup passed.\n";
        return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
