#include "portal.h"
#include "twitch.h"
#include "server.h"
#include "master_gate.h"
#include "master_history.h"
#include <Windows.h>
#include <httplib/httplib.h>
#include <fstream>
#include <iostream>
#include <thread>

using Json=nlohmann::json;
using deckstatus::Portal;
using deckstatus::TwitchTransport;
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class F>void fails(int status,F fn){try{fn();}catch(const deckstatus::PortalError& e){check(e.status==status,"Wrong error status");return;}throw std::runtime_error("Expected rejection");}
#include "creative_portal_checks.h"
#include "audio_settings_checks.h"
#include "linked_presets_checks.h"
int main(int argc,char** argv){
    const auto root=std::filesystem::temp_directory_path()/(L"DeckStatus-portal-test-"+std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(GetTickCount64()));
    try{
        check(argc==2,"Expected web directory");std::string scene_id,scene_key,track,session,admin_id,preset_id;
        const std::string password="Test-only strong passphrase 42",operator_password="Operator test passphrase 99";
        const Json entry={{"title","Night Drive"},{"artist","Studio North"},{"album","After Hours"},{"trackId",1},{"entryId",1},{"id",1},{"loaded",true},{"metadataAvailable",true}};
        {
            Portal portal(root);check(!portal.initial_password().empty(),"Bootstrap missing");
            fails(500,[&]{Portal second(root);});
            auto login=portal.login("admin",portal.initial_password(),"test");session=login["session"];admin_id=login["user"]["id"];
            check(login["user"]["mustChangePassword"],"Initial password not forced");
            fails(400,[&]{portal.change_password(session,{{"currentPassword",portal.initial_password()},{"password","short"}});});
            portal.change_password(session,{{"currentPassword",portal.initial_password()},{"password",password}});
            check(portal.identity(session).is_null(),"Old session survived password change");check(portal.initial_password().empty(),"Bootstrap retained");
            session=portal.login("admin",password,"test")["session"];
            fails(400,[&]{portal.edit_user(admin_id,{{"action","save"},{"id",admin_id},{"username","admin"},{"role","operator"}});});
            fails(400,[&]{portal.edit_user(admin_id,{{"action","delete"},{"id",admin_id}});});
            auto users=portal.edit_user(admin_id,{{"action","save"},{"username","operator"},{"role","operator"},{"password",operator_password}});
            check(users.size()==2,"User not created");const auto op=portal.login("operator",operator_password,"operator");
            check(op["user"]["mustChangePassword"],"New user not forced");const auto op_id=op["user"]["id"].get<std::string>();
            portal.change_password(op["session"],{{"currentPassword",operator_password},{"password",operator_password+"!"}});
            const auto op_session=portal.login("operator",operator_password+"!","operator")["session"].get<std::string>();
            portal.edit_user(admin_id,{{"action","save"},{"id",op_id},{"username","renamed"},{"role","operator"}});
            check(portal.identity(op_session).is_null(),"Edited user session not revoked");
            fails(409,[&]{portal.edit_user(admin_id,{{"action","save"},{"username","ADMIN"},{"role","operator"},{"password",operator_password}});});
            const auto before=portal.users();
            const auto held=CreateFileW((root/"portal.json").c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);check(held!=INVALID_HANDLE_VALUE,"Could not hold saved file");
            fails(500,[&]{portal.edit_user(admin_id,{{"action","delete"},{"id",op_id}});});CloseHandle(held);check(portal.users()==before,"Failed save changed in-memory data");
            const auto voter=portal.visitor("");check(portal.visitor(voter)==voter,"Visitor signature unstable");check(portal.visitor(voter+"bad")!=voter,"Bad visitor accepted");
            auto history=portal.public_history({{"entries",Json::array({entry})}},voter);track=history["entries"][0]["ratingId"];
            check(portal.rate(voter,"rating",{{"track",track},{"stars",5}})["count"]==1,"First rating missing");
            check(portal.rate(voter,"rating",{{"track",track},{"stars",3}})["count"]==1,"Duplicate voter counted");
            portal.rate(portal.visitor(""),"rating",{{"track",track},{"stars",5}});
            check(portal.ratings()[0]["average"]==4,"Average incorrect");fails(400,[&]{portal.rate(voter,"rating",{{"track",track},{"stars",0}});});
            auto repeat=entry;repeat["title"]="  NIGHT   DRIVE ";repeat["trackId"]=999;repeat["entryId"]=200;
            check(portal.public_history({{"entries",Json::array({repeat})}},voter)["entries"][0]["ratingId"]==track,"Ratings depend on session ID");
            check(portal.presets().empty(),"New library not empty");
            Json preset={{"name","Studio master"},{"type","master"},{"options",{{"width",720},{"history",2},{"fields",Json::array({"title","artist","bpm"})},{"timeline",true}}}};
            auto stored=portal.edit_preset({{"action","save"},{"preset",preset}});preset_id=stored["id"];
            check(stored["revision"]==1&&stored["options"]==preset["options"],"Preset options lost");
            preset["name"]="Renamed master";
            stored=portal.edit_preset({{"action","save"},{"id",preset_id},{"revision",1},{"preset",preset}});
            check(stored["revision"]==2&&stored["name"]=="Renamed master","Preset update failed");
            fails(409,[&]{portal.edit_preset({{"action","save"},{"id",preset_id},{"revision",1},{"preset",preset}});});
            fails(409,[&]{portal.edit_preset({{"action","delete"},{"id",preset_id},{"revision",1}});});
            auto invalid=preset;invalid["options"]["url"]="https://invalid.example";fails(400,[&]{portal.edit_preset({{"action","save"},{"preset",invalid}});});
            invalid=preset;invalid["options"]["fields"]=Json::array({Json::object()});fails(400,[&]{portal.edit_preset({{"action","save"},{"preset",invalid}});});
            invalid=preset;invalid["name"]="   ";fails(400,[&]{portal.edit_preset({{"action","save"},{"preset",invalid}});});
            invalid=preset;invalid["type"]="deck";fails(400,[&]{portal.edit_preset({{"action","save"},{"id",preset_id},{"revision",2},{"preset",invalid}});});
            const auto preset_before=portal.presets();
            const auto held_presets=CreateFileW((root/"portal.json").c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);check(held_presets!=INVALID_HANDLE_VALUE,"Could not hold presets file");
            fails(500,[&]{portal.edit_preset({{"action","delete"},{"id",preset_id},{"revision",2}});});CloseHandle(held_presets);check(portal.presets()==preset_before,"Failed preset save changed memory");
            const auto disposable=portal.edit_preset({{"action","save"},{"preset",{{"name","Temporary wave"},{"type","waveform"},{"options",{{"mode","bars"},{"gain",2.5}}}}}});
            portal.edit_preset({{"action","delete"},{"id",disposable["id"]},{"revision",1}});check(portal.presets()==preset_before,"Preset delete changed another preset");
            Json scene={{"name","Test scene"},{"width",1920},{"height",1080},{"background","transparent"},{"items",Json::array({{{"id","master"},{"type","master"},{"x",30},{"y",40},{"width",736},{"height",500},{"opacity",1},{"visible",true},{"options",{{"width",720},{"history",2}}}}})}};
            const auto saved=portal.edit_scene({{"action","save"},{"scene",scene}});scene_id=saved["id"];scene_key=saved["key"];
            check(portal.broadcast_access(scene_key,"/api/scene",scene_id),"Scene capability missing");check(portal.broadcast_access(scene_key,"/api/master",scene_id),"Scene master capability missing");
            check(!portal.broadcast_access(scene_key,"/api/admin/users",scene_id),"Scene grants admin");check(!portal.broadcast_access(scene_key,"/api/audio/state",scene_id),"Scene grants absent overlay");
            fails(409,[&]{portal.edit_scene({{"action","save"},{"id",scene_id},{"revision",0},{"scene",scene}});});
            scene["items"][0]["options"]["url"]="https://evil.example";fails(400,[&]{portal.edit_scene({{"action","save"},{"scene",scene}});});
            const auto oldkey=portal.overlay_keys(false)["master"].get<std::string>();check(portal.broadcast_access(oldkey,"/api/master",""),"Overlay key missing");portal.overlay_keys(true);check(!portal.broadcast_access(oldkey,"/api/master",""),"Overlay rotation failed");
            // Real HTTP boundary: anonymous, forced-password, operator and admin.
            deckstatus::MasterHistory master;Json state={{"status","demo"},{"demo",true},{"masterDeckId",1},{"decks",Json::array({entry})}};master.update(state);
            httplib::Server reserve;const auto port=reserve.bind_to_any_port("127.0.0.1");std::jthread reserve_thread([&]{reserve.listen_after_bind();});reserve.wait_until_ready();reserve.stop();reserve_thread.join();
            deckstatus::ServerFeatures features;features.portal=&portal;deckstatus::MasterGate master_gate;features.master_gate=&master_gate;std::atomic_bool stop=false;
            auto twitchSettings=portal.twitch_settings();twitchSettings["clientId"]="viewerclient123";portal.save_twitch_settings(twitchSettings);
            features.twitch_transport=std::make_shared<TwitchTransport>();
            features.twitch_transport->request=[](const auto&,const auto& path,const auto&,const auto&,const auto&)->TwitchTransport::Response{
                if(path==L"/oauth2/device")return {200,{{"device_code","fixture-device"},{"user_code","HTTP123"},{"interval",5},{"expires_in",300}}};
                if(path==L"/oauth2/token")return {200,{{"access_token","fixture-token"}}};
                if(path==L"/oauth2/validate")return {200,{{"client_id","viewerclient123"},{"user_id","789"},{"login","http_viewer"},{"expires_in",3600}}};
                throw std::runtime_error("Unexpected viewer HTTP fixture request");
            };
            std::jthread server([&]{deckstatus::run_server("127.0.0.1",port,argv[1],[&]{return state;},[](int){return std::pair<std::string,std::string>{};},stop,&master,&features);});
            struct Stop {std::atomic_bool& flag;~Stop(){flag=true;}} cleanup{stop};
            httplib::Client client("127.0.0.1",port);client.set_read_timeout(5);client.set_connection_timeout(1);client.set_keep_alive(false);
            for(int i=0;i<100;++i){if(client.Get("/login"))break;std::this_thread::sleep_for(std::chrono::milliseconds(30));}
            const auto status=[&](const httplib::Result& r,int code){check(bool(r),"HTTP transport failed");check(r->status==code,"Unexpected HTTP status");};
            status(client.Get("/"),303);status(client.Get("/api/state"),401);status(client.Get("/history"),200);status(client.Get("/api/history"),200);
            status(client.Get("/api/presets"),401);status(client.Post("/api/presets","{}","application/json"),401);
            status(client.Get("/api/admin/audio"),401);status(client.Post("/api/admin/audio",R"({"action":"stop"})","application/json"),401);
            status(client.Get("/api/admin/master"),401);status(client.Post("/api/admin/master",R"({"holdMs":1000})","application/json"),401);
            status(client.Get("/api/admin/twitch"),401);status(client.Post("/api/admin/twitch",R"({"action":"reset"})","application/json"),401);
            status(client.Get("/api/public/twitch"),200);status(client.Post("/api/public/rating",R"({"track":"fake","stars":5,"login":"spoof"})","application/json"),401);
            status(client.Get("/api/admin/ratings/"+track+"/viewers"),401);
            // Every route carries its own access level; none falls back to a permissive default.
            const auto keys=portal.overlay_keys(false);
            for(const auto* guarded:{"/api/app","/api/health","/api/broadcast","/api/media","/api/scenes","/api/decks","/api/audio/devices","/api/audio/state","/api/state","/api/master","/api/master/covers/1","/api/network","/api/admin/audio","/api/admin/master"})
                status(client.Get(guarded),401);
            status(client.Get("/api/history/covers/1"),404);
            status(client.Get("/api/state?key="+keys["deck"].get<std::string>()),200);
            status(client.Get("/api/master?key="+keys["master"].get<std::string>()),200);
            status(client.Get("/api/master?key="+keys["deck"].get<std::string>()),401);
            status(client.Get("/api/presets?key="+keys["deck"].get<std::string>()),401);
            status(client.Get("/api/media/"+std::string(64,'a')+"?key="+keys["deck"].get<std::string>()),401);
            check(!Json::parse(client.Get("/api/history")->body)["canViewRatings"].get<bool>(),"Anonymous history exposed admin link");
            status(client.Post("/api/public/twitch",{{"Origin","http://evil.example"}},R"({"action":"start"})","application/json"),403);
            const auto startViewer=client.Post("/api/public/twitch",R"({"action":"start"})","application/json");status(startViewer,200);
            check(startViewer->body.find("fixture-device")==std::string::npos&&startViewer->body.find("session")==std::string::npos,"Viewer HTTP response leaked secrets");
            auto viewerCookie=startViewer->get_header_value("Set-Cookie");check(viewerCookie.find("HttpOnly")!=std::string::npos&&viewerCookie.find("SameSite=Strict")!=std::string::npos,"Viewer cookie flags missing");viewerCookie=viewerCookie.substr(0,viewerCookie.find(';'));
            client.set_default_headers({{"Cookie",viewerCookie}});std::this_thread::sleep_for(std::chrono::milliseconds(5050));
            const auto viewerLogin=client.Post("/api/public/twitch",R"({"action":"poll"})","application/json");status(viewerLogin,200);check(Json::parse(viewerLogin->body)["user"]["login"]=="http_viewer","Viewer HTTP login failed");
            status(client.Post("/api/public/rating",Json{{"track",track},{"stars",4}}.dump(),"application/json"),200);
            check(Json::parse(client.Get("/api/history")->body)["entries"][0]["rating"]["mine"]==4,"Viewer HTTP history missing own vote");
            status(client.Get("/api/admin/ratings/"+track+"/viewers"),401);status(client.Get("/api/state"),401);
            status(client.Post("/api/public/twitch",R"({"action":"logout"})","application/json"),200);status(client.Post("/api/public/rating",Json{{"track",track},{"stars",4}}.dump(),"application/json"),401);client.set_default_headers({});
            status(client.Get("/automations"),303);status(client.Get("/api/admin/automations"),401);status(client.Post("/api/admin/automations",R"({"action":"reset"})","application/json"),401);
            const httplib::Headers external_link={{"Sec-Fetch-Site","cross-site"},{"Sec-Fetch-Mode","navigate"},{"Sec-Fetch-Dest","document"}};
            status(client.Get("/history",external_link),200);status(client.Get("/login",external_link),200);status(client.Get("/api/state",external_link),403);
            status(client.Post("/api/admin/users","{}","application/json"),401);
            const auto sign_in=client.Post("/api/auth/login",Json{{"username","admin"},{"password",password}}.dump(),"application/json");status(sign_in,200);
            auto cookie=sign_in->get_header_value("Set-Cookie");check(cookie.find("HttpOnly")!=std::string::npos&&cookie.find("SameSite=Strict")!=std::string::npos,"Session cookie flags missing");cookie=cookie.substr(0,cookie.find(';'));client.set_default_headers({{"Cookie",cookie}});
            status(client.Get("/api/state"),200);status(client.Get("/api/admin/users"),200);status(client.Post("/api/auth/logout","{}","text/plain"),415);
            status(client.Get("/api/admin/users",{{"Origin","http://evil.example"}}),403);
            status(client.Get("/api/admin/twitch"),200);
            // The master hold time is server-wide, so only an administrator may change it.
            check(Json::parse(client.Get("/api/admin/master")->body)["holdMs"]==deckstatus::MasterGate::default_hold_ms,"Master hold default changed");
            status(client.Post("/api/admin/master",R"({"holdMs":-1})","application/json"),400);
            status(client.Post("/api/admin/master",R"({"holdMs":30001})","application/json"),400);
            status(client.Post("/api/admin/master",R"({"holdMs":"4000"})","application/json"),400);
            status(client.Post("/api/admin/master",R"({"holdMs":1000,"other":1})","application/json"),400);
            check(portal.master_settings()["holdMs"]==deckstatus::MasterGate::default_hold_ms&&master_gate.hold_ms()==deckstatus::MasterGate::default_hold_ms,"Rejected hold time was applied");
            status(client.Post("/api/admin/master",R"({"holdMs":9500})","application/json"),200);
            check(master_gate.hold_ms()==9500&&portal.master_settings()["holdMs"]==9500,"Accepted hold time was not applied or persisted");
            status(client.Get("/api/admin/ratings/"+track+"/viewers"),200);
            check(Json::parse(client.Get("/api/history")->body)["canViewRatings"].get<bool>(),"Admin history lacks ratings link");
            status(client.Post("/api/public/rating",R"({"track":"fake","stars":5})","application/json"),401);
            status(client.Get("/automations"),200);status(client.Get("/api/admin/automations"),200);status(client.Post("/api/admin/automations",R"({"action":"reset"})","application/json"),200);
            status(client.Post("/api/admin/twitch",R"({"action":"reset"})","application/json"),200);
            status(client.Post("/api/admin/twitch",{{"Origin","http://evil.example"}},R"({"action":"reset"})","application/json"),403);
            status(client.Post("/api/admin/twitch",R"({"action":"save","settings":{}})","application/json"),400);
            status(client.Post("/api/auth/logout","{}","application/json"),200);status(client.Get("/api/state"),401);
            const auto forced=portal.edit_user(admin_id,{{"action","save"},{"id",op_id},{"username","renamed"},{"role","operator"},{"password",operator_password}});
            const auto op_login=client.Post("/api/auth/login",Json{{"username","renamed"},{"password",operator_password}}.dump(),"application/json");status(op_login,200);cookie=op_login->get_header_value("Set-Cookie");cookie=cookie.substr(0,cookie.find(';'));client.set_default_headers({{"Cookie",cookie}});
            status(client.Get("/api/scenes"),403);status(client.Get("/"),303);status(client.Get("/account/password"),200);
            status(client.Get("/api/presets"),403);status(client.Post("/api/presets","{}","application/json"),403);
            status(client.Post("/api/auth/password",Json{{"currentPassword",operator_password},{"password",operator_password+"!!"}}.dump(),"application/json"),200);
            auto op_login2=client.Post("/api/auth/login",Json{{"username","renamed"},{"password",operator_password+"!!"}}.dump(),"application/json");status(op_login2,200);cookie=op_login2->get_header_value("Set-Cookie");client.set_default_headers({{"Cookie",cookie.substr(0,cookie.find(';'))}});
            status(client.Get("/api/scenes"),200);status(client.Get("/api/admin/ratings"),403);status(client.Get("/api/admin/users"),403);status(client.Get("/api/network"),403);
            status(client.Get("/api/admin/audio"),403);status(client.Post("/api/admin/audio",R"({"action":"stop"})","application/json"),403);
            status(client.Get("/api/admin/master"),403);status(client.Post("/api/admin/master",R"({"holdMs":0})","application/json"),403);
            status(client.Get("/api/admin/twitch"),403);status(client.Post("/api/admin/twitch",R"({"action":"reset"})","application/json"),403);
            status(client.Get("/api/admin/ratings/"+track+"/viewers"),403);
            check(!Json::parse(client.Get("/api/history")->body)["canViewRatings"].get<bool>(),"Operator history exposed admin link");
            status(client.Get("/automations"),403);status(client.Get("/api/admin/automations"),403);status(client.Post("/api/admin/automations",R"({"action":"reset"})","application/json"),403);
            status(client.Post("/api/audio/source",R"({"deviceId":""})","application/json"),403);
            status(client.Get("/api/presets"),200);
            const auto op_preset=client.Post("/api/presets",Json{{"action","save"},{"preset",{{"name","Operator deck"},{"type","deck"},{"options",{{"deck",3},{"timeline",true}}}}}}.dump(),"application/json");status(op_preset,200);
            const auto op_saved=Json::parse(op_preset->body);
            status(client.Post("/api/presets",Json{{"action","delete"},{"id",op_saved["id"]},{"revision",op_saved["revision"]}}.dump(),"application/json"),200);
            client.set_default_headers({});status(client.Get("/scene?scene="+scene_id+"&key="+scene_key),200);status(client.Get("/api/scene?scene="+scene_id+"&key="+scene_key),200);status(client.Get("/api/admin/users?scene="+scene_id+"&key="+scene_key),401);
            status(client.Get("/scene?scene="+scene_id+"&key="+scene_key,external_link),200);
            status(client.Get("/scene?scene="+scene_id+"&key=invalid",external_link),403);
            status(client.Get("/api/scene?scene="+scene_id+"&key="+scene_key,external_link),403);
            status(client.Get("/api/admin/users?scene="+scene_id+"&key="+scene_key,external_link),403);
            status(client.Get("/api/presets?scene="+scene_id+"&key="+scene_key),401);
            status(client.Post("/api/presets?scene="+scene_id+"&key="+scene_key,"{}","application/json"),401);
            const auto rotated=portal.edit_scene({{"action","rotate"},{"id",scene_id},{"revision",1}});status(client.Get("/api/scene?scene="+scene_id+"&key="+scene_key),401);scene_key=rotated["key"];
            for(int i=0;i<10;++i)fails(401,[&]{portal.login("nonexistent","incorrect","rate-test");});fails(429,[&]{portal.login("nonexistent","incorrect","rate-test");});
        }
        {
            Portal restored(root);check(restored.identity(session).is_null(),"Sessions persisted across restart");check(restored.ratings()[0]["average"]==4&&restored.ratings()[0]["count"]==3,"Ratings lost on restart");check(restored.scene(scene_id)["name"]=="Test scene","Scene lost on restart");check(restored.broadcast_access(scene_key,"/api/scene",scene_id),"Scene key lost");check(restored.initial_password().empty(),"Bootstrap returned");
            check(restored.presets().size()==1&&restored.presets()[0]["id"]==preset_id&&restored.presets()[0]["revision"]==2,"Presets lost on restart");
            std::ifstream input(root/"portal.json",std::ios::binary);const std::string file((std::istreambuf_iterator<char>(input)),{});check(file.find(password)==std::string::npos&&file.find(operator_password)==std::string::npos,"Plaintext password persisted");
        }
        // A stored hold time survives a restart; an impossible one is rejected, not repaired.
        {Portal restarted(root);check(restarted.master_settings()["holdMs"]==9500,"Master hold time lost on restart");
         fails(400,[&]{restarted.save_master_settings({{"holdMs",30001}});});
         fails(400,[&]{restarted.save_master_settings({{"holdMs",1000},{"extra",true}});});
         check(restarted.master_settings()["holdMs"]==9500,"Rejected hold time replaced the stored value");}
        Json stored_hold;{std::ifstream input(root/"portal.json");input>>stored_hold;}
        auto broken_hold=stored_hold;broken_hold["masterSettings"]={{"holdMs",-5}};
        {std::ofstream output(root/"portal.json",std::ios::binary);output<<broken_hold.dump();}
        fails(400,[&]{Portal invalid_hold(root);});
        {std::ofstream output(root/"portal.json",std::ios::binary);output<<stored_hold.dump();}
        // Existing version-1 stores have no presets property. Migration must preserve every other byte of data semantically.
        Json legacy;{std::ifstream input(root/"portal.json");input>>legacy;}legacy.erase("presets");
        {std::ofstream output(root/"portal.json",std::ios::binary);output<<legacy.dump();}
        {Portal upgraded(root);check(upgraded.presets().empty(),"Legacy library not initialized");check(upgraded.login("admin",password,"upgrade")["user"]["id"]==admin_id,"Migration replaced user credentials");}
        Json upgraded;{std::ifstream input(root/"portal.json");input>>upgraded;}check(upgraded.erase("presets")==1&&upgraded==legacy,"Migration changed existing data");
        auto invalid_store=legacy;invalid_store["presets"]=Json::array();{std::ofstream output(root/"portal.json");output<<invalid_store.dump();}
        fails(500,[&]{Portal invalid(root);});Json preserved;{std::ifstream input(root/"portal.json");input>>preserved;}check(preserved==invalid_store,"Invalid store was reset");
        linked_presets_checks(root/"linked");
        creative_portal_checks(root/"creative");
        audio_settings_checks(root/"audio");
        check(std::filesystem::weakly_canonical(root).parent_path()==std::filesystem::weakly_canonical(std::filesystem::temp_directory_path()),"Unexpected cleanup path");std::filesystem::remove_all(root);
        std::cout<<"Portal persistence, migration, component presets, password lifecycle, roles, ratings, scene capabilities, conflicts and HTTP access passed.\n";return 0;
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
