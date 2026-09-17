#include "automation.h"
#include "twitch.h"
#include "twitch_viewers.h"
#include <Windows.h>
#include <fstream>
#include <iostream>
#include <thread>
#include <atomic>
using namespace deckstatus;
using Json=nlohmann::json;
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class F>void fails(int status,F fn){try{fn();}catch(const PortalError& e){check(e.status==status,"Wrong error status");return;}throw std::runtime_error("Expected failure");}
template<class F>void until(F fn,const char* message,int seconds=10){const auto end=Automation::Clock::now()+std::chrono::seconds(seconds);while(Automation::Clock::now()<end){if(fn())return;std::this_thread::sleep_for(std::chrono::milliseconds(25));}throw std::runtime_error(message);}
Json rule(std::string id="r",std::string action="show"){return {{"id",id},{"name",id},{"enabled",true},{"trigger","command"},{"match","!lights"},{"role","moderator"},{"action",action},{"cooldown",2},{"duration",60},{"minimum",0},{"target",{{"scene","scene"},{"item","layer"}}},{"value",nullptr}};}
int main(){
    const auto root=std::filesystem::temp_directory_path()/("DeckStatus-twitch-test-"+std::to_string(GetCurrentProcessId()));
    try{
        // Viewer authorization: no external service, no chat scopes or admin grants.
        {
            int exchanges=0,validations=0;bool revoked=false;std::string returnedClient="viewerclient123",returnedId="456";
            TwitchViewers viewers([&](const std::wstring& path,const std::string& body,const std::string& headers)->TwitchTransport::Response{
                if(path==L"/oauth2/device"){check(body=="client_id=viewerclient123&scopes=","Viewer requested unnecessary permissions");return {200,{{"device_code","viewer-private-device"},{"user_code","VIEW123"},{"interval",5},{"expires_in",300}}};}
                if(path==L"/oauth2/token"){++exchanges;return {200,{{"access_token","viewer-private-access"},{"refresh_token","discarded-refresh"}}};}
                if(path==L"/oauth2/validate"){++validations;check(headers.find("viewer-private-access")!=std::string::npos,"Viewer token not validated");if(revoked)return {401,Json::object()};return {200,{{"client_id",returnedClient},{"user_id",returnedId},{"login","fixture_viewer"},{"expires_in",3600}}};}
                throw std::runtime_error("Unexpected viewer request");
            });
            fails(409,[&]{viewers.command("","","fixture",{{"action","start"}});});
            const auto first=viewers.command("","viewerclient123","fixture",{{"action","start"}}),second=viewers.command("","viewerclient123","fixture",{{"action","start"}});const std::string a=first["session"],b=second["session"];
            check(a!=b&&first.dump().find("private")==std::string::npos,"Viewer session/token isolation failed");
            fails(401,[&]{viewers.identity("forged","viewerclient123",true);});
            viewers.command(a,"viewerclient123","fixture",{{"action","poll"}});check(exchanges==0,"Viewer polling interval ignored");
            std::this_thread::sleep_for(std::chrono::milliseconds(5050));
            const auto signedIn=viewers.command(a,"viewerclient123","fixture",{{"action","poll"}});check(signedIn["user"]["id"]=="456"&&!signedIn["user"].contains("role")&&signedIn.dump().find("private")==std::string::npos,"Viewer identity/secret isolation failed");
            returnedClient="wrong-client";fails(401,[&]{viewers.command(b,"viewerclient123","fixture",{{"action","poll"}});});returnedClient="viewerclient123";
            check(viewers.identity(b,"viewerclient123").is_null(),"Wrong-client token created a session");
            const auto user=viewers.identity(a,"viewerclient123",true);check(validations==3,"Vote did not revalidate token");
            fails(429,[&]{viewers.identity(a,"viewerclient123",true);});
            std::string track;
            {Portal p(root/"viewer-ratings");const Json history={{"entries",Json::array({{{"title","Viewer track"},{"artist","Fixture"},{"album","Test"}}})}};track=p.public_history(history,"")["entries"][0]["ratingId"];
                p.rate(p.visitor(""),"legacy",{{"track",track},{"stars",5}});
                p.rate_twitch(user,"fixture",{{"track",track},{"stars",1}});auto renamed=user;renamed["login"]="renamed_viewer";
                check(p.rate_twitch(renamed,"other-browser",{{"track",track},{"stars",3}})["count"]==2,"Same Twitch account counted twice");
                const auto details=p.rating_viewers(track);check(details["legacyCount"]==1&&details["viewers"][0]["login"]=="renamed_viewer"&&details["viewers"][0]["stars"]==3,"Viewer details lost identity or legacy votes");
                check(p.public_history(history,"twitch:456")["entries"][0]["rating"]["mine"]==3,"Own vote missing across browsers");
                check(p.public_history(history,"").dump().find("renamed_viewer")==std::string::npos&&p.ratings().dump().find("renamed_viewer")==std::string::npos,"Viewer names leaked into aggregate API");
                fails(401,[&]{p.rate_twitch(nullptr,"fixture",{{"track",track},{"stars",3}});});
                fails(400,[&]{p.rate_twitch(user,"fixture",{{"track",track},{"stars",3},{"login","spoof"}});});
            }
            {Portal restored(root/"viewer-ratings");check(restored.rating_viewers(track)["viewers"][0]["login"]=="renamed_viewer"&&restored.ratings()[0]["average"]==4,"Viewer ratings lost on restart");}
            std::this_thread::sleep_for(std::chrono::milliseconds(1050));revoked=true;fails(401,[&]{viewers.identity(a,"viewerclient123",true);});check(viewers.identity(a,"viewerclient123").is_null(),"Revoked token kept session");
            auto pending=viewers.command("","viewerclient123","fixture",{{"action","start"}});viewers.command(pending["session"],"viewerclient123","fixture",{{"action","logout"}});check(viewers.status(pending["session"],"viewerclient123")["pending"].is_null(),"Logout kept authorization code");
            pending=viewers.command("","viewerclient123","fixture",{{"action","start"}});check(viewers.status(pending["session"],"changedclient")["pending"].is_null(),"Client ID change retained viewer session");
        }
        Automation engine;auto r=rule();Json scene={{"id","scene"},{"revision",1},{"items",Json::array({{{"id","layer"},{"type","text"},{"visible",false},{"x",0},{"opacity",1},{"options",{{"text","Saved"}}}}})}};
        const auto get=[&](const std::string& id){if(id!="scene")throw PortalError(404,"portalNotFound");return scene;};
        const auto now=Automation::Clock::now();Automation::Event e{"event","chat","Viewer","!LIGHTS now","","everyone"};
        engine.configure(Json::array({r}));check(engine.dispatch(e,get,now).empty(),"Role restriction bypassed");e.id="mod";e.role="moderator";
        check(engine.dispatch(e,get,now,true).size()==1&&!engine.apply(scene,now)["items"][0]["visible"].get<bool>(),"Dry run mutated live scene");
        check(engine.dispatch(e,get,now).size()==1&&engine.apply(scene,now)["items"][0]["visible"].get<bool>(),"Show action failed");
        check(engine.dispatch(e,get,now+std::chrono::seconds(3)).empty(),"Duplicate delivery executed twice");
        e.id="cooldown";check(engine.dispatch(e,get,now+std::chrono::seconds(1)).empty(),"Cooldown failed");
        e.id="extend";engine.dispatch(e,get,now+std::chrono::seconds(30));
        check(engine.apply(scene,now+std::chrono::seconds(65))["items"][0]["visible"],"Repeated trigger did not extend duration");
        check(!engine.apply(scene,now+std::chrono::seconds(91))["items"][0]["visible"].get<bool>(),"Timed override did not expire");
        e.id="again";engine.dispatch(e,get,now+std::chrono::seconds(100));scene["revision"]=2;check(!engine.apply(scene,now+std::chrono::seconds(101))["items"][0]["visible"].get<bool>(),"Scene save did not discard override");
        auto text=rule("text","text");text["value"]="Hello {user}: {message}";text["duration"]=0;auto opacity=rule("opacity","opacity");opacity["value"]=.25;
        engine.configure(Json::array({text,opacity}));e.id="text";e.message="!lights <script>{user}</script>";engine.dispatch(e,get,now);
        auto result=engine.apply(scene,now);check(result["items"][0]["options"]["text"]=="Hello Viewer: !lights <script>{user}</script>","Templates expanded recursively or incorrectly");
        check(result["items"][0]["opacity"]==.25&&scene["items"][0]["options"]["text"]=="Saved","Runtime design was persisted or property failed");
        engine.reset();check(engine.apply(scene,now)==scene,"Emergency reset failed");
        r["trigger"]="command";e.id="suffix";e.message="!lightsbad";engine.configure(Json::array({r}));check(engine.dispatch(e,get,now).empty(),"Command matched a longer name");
        auto invalid=r;invalid["value"]="javascript:bad";fails(400,[&]{Automation::validate(Json::array({invalid}));});invalid=r;invalid["duration"]=-1;fails(400,[&]{Automation::validate(Json::array({invalid}));});
        fails(400,[&]{Automation::validate(Json::array({r,r}));});
        check(Automation::expand("{message}",{"","","",std::string(448,'a')+"\xF0\x9F\x98\x80"},450)==std::string(448,'a'),"UTF-8 boundary split");
        check(Automation::expand(std::string(448,'a')+"\xF0\x9F\x98\x80",{},450)==std::string(448,'a'),"Literal UTF-8 boundary split");
        auto reward=rule();reward["trigger"]="reward";reward["match"]="reward-id";engine.configure(Json::array({reward}));check(engine.dispatch({"reward-event","reward","Viewer","","reward-id"},get,now).size()==1,"Reward matching failed");
        // Audio actions share one options property; expiry restores the saved design,
        // including legacy components that have no explicit audioEnabled value.
        for(const auto* kind:{"deck","master","waveform","text","image","fx"})for(const Json baseline:{Json(),Json(false),Json(true)}){
            Automation audio;auto design=scene;design["items"][0]["type"]=kind;
            if(!baseline.is_null())design["items"][0]["options"]["audioEnabled"]=baseline;
            const auto saved=design;const auto lookup=[&](const std::string&){return design;};
            auto on=rule("on","audioOn"),off=rule("off","audioOff");on["match"]="!on";off["match"]="!off";off["duration"]=10;
            audio.configure(Json::array({on,off}));
            Automation::Event input{"audio-on","chat","Viewer","!on","","moderator"};
            auto preview=audio.dispatch(input,lookup,now,true);check(preview[0]["value"]==true&&audio.apply(design,now)==saved,"Audio dry run changed scene");
            audio.dispatch(input,lookup,now);auto active=audio.apply(design,now);
            check(active["items"][0]["options"]["audioEnabled"]==true&&!active["items"][0].contains("audioEnabled"),"Audio enable was not applied to component options");
            input.id="audio-off";input.message="!off";audio.dispatch(input,lookup,now+std::chrono::seconds(5));
            check(audio.apply(design,now+std::chrono::seconds(6))["items"][0]["options"]["audioEnabled"]==false,"Later audio disable failed");
            check(audio.apply(design,now+std::chrono::seconds(16))==saved,"Audio expiry restored an older override instead of saved options");
            on["duration"]=0;audio.configure(Json::array({on}));input.id="audio-persistent";input.message="!on";audio.dispatch(input,lookup,now);
            check(audio.apply(design,now+std::chrono::hours(48))["items"][0]["options"]["audioEnabled"]==true,"Zero-duration audio action expired");
            audio.reset();check(audio.apply(design,now)==saved&&design==saved,"Audio reset altered saved component");
            auto invalid_audio=on;invalid_audio["value"]="true";fails(400,[&]{Automation::validate(Json::array({invalid_audio}));});
        }
        // One trigger applies every action, with one cooldown and independent timers.
        {
            Automation multiple;auto rules=Automation::validate(Json::array({rule()}));auto& multi=rules[0];
            const auto a=multi["actions"][0];auto audio=a,caption=a;audio["action"]="audioOn";audio["duration"]=20;caption["action"]="text";caption["duration"]=30;caption["value"]="Hello {user}";multi["actions"].push_back(audio);multi["actions"].push_back(caption);
            multiple.configure(rules);Automation::Event input{"multi","chat","Viewer","!lights","","moderator"};
            check(multiple.dispatch(input,get,now).size()==3,"A matching automation skipped actions after its first action");
            auto current=multiple.apply(scene,now+std::chrono::seconds(21));check(current["items"][0]["visible"]&&current["items"][0]["options"]["text"]=="Hello Viewer"&&!current["items"][0]["options"].contains("audioEnabled"),"Independent action timers failed");
            check(multiple.apply(scene,now+std::chrono::seconds(31))["items"][0]["options"]["text"]=="Saved","Text timer did not restore its own property");
            check(multiple.apply(scene,now+std::chrono::seconds(61))==scene,"Multi-action expiry altered the saved design");
            input.id="cooldown-multi";check(multiple.dispatch(input,get,now+std::chrono::seconds(1)).empty(),"Cooldown did not gate entire automation");
            multi["actions"]=Json::array({a,a});multi["actions"][1]["action"]="toggle";multiple.configure(rules);input.id="ordered";
            auto preview=multiple.dispatch(input,get,now,true);check(preview[0]["value"]==true&&preview[1]["value"]==false&&multiple.apply(scene,now)==scene,"Dry run did not simulate ordered actions independently");
            auto live=multiple.dispatch(input,get,now);check(live[1]["value"]==false&&!multiple.apply(scene,now)["items"][0]["visible"].get<bool>(),"Later action did not win");
            multi["actions"]=Json::array({a,a,caption});multi["actions"][1]["target"]["item"]="deleted";multiple.configure(rules);input.id="missing-target";
            preview=multiple.dispatch(input,get,now,true);check(preview.size()==3&&preview[1]["error"]=="twitchTargetMissing"&&preview[2]["value"]=="Hello Viewer","Dry run did not report a failed action");
            check(multiple.dispatch(input,get,now).size()==2&&multiple.apply(scene,now)["items"][0]["options"]["text"]=="Hello Viewer","Missing target blocked remaining actions");
            multi["actions"]=Json::array();fails(400,[&]{Automation::validate(rules);});for(int i=0;i<17;++i)multi["actions"].push_back(a);fails(400,[&]{Automation::validate(rules);});
        }
        // Upgrade old stores once, preserving credentials, scene data and action options.
        {
            Portal p(root/"migration");auto legacy=rule("legacy","audioOff");legacy["duration"]=37;
            p.save_twitch_settings({{"clientId","fixtureclient123"},{"enabled",false},{"revision",4},{"rules",Json::array({legacy})}});
            p.save_twitch_credentials({{"streamer",{{"id","123"},{"login","fixture"},{"access","migration-access"},{"refresh","migration-refresh"}}}});
            const auto keys=p.overlay_keys(false),users=p.users(),credentials=p.twitch_credentials();
            const auto held=CreateFileW((root/"migration"/"portal.json").c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
            check(held!=INVALID_HANDLE_VALUE,"Could not lock migration store");fails(500,[&]{TwitchIntegration failed(&p);});CloseHandle(held);
            check(p.twitch_settings()["rules"][0]==legacy,"Failed migration changed stored rule");
            TwitchIntegration upgraded(&p);const auto upgradedSettings=p.twitch_settings();check(upgradedSettings["revision"]==5&&upgradedSettings["rules"][0]["actions"][0]["action"]=="audioOff"&&upgradedSettings["rules"][0]["actions"][0]["duration"]==37,"Legacy action/timer migration failed");
            check(p.overlay_keys(false)==keys&&p.users()==users&&p.twitch_credentials()==credentials,"Migration changed unrelated data or credentials");
            TwitchIntegration second(&p);check(p.twitch_settings()==upgradedSettings,"Migration repeated on the new schema");
        }
        // Protocol fixture: no external network, real persistence/encryption and worker.
        {
            Portal portal(root);auto wire=std::make_shared<TwitchTransport>();std::mutex lock;std::function<void(Json)> deliver;
            std::atomic_int requests=0,subscriptions=0,chats=0,refreshes=0;std::atomic_bool expired=false;
            wire->connect=[&](auto receive){{std::lock_guard guard(lock);deliver=receive;}receive({{"metadata",{{"message_type","session_welcome"}}},{"payload",{{"session",{{"id","fixture-session"}}}}}});};
            wire->disconnect=[&]{std::lock_guard guard(lock);deliver={};};
            wire->request=[&](const auto& host,const auto& path,const auto&,const std::string& body,const std::string& headers)->TwitchTransport::Response{
                ++requests;check(host==L"id.twitch.tv"||host==L"api.twitch.tv","Untrusted host");
                if(path==L"/oauth2/device")return {200,{{"device_code","fake-device-secret"},{"user_code","TEST123"},{"interval",5},{"expires_in",300}}};
                if(path==L"/oauth2/token"){if(body.find("refresh_token")!=std::string::npos)++refreshes;return {200,{{"access_token","fake-private-access"},{"refresh_token","fake-private-refresh"}}};}
                if(path==L"/oauth2/validate"){if(expired.exchange(false))return {401,Json::object()};return {200,{{"client_id","fixtureclient123"},{"user_id","123"},{"login","fixture_streamer"},{"scopes",Json::array({"user:read:chat","user:write:chat","channel:read:redemptions"})},{"expires_in",14400}}};}
                if(path==L"/oauth2/revoke")return {200,Json::object()};
                if(path.starts_with(L"/helix/channel_points/custom_rewards"))return {200,{{"data",Json::array({{{"id","fixture-reward"},{"title","Show lights"},{"cost",500}}})}}};
                if(path==L"/helix/eventsub/subscriptions"){++subscriptions;auto j=Json::parse(body);check(j["transport"]["session_id"]=="fixture-session","Wrong EventSub session");check(headers.find("fake-private-access")!=std::string::npos,"Token not sent server-side");return {202,{{"data",Json::array()}}};}
                if(path==L"/helix/chat/messages"){++chats;check(Json::parse(body)["message"]=="Hello Viewer","Chat template failed");return {200,{{"data",Json::array({{{"message_id","outgoing-message"},{"is_sent",true}}})}}};}
                throw std::runtime_error("Unexpected Twitch request");
            };
            TwitchIntegration twitch(&portal,wire);twitch.start();std::this_thread::sleep_for(std::chrono::milliseconds(250));check(requests==0,"Unconfigured Twitch contacted external service");
            auto design=portal.edit_scene(Json::parse(R"({"action":"save","scene":{"name":"Twitch scene","width":1920,"height":1080,"background":"transparent","items":[{"id":"layer","type":"master","x":0,"y":0,"width":700,"height":400,"opacity":1,"visible":false,"options":{}}]}})"));
            auto show=rule();show["role"]="everyone";show["target"]["scene"]=design["id"];show["duration"]=1;
            auto reply=rule("reply","chat");reply["role"]="everyone";reply["value"]="Hello {user}";
            auto reactive=show;reactive["id"]="reactive";reactive["action"]="audioOn";
            Json config={{"clientId","fixtureclient123"},{"enabled",true},{"rules",Json::array({show,reply,reactive})},{"revision",0}};
            twitch.command({{"action","save"},{"settings",config}});fails(409,[&]{twitch.command({{"action","save"},{"settings",config}});});
            twitch.command({{"action","authorize"},{"role","streamer"}});
            until([&]{return !twitch.describe()["pending"].empty();},"Device code was not exposed");
            check(twitch.describe().dump().find("fake-device-secret")==std::string::npos,"Device secret exposed");
            until([&]{return subscriptions==5;},"EventSub subscriptions failed");
            twitch.command({{"action","rewards"}});until([&]{return twitch.describe().value("rewards",Json::array()).size()==1;},"Reward discovery failed");
            auto describe=twitch.describe();check(describe["accounts"]["streamer"]["login"]=="fixture_streamer","Authorized account missing");check(describe.dump().find("fake-private")==std::string::npos,"Tokens exposed by status");
            auto changed_settings=describe["settings"];changed_settings["enabled"]=false;
            const auto held=CreateFileW((root/"portal.json").c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);check(held!=INVALID_HANDLE_VALUE,"Could not hold settings store");
            fails(500,[&]{twitch.command({{"action","save"},{"settings",changed_settings}});});CloseHandle(held);check(twitch.describe()["settings"]==describe["settings"],"Failed save changed active Twitch settings");
            auto emit=[&](Json j){std::lock_guard guard(lock);check(bool(deliver),"No fixture socket");deliver(std::move(j));};
            Json event={{"metadata",{{"message_type","notification"},{"message_id","message-1"}}},{"payload",{{"subscription",{{"type","channel.chat.message"}}},{"event",{{"chatter_user_id","456"},{"chatter_user_name","Viewer"},{"message_id","chat-message"},{"message",{{"text","!lights"}}},{"badges",Json::array()}}}}}};
            emit(event);until([&]{return chats==1;},"Chat action did not send through fixture");
            check(twitch.render(portal.scene(design["id"]))["items"][0]["visible"],"Event did not show hidden component");
            check(twitch.broadcast_access(design["key"],"/api/master",design["id"]),"Dynamically visible component read scope missing");
            check(twitch.broadcast_access(design["key"],"/api/audio/state",design["id"]),"Audio-reactive action did not grant scoped sample reads");
            check(twitch.render(portal.scene(design["id"]))["items"][0]["options"]["audioEnabled"]==true,"Audio action missing from rendered scene");
            check(!twitch.broadcast_access(design["key"],"/api/admin/twitch",design["id"]),"Scene key grants Twitch admin");
            emit(event);std::this_thread::sleep_for(std::chrono::milliseconds(250));check(chats==1,"EventSub duplicate sent twice");
            until([&]{return !twitch.render(portal.scene(design["id"]))["items"][0]["visible"].get<bool>();},"Timer needs browser to expire");
            check(!twitch.broadcast_access(design["key"],"/api/master",design["id"]),"Expired override retained hidden read access");
            check(!twitch.broadcast_access(design["key"],"/api/audio/state",design["id"]),"Expired audio action retained sample read access");
            check(!portal.scene(design["id"])["items"][0]["visible"].get<bool>(),"Automation rewrote saved design");
            const auto before=chats.load();check(twitch.command({{"action","test"},{"event",{{"type","chat"},{"message","!lights"}}}})["results"].size()==3,"Dry run failed");check(chats==before,"Dry run sent chat");
            event["metadata"]["message_id"]="own-event";event["payload"]["event"]["message_id"]="outgoing-message";event["payload"]["event"]["chatter_user_id"]="123";emit(event);std::this_thread::sleep_for(std::chrono::milliseconds(250));check(chats==before,"Own reply caused feedback");
            const auto oldRules=twitch.describe()["settings"]["rules"];
            auto connection=twitch.command({{"action","saveConnection"},{"settings",{{"clientId","fixtureclient123"},{"revision",twitch.describe()["settings"]["revision"]}}}});
            check(connection["settings"]["rules"]==oldRules&&connection["settings"]["enabled"]==true,"Connection save overwrote automations");
            auto automationSettings=twitch.automation_description()["settings"];
            check(!automationSettings.contains("clientId")&&!twitch.automation_description().contains("accounts"),"Automation API exposes connection settings");
            fails(400,[&]{twitch.automation_command({{"action","authorize"},{"role","streamer"}});});
            auto invalidSettings=automationSettings;invalidSettings["clientId"]="anotherclient123";
            fails(400,[&]{twitch.automation_command({{"action","save"},{"settings",invalidSettings}});});
            automationSettings["rules"][1]["actions"].push_back(automationSettings["rules"][1]["actions"][0]);
            expired=true;twitch.automation_command({{"action","save"},{"settings",automationSettings}});
            fails(409,[&]{twitch.automation_command({{"action","save"},{"settings",automationSettings}});});
            check(twitch.describe()["settings"]["clientId"]=="fixtureclient123","Automation save overwrote Client ID");
            until([&]{return refreshes==1&&subscriptions>=10;},"Expired token was not refreshed");
            event["metadata"]["message_id"]="multi-reply";event["payload"]["event"]["message_id"]="multi-chat";event["payload"]["event"]["chatter_user_id"]="456";
            const auto sentBefore=chats.load();const auto started=Automation::Clock::now();emit(event);
            until([&]{return chats==sentBefore+2;},"Multiple chat actions were dropped");
            check(Automation::Clock::now()-started>=std::chrono::seconds(2),"Multiple replies bypassed chat send limit");
            std::ifstream input(root/"portal.json");const std::string bytes((std::istreambuf_iterator<char>(input)),{});input.close();check(bytes.find("fake-private")==std::string::npos,"Tokens stored in plaintext");
            twitch.command({{"action","unlink"},{"role","streamer"}});until([&]{return twitch.describe()["accounts"].empty();},"Account unlink failed");check(portal.twitch_credentials().empty(),"Unlink retained credentials");
        }
        {Portal p(root);TwitchIntegration restored(&p);check(restored.describe()["settings"]["rules"].size()==3&&restored.describe()["settings"]["rules"][2]["actions"][0]["action"]=="audioOn","Audio rules not persisted");}
        check(std::filesystem::weakly_canonical(root).parent_path()==std::filesystem::weakly_canonical(std::filesystem::temp_directory_path()),"Unsafe cleanup path");std::filesystem::remove_all(root);
        std::cout<<"Twitch automation and isolated OAuth/EventSub/Helix protocol tests passed. No live account or network used.\n";return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
