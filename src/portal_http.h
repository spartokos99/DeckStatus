#pragma once
#include "portal.h"
#include <httplib/httplib.h>

namespace deckstatus {
// Gate every registered handler after the request body has been consumed.
class PortalServer : public httplib::Server {
public:
    std::function<bool(const httplib::Request&,httplib::Response&)> authorize;
    Handler guarded(Handler handler) { return [this,handler](const auto& request,auto& response) {
        try { if(!authorize||authorize(request,response))handler(request,response); }
        catch(const PortalError& error) { response.status=error.status;if(error.status==429)response.set_header("Retry-After","60");response.set_content(nlohmann::json{{"error",error.what()}}.dump(),"application/json; charset=utf-8"); }
        catch(const nlohmann::json::exception&) {response.status=400;response.set_content("{\"error\":\"portalInvalid\"}","application/json; charset=utf-8");}
    }; }
    void Get(const std::string& path,Handler handler){httplib::Server::Get(path,guarded(std::move(handler)));}
    void Post(const std::string& path,Handler handler){httplib::Server::Post(path,guarded(std::move(handler)));}
    void Put(const std::string& path,Handler handler){httplib::Server::Put(path,guarded(std::move(handler)));}
    void Patch(const std::string& path,Handler handler){httplib::Server::Patch(path,guarded(std::move(handler)));}
    void Delete(const std::string& path,Handler handler){httplib::Server::Delete(path,guarded(std::move(handler)));}
    void Options(const std::string& path,Handler handler){httplib::Server::Options(path,guarded(std::move(handler)));}
};
inline std::string portal_cookie(const httplib::Request& request,const std::string& name) {
    const auto header=request.get_header_value("Cookie");std::string result;std::size_t start=0;
    while(start<header.size()) {auto end=header.find(';',start);if(end==std::string::npos)end=header.size();auto field=header.substr(start,end-start);const auto left=field.find_first_not_of(' ');if(left!=std::string::npos)field.erase(0,left);
        if(field.starts_with(name+'=')){if(!result.empty())return "";result=field.substr(name.size()+1);}start=end+1;}
    return result;
}
inline std::string portal_session(const httplib::Request& request){return portal_cookie(request,"deckstatus_session");}
inline void session_cookie(httplib::Response& response,const std::string& value){response.set_header("Set-Cookie","deckstatus_session="+value+"; Path=/; HttpOnly; SameSite=Strict; Max-Age="+(value.empty()?"0":"43200"));}
inline nlohmann::json portal_body(const httplib::Request& request) {
    const auto type=request.get_header_value("Content-Type");if(type!="application/json"&&type!="application/json; charset=utf-8")throw PortalError(415,"portalJsonRequired");
    const auto body=nlohmann::json::parse(request.body,nullptr,false);if(!body.is_object())throw PortalError(400,"portalInvalid");return body;
}
}
