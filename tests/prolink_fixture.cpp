// Isolated helper-process fixture. It opens no sockets and contains no Java or Rekordbox code.
#include "prolink.h"
#include <Windows.h>
#include <iostream>
#include <thread>
int main(int argc, char** argv) {
    using Json = nlohmann::json;
    const auto base = static_cast<unsigned>(std::stoul(argv[argc - 1]));
    auto state = deckstatus::prolink_empty_state("prolinkStopped");
    state["type"] = "snapshot";
    state["setup"] = {{"status", "stopped"}, {"message", "prolinkStopped"}, {"devices", Json::array()}, {"players", Json::array()}, {"helperPid", GetCurrentProcessId()}};
    auto emit = [&] { std::cout << state.dump() << '\n' << std::flush; };
    emit();
    std::string line;
    while (std::getline(std::cin, line)) {
        const auto command = Json::parse(line);
        if(command["action"]=="discover") {
            state["setup"]["status"]="discovering";
            state["setup"]["devices"]=Json::array({{{"number",1},{"name","CDJ-3000"},{"selectable",true}},{{"number",4},{"name","DJS-1000"},{"selectable",false}}});
        }
        if(command.contains("mapping")) {
            state["status"]="connected";state["setup"]["status"]="connected";state["setup"]["mapping"]=command["mapping"];
            auto& deck=state["decks"][command["mapping"][0]["deck"].get<int>()-1];deck["loaded"]=true;deck["trackId"]=base+1;
            emit();continue;
        }
        if (command["action"] == "connect" && command["players"][0] == 2) return 7;
        if (command["action"] == "connect" && command["players"][0] == 3) {
            std::this_thread::sleep_for(std::chrono::seconds(4)); emit(); continue;
        }
        if (command["action"] == "connect") {
            state["status"] = "connected"; state["masterDeckId"] = 1;
            auto& deck = state["decks"][0];
            deck["loaded"] = true; deck["trackId"] = base + 1; deck["metadataAvailable"] = true;
            deck["title"] = "Network fixture"; deck["bpm"] = 129.5; deck["originalBpm"] = 128;
            std::cout << Json({{"type", "cover"}, {"trackId", base + 1}, {"mime", "image/png"}, {"data", "iVBORwABAg=="}}).dump() << '\n';
        }
        if (command["action"] == "disconnect") { state["status"] = "disconnected"; state["setup"]["status"]="stopped"; state["masterDeckId"] = nullptr; state["decks"] = deckstatus::prolink_empty_state("")["decks"]; }
        emit();
    }
}
