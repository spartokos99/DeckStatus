#include "network.h"
#include "server.h"
#include <httplib/httplib.h>
#include <Windows.h>
#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <thread>
using Json = nlohmann::json;
using namespace deckstatus;
void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
void status(const httplib::Result& response, int expected) {
    require(response && response->status == expected, ("Unexpected HTTP status; expected " + std::to_string(expected) + ", received " +
        (response ? std::to_string(response->status) : httplib::to_string(response.error()))).c_str());
}
template<class F> void rejects(F action) { bool rejected=false;try{action();}catch(const std::runtime_error&){rejected=true;}require(rejected,"Invalid configuration accepted"); }
int free_port() {
    httplib::Server reservation; reservation.new_task_queue=[] { return new httplib::ThreadPool(1); };
    const int port=reservation.bind_to_any_port("127.0.0.1");require(port>0,"No test port");
    std::jthread thread([&]{reservation.listen_after_bind();});reservation.wait_until_ready();reservation.stop();return port;
}
struct StopOnExit { std::atomic_bool& stop; ~StopOnExit(){stop=true;} };
void exercise_bind_failure(const std::filesystem::path& web) {
    const auto port=free_port();
    httplib::Server occupied;
    require(occupied.bind_to_port("127.0.0.1",port),"Could not occupy the loopback test port");
    std::jthread occupied_thread([&]{occupied.listen_after_bind();});occupied.wait_until_ready();
    struct StopServer { httplib::Server& server; ~StopServer(){server.stop();} } guard{occupied};
    std::atomic_bool stop{};
    require(run_server("127.0.0.2",port,web,[]{return Json::object();},[](int){return std::pair<std::string,std::string>{};},stop)==1,
        "Secondary listener failure did not abort startup");
    httplib::Server probe;
    require(probe.bind_to_port("127.0.0.2",port),"Failed startup leaked the primary socket");
    std::jthread probe_thread([&]{probe.listen_after_bind();});probe.wait_until_ready();probe.stop();
}
void exercise_server(NetworkConfig& config, const std::filesystem::path& web, bool allow_remote) {
    const auto port=config.active().port;
    ServerFeatures features;features.mode="prolink";features.network=&config;
    std::atomic_int actions{};
    features.prolink_setup=[] {return Json{{"status","stopped"}};};
    features.prolink_control=[&](const Json&){++actions;return Json{{"accepted",true}};};
    std::atomic_bool stop{},done{};std::atomic_int exit{-1};
    std::jthread thread([&]{exit=run_server(config.active().bind,port,web,[]{return Json{{"status","demo"},{"decks",Json::array()}};},[](int){return std::pair<std::string,std::string>{};},stop,nullptr,&features);done=true;});
    StopOnExit guard{stop};
    const auto address=config.active().bind=="0.0.0.0"?std::string("127.0.0.2"):config.active().bind;
    httplib::Client client(address,port);client.set_connection_timeout(0,100000);client.set_read_timeout(2);
    bool ready=false;for(int i=0;i<150&&!done;++i){if(auto result=client.Get("/api/app");result&&result->status==200){ready=true;break;}std::this_thread::sleep_for(std::chrono::milliseconds(10));}
    require(ready,"Wildcard listener rejected its real destination address");
    const auto host=address+":"+std::to_string(port);
    httplib::Client local("127.0.0.1",port);local.set_connection_timeout(1);local.set_read_timeout(2);
    for(const auto* route:{"/overlay?deck=1","/master-overlay","/waveform","/api/state"}){
        status(local.Get(route),200);
        status(local.Get(route,{{"Host","localhost:"+std::to_string(port)}}),200);
    }
    require(Json::parse(local.Get("/api/app")->body)["obsBaseUrl"]=="http://127.0.0.1:"+std::to_string(port),"OBS base is not localhost");
    status(client.Get("/api/network"),200);
    require(Json::parse(client.Get("/api/network")->body)["canConfigure"]==true,"Local setup disabled");
    for(const auto* route:{"/network/settings","/network-settings.js","/network-settings.css","/overlay?deck=1","/master-overlay","/waveform","/api/state"})status(client.Get(route),200);
    status(client.Get("/api/state",{{"Host","evil.example:"+std::to_string(port)}}),403);
    status(client.Get("/api/state",{{"Host","127.0.0.3:"+std::to_string(port)}}),403);
    status(client.Get("/api/state",{{"Host",host},{"Host",host}}),403);
    status(client.Get("/api/state",{{"Origin","http://"+host}}),200);
    status(client.Get("/api/state",{{"Origin","http://evil.example"}}),403);
    const auto domain=config.active().public_domain;
    if (domain.empty()) {
        status(client.Get("/api/state",{{"Host","ds.example.net"},{"X-Forwarded-Host","ds.example.net"},{"X-Forwarded-Proto","https"}}),403);
    } else {
        const httplib::Headers proxy{{"Host",domain},{"Origin","https://"+domain},{"X-Forwarded-For","127.0.0.1"}};
        status(client.Get("/api/state",proxy),200);
        status(client.Get("/api/state",{{"Host",domain+":443"},{"Origin","https://"+domain}}),200);
        status(client.Get("/api/state",{{"Host",domain},{"Origin","https://"+domain+":443"}}),200);
        status(client.Get("/api/state",{{"Host","DS.EXAMPLE.NET"},{"Origin","https://DS.EXAMPLE.NET"}}),200);
        for(const auto& rejected:{domain+":80",domain+":18740",domain+".","sub."+domain,domain+".evil.example"})
            status(client.Get("/api/state",{{"Host",rejected}}),403);
        for(const auto& rejected:std::initializer_list<std::string>{"http://"+domain,"https://"+domain+":444","https://evil.example","http://"+host,"null"})
            status(client.Post("/api/audio/source",{{"Host",domain},{"Origin",rejected}},R"({"deviceId":""})","application/json"),403);
        status(client.Get("/api/state",{{"Host",domain},{"Origin","https://"+domain},{"Origin","https://"+domain}}),403);
        status(client.Get("/api/state",{{"Host",domain},{"Host",domain}}),403);
        status(client.Get("/api/state",{{"Host",host},{"Origin","https://"+domain},{"X-Forwarded-Proto","https"},{"X-Forwarded-Host",domain}}),403);
        require(Json::parse(client.Get("/api/app",proxy)->body)["canControl"]==allow_remote,"Proxy inherited local controls");
        const auto description=Json::parse(client.Get("/api/network",proxy)->body);
        require(description["canConfigure"]==false,"Proxy inherited local network management");
        require(description["urls"][0]=="http://127.0.0.1:"+std::to_string(port)&&description["urls"][1]=="https://"+domain,"Local/public URLs missing");
        status(client.Post("/api/network",proxy,"{}","application/json"),403);
        status(client.Post("/api/audio/source",proxy,R"({"deviceId":""})","application/json"),allow_remote?200:403);
        status(client.Post("/api/prolink/control",proxy,R"({"action":"disconnect"})","application/json"),allow_remote?202:403);
        actions=0;
    }
    status(client.Post("/api/network","{}","text/plain"),415);
    status(client.Post("/api/network","{}","application/json"),400);
    status(client.Post("/api/network","{","application/json"),400);
    status(client.Post("/api/network",{{"Origin","http://evil.example"}},"{}","application/json"),403);
    status(client.Put("/api/network","{}","application/json"),405);
    const auto saved=Json{{"bind","0.0.0.0"},{"port",port},{"allowRemoteControl",!allow_remote},{"publicDomain",domain}};
    auto invalid_domain=saved;invalid_domain["publicDomain"]="https://evil.example";
    status(client.Post("/api/network",invalid_domain.dump(),"application/json"),400);
    status(client.Post("/api/network",saved.dump(),"application/json"),200);
    const auto after=Json::parse(client.Get("/api/network")->body);
    require(after["active"]["allowRemoteControl"]==allow_remote && after["restartRequired"]==true,"Settings applied without restart");

    // Bind a client to a real non-loopback source IP while connecting to loopback.
    // This exercises the server's remote-peer policy without another PC or real DJ/audio devices.
    bool remote_checked=false;
    for(const auto& adapter:network_interfaces()) {
        httplib::Client remote("127.0.0.1",port);
        // cpp-httplib 0.20.0 ignores set_interface() on Windows.
        remote.set_socket_options([ip=adapter["address"].get<std::string>()](socket_t socket) {
            sockaddr_in source{};source.sin_family=AF_INET;
            require(InetPtonA(AF_INET,ip.c_str(),&source.sin_addr)==1,"Invalid test interface");
            require(::bind(socket,reinterpret_cast<sockaddr*>(&source),sizeof(source))==0,"Could not bind test client source IP");
        });
        remote.set_connection_timeout(0,300000);remote.set_read_timeout(2);
        const httplib::Headers spoofed{{"X-Forwarded-For","127.0.0.1"},{"Forwarded","for=127.0.0.1"},{"REMOTE_ADDR","127.0.0.1"}};
        const auto app=remote.Get("/api/app",spoofed);if(!app)continue;
        status(app,200);require(Json::parse(app->body)["canControl"]==allow_remote,"Remote policy or header spoofing failed");
        const auto network=remote.Get("/api/network");status(network,200);
        require(Json::parse(network->body)["canConfigure"]==false,"Remote client can edit network settings");
        status(remote.Post("/api/network",spoofed,saved.dump(),"application/json"),403);
        status(remote.Post("/api/audio/source",spoofed,R"({"deviceId":""})","application/json"),allow_remote?200:403);
        status(remote.Post("/api/prolink/control",spoofed,R"({"action":"disconnect"})","application/json"),allow_remote?202:403);
        require(actions== (allow_remote?1:0),"Denied ProLink request reached the callback");
        status(remote.Get("/api/state"),200);remote_checked=true;break;
    }
    std::cout << (remote_checked?"Remote HTTP permissions and spoofed headers verified.\n":"Remote HTTP binding unavailable; pure peer-policy checks still ran.\n");
    status(client.Post("/api/audio/source",R"({"deviceId":""})","application/json"),200);
    require(Json::parse(client.Get("/api/audio/state")->body)["status"]=="stopped","Test opened audio capture");
    stop=true;thread.join();require(exit==0,"Server did not stop cleanly");
}
int main(int argc,char** argv) {
    try {
        require(argc==2,"Expected web directory");
        exercise_bind_failure(argv[1]);
        for(const char* ip:{"127.0.0.1","0.0.0.0","192.168.1.20","10.0.0.5","169.254.1.1"})require(valid_bind_address(ip),"Valid IPv4 rejected");
        for(const char* ip:{"","*","localhost","::","::1","127.1","127.0.0.01","1.2.3.256","1.2.3.4:80","1.2.3.4\n","0.1.2.3","224.0.0.1","255.255.255.255"})require(!valid_bind_address(ip),"Invalid bind address accepted");
        require(loopback_peer("127.0.0.2")&&loopback_peer("::ffff:127.0.0.1")&&!loopback_peer("192.168.1.20"),"Peer classification failed");
        require(!may_control_network("192.168.1.20",false)&&may_control_network("192.168.1.20",true)&&!may_control_network("",true),"Remote control default incorrect");
        require(local_network_peer("192.168.1.20","192.168.1.20")&&!local_network_peer("192.168.1.21","192.168.1.20"),"Local interface management failed");
        const auto folder=std::filesystem::temp_directory_path()/("DeckStatus-network-test-"+std::to_string(GetCurrentProcessId())+"-"+std::to_string(GetTickCount64()));
        std::filesystem::create_directory(folder);const auto file=folder/"DeckStatus.network.json";
        NetworkConfig initial(file);require(initial.active()==NetworkOptions{}&&!std::filesystem::exists(file),"Default changed or auto-saved");
        auto value=Json{{"bind","0.0.0.0"},{"port",free_port()},{"allowRemoteControl",false}};
        initial.save(value);require(initial.active().bind=="127.0.0.1","Saving opened LAN immediately");
        NetworkConfig loaded(file);require(loaded.active().bind=="0.0.0.0","Saved binding not restored");
        require(loaded.active().public_domain.empty(),"Legacy settings enabled a domain");
        for (const auto& bad : {Json(nullptr),Json(42),Json(true),Json("localhost"),Json("https://ds.example.net"),
             Json("ds.example.net:443"),Json("*.example.net"),Json("192.168.0.221"),Json("ds.example.net/"),Json("ds.example.net."),
             Json("ds..example.net"),Json("-ds.example.net"),Json("ds-.example.net"),Json("ds_example.net"),Json("ds.example.net\r\nHost: evil.example"),
             Json(std::string(64,'a')+".net"),Json(std::string(254,'a')),Json("user@ds.example.net")}) {
            auto invalid=value;invalid["publicDomain"]=bad;rejects([&]{loaded.save(invalid);});
        }
        NetworkConfig overridden(file,std::string("127.0.0.1"),18741,false);
        require(overridden.active().bind=="127.0.0.1"&&overridden.describe(true)["saved"]["bind"]=="0.0.0.0","CLI override changed persisted values");
        for(const auto& bad:{Json(),Json::array(),Json{{"bind","0.0.0.0"},{"port",0},{"allowRemoteControl",false}},
             Json{{"bind","0.0.0.0"},{"port",70000},{"allowRemoteControl",false}},Json{{"bind","0.0.0.0"},{"port",80.5},{"allowRemoteControl",false}},
             Json{{"bind","evil.example"},{"port",80},{"allowRemoteControl",false}},Json{{"bind","0.0.0.0"},{"port",80},{"allowRemoteControl","false"}},
             Json{{"bind","0.0.0.0"},{"port",80},{"allowRemoteControl",false},{"extra",true}}})rejects([&]{loaded.save(bad);});
        HANDLE held=CreateFileW(file.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,0,nullptr);
        require(held!=INVALID_HANDLE_VALUE,"Could not lock test settings");
        value["port"]=18749;rejects([&]{loaded.save(value);});CloseHandle(held);
        require(NetworkConfig(file).active().port==loaded.active().port,"Failed save damaged previous settings");
        exercise_server(loaded,argv[1],false);
        value["port"]=free_port();value["publicDomain"]="DS.EXAMPLE.NET";loaded.save(value);
        NetworkConfig domain_enabled(file);require(domain_enabled.active().public_domain=="ds.example.net","Domain not normalized/persisted");
        exercise_server(domain_enabled,argv[1],false);
        NetworkConfig remote_enabled(file,{},free_port());exercise_server(remote_enabled,argv[1],true);
        NetworkConfig specific(file,std::string("127.0.0.2"),free_port(),false);exercise_server(specific,argv[1],false);
        value["publicDomain"]="";remote_enabled.save(value);require(NetworkConfig(file).active().public_domain.empty(),"Domain not disabled");
        const auto corrupt=folder/"corrupt.json";{std::ofstream stream(corrupt);stream<<"{";}
        rejects([&]{NetworkConfig invalid(corrupt);});
        std::filesystem::remove(file);std::filesystem::remove(corrupt);std::filesystem::remove(folder);
        std::cout<<"Network configuration, persistence, rollback, CLI precedence, wildcard HTTP, Host/Origin and local/remote access checks passed.\n";
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
