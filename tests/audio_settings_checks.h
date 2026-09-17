#pragma once
#include "audio_control.h"
void audio_settings_checks(const std::filesystem::path& root){
    std::vector<std::string> opened;bool missing=false;
    Json runtime={{"status","stopped"},{"deviceId",""}};
    const auto devices=[&]{return Json{{"devices",missing?Json::array():Json::array({{{"id","fixture-input"},{"name","Fixture input"},{"kind","input"}}})}};};
    const auto state=[&]{return runtime;};
    const auto select=[&](const std::string& id){if(missing&&!id.empty())return false;opened.push_back(id);runtime={{"status",id.empty()?"stopped":"capturing"},{"deviceId",id}};return true;};
    {
        Portal p(root);deckstatus::AudioControl control(&p,devices,state,select);
        control.start_on_launch();check(opened.empty(),"Audio started without opt-in");
        check(p.audio_settings()["autoStart"]==false,"Audio migration default changed");
        control.save({{"deviceId","fixture-input"},{"autoStart",false}});check(opened.empty(),"Saving input started capture");
        fails(400,[&]{control.save({{"deviceId","unknown"},{"autoStart",false}});});
        fails(400,[&]{control.save({{"deviceId",""},{"autoStart",true}});});
        fails(400,[&]{control.save({{"deviceId","fixture-input"},{"autoStart","true"}});});
        const auto before=p.audio_settings();
        const auto held=CreateFileW((root/"portal.json").c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);check(held!=INVALID_HANDLE_VALUE,"Could not lock audio settings");
        fails(500,[&]{control.save({{"deviceId","fixture-input"},{"autoStart",true}});});CloseHandle(held);
        check(p.audio_settings()==before&&control.describe()["settings"]==before,"Failed audio save modified settings");
    }
    {
        Portal p(root);deckstatus::AudioControl control(&p,devices,state,select);
        check(control.describe()["settings"]["deviceId"]=="fixture-input","Input selection lost on restart");
        control.start_on_launch();check(opened.empty(),"Disabled autostart captured after restart");
        control.save({{"deviceId","fixture-input"},{"autoStart",true}});check(opened.empty(),"Enabling autostart opened current session");
    }
    {
        Portal p(root);deckstatus::AudioControl control(&p,devices,state,select);control.start_on_launch();
        check(opened==std::vector<std::string>{"fixture-input"},"Saved autostart input not opened");
        control.stop();check(control.describe()["settings"]["deviceId"]=="fixture-input"&&p.audio_settings()["autoStart"],"Stop erased persisted input or policy");
        control.start_saved();check(runtime["status"]=="capturing","Explicit restart failed");control.stop();
    }
    {
        missing=true;const auto count=opened.size();Portal p(root);deckstatus::AudioControl control(&p,devices,state,select);
        control.start_on_launch();check(opened.size()==count&&control.describe()["controlError"]=="audioDeviceLost","Missing saved input fell back or hid failure");
        control.save({{"deviceId","fixture-input"},{"autoStart",false}});check(p.audio_settings()["deviceName"]=="Fixture input","Missing input identity lost");
    }
    std::cout<<"Audio settings: persistence, opt-in autostart, stop retention, missing device and failed-write safety passed with fake capture.\n";
}
