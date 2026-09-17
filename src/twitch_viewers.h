#pragma once
#include "twitch.h"
#include <algorithm>
#include <regex>

namespace deckstatus {
// Viewer OAuth is separate from streamer/bot permissions. Tokens live only in memory.
class TwitchViewers {
    using Json=nlohmann::json;
    using Clock=std::chrono::steady_clock;
    using Request=std::function<TwitchTransport::Response(const std::wstring&,const std::string&,const std::string&)>;
    struct Session {std::string client,device,code,access;Json user=nullptr;Clock::time_point expires,next;int interval=5;};
    struct Limit {int count=0;Clock::time_point until;};
    std::mutex mutex_;
    std::map<std::string,Session> sessions_;
    std::map<std::string,Limit> limits_;
    Request request_;
    static std::string encode(const std::string& s){std::string out;const char* hex="0123456789ABCDEF";for(unsigned char c:s)if(isalnum(c)||c=='-'||c=='_'||c=='.')out+=c;else{out+='%';out+=hex[c>>4];out+=hex[c&15];}return out;}
    static Json verified(const Json& body,const std::string& client){
        if(body.value("client_id",std::string{})!=client||!body.contains("user_id")||!body["user_id"].is_string()||!body.contains("login")||!body["login"].is_string()||!std::regex_match(body["user_id"].get<std::string>(),std::regex("[0-9]{1,32}"))||!std::regex_match(body["login"].get<std::string>(),std::regex("[a-zA-Z0-9_]{1,100}"))||body.value("expires_in",0)<=0)throw PortalError(401,"viewerExpired");
        return {{"id",body["user_id"]},{"login",body["login"]}};
    }
    void clean(const std::string& client){auto now=Clock::now();for(auto it=sessions_.begin();it!=sessions_.end();)if(it->second.expires<=now||it->second.client!=client)it=sessions_.erase(it);else ++it;for(auto it=limits_.begin();it!=limits_.end();)if(it->second.until<=now)it=limits_.erase(it);else ++it;}
    void limit(const std::string& key,int max){auto& l=limits_[key];if(l.until<=Clock::now())l={0,Clock::now()+std::chrono::minutes(1)};if(++l.count>max)throw PortalError(429,"viewerRateLimit");}
    Json description(const Session* s,const std::string& client){Json out={{"available",!client.empty()},{"user",s?s->user:Json(nullptr)},{"pending",nullptr}};if(s&&!s->device.empty())out["pending"]={{"code",s->code},{"url","https://www.twitch.tv/activate?public=true&device-code="+s->code}};return out;}
public:
    explicit TwitchViewers(Request request):request_(std::move(request)){}
    Json status(const std::string& cookie,const std::string& client){std::lock_guard lock(mutex_);clean(client);auto it=sessions_.find(cookie);return description(it==sessions_.end()?nullptr:&it->second,client);}
    Json identity(const std::string& cookie,const std::string& client,bool validate=false){
        std::lock_guard lock(mutex_);clean(client);auto it=sessions_.find(cookie);if(it==sessions_.end()||it->second.user.is_null()){if(validate)throw PortalError(401,"viewerRequired");return nullptr;}
        auto& s=it->second;
        if(validate){if(Clock::now()<s.next)throw PortalError(429,"viewerRateLimit");s.next=Clock::now()+std::chrono::seconds(1);
            const auto result=request_(L"/oauth2/validate","","Authorization: OAuth "+s.access+"\r\n");
            if(result.status==401){sessions_.erase(it);throw PortalError(401,"viewerExpired");}if(result.status!=200)throw PortalError(502,"twitchNetworkError");
            try{auto user=verified(result.body,client);if(user["id"]!=s.user["id"])throw PortalError(401,"viewerExpired");s.user=user;}catch(...){sessions_.erase(it);throw;}
        }return s.user;
    }
    Json command(const std::string& cookie,const std::string& client,const std::string& peer,const Json& cmd){
        std::lock_guard lock(mutex_);clean(client);const auto action=cmd.value("action",std::string{});
        if(action=="logout"){sessions_.erase(cookie);return description(nullptr,client);}
        if(client.empty())throw PortalError(409,"viewerUnavailable");
        if(action=="start"){
            limit("global",60);limit("peer:"+peer,10);size_t pending=0;for(const auto& [id,s]:sessions_)if(!s.device.empty())++pending;
            if(sessions_.size()>=1024||pending>=64)throw PortalError(429,"viewerRateLimit");
            auto reply=request_(L"/oauth2/device","client_id="+encode(client)+"&scopes=","");if(reply.status!=200)throw PortalError(502,"twitchClientInvalid");
            Session s;s.client=client;s.device=reply.body.at("device_code");s.code=reply.body.at("user_code");
            if(s.device.empty()||s.device.size()>2048||!std::regex_match(s.code,std::regex("[A-Za-z0-9-]{1,40}")))throw PortalError(502,"twitchNetworkError");
            s.interval=std::clamp(reply.body.value("interval",5),5,60);s.next=Clock::now()+std::chrono::seconds(s.interval);s.expires=Clock::now()+std::chrono::seconds(std::clamp(reply.body.value("expires_in",300),1,1800));
            const auto token=Portal::random_token();sessions_.erase(cookie);sessions_[token]=s;auto result=description(&s,client);result["session"]=token;return result;
        }
        if(action!="poll")throw PortalError(400,"twitchInvalid");auto it=sessions_.find(cookie);if(it==sessions_.end())throw PortalError(401,"viewerExpired");auto& s=it->second;
        if(s.device.empty()||Clock::now()<s.next)return description(&s,client);s.next=Clock::now()+std::chrono::seconds(s.interval);
        auto reply=request_(L"/oauth2/token","client_id="+encode(client)+"&scopes=&device_code="+encode(s.device)+"&grant_type=urn%3Aietf%3Aparams%3Aoauth%3Agrant-type%3Adevice_code","");
        const auto error=reply.body.value("message",reply.body.value("error",std::string{}));
        if(reply.status!=200){if(error=="authorization_pending")return description(&s,client);if(error=="slow_down"){s.interval=std::min(60,s.interval+5);s.next=Clock::now()+std::chrono::seconds(s.interval);return description(&s,client);}if(reply.status>=500)throw PortalError(502,"twitchNetworkError");sessions_.erase(it);throw PortalError(401,"viewerExpired");}
        const auto access=reply.body.at("access_token").get<std::string>();if(!std::regex_match(access,std::regex("[A-Za-z0-9_-]{1,2048}")))throw PortalError(502,"twitchNetworkError");
        auto validation=request_(L"/oauth2/validate","","Authorization: OAuth "+access+"\r\n");
        if(validation.status!=200){sessions_.erase(it);throw PortalError(401,"viewerExpired");}
        try{s.user=verified(validation.body,client);}catch(...){sessions_.erase(it);throw;}
        s.access=access;s.device.clear();s.code.clear();s.expires=Clock::now()+std::chrono::seconds(std::min(3600,validation.body.value("expires_in",0)));s.next={};return description(&s,client);
    }
};
}
