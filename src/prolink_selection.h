#pragma once
#include <nlohmann/json.hpp>
#include <set>
#include <string>

namespace deckstatus {
inline bool valid_prolink_mapping(const nlohmann::json& devices,bool saved=false,bool empty=false) {
    if(!devices.is_array()||(!empty&&devices.empty())||devices.size()>4)return false;
    std::set<int> players,decks;
    for(const auto& device:devices) {
        if(!device.is_object()||device.size()!=(saved?3:2)||!device.contains("player")||!device.contains("deck")||
           !device["player"].is_number_integer()||!device["deck"].is_number_integer()||device["player"]<1||device["player"]>6||device["deck"]<1||device["deck"]>4)return false;
        if(!players.insert(device["player"].get<int>()).second||!decks.insert(device["deck"].get<int>()).second)return false;
        if(saved&&(!device.contains("name")||!device["name"].is_string()||!std::set<std::string>{"CDJ-3000","CDJ-3000X","XDJ-AZ"}.contains(device["name"].get<std::string>())))return false;
    }
    return true;
}
inline bool valid_prolink_settings(const nlohmann::json& value) {
    return value.is_object()&&value.size()==2&&value.contains("autoConnect")&&value["autoConnect"].is_boolean()&&value.contains("devices")&&valid_prolink_mapping(value["devices"],true,true);
}
}
