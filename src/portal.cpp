#include "portal.h"
#include <Windows.h>
#include <bcrypt.h>
#include <wincrypt.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <set>
#include <regex>

namespace deckstatus {
namespace {
using Json = nlohmann::json;
using Clock = std::chrono::steady_clock;
constexpr unsigned iterations = 600000;
std::string hex(const unsigned char* bytes, std::size_t size) {
    constexpr char digits[] = "0123456789abcdef"; std::string result;
    for (std::size_t i=0;i<size;++i) { result += digits[bytes[i] >> 4]; result += digits[bytes[i] & 15]; }
    return result;
}
bool token(const std::string& value) { return value.size()==64 && value.find_first_not_of("0123456789abcdef")==std::string::npos; }
std::string sha(const std::string& input) {
    std::array<unsigned char,32> bytes{};
    if (BCryptHash(BCRYPT_SHA256_ALG_HANDLE,nullptr,0,reinterpret_cast<PUCHAR>(const_cast<char*>(input.data())),static_cast<ULONG>(input.size()),bytes.data(),32)<0) throw PortalError(500,"portalCryptoFailed");
    return hex(bytes.data(),bytes.size());
}
std::string derive(const std::string& password,const std::string& salt) {
    std::array<unsigned char,32> bytes{};
    if (BCryptDeriveKeyPBKDF2(BCRYPT_HMAC_SHA256_ALG_HANDLE,reinterpret_cast<PUCHAR>(const_cast<char*>(password.data())),static_cast<ULONG>(password.size()),reinterpret_cast<PUCHAR>(const_cast<char*>(salt.data())),static_cast<ULONG>(salt.size()),iterations,bytes.data(),32,0)<0) throw PortalError(500,"portalCryptoFailed");
    return hex(bytes.data(),bytes.size());
}
std::string visitor_mac(const std::string& secret,const std::string& id) {
    std::array<unsigned char,32> bytes{};
    if(BCryptHash(BCRYPT_HMAC_SHA256_ALG_HANDLE,reinterpret_cast<PUCHAR>(const_cast<char*>(secret.data())),static_cast<ULONG>(secret.size()),reinterpret_cast<PUCHAR>(const_cast<char*>(id.data())),static_cast<ULONG>(id.size()),bytes.data(),32)<0)throw PortalError(500,"portalCryptoFailed");
    return hex(bytes.data(),bytes.size());
}
bool equal(const std::string& a,const std::string& b) {
    if(a.size()!=b.size())return false;
    unsigned difference=0;for(std::size_t i=0;i<a.size();++i)difference|=static_cast<unsigned char>(a[i])^static_cast<unsigned char>(b[i]);return difference==0;
}
std::string text(const Json& value,const char* key,std::size_t max=256) {
    if(!value.contains(key)||!value[key].is_string()||value[key].get_ref<const std::string&>().size()>max)throw PortalError(400,"portalInvalid");
    return value[key].get<std::string>();
}
std::string canonical(std::string value) {
    std::string out;bool space=false;
    for(unsigned char c:value) { if(c<=32) {space=!out.empty();continue;} if(space)out+=' ';space=false;out+=static_cast<char>(c>='A'&&c<='Z'?c+32:c); }
    return out;
}
std::string username(const Json& value) {
    auto name=canonical(text(value,"username",40));
    if(name.size()<3||name.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789._-")!=std::string::npos)throw PortalError(400,"authUsernameInvalid");return name;
}
void password_ok(const std::string& value) { if(value.size()<12||value.size()>256)throw PortalError(400,"authPasswordLength"); }
void set_password(Json& user,const std::string& password,bool force) {
    password_ok(password);user["salt"]=Portal::random_token();user["hash"]=derive(password,user["salt"]);user["mustChangePassword"]=force;
}
Json safe_user(const Json& user) { return {{"id",user["id"]},{"username",user["username"]},{"role",user["role"]},{"mustChangePassword",user["mustChangePassword"]}}; }
std::string protect(const std::string& input,bool encrypt) {
    std::string bytes=input;
    if(!encrypt) { if(input.size()%2)throw PortalError(500,"portalDataInvalid");bytes.clear();for(std::size_t i=0;i<input.size();i+=2)bytes+=static_cast<char>(std::stoul(input.substr(i,2),nullptr,16)); }
    DATA_BLOB source{static_cast<DWORD>(bytes.size()),reinterpret_cast<BYTE*>(bytes.data())},out{};
    const bool ok=encrypt?CryptProtectData(&source,L"DeckStatus bootstrap",nullptr,nullptr,nullptr,CRYPTPROTECT_UI_FORBIDDEN,&out):CryptUnprotectData(&source,nullptr,nullptr,nullptr,nullptr,CRYPTPROTECT_UI_FORBIDDEN,&out);
    if(!ok)throw PortalError(500,"portalBootstrapFailed");
    std::string result=encrypt?hex(out.pbData,out.cbData):std::string(reinterpret_cast<char*>(out.pbData),out.cbData);
    SecureZeroMemory(out.pbData,out.cbData);LocalFree(out.pbData);return result;
}
Json summary(const Json& record,const std::string& voter) {
    int sum=0;std::array<int,5> counts{};
    for(const auto& vote:record["votes"]) {const int n=vote.get<int>();sum+=n;++counts[n-1];}
    const auto count=record["votes"].size();
    return {{"count",count},{"average",count?Json(static_cast<double>(sum)/count):Json(nullptr)},{"distribution",counts},{"mine",record["votes"].value(sha(voter),0)}};
}
double number(const Json& value,const char* key,double low,double high) {
    if(!value.contains(key)||!value[key].is_number())throw PortalError(400,"sceneInvalid");
    const auto n=value[key].get<double>();if(!std::isfinite(n)||n<low||n>high)throw PortalError(400,"sceneInvalid");return n;
}
Json valid_preset(const Json& value) {
    const auto name=text(value,"name",80),kind=text(value,"type",12);
    if(canonical(name).empty()||(kind!="deck"&&kind!="master"&&kind!="waveform"))throw PortalError(400,"presetInvalid");
    if(!value.contains("options")||!value["options"].is_object()||value["options"].size()>50)throw PortalError(400,"presetInvalid");
    static const std::set<std::string> track={"deck","history","fields","duration","width","historyScale","align","lang","timeline","font","fontSize","coverSize","padding","gap","layout","badges","background","textColor","mutedColor","accent","opacity","radius","border","shadow"};
    static const std::set<std::string> wave={"width","height","lang","background","opacity","gap","mode","color","color2","gradient","gain","smoothing","gate","lineWidth","bars","rounding","glow","trails","channel","minHz","maxHz","historySeconds","fps","grid","centerLine","hideSilent"};
    static const std::set<std::string> fields={"title","artist","album","key","bpm","cover"};
    const auto& keys=kind=="waveform"?wave:track;
    for(auto it=value["options"].begin();it!=value["options"].end();++it) {
        if(!keys.contains(it.key())||it.value().dump().size()>256)throw PortalError(400,"presetInvalid");
        if(it.key()=="fields") {
            if(!it.value().is_array()||it.value().empty()||it.value().size()>6)throw PortalError(400,"presetInvalid");
            for(const auto& field:it.value())if(!field.is_string()||!fields.contains(field.get<std::string>()))throw PortalError(400,"presetInvalid");
        } else if(!it.value().is_string()&&!it.value().is_boolean()&&!(it.value().is_number()&&std::isfinite(it.value().get<double>())))throw PortalError(400,"presetInvalid");
    }
    return {{"name",name},{"type",kind},{"options",value["options"]}};
}
Json valid_scene(const Json& value) {
    auto name=text(value,"name",80);if(canonical(name).empty())throw PortalError(400,"sceneInvalid");
    const auto width=number(value,"width",320,7680),height=number(value,"height",180,4320);
    if(width!=std::floor(width)||height!=std::floor(height))throw PortalError(400,"sceneInvalid");
    auto background=text(value,"background",16);
    if(background!="transparent"&&!std::regex_match(background,std::regex("#[0-9a-fA-F]{6}")))throw PortalError(400,"sceneInvalid");
    if(!value.contains("items")||!value["items"].is_array()||value["items"].size()>32)throw PortalError(400,"sceneInvalid");
    Json items=Json::array();std::set<std::string> ids;
    for(const auto& item:value["items"]) {
        auto id=text(item,"id",64),kind=text(item,"type",12);
        if(!std::regex_match(id,std::regex("[a-zA-Z0-9_-]{1,64}"))||!ids.insert(id).second||(kind!="deck"&&kind!="master"&&kind!="waveform"))throw PortalError(400,"sceneInvalid");
        if(!item.contains("visible")||!item["visible"].is_boolean()||!item.contains("options")||!item["options"].is_object()||item["options"].size()>50)throw PortalError(400,"sceneInvalid");
        // Options are data, never URLs/HTML/scripts. Renderers normalize this allowlist.
        static const std::set<std::string> keys={"deck","history","fields","duration","width","height","historyScale","align","lang","timeline","font","fontSize","coverSize","padding","gap","layout","badges","background","textColor","mutedColor","accent","opacity","radius","border","shadow","mode","color","color2","gradient","gain","smoothing","gate","lineWidth","bars","rounding","glow","trails","channel","minHz","maxHz","historySeconds","fps","grid","centerLine","hideSilent"};
        for(auto it=item["options"].begin();it!=item["options"].end();++it)if(!keys.contains(it.key())||it.value().is_object()||it.value().dump().size()>256)throw PortalError(400,"sceneInvalid");
        items.push_back({{"id",id},{"type",kind},{"x",number(item,"x",-7680,7680)},{"y",number(item,"y",-4320,4320)},{"width",number(item,"width",32,7680)},{"height",number(item,"height",32,4320)},{"opacity",number(item,"opacity",0,1)},{"visible",item["visible"]},{"options",item["options"]}});
        if(item.contains("name"))items.back()["name"]=text(item,"name",80);
    }
    return {{"name",name},{"width",width},{"height",height},{"background",background},{"items",items}};
}
}
std::string Portal::random_token() {std::array<unsigned char,32> bytes{};if(BCryptGenRandom(nullptr,bytes.data(),32,BCRYPT_USE_SYSTEM_PREFERRED_RNG)<0)throw PortalError(500,"portalCryptoFailed");return hex(bytes.data(),32);}
Portal::Portal(const std::filesystem::path& directory) : file_(directory/"portal.json") {
    std::filesystem::create_directories(directory);
    const auto lock=directory/"portal.lock";
    lock_file_=CreateFileW(lock.c_str(),GENERIC_READ|GENERIC_WRITE,0,nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(lock_file_==INVALID_HANDLE_VALUE){lock_file_=nullptr;throw PortalError(500,"portalInUse");}
    try {
        if(std::filesystem::exists(file_)) {
            if(std::filesystem::file_size(file_)>64*1024*1024)throw PortalError(500,"portalDataInvalid");
            std::ifstream input(file_,std::ios::binary);data_=Json::parse(input,nullptr,false);input.close();
            if(!data_.is_object()||data_.value("version",0)!=1||!data_["users"].is_object()||data_["users"].empty()||!data_["scenes"].is_object()||!data_["ratings"].is_object()||!data_["overlayKeys"].is_object()||!token(data_.value("visitorSecret",std::string{})))throw PortalError(500,"portalDataInvalid");
            bool admin=false;
            for(auto it=data_["users"].begin();it!=data_["users"].end();++it) {const auto& u=it.value();if(u.at("id")!=it.key()||!token(text(u,"salt"))||!token(text(u,"hash"))||username(u)!=text(u,"username")||!u.at("mustChangePassword").is_boolean()||(u.at("role")!="admin"&&u.at("role")!="operator"))throw PortalError(500,"portalDataInvalid");admin|=u["role"]=="admin";}
            if(!admin)throw PortalError(500,"portalDataInvalid");
            for(const auto* kind:{"deck","master","waveform"})if(!token(data_["overlayKeys"].value(kind,std::string{})))throw PortalError(500,"portalDataInvalid");
            for(const auto& s:data_["scenes"]) {valid_scene(s);if(!token(s.value("key",std::string{}))||!s.at("revision").is_number_integer())throw PortalError(500,"portalDataInvalid");}
            for(const auto& r:data_["ratings"]) {if(!r.at("votes").is_object())throw PortalError(500,"portalDataInvalid");for(auto it=r["votes"].begin();it!=r["votes"].end();++it)if(!token(it.key())||!it.value().is_number_integer()||it.value()<1||it.value()>5)throw PortalError(500,"portalDataInvalid");}
            if(data_.contains("presets")) {
                if(!data_["presets"].is_object()||data_["presets"].size()>200)throw PortalError(500,"portalDataInvalid");
                for(auto it=data_["presets"].begin();it!=data_["presets"].end();++it) {
                    valid_preset(it.value());
                    if(!token(it.key())||it.value().at("id")!=it.key()||!it.value().at("revision").is_number_integer()||it.value()["revision"]<1)throw PortalError(500,"portalDataInvalid");
                }
            } else {
                // Upgrade existing portal stores without replacing accounts, ratings or scenes.
                auto next=data_;next["presets"]=Json::object();commit(next);
            }
        } else {
            const auto password=random_token().substr(0,24),id=random_token();
            Json user={{"id",id},{"username","admin"},{"role","admin"}};set_password(user,password,true);
            commit({{"version",1},{"users",{{id,user}}},{"scenes",Json::object()},{"presets",Json::object()},{"ratings",Json::object()},{"overlayKeys",{{"deck",random_token()},{"master",random_token()},{"waveform",random_token()}}},{"visitorSecret",random_token()},{"bootstrap",protect(password,true)}});
        }
    } catch(...) {CloseHandle(lock_file_);lock_file_=nullptr;throw;}
}
Portal::~Portal(){if(lock_file_)CloseHandle(lock_file_);}
void Portal::commit(const Json& next) {
    const auto content=next.dump(2,' ',false,Json::error_handler_t::replace);
    if(content.size()>64*1024*1024)throw PortalError(507,"portalCapacity");
    auto temp=file_;temp+=L".tmp-"+std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(GetTickCount64());
    HANDLE out=CreateFileW(temp.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(out==INVALID_HANDLE_VALUE)throw PortalError(500,"portalSaveFailed");
    DWORD written=0;const bool ok=WriteFile(out,content.data(),static_cast<DWORD>(content.size()),&written,nullptr)&&written==content.size()&&FlushFileBuffers(out);CloseHandle(out);
    if(!ok||!MoveFileExW(temp.c_str(),file_.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)){DeleteFileW(temp.c_str());throw PortalError(500,"portalSaveFailed");}
    data_=next;
}
std::string Portal::initial_password() const {std::lock_guard lock(mutex_);const auto encrypted=data_.value("bootstrap",std::string{});return encrypted.empty()?"":protect(encrypted,false);}
void Portal::throttle(const std::string& key,int limit) {
    const auto now=Clock::now();for(auto it=attempts_.begin();it!=attempts_.end();)if(it->second.until<=now)it=attempts_.erase(it);else ++it;
    if(attempts_.size()>=4096&&!attempts_.contains(key))throw PortalError(429,"portalRateLimit");
    auto& entry=attempts_[key];if(entry.count==0)entry.until=now+std::chrono::minutes(1);if(++entry.count>limit)throw PortalError(429,"portalRateLimit");
}
Json Portal::login(const std::string& name,const std::string& password,const std::string& peer) {
    std::lock_guard lock(mutex_);throttle("login-global",60);throttle("login-peer:"+peer,10);throttle("login-user:"+canonical(name),10);
    if(name.size()>40||password.size()>256)throw PortalError(401,"authInvalidLogin");
    Json found=nullptr;for(const auto& user:data_["users"])if(user["username"]==canonical(name))found=user;
    const auto candidate=derive(password,found.is_null()?std::string(64,'0'):found["salt"].get<std::string>());
    if(found.is_null()||!equal(candidate,found["hash"]))throw PortalError(401,"authInvalidLogin");
    for(auto it=sessions_.begin();it!=sessions_.end();)if(it->second.expires<=Clock::now())it=sessions_.erase(it);else ++it;
    if(sessions_.size()>=1024)throw PortalError(429,"portalRateLimit");
    const auto session=random_token();sessions_[sha(session)]={found["id"],Clock::now()+std::chrono::hours(12)};
    return {{"session",session},{"user",safe_user(found)}};
}
Json Portal::user_identity(const std::string& session) {
    const auto it=sessions_.find(sha(session));if(it==sessions_.end())return nullptr;
    if(it->second.expires<=Clock::now()||!data_["users"].contains(it->second.user)){sessions_.erase(it);return nullptr;}
    return safe_user(data_["users"][it->second.user]);
}
Json Portal::identity(const std::string& session){std::lock_guard lock(mutex_);return user_identity(session);}
void Portal::logout(const std::string& session){std::lock_guard lock(mutex_);sessions_.erase(sha(session));}
void Portal::invalidate(const std::string& user){for(auto it=sessions_.begin();it!=sessions_.end();)if(it->second.user==user)it=sessions_.erase(it);else ++it;}
void Portal::change_password(const std::string& session,const Json& command) {
    std::lock_guard lock(mutex_);const auto identity=user_identity(session);if(identity.is_null())throw PortalError(401,"authRequired");
    const auto id=identity["id"].get<std::string>();throttle("password:"+id,8);
    const auto previous=text(command,"currentPassword"),password=text(command,"password");auto next=data_;auto& user=next["users"][id];
    if(!equal(derive(previous,user["salt"]),user["hash"]))throw PortalError(400,"authCurrentIncorrect");
    if(previous==password)throw PortalError(400,"authPasswordDifferent");set_password(user,password,false);next["bootstrap"]="";commit(next);invalidate(id);
}
Json Portal::users() const {std::lock_guard lock(mutex_);Json out=Json::array();for(const auto& user:data_["users"])out.push_back(safe_user(user));return out;}
Json Portal::edit_user(const std::string& actor,const Json& command) {
    std::lock_guard lock(mutex_);auto next=data_;const auto action=text(command,"action",16),id=command.value("id",std::string{});
    if(action=="delete") {if(!next["users"].contains(id))throw PortalError(404,"portalNotFound");if(id==actor)throw PortalError(400,"authDeleteSelf");next["users"].erase(id);}
    else if(action=="save") {
        const auto name=username(command),role=text(command,"role",16);if(role!="admin"&&role!="operator")throw PortalError(400,"portalInvalid");
        if(!id.empty()&&!next["users"].contains(id))throw PortalError(404,"portalNotFound");
        if(id.empty()&&next["users"].size()>=100)throw PortalError(400,"portalCapacity");
        for(const auto& u:next["users"])if(u["username"]==name&&u["id"]!=id)throw PortalError(409,"authUsernameTaken");
        const auto target=id.empty()?random_token():id;Json user=id.empty()?Json{{"id",target}}:next["users"][id];
        user["username"]=name;user["role"]=role;const auto password=command.value("password",std::string{});
        if(id.empty()||!password.empty())set_password(user,password,true);next["users"][target]=user;
        if(user["username"]=="admin"&&!password.empty())next["bootstrap"]="";
    } else throw PortalError(400,"portalInvalid");
    bool admin=false;for(const auto& u:next["users"])admin|=u["role"]=="admin";if(!admin)throw PortalError(400,"authLastAdmin");
    commit(next);if(!id.empty())invalidate(id);Json out=Json::array();for(const auto& u:data_["users"])out.push_back(safe_user(u));return out;
}
Json Portal::presets() const {std::lock_guard lock(mutex_);Json out=Json::array();for(const auto& p:data_["presets"])out.push_back(p);return out;}
Json Portal::edit_preset(const Json& command) {
    std::lock_guard lock(mutex_);const auto action=text(command,"action",16),id=command.value("id",std::string{});auto next=data_;
    if(!id.empty()&&!next["presets"].contains(id))throw PortalError(404,"portalNotFound");
    if(!id.empty()&&command.value("revision",0)!=next["presets"][id]["revision"])throw PortalError(409,"presetConflict");
    if(action=="delete") {if(id.empty())throw PortalError(400,"presetInvalid");next["presets"].erase(id);commit(next);return {{"deleted",true}};}
    if(action!="save")throw PortalError(400,"presetInvalid");if(id.empty()&&next["presets"].size()>=200)throw PortalError(400,"portalCapacity");
    auto p=valid_preset(command.at("preset"));
    if(!id.empty()&&p["type"]!=next["presets"][id]["type"])throw PortalError(400,"presetInvalid");
    const auto target=id.empty()?random_token():id;p["id"]=target;p["revision"]=id.empty()?1:next["presets"][id]["revision"].get<int>()+1;
    next["presets"][target]=p;commit(next);return p;
}
Json Portal::scenes() const {std::lock_guard lock(mutex_);Json out=Json::array();for(const auto& s:data_["scenes"])out.push_back(s);return out;}
Json Portal::scene(const std::string& id,bool include_secret) const {std::lock_guard lock(mutex_);if(!data_["scenes"].contains(id))throw PortalError(404,"portalNotFound");auto result=data_["scenes"][id];if(!include_secret)result.erase("key");return result;}
Json Portal::edit_scene(const Json& command) {
    std::lock_guard lock(mutex_);const auto action=text(command,"action",16),id=command.value("id",std::string{});auto next=data_;
    if(!id.empty()&&!next["scenes"].contains(id))throw PortalError(404,"portalNotFound");
    if(!id.empty()&&command.value("revision",0)!=next["scenes"][id]["revision"])throw PortalError(409,"sceneConflict");
    if(action=="delete") {if(id.empty())throw PortalError(400,"sceneInvalid");next["scenes"].erase(id);commit(next);return {{"deleted",true}};}
    if(action=="rotate") {if(id.empty())throw PortalError(400,"sceneInvalid");auto& s=next["scenes"][id];s["key"]=random_token();s["revision"]=s["revision"].get<int>()+1;commit(next);return s;}
    if(action!="save")throw PortalError(400,"sceneInvalid");if(id.empty()&&next["scenes"].size()>=100)throw PortalError(400,"portalCapacity");
    auto s=valid_scene(command.at("scene"));const auto target=id.empty()?random_token():id;s["id"]=target;s["revision"]=id.empty()?1:next["scenes"][id]["revision"].get<int>()+1;s["key"]=id.empty()?random_token():next["scenes"][id]["key"].get<std::string>();
    next["scenes"][target]=s;commit(next);return s;
}
bool Portal::broadcast_access(const std::string& key,const std::string& path,const std::string& scene_id) const {
    if(!token(key))return false;std::lock_guard lock(mutex_);std::set<std::string> kinds;
    if(!scene_id.empty()) {
        if(!data_["scenes"].contains(scene_id)||!equal(data_["scenes"][scene_id]["key"],key))return false;
        if(path=="/scene"||path=="/api/scene")return true;
        for(const auto& item:data_["scenes"][scene_id]["items"])if(item["visible"].get<bool>())kinds.insert(item["type"]);
    } else for(auto it=data_["overlayKeys"].begin();it!=data_["overlayKeys"].end();++it)if(equal(it.value(),key))kinds.insert(it.key());
    return (kinds.contains("deck")&&(path=="/overlay"||path=="/overlay.html"||path=="/api/state"||std::regex_match(path,std::regex("/api/decks/[1-4]/cover"))))||
        (kinds.contains("master")&&(path=="/master-overlay"||path=="/api/master"||std::regex_match(path,std::regex("/api/master/covers/[1-9][0-9]{0,9}"))))||
        (kinds.contains("waveform")&&(path=="/waveform"||path=="/api/audio/state"));
}
Json Portal::overlay_keys(bool rotate){std::lock_guard lock(mutex_);if(rotate){auto next=data_;for(auto& key:next["overlayKeys"])key=random_token();commit(next);}return data_["overlayKeys"];}
std::string Portal::visitor(const std::string& cookie) const {
    std::lock_guard lock(mutex_);if(cookie.size()==129&&cookie[64]=='.'&&token(cookie.substr(0,64))&&equal(cookie.substr(65),visitor_mac(data_["visitorSecret"],cookie.substr(0,64))))return cookie;
    const auto id=random_token();return id+'.'+visitor_mac(data_["visitorSecret"],id);
}
Json Portal::public_history(Json history,const std::string& voter) {
    std::lock_guard lock(mutex_);
    for(auto& entry:history["entries"]) {
        Json metadata=Json::object();for(const auto* name:{"title","artist","album"})metadata[name]=entry.contains(name)&&entry[name].is_string()?entry[name]:Json("");
        const auto title=canonical(metadata["title"]);if(title.empty()){entry["rating"]=nullptr;continue;}
        const auto id=sha(Json::array({title,canonical(metadata["artist"]),canonical(metadata["album"])}).dump());
        catalog_[id]=metadata;entry["ratingId"]=id;entry["rating"]=data_["ratings"].contains(id)?summary(data_["ratings"][id],voter):Json{{"count",0},{"average",nullptr},{"mine",0}};
    }
    return history;
}
Json Portal::rate(const std::string& voter,const std::string& peer,const Json& command) {
    std::lock_guard lock(mutex_);throttle("rating-peer:"+peer,120);throttle("rating:"+sha(voter),30);
    const auto id=text(command,"track",64);if(!catalog_.contains(id))throw PortalError(404,"ratingUnknown");
    if(!command.contains("stars")||!command["stars"].is_number_integer()||command["stars"]<1||command["stars"]>5)throw PortalError(400,"ratingInvalid");
    auto next=data_;auto& record=next["ratings"][id];if(record.is_null()){record=catalog_[id];record["id"]=id;record["votes"]=Json::object();}
    record["votes"][sha(voter)]=command["stars"];commit(next);return summary(record,voter);
}
Json Portal::ratings() const {std::lock_guard lock(mutex_);Json out=Json::array();for(const auto& record:data_["ratings"]){auto row=record;row.erase("votes");row.update(summary(record,""));row.erase("mine");out.push_back(row);}return out;}
}
