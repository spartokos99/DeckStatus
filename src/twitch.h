#pragma once
#include "portal.h"
#include <memory>
#include <functional>
namespace deckstatus {
// Optional deterministic transport for isolated protocol tests. Production uses WinHTTP.
struct TwitchTransport {
    using Json=nlohmann::json;
    struct Response {unsigned long status;Json body;};
    std::function<Response(const std::wstring&,const std::wstring&,const std::wstring&,const std::string&,const std::string&)> request;
    std::function<void(std::function<void(Json)>)> connect;
    std::function<void()> disconnect;
};
class TwitchIntegration {
    struct Impl;
    std::unique_ptr<Impl> impl_;
public:
    explicit TwitchIntegration(Portal* portal);
    TwitchIntegration(Portal* portal,std::shared_ptr<TwitchTransport> transport);
    ~TwitchIntegration();
    void start();
    nlohmann::json describe() const;
    nlohmann::json command(const nlohmann::json& command);
    nlohmann::json automation_description() const;
    nlohmann::json automation_command(const nlohmann::json& command);
    nlohmann::json viewer_status(const std::string& session);
    nlohmann::json viewer_identity(const std::string& session,bool validate=false);
    nlohmann::json viewer_command(const std::string& session,const std::string& peer,const nlohmann::json& command);
    nlohmann::json render(nlohmann::json scene);
    bool broadcast_access(const std::string& key,const std::string& path,const std::string& scene);
};
}
