#pragma once
#include "portal.h"
#include <functional>
#include <mutex>

namespace deckstatus {
// Shared by both HTTP listeners. Callbacks allow lifecycle tests without opening WASAPI.
class AudioControl {
    using Json=nlohmann::json;
    Portal* portal_;
    std::function<Json()> devices_,state_;
    std::function<bool(const std::string&)> select_;
    Json settings_;
    std::string error_;
    mutable std::mutex mutex_;
    void persist(const Json& value){if(portal_)portal_->save_audio_settings(value);settings_=value;}
    std::string device_name(const std::string& id,bool allow_saved){
        if(id.empty())return "";
        const auto listing=devices_();for(const auto& device:listing["devices"])if(device["id"]==id)return device["name"];
        if(allow_saved&&settings_["deviceId"]==id)return settings_["deviceName"];
        throw PortalError(400,"audioDeviceLost");
    }
    void start(const std::string& id){if(id.empty())throw PortalError(400,"audioChooseDevice");if(!select_(id)){error_="audioDeviceLost";throw PortalError(400,"audioDeviceLost");}error_.clear();}
public:
    AudioControl(Portal* portal,std::function<Json()> devices,std::function<Json()> state,std::function<bool(const std::string&)> select)
        :portal_(portal),devices_(std::move(devices)),state_(std::move(state)),select_(std::move(select)),settings_(portal?portal->audio_settings():Json{{"deviceId",""},{"deviceName",""},{"autoStart",false}}){}
    Json describe() const {std::lock_guard lock(mutex_);return {{"settings",settings_},{"state",state_()},{"controlError",error_.empty()?Json(nullptr):Json(error_)}};}
    void save(const Json& value){
        std::lock_guard lock(mutex_);
        if(!value.is_object()||value.size()!=2||!value.contains("deviceId")||!value["deviceId"].is_string()||!value.contains("autoStart")||!value["autoStart"].is_boolean())throw PortalError(400,"audioSettingsInvalid");
        const auto id=value["deviceId"].get<std::string>();
        if(id.size()>2048||id.find('\0')!=std::string::npos||(id.empty()&&value["autoStart"].get<bool>()))throw PortalError(400,"audioSettingsInvalid");
        // A missing saved device can still be retained while disabling autostart.
        const auto name=device_name(id,true);
        persist({{"deviceId",id},{"deviceName",name},{"autoStart",value["autoStart"]}});error_.clear();
    }
    void start_saved(){std::lock_guard lock(mutex_);start(settings_["deviceId"]);}
    void stop(){std::lock_guard lock(mutex_);select_("");error_.clear();}
    void select(const std::string& id){
        std::lock_guard lock(mutex_);
        if(id.empty()){select_("");error_.clear();return;}
        if(id.size()>2048||id.find('\0')!=std::string::npos)throw PortalError(400,"audioSettingsInvalid");
        const auto name=device_name(id,false);auto next=settings_;next["deviceId"]=id;next["deviceName"]=name;persist(next);start(id);
    }
    void start_on_launch(){std::lock_guard lock(mutex_);if(!settings_["autoStart"].get<bool>())return;try{start(settings_["deviceId"]);}catch(...){if(error_.empty())error_="audioError";}}
};
}
