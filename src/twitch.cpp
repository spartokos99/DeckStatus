#include "twitch.h"
#include "automation.h"
#include "twitch_viewers.h"
#include <Windows.h>
#include <winhttp.h>
#include <atomic>
#include <thread>
#include <deque>
#include <regex>

namespace deckstatus {
namespace {
using Json=nlohmann::json;
using Clock=std::chrono::steady_clock;
struct Handle {
    HINTERNET value=nullptr;
    explicit Handle(HINTERNET h=nullptr):value(h){}
    ~Handle(){if(value)WinHttpCloseHandle(value);}
    Handle(const Handle&)=delete;Handle& operator=(const Handle&)=delete;
};
std::wstring wide(const std::string& s){return std::wstring(s.begin(),s.end());}
std::string encode(const std::string& s){std::string out;const char* h="0123456789ABCDEF";for(unsigned char c:s)if((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='-'||c=='_'||c=='.')out+=c;else{out+='%';out+=h[c>>4];out+=h[c&15];}return out;}
std::string form(const Json& j){std::string out;for(auto i=j.begin();i!=j.end();++i){if(!out.empty())out+='&';out+=encode(i.key())+'='+encode(i.value().get<std::string>());}return out;}
struct Reply {DWORD status;Json body;};
Reply live_request(const wchar_t* host,const wchar_t* path,const wchar_t* method,const std::string& body,const std::string& headers){
    Handle session(WinHttpOpen(L"DeckStatus/2 Twitch",WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,WINHTTP_NO_PROXY_NAME,WINHTTP_NO_PROXY_BYPASS,0));
    if(!session.value)throw PortalError(502,"twitchNetworkError");WinHttpSetTimeouts(session.value,4000,4000,4000,5000);
    Handle connection(WinHttpConnect(session.value,host,INTERNET_DEFAULT_HTTPS_PORT,0));
    Handle req(WinHttpOpenRequest(connection.value,method,path,nullptr,WINHTTP_NO_REFERER,WINHTTP_DEFAULT_ACCEPT_TYPES,WINHTTP_FLAG_SECURE));
    DWORD redirect=WINHTTP_OPTION_REDIRECT_POLICY_NEVER;WinHttpSetOption(req.value,WINHTTP_OPTION_REDIRECT_POLICY,&redirect,sizeof(redirect));
    const auto h=wide(headers);
    if(!req.value||!WinHttpSendRequest(req.value,h.c_str(),static_cast<DWORD>(h.size()),body.empty()?WINHTTP_NO_REQUEST_DATA:const_cast<char*>(body.data()),static_cast<DWORD>(body.size()),static_cast<DWORD>(body.size()),0)||!WinHttpReceiveResponse(req.value,nullptr))throw PortalError(502,"twitchNetworkError");
    DWORD code=0,size=sizeof(code);WinHttpQueryHeaders(req.value,WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,WINHTTP_HEADER_NAME_BY_INDEX,&code,&size,WINHTTP_NO_HEADER_INDEX);
    std::string data;char buffer[8192];DWORD received=0;
    do{if(!WinHttpReadData(req.value,buffer,sizeof(buffer),&received))throw PortalError(502,"twitchNetworkError");data.append(buffer,received);if(data.size()>1024*1024)throw PortalError(502,"twitchNetworkError");}while(received);
    auto json=Json::parse(data,nullptr,false);return {code,json.is_discarded()?Json::object():json};
}
const std::string streamer_scopes="user:read:chat user:write:chat channel:read:redemptions";
const std::string bot_scopes="user:write:chat";
}
struct TwitchIntegration::Impl {
    Portal* portal;
    std::shared_ptr<TwitchTransport> transport;
    Automation automation;
    std::unique_ptr<TwitchViewers> viewers;
    mutable std::mutex mutex;
    Json settings,credentials=Json::object(),state={{"connection","twitchDisabled"},{"error",""},{"accounts",Json::object()},{"pending",Json::object()},{"subscriptions",Json::object()}};
    std::deque<Json> commands,events;
    std::jthread worker,socket;
    std::mutex socket_mutex;
    HINTERNET active_socket=nullptr;
    Clock::time_point retry{},validated{},chat_after{};
    std::atomic_llong socket_deadline{};
    bool socket_open=false;
    Json pending=Json::object();
    std::deque<std::string> sent_messages;
    struct Outbound {Json output;int revision;unsigned epoch;Clock::time_point expires;};
    std::deque<Outbound> outbound;
    unsigned chat_epoch=0; // Protected by mutex; invalidates queued replies on reset.
    explicit Impl(Portal* p,std::shared_ptr<TwitchTransport> wire={}):portal(p),transport(std::move(wire)),settings(p?p->twitch_settings():Json{{"clientId",""},{"enabled",false},{"rules",Json::array()},{"revision",0}}){
        const auto rules=Automation::validate(settings["rules"]);
        if(rules!=settings["rules"]){auto migrated=settings;migrated["rules"]=rules;migrated["revision"]=settings["revision"].get<int>()+1;if(portal)portal->save_twitch_settings(migrated);settings=std::move(migrated);}
        automation.configure(rules);
        if(portal)try{credentials=portal->twitch_credentials();}catch(...){state["error"]="twitchCredentialsLost";}
        accounts();
        viewers=std::make_unique<TwitchViewers>([this](const std::wstring& path,const std::string& body,const std::string& headers){auto r=request(L"id.twitch.tv",path.c_str(),headers.empty()?L"POST":L"GET",body,headers.empty()?"Content-Type: application/x-www-form-urlencoded\r\n":headers);return TwitchTransport::Response{r.status,r.body};});
    }
    ~Impl(){worker.request_stop();{std::lock_guard lock(socket_mutex);if(active_socket){WinHttpCloseHandle(active_socket);active_socket=nullptr;}}if(worker.joinable())worker.join();stop_socket();}
    void accounts(){Json rows=Json::object();for(auto i=credentials.begin();i!=credentials.end();++i)rows[i.key()]={{"login",i.value().value("login",std::string{})},{"id",i.value().value("id",std::string{})}};std::lock_guard lock(mutex);state["accounts"]=rows;}
    Reply request(const wchar_t* host,const wchar_t* path,const wchar_t* method,const std::string& body="",const std::string& headers="Content-Type: application/x-www-form-urlencoded\r\n"){
        if(transport){const auto r=transport->request(host,path,method,body,headers);return {r.status,r.body};}return live_request(host,path,method,body,headers);
    }
    void set(const char* key,const Json& value){std::lock_guard lock(mutex);state[key]=value;}
    void stop_socket(){socket_open=false;if(transport&&transport->disconnect)transport->disconnect();socket.request_stop();{std::lock_guard lock(socket_mutex);if(active_socket){WinHttpCloseHandle(active_socket);active_socket=nullptr;}}if(socket.joinable())socket.join();std::lock_guard lock(mutex);events.clear();state["subscriptions"]=Json::object();}
    void push(Json event){std::lock_guard lock(mutex);if(events.size()<512)events.push_back(std::move(event));else state["error"]="twitchQueueFull";}
    void open_socket(){
        stop_socket();set("connection","twitchConnecting");
        socket_open=true;socket_deadline=std::chrono::duration_cast<std::chrono::seconds>(Clock::now().time_since_epoch()).count()+20;
        if(transport){transport->connect([this](Json event){push(std::move(event));});return;}
        socket=std::jthread([this](std::stop_token stop){
            std::string path="/ws?keepalive_timeout_seconds=10";bool resume=false;
            Handle old;
            while(!stop.stop_requested())try{
                Handle session(WinHttpOpen(L"DeckStatus EventSub",WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,WINHTTP_NO_PROXY_NAME,WINHTTP_NO_PROXY_BYPASS,0));WinHttpSetTimeouts(session.value,4000,4000,4000,15000);
                Handle connection(WinHttpConnect(session.value,L"eventsub.wss.twitch.tv",443,0));
                Handle req(WinHttpOpenRequest(connection.value,L"GET",wide(path).c_str(),nullptr,WINHTTP_NO_REFERER,WINHTTP_DEFAULT_ACCEPT_TYPES,WINHTTP_FLAG_SECURE));
                DWORD redirect=WINHTTP_OPTION_REDIRECT_POLICY_NEVER;WinHttpSetOption(req.value,WINHTTP_OPTION_REDIRECT_POLICY,&redirect,sizeof(redirect));
                if(!WinHttpSetOption(req.value,WINHTTP_OPTION_UPGRADE_TO_WEB_SOCKET,nullptr,0)||!WinHttpSendRequest(req.value,WINHTTP_NO_ADDITIONAL_HEADERS,0,WINHTTP_NO_REQUEST_DATA,0,0,0)||!WinHttpReceiveResponse(req.value,nullptr))throw PortalError(502,"twitchNetworkError");
                auto ws=WinHttpWebSocketCompleteUpgrade(req.value,0);if(!ws)throw PortalError(502,"twitchNetworkError");
                {std::lock_guard lock(socket_mutex);if(stop.stop_requested()){WinHttpCloseHandle(ws);break;}active_socket=ws;}
                std::string frame;bool migrating=false,welcome=false;auto last=Clock::now();
                while(!stop.stop_requested()){
                    char buffer[8192];DWORD read=0;WINHTTP_WEB_SOCKET_BUFFER_TYPE type{};
                    const auto result=WinHttpWebSocketReceive(ws,buffer,sizeof(buffer),&read,&type);
                    if(result!=NO_ERROR||type==WINHTTP_WEB_SOCKET_CLOSE_BUFFER_TYPE)throw PortalError(502,"twitchNetworkError");
                    frame.append(buffer,read);if(frame.size()>512*1024||Clock::now()-last>std::chrono::seconds(20))throw PortalError(502,"twitchNetworkError");
                    if(type!=WINHTTP_WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE)continue;
                    auto message=Json::parse(frame);frame.clear();last=Clock::now();const auto kind=message.at("metadata").at("message_type").get<std::string>();
                    socket_deadline=std::chrono::duration_cast<std::chrono::seconds>(last.time_since_epoch()).count()+15;
                    if(kind=="session_welcome"){welcome=true;if(old.value){WinHttpCloseHandle(old.value);old.value=nullptr;}message["resumed"]=resume;push(message);}
                    else if(kind=="session_reconnect"){
                        const auto url=message.at("payload").at("session").at("reconnect_url").get<std::string>();
                        const std::string prefix="wss://eventsub.wss.twitch.tv/";
                        if(!url.starts_with(prefix)||url.size()>2048||url.find_first_of("\r\n")!=std::string::npos)throw PortalError(502,"twitchNetworkError");
                        path=url.substr(prefix.size()-1);resume=true;migrating=true;
                        {std::lock_guard lock(socket_mutex);if(active_socket==ws){old.value=ws;active_socket=nullptr;}}break;
                    }else if(kind=="notification"||kind=="revocation") {if(!welcome)throw PortalError(502,"twitchNetworkError");push(message);}
                }
                if(!migrating)break;
            }catch(...){if(!stop.stop_requested())push({{"disconnected",true}});break;}
            {std::lock_guard lock(socket_mutex);if(active_socket){WinHttpCloseHandle(active_socket);active_socket=nullptr;}}
        });
    }
    void store(){portal->save_twitch_credentials(credentials);accounts();}
    Json validate_account(const std::string& role,const std::string& client){
        auto& c=credentials.at(role);
        auto response=request(L"id.twitch.tv",L"/oauth2/validate",L"GET","","Authorization: OAuth "+c.at("access").get<std::string>()+"\r\n");
        if(response.status==401){
            auto refresh=request(L"id.twitch.tv",L"/oauth2/token",L"POST",form({{"client_id",client},{"grant_type","refresh_token"},{"refresh_token",c.at("refresh")}}));
            if(refresh.status!=200){if(refresh.status==400||refresh.status==401){credentials.erase(role);store();}throw PortalError(502,"twitchReconnectAccount");}
            c["access"]=refresh.body.at("access_token");c["refresh"]=refresh.body.at("refresh_token");store();
            response=request(L"id.twitch.tv",L"/oauth2/validate",L"GET","","Authorization: OAuth "+c.at("access").get<std::string>()+"\r\n");
        }
        if(response.status!=200)throw PortalError(502,"twitchReconnectAccount");
        const auto scopes=response.body.at("scopes");const auto has=[&](const std::string& s){return std::find(scopes.begin(),scopes.end(),s)!=scopes.end();};
        if(response.body.at("client_id")!=client||!has("user:write:chat")||(role=="streamer"&&(!has("user:read:chat")||!has("channel:read:redemptions"))))throw PortalError(400,"twitchScopesMissing");
        if(c.contains("id")&&c["id"]!=response.body.at("user_id"))throw PortalError(400,"twitchReconnectAccount");
        c["id"]=response.body.at("user_id");c["login"]=response.body.at("login");return response.body;
    }
    Reply helix(const wchar_t* path,const std::string& role,const Json& body,const std::string& client){
        return request(L"api.twitch.tv",path,L"POST",body.dump(),"Content-Type: application/json\r\nClient-Id: "+client+"\r\nAuthorization: Bearer "+credentials.at(role).at("access").get<std::string>()+"\r\n");
    }
    void subscribe(const Json& message,const std::string& client){
        if(message.value("resumed",false)){set("connection","twitchConnected");return;}
        const auto id=credentials.at("streamer").at("id"),session=message.at("payload").at("session").at("id");
        Json subscriptions=Json::object();
        for(const auto& type:{"channel.chat.message","channel.channel_points_custom_reward_redemption.add","channel.raid","stream.online","stream.offline"}){
            Json condition={{"broadcaster_user_id",id}};if(std::string(type)=="channel.chat.message")condition["user_id"]=id;if(std::string(type)=="channel.raid")condition={{"to_broadcaster_user_id",id}};
            auto r=helix(L"/helix/eventsub/subscriptions","streamer",{{"type",type},{"version","1"},{"condition",condition},{"transport",{{"method","websocket"},{"session_id",session}}}},client);
            subscriptions[type]=r.status==202?"twitchConnected":"twitchSubscriptionFailed";
            if(r.status==401)throw PortalError(502,"twitchReconnectAccount");
            set("subscriptions",subscriptions);
        }const bool partial=std::any_of(subscriptions.begin(),subscriptions.end(),[](const auto& value){return value!="twitchConnected";});set("connection",partial?"twitchPartial":"twitchConnected");set("error",partial?"twitchSubscriptionFailed":"");
    }
    void notify(const Json& message,const std::string&){
        const auto& e=message.at("payload").at("event");const auto type=message.at("payload").at("subscription").at("type").get<std::string>();
        Automation::Event event;event.id=message.at("metadata").at("message_id");
        if(type=="channel.chat.message"){
            const auto user=e.at("chatter_user_id");if(credentials.contains("bot")&&user==credentials["bot"]["id"])return;
            event.type="chat";event.user=e.at("chatter_user_name");event.message=e.at("message").at("text");
            for(const auto& badge:e.at("badges")){if(badge["set_id"]=="moderator")event.role="moderator";if(badge["set_id"]=="broadcaster"){event.role="broadcaster";break;}}
            if(std::find(sent_messages.begin(),sent_messages.end(),e.at("message_id").get<std::string>())!=sent_messages.end())return;
        }else if(type=="channel.channel_points_custom_reward_redemption.add"){event.type="reward";event.user=e.at("user_name");event.message=e.at("user_input");event.reward=e.at("reward").at("id");}
        else if(type=="channel.raid"){event.type="raid";event.user=e.at("from_broadcaster_user_name");event.viewers=e.at("viewers");}
        else if(type=="stream.online")event.type="online";else if(type=="stream.offline")event.type="offline";else return;
        Json outputs;int revision=0;unsigned epoch=0;
        // Serialize with settings saves: an old queued event must not reapply
        // overrides after the administrator has disabled the integration.
        {std::lock_guard lock(mutex);if(!settings["enabled"].get<bool>())return;revision=settings["revision"];epoch=chat_epoch;outputs=automation.dispatch(event,[&](const auto& id){return portal->scene(id);});}
        for(const auto& output:outputs)if(output["action"]=="chat"){
            std::lock_guard lock(mutex);if(!settings["enabled"].get<bool>()||settings["revision"]!=revision||chat_epoch!=epoch)return;
            if(outbound.size()>=20){state["error"]="twitchQueueFull";continue;}
            outbound.push_back({output,revision,epoch,Clock::now()+std::chrono::seconds(60)});
        }
    }
    void send_next(const std::string& client){
        if(outbound.empty()||Clock::now()<chat_after)return;
        auto entry=outbound.front();outbound.pop_front();
        {std::lock_guard lock(mutex);if(!settings["enabled"].get<bool>()||settings["revision"]!=entry.revision||chat_epoch!=entry.epoch||Clock::now()>=entry.expires)return;}
        if(!credentials.contains("streamer"))return;
        const auto& output=entry.output;
        chat_after=Clock::now()+std::chrono::seconds(2);
            const std::string sender=credentials.contains("bot")?"bot":"streamer";
            auto result=helix(L"/helix/chat/messages",sender,{{"broadcaster_id",credentials["streamer"]["id"]},{"sender_id",credentials[sender]["id"]},{"message",output["message"]}},client);
            if(result.status==401)throw PortalError(502,"twitchReconnectAccount");
            if(result.status!=200||!result.body.contains("data")||result.body["data"].empty()||!result.body["data"][0].value("is_sent",false)){
                set("error",result.status==401?"twitchReconnectAccount":"twitchChatFailed");if(result.status==429)chat_after=Clock::now()+std::chrono::seconds(30);
            }else{sent_messages.push_back(result.body["data"][0].at("message_id"));if(sent_messages.size()>128)sent_messages.pop_front();}
    }
    void handle_command(const Json& cmd,const std::string& client){
        const auto action=cmd.at("action").get<std::string>();
        if(action=="restart"){outbound.clear();stop_socket();retry={};validated={};return;}
        if(action=="rewards"){
            if(!credentials.contains("streamer"))throw PortalError(400,"twitchNeedsAccount");validate_account("streamer",client);store();
            const auto path=wide("/helix/channel_points/custom_rewards?broadcaster_id="+encode(credentials["streamer"]["id"]));
            auto r=request(L"api.twitch.tv",path.c_str(),L"GET","","Client-Id: "+client+"\r\nAuthorization: Bearer "+credentials["streamer"]["access"].get<std::string>()+"\r\n");
            if(r.status!=200)throw PortalError(502,"twitchSubscriptionFailed");Json rewards=Json::array();for(const auto& item:r.body.at("data")){if(rewards.size()>=100)break;rewards.push_back({{"id",item.at("id")},{"title",item.at("title")},{"cost",item.at("cost")}});}set("rewards",rewards);return;
        }
        const auto role=cmd.at("role").get<std::string>();
        if(action=="unlink"){
            outbound.clear();
            stop_socket();pending.erase(role);{std::lock_guard lock(mutex);state["pending"].erase(role);}
            if(role=="streamer"){set("rewards",Json::array());automation.reset();}
            // Forget locally even if Twitch cannot be reached. Revocation is best-effort.
            auto previous=credentials.value(role,Json::object());credentials.erase(role);store();validated={};retry={};
            if(previous.contains("access"))try{request(L"id.twitch.tv",L"/oauth2/revoke",L"POST",form({{"client_id",client},{"token",previous["access"]}}));}catch(...){}return;
        }
        const auto scopes=role=="streamer"?streamer_scopes:bot_scopes;
        auto r=request(L"id.twitch.tv",L"/oauth2/device",L"POST",form({{"client_id",client},{"scopes",scopes}}));
        if(r.status!=200)throw PortalError(400,"twitchClientInvalid");
        const auto code=r.body.at("user_code").get<std::string>();if(!std::regex_match(code,std::regex("[A-Za-z0-9-]{1,40}")))throw PortalError(502,"twitchNetworkError");
        auto now=std::chrono::duration_cast<std::chrono::seconds>(Clock::now().time_since_epoch()).count();
        pending[role]={{"device",r.body.at("device_code")},{"code",code},{"next",now+std::max(5,r.body.value("interval",5))},{"interval",std::max(5,r.body.value("interval",5))},{"expires",now+std::min(1800,r.body.value("expires_in",1800))}};
        std::lock_guard lock(mutex);state["pending"][role]={{"code",code},{"url","https://www.twitch.tv/activate?public=true&device-code="+code}};
    }
    void poll_authorization(const std::string& client){
        auto now=std::chrono::duration_cast<std::chrono::seconds>(Clock::now().time_since_epoch()).count();
        for(const auto* role:{"streamer","bot"}){
            if(!pending.contains(role))continue;auto& p=pending[role];if(now<p["next"].get<long long>())continue;
            if(now>=p["expires"].get<long long>()){pending.erase(role);std::lock_guard lock(mutex);state["pending"].erase(role);state["error"]="twitchAuthExpired";continue;}
            p["next"]=now+p["interval"].get<int>();
            auto r=request(L"id.twitch.tv",L"/oauth2/token",L"POST",form({{"client_id",client},{"device_code",p["device"]},{"scopes",role==std::string("streamer")?streamer_scopes:bot_scopes},{"grant_type","urn:ietf:params:oauth:grant-type:device_code"}}));
            if(r.status==200){
                stop_socket();credentials[role]={{"access",r.body.at("access_token")},{"refresh",r.body.at("refresh_token")}};
                try{validate_account(role,client);store();}catch(...){credentials.erase(role);store();pending.erase(role);{std::lock_guard lock(mutex);state["pending"].erase(role);}throw;}
                pending.erase(role);{std::lock_guard lock(mutex);state["pending"].erase(role);state["error"]="";}validated={};retry={};
            }else{const auto error=r.body.value("message",r.body.value("error",std::string{}));if(error=="slow_down")p["interval"]=p["interval"].get<int>()+5;else if(error!="authorization_pending"){pending.erase(role);std::lock_guard lock(mutex);state["pending"].erase(role);state["error"]="twitchAuthExpired";}}
        }
    }
    void run(std::stop_token stop){
        while(!stop.stop_requested()){
            Json config,cmd;std::deque<Json> incoming;{std::lock_guard lock(mutex);config=settings;if(!commands.empty()){cmd=commands.front();commands.pop_front();}incoming.swap(events);}
            try{
                const auto client=config["clientId"].get<std::string>();
                if(!cmd.is_null())try{handle_command(cmd,client);}catch(const PortalError& error){set("error",error.what());}
                if(!pending.empty())poll_authorization(client);
                if(!config["enabled"].get<bool>()){if(socket_open)stop_socket();set("connection","twitchDisabled");}
                else if(!credentials.contains("streamer")){set("connection","twitchNeedsAccount");}
                else if(Clock::now()>=retry){
                    if(socket_open&&!transport&&std::chrono::duration_cast<std::chrono::seconds>(Clock::now().time_since_epoch()).count()>socket_deadline.load())throw PortalError(502,"twitchNetworkError");
                    if(Clock::now()>=validated){
                        stop_socket();validate_account("streamer",client);if(credentials.contains("bot"))validate_account("bot",client);store();validated=Clock::now()+std::chrono::minutes(55);open_socket();incoming.clear();
                    }
                    for(const auto& event:incoming){
                        if(event.value("disconnected",false))throw PortalError(502,"twitchNetworkError");
                        const auto type=event.at("metadata").at("message_type").get<std::string>();
                        if(type=="session_welcome")subscribe(event,client);
                        else if(type=="notification")notify(event,client);
                        else if(type=="revocation"){
                            const auto sub=event.at("payload").at("subscription");const auto reason=sub.value("status",std::string{});
                            if(reason=="authorization_revoked"||reason=="user_removed"){credentials.erase("streamer");store();throw PortalError(502,"twitchReconnectAccount");}
                            const auto name=sub.at("type").get<std::string>();std::lock_guard lock(mutex);state["subscriptions"][name]="twitchSubscriptionFailed";state["error"]="twitchSubscriptionFailed";
                        }
                    }
                    send_next(client);
                }
            }catch(const std::exception& error){outbound.clear();stop_socket();const auto* p=dynamic_cast<const PortalError*>(&error);set("error",p?p->what():"twitchNetworkError");set("connection","twitchRetrying");validated={};retry=Clock::now()+std::chrono::seconds(15);}
            for(int i=0;i<10&&!stop.stop_requested();++i)std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }stop_socket();
    }
};
TwitchIntegration::TwitchIntegration(Portal* portal):impl_(std::make_unique<Impl>(portal)){}
TwitchIntegration::TwitchIntegration(Portal* portal,std::shared_ptr<TwitchTransport> transport):impl_(std::make_unique<Impl>(portal,std::move(transport))){}
TwitchIntegration::~TwitchIntegration()=default;
void TwitchIntegration::start(){if(impl_->portal&&!impl_->worker.joinable())impl_->worker=std::jthread([this](std::stop_token stop){impl_->run(stop);});}
Json TwitchIntegration::describe() const {std::lock_guard lock(impl_->mutex);auto result=impl_->state;result["settings"]=impl_->settings;result["log"]=impl_->automation.status();return result;}
Json TwitchIntegration::render(Json scene){return impl_->automation.apply(std::move(scene));}
bool TwitchIntegration::broadcast_access(const std::string& key,const std::string& path,const std::string& id){
    if(!impl_->portal||id.empty()||!impl_->portal->broadcast_access(key,"/api/scene",id))return false;
    auto scene=render(impl_->portal->scene(id));
    for(const auto& item:scene["items"])if(item["visible"].get<bool>()){
        const auto type=item["type"].get<std::string>();
        if(type=="image"&&path=="/api/media/"+item["options"].value("assetId",std::string{}))return true;
        if(path=="/api/audio/state"&&(type=="waveform"||type=="fx"||item["options"].value("audioEnabled",false)))return true;
        if(type=="master"&&(path=="/master-overlay"||path=="/api/master"||std::regex_match(path,std::regex("/api/master/covers/[1-9][0-9]{0,9}"))))return true;
        if(type=="deck"&&(path=="/overlay"||path=="/overlay.html"||path=="/api/state"||std::regex_match(path,std::regex("/api/decks/[1-4]/cover"))))return true;
        if(type=="waveform"&&path=="/waveform")return true;
    }return false;
}
Json TwitchIntegration::command(const Json& cmd){
    try{
        if(!impl_->portal||!cmd.is_object())throw PortalError(400,"twitchInvalid");const auto action=cmd.at("action").get<std::string>();
        if(action=="save"||action=="saveConnection"||action=="saveAutomations"){
            std::lock_guard lock(impl_->mutex);auto config=cmd.at("settings");
            if(action=="saveConnection"){
                if(!config.is_object()||config.size()!=2||!config.contains("clientId")||!config.contains("revision"))throw PortalError(400,"twitchInvalid");
                config["enabled"]=impl_->settings["enabled"];config["rules"]=impl_->settings["rules"];
            }else if(action=="saveAutomations"){
                if(!config.is_object()||config.size()!=3||!config.contains("rules")||!config.contains("enabled")||!config.contains("revision"))throw PortalError(400,"twitchInvalid");config["clientId"]=impl_->settings["clientId"];
            }
            if(config.size()!=4||!config.at("enabled").is_boolean()||!config.at("revision").is_number_integer())throw PortalError(400,"twitchInvalid");
            const auto client=config.at("clientId").get<std::string>();if((!client.empty()&&!std::regex_match(client,std::regex("[A-Za-z0-9]{10,100}")))||(config["enabled"].get<bool>()&&client.empty()))throw PortalError(400,"twitchClientInvalid");
            config["rules"]=Automation::validate(config.at("rules"));
            if(config["revision"]!=impl_->settings["revision"])throw PortalError(409,"twitchConflict");
            if(client!=impl_->settings["clientId"].get<std::string>()&&(!impl_->state["accounts"].empty()||!impl_->state["pending"].empty()||!impl_->commands.empty()))throw PortalError(409,"twitchUnlinkFirst");
            config["revision"]=config["revision"].get<int>()+1;impl_->portal->save_twitch_settings(config);impl_->settings=config;impl_->automation.configure(config["rules"]);
            if(impl_->commands.size()<16)impl_->commands.push_back({{"action","restart"}});
        }else if(action=="reset"){std::lock_guard lock(impl_->mutex);++impl_->chat_epoch;impl_->automation.reset();}
        else if(action=="test"){
            const auto e=cmd.at("event");if(e.dump().size()>4096)throw PortalError(400,"twitchInvalid");Automation::Event event;event.type=e.at("type");event.user=e.value("user",std::string("TestViewer"));event.message=e.value("message",std::string{});event.reward=e.value("reward",std::string{});event.role=e.value("role",std::string("everyone"));event.viewers=e.value("viewers",0);
            return {{"results",impl_->automation.dispatch(event,[&](const auto& id){return impl_->portal->scene(id);},Clock::now(),true)}};
        }else if(action=="authorize"||action=="unlink"||action=="rewards"){
            const auto role=action=="rewards"?std::string("streamer"):cmd.at("role").get<std::string>();if(role!="streamer"&&role!="bot")throw PortalError(400,"twitchInvalid");std::lock_guard lock(impl_->mutex);
            if(impl_->settings["clientId"]=="")throw PortalError(400,"twitchClientInvalid");if(impl_->commands.size()>=16)throw PortalError(429,"twitchQueueFull");impl_->commands.push_back({{"action",action},{"role",role}});
        }else throw PortalError(400,"twitchInvalid");return describe();
    }catch(const nlohmann::json::exception&){throw PortalError(400,"twitchInvalid");}
}
Json TwitchIntegration::automation_description() const {
    const auto all=describe();auto result=all;
    result.erase("accounts");result.erase("pending");result["settings"].erase("clientId");
    result["accountLinked"]=all["accounts"].contains("streamer");return result;
}
Json TwitchIntegration::viewer_status(const std::string& session){std::string client;{std::lock_guard lock(impl_->mutex);client=impl_->settings["clientId"];}return impl_->viewers->status(session,client);}
Json TwitchIntegration::viewer_identity(const std::string& session,bool validate){std::string client;{std::lock_guard lock(impl_->mutex);client=impl_->settings["clientId"];}return impl_->viewers->identity(session,client,validate);}
Json TwitchIntegration::viewer_command(const std::string& session,const std::string& peer,const Json& cmd){std::string client;{std::lock_guard lock(impl_->mutex);client=impl_->settings["clientId"];}try{return impl_->viewers->command(session,client,peer,cmd);}catch(const Json::exception&){throw PortalError(400,"twitchInvalid");}}
Json TwitchIntegration::automation_command(const Json& cmd){
    if(!cmd.is_object()||!cmd.contains("action")||!cmd["action"].is_string())throw PortalError(400,"twitchInvalid");
    const auto action=cmd["action"].get<std::string>();
    if(action!="save"&&action!="reset"&&action!="test"&&action!="rewards")throw PortalError(400,"twitchInvalid");
    auto translated=cmd;if(action=="save")translated["action"]="saveAutomations";
    auto result=command(translated);return action=="test"?result:automation_description();
}
}
