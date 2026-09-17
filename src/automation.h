#pragma once
#include "portal.h"
#include <algorithm>
#include <cmath>
#include <deque>
#include <set>
#include <functional>

namespace deckstatus {
// A server-owned, bounded runtime layer. Saved scene designs are never rewritten.
class Automation {
public:
    using Json = nlohmann::json;
    using Clock = std::chrono::steady_clock;
    using Time = Clock::time_point;
    struct Event { std::string id,type,user,message,reward,role="everyone"; int viewers=0; };
private:
    struct Override { Json value; Time until; int revision; };
    mutable std::mutex mutex_;
    Json rules_=Json::array(), log_=Json::array();
    std::map<std::string,Time> cooldowns_;
    std::map<std::string,std::map<std::string,Override>> overrides_;
    std::deque<std::pair<std::string,Time>> seen_;
    static std::string lower(std::string s) { for(auto& c:s)if(c>='A'&&c<='Z')c+=32;return s; }
    static std::string str(const Json& j,const char* k,size_t max,bool empty=true) {
        if(!j.contains(k)||!j[k].is_string())throw PortalError(400,"twitchInvalid");
        auto s=j[k].get<std::string>();if(s.size()>max||(!empty&&s.empty())||s.find('\0')!=std::string::npos)throw PortalError(400,"twitchInvalid");return s;
    }
    static double num(const Json& j,const char* k,double lo,double hi) {if(!j.contains(k)||!j[k].is_number())throw PortalError(400,"twitchInvalid");double n=j[k].get<double>();if(!std::isfinite(n)||n<lo||n>hi)throw PortalError(400,"twitchInvalid");return n;}
    void record(const std::string& rule,const std::string& result) {log_.push_back({{"rule",rule},{"result",result},{"time",std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count()}});if(log_.size()>50)log_.erase(log_.begin());}
    static bool matches(const Json& r,const Event& e) {
        const auto trigger=r["trigger"].get<std::string>(),filter=lower(r["match"]);
        if(trigger=="command"||trigger=="contains"||trigger=="exact"){
            if(e.type!="chat")return false;
            if(r["role"]=="moderator"&&e.role!="moderator"&&e.role!="broadcaster")return false;
            if(r["role"]=="broadcaster"&&e.role!="broadcaster")return false;
            const auto msg=lower(e.message);
            if(trigger=="contains")return msg.find(filter)!=std::string::npos;
            if(trigger=="exact")return msg==filter;
            return msg==filter||(msg.starts_with(filter)&&msg.size()>filter.size()&&msg[filter.size()]==' ');
        }
        if(trigger!=e.type)return false;
        if(trigger=="reward")return filter.empty()||filter==lower(e.reward);
        return trigger!="raid"||e.viewers>=r["minimum"].get<int>();
    }
public:
    static Json validate(const Json& rules) {
        if(!rules.is_array()||rules.size()>64)throw PortalError(400,"twitchInvalid");std::set<std::string> ids;Json normalized=Json::array();
        const std::set<std::string> triggers={"reward","command","contains","exact","raid","online","offline"};
        const std::set<std::string> actions={"show","hide","toggle","audioOn","audioOff","text","opacity","x","y","rotation","chat"};
        for(auto r:rules){
            if(!r.is_object())throw PortalError(400,"twitchInvalid");
            // Preview stores used one action on the rule itself. Preserve its
            // exact target, value and timer when upgrading to the action list.
            if(!r.contains("actions")&&r.size()==12){Json a;for(const auto* key:{"action","target","value","duration"}){a[key]=r.at(key);r.erase(key);}r["actions"]=Json::array({a});}
            if(r.size()!=9||!ids.insert(str(r,"id",64,false)).second||str(r,"name",80,false).empty()||!r.at("enabled").is_boolean())throw PortalError(400,"twitchInvalid");
            const auto trigger=str(r,"trigger",16),filter=str(r,"match",200),role=str(r,"role",16);
            if(!triggers.contains(trigger)||(role!="everyone"&&role!="moderator"&&role!="broadcaster"))throw PortalError(400,"twitchInvalid");
            if((trigger=="command"||trigger=="contains"||trigger=="exact")&&filter.empty())throw PortalError(400,"twitchInvalid");
            if(trigger=="command"&&(filter[0]!='!'||filter.find(' ')!=std::string::npos))throw PortalError(400,"twitchInvalid");
            num(r,"cooldown",0,86400);num(r,"minimum",0,1000000);
            if(!r.contains("actions")||!r["actions"].is_array()||r["actions"].empty()||r["actions"].size()>16)throw PortalError(400,"twitchInvalid");
            for(const auto& a:r["actions"]){
                if(!a.is_object()||a.size()!=4)throw PortalError(400,"twitchInvalid");const auto action=str(a,"action",16);
                if(!actions.contains(action))throw PortalError(400,"twitchInvalid");num(a,"duration",0,86400);
                if(!a.contains("target")||!a["target"].is_object()||a["target"].size()!=2)throw PortalError(400,"twitchInvalid");
                str(a["target"],"scene",64,action=="chat");str(a["target"],"item",64,action=="chat");
                if(action=="text"||action=="chat")str(a,"value",action=="chat"?450:2000,false);
                else if(action=="opacity")num(a,"value",0,1);
                else if(action=="x")num(a,"value",-7680,7680);
                else if(action=="y")num(a,"value",-4320,4320);
                else if(action=="rotation")num(a,"value",-360,360);
                else if(!a.contains("value")||!a["value"].is_null())throw PortalError(400,"twitchInvalid");
            }normalized.push_back(std::move(r));
        }return normalized;
    }
    static std::string expand(const std::string& input,const Event& e,size_t max=2000) {
        const std::map<std::string,std::string> values={{"user",e.user},{"message",e.message},{"reward",e.reward},{"viewers",std::to_string(e.viewers)}};
        std::string out;for(size_t i=0;i<input.size()&&out.size()<=max;){bool found=false;if(input[i]=='{')for(const auto& [key,value]:values){auto token="{"+key+"}";if(input.compare(i,token.size(),token)==0){out+=value;i+=token.size();found=true;break;}}if(!found)out+=input[i++];}
        if(out.size()>max){size_t end=max;while(end>0&&(static_cast<unsigned char>(out[end])&0xc0)==0x80)--end;out.resize(end);}return out;
    }
    void configure(const Json& rules){auto normalized=validate(rules);std::lock_guard lock(mutex_);rules_=std::move(normalized);cooldowns_.clear();overrides_.clear();}
    void reset(){std::lock_guard lock(mutex_);overrides_.clear();cooldowns_.clear();record("","twitchResetDone");}
    Json status() const {std::lock_guard lock(mutex_);return log_;}
    Json apply(Json scene,Time now=Clock::now()) {
        std::lock_guard lock(mutex_);for(auto& item:scene["items"]){auto key=scene["id"].get<std::string>()+":"+item["id"].get<std::string>();auto found=overrides_.find(key);if(found==overrides_.end())continue;
            for(auto it=found->second.begin();it!=found->second.end();){if(it->second.until<=now||it->second.revision!=scene["revision"].get<int>()){it=found->second.erase(it);continue;}if(it->first=="text"||it->first=="audioEnabled")item["options"][it->first]=it->second.value;else item[it->first]=it->second.value;++it;}
            if(found->second.empty())overrides_.erase(found);
        }return scene;
    }
    // Returns outbound messages to the integration; never sends while holding a lock.
    Json dispatch(const Event& event,const std::function<Json(const std::string&)>& scene,Time now=Clock::now(),bool dry=false) {
        std::lock_guard lock(mutex_);Json result=Json::array();
        auto preview=dry?overrides_:decltype(overrides_){};auto& active=dry?preview:overrides_;
        if(!dry){while(!seen_.empty()&&(seen_.front().second+std::chrono::minutes(10)<now||seen_.size()>=2048))seen_.pop_front();for(const auto& s:seen_)if(s.first==event.id)return result;if(!event.id.empty())seen_.push_back({event.id,now});}
        for(const auto& r:rules_){if(!r["enabled"].get<bool>()||!matches(r,event))continue;auto id=r["id"].get<std::string>();if(!dry&&cooldowns_.contains(id)&&cooldowns_[id]>now)continue;
            bool applied=false;int index=0;
            for(const auto& a:r["actions"]){
            const auto action=a["action"].get<std::string>();Json output={{"rule",r["name"]},{"action",action},{"index",++index}};
            try {
                if(action=="chat"){output["message"]=expand(a["value"],event,450);}
                else{const auto target=a["target"];const auto sid=target["scene"].get<std::string>(),iid=target["item"].get<std::string>();auto design=scene(sid);auto it=std::find_if(design["items"].begin(),design["items"].end(),[&](const auto& item){return item["id"]==iid;});if(it==design["items"].end()||(action=="text"&&(*it)["type"]!="text"))throw PortalError(404,"twitchTargetMissing");
                    const auto key=sid+":"+iid,property=action=="show"||action=="hide"||action=="toggle"?"visible":action=="audioOn"||action=="audioOff"?"audioEnabled":action;Json value=a["value"];
                    if(action=="audioOn"||action=="audioOff")value=action=="audioOn";
                    if(action=="text")value=expand(value,event);
                    if(action=="show"||action=="hide")value=action=="show";
                    if(action=="toggle"){bool old=(*it)["visible"];auto row=active.find(key);if(row!=active.end()){auto v=row->second.find(property);if(v!=row->second.end()&&v->second.until>now&&v->second.revision==design["revision"].get<int>())old=v->second.value;}value=!old;}
                    output["target"]=target;output["value"]=value;output["duration"]=a["duration"];
                    double seconds=a["duration"];active[key][property]={value,seconds>0?now+std::chrono::milliseconds(static_cast<long long>(seconds*1000)):Time::max(),design["revision"]};
                }
                result.push_back(output);applied=true;if(!dry)record(r["name"],action=="chat"?"twitchChatRequested":"twitchActionApplied");
            }catch(const PortalError&){if(dry){output["error"]="twitchTargetMissing";result.push_back(output);}else record(r["name"],"twitchTargetMissing");}
            }
            if(!dry&&applied)cooldowns_[id]=now+std::chrono::milliseconds(static_cast<long long>(r["cooldown"].get<double>()*1000));
        }return result;
    }
};
}
