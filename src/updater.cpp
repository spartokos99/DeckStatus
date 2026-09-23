#include "updater.h"
#include "portal.h"
#include "language.h"
#include "deckstatus_version.h"
#include <Windows.h>
#include <condition_variable>
#include <fstream>
#include <iostream>
#include <mutex>
#include <thread>
#include <utility>

namespace deckstatus {
namespace {
using Json = nlohmann::json;
using Clock = std::chrono::steady_clock;
constexpr std::uint64_t max_zip = 512ULL * 1024 * 1024, max_chunk = 4ULL * 1024 * 1024;
std::string utf8(const std::wstring& text) {
    if(text.empty())return {};
    const int size=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,text.data(),static_cast<int>(text.size()),nullptr,0,nullptr,nullptr);
    if(!size)throw PortalError(500,"updateUnsafePath");std::string result(size,'\0');
    WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,text.data(),static_cast<int>(text.size()),result.data(),size,nullptr,nullptr);return result;
}
std::wstring quote(const std::wstring& value) {
    std::wstring result=L"\"";unsigned slashes=0;
    for(const auto character:value){if(character==L'\\'){++slashes;continue;}result.append(slashes*(character==L'"'?2:1),L'\\');slashes=0;if(character==L'"')result+=L'\\';result+=character;}
    result.append(slashes*2,L'\\');return result+L'"';
}
void no_reparse(std::filesystem::path path) {
    for(;!path.empty();){const auto attrs=GetFileAttributesW(path.c_str());if(attrs!=INVALID_FILE_ATTRIBUTES&&(attrs&FILE_ATTRIBUTE_REPARSE_POINT))throw PortalError(400,"updateUnsafePath");const auto parent=path.parent_path();if(parent==path)break;path=parent;}
}
Json read_json(const std::filesystem::path& path) {
    if(!std::filesystem::is_regular_file(path)||std::filesystem::file_size(path)>256*1024)throw PortalError(500,"updateOperationFailed");
    std::ifstream input(path);return Json::parse(input);
}
void write_json(const std::filesystem::path& path,const Json& value) {
    std::ofstream output(path,std::ios::binary|std::ios::trunc);output<<value.dump();output.close();if(!output)throw PortalError(500,"updateOperationFailed");
}
struct Process {
    HANDLE handle{};
    ~Process(){if(handle)CloseHandle(handle);}
    Process(const std::filesystem::path& folder,const std::string& action) {
        wchar_t system[MAX_PATH]{};if(!GetSystemDirectoryW(system,MAX_PATH))throw PortalError(500,"updateHelperUnavailable");
        const auto exe=std::filesystem::path(system)/L"WindowsPowerShell/v1.0/powershell.exe";
        auto command=quote(exe.wstring())+L" -NoProfile -NonInteractive -ExecutionPolicy Bypass -File "+quote((folder/L"helper.ps1").wstring())+
                     L" -Action "+std::wstring(action.begin(),action.end())+L" -Plan "+quote((folder/L"plan.json").wstring());
        STARTUPINFOW startup{};startup.cb=sizeof(startup);startup.dwFlags=STARTF_USESHOWWINDOW;startup.wShowWindow=SW_HIDE;PROCESS_INFORMATION process{};
        if(!CreateProcessW(exe.c_str(),command.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,folder.c_str(),&startup,&process))throw PortalError(500,"updateHelperUnavailable");
        handle=process.hProcess;CloseHandle(process.hThread);
    }
};
}
struct Updater::Impl {
    const std::filesystem::path directory;
    std::atomic_bool& stopping;
    const bool automatic;
    Json plan,state={{"current",DECKSTATUS_VERSION},{"status","idle"},{"busy",false},{"latest",nullptr},{"package",nullptr},{"error",""},{"checkedAt",nullptr},{"lastResult",nullptr}};
    mutable std::mutex mutex;
    std::condition_variable_any changed;
    std::filesystem::path pending_folder,prepared_folder,upload_folder;
    std::string pending,upload_id,announced;
    std::uint64_t uploaded=0,upload_size=0;
    Clock::time_point next_check=Clock::now(),last_check{};
    std::jthread worker;
    Impl(const std::filesystem::path& root,const std::filesystem::path& data,const std::filesystem::path& network,
         const std::vector<std::wstring>& arguments,std::atomic_bool& stop,bool auto_check):directory(std::filesystem::absolute(root)),stopping(stop),automatic(auto_check){
        FILETIME created{},exited{},kernel{},user{};GetProcessTimes(GetCurrentProcess(),&created,&exited,&kernel,&user);
        const auto ticks=(static_cast<std::uint64_t>(created.dwHighDateTime)<<32)|created.dwLowDateTime;
        plan={{"root",utf8(directory.wstring())},{"data",utf8(std::filesystem::absolute(data).wstring())},{"network",utf8(std::filesystem::absolute(network).wstring())},
              {"workingDirectory",utf8(std::filesystem::current_path().wstring())},{"arguments",Json::array()},{"current",DECKSTATUS_VERSION},{"pid",GetCurrentProcessId()},{"startTime",std::to_string(ticks)}};
        for(const auto& argument:arguments)plan["arguments"].push_back(utf8(argument));
        try{const auto result=read_json(directory/L"DeckStatus.update/last-result.json");state["lastResult"]={{"status",result.value("status",std::string{})},{"version",result.value("version",std::string{})},{"backup",result.value("backup",std::string{})}};}catch(...){}
        worker=std::jthread([this](std::stop_token token){run(token);});
    }
    ~Impl(){worker.request_stop();changed.notify_all();if(worker.joinable())worker.join();}
    std::filesystem::path folder() {
        const auto path=directory/L"DeckStatus.update"/std::filesystem::path("job-"+Portal::random_token().substr(0,32));no_reparse(path);
        std::filesystem::create_directories(path);
        if(!std::filesystem::is_regular_file(directory/L"DeckStatus.Update.ps1"))throw PortalError(500,"updateHelperUnavailable");
        std::filesystem::copy_file(directory/L"DeckStatus.Update.ps1",path/L"helper.ps1");write_json(path/L"plan.json",plan);return path;
    }
    void queue(const std::string& action,const std::filesystem::path& path) {
        pending=action;pending_folder=path;state["busy"]=true;state["error"]="";
        state["status"]=action=="Check"?"checking":action=="Download"?"downloading":action=="Apply"?"installing":"preparing";changed.notify_all();
    }
    void run(std::stop_token token) {
        while(!token.stop_requested()) {
            std::string action;std::filesystem::path path;
            {
                std::unique_lock lock(mutex);
                changed.wait_for(lock,token,std::chrono::seconds(1),[&]{return !pending.empty();});
                if(token.stop_requested())break;
                if(pending.empty()&&automatic&&Clock::now()>=next_check&&!state["busy"].get<bool>()&&upload_id.empty()) {
                    next_check=Clock::now()+std::chrono::hours(6);
                    try{queue("Check",folder());}catch(...){state["error"]="updateOperationFailed";std::cerr<<"[Updater] "<<tr("updateOperationFailed")<<'\n';}
                }
                if(pending.empty())continue;action=std::exchange(pending,{});path=pending_folder;
            }
            try {
                std::filesystem::remove(path/L"result.json");std::filesystem::remove(path/L"ready.json");
                Process helper(path,action);const auto deadline=Clock::now()+std::chrono::minutes(10);
                bool handed_off=false;
                while(WaitForSingleObject(helper.handle,100)==WAIT_TIMEOUT) {
                    if(token.stop_requested()||Clock::now()>deadline){TerminateProcess(helper.handle,1);WaitForSingleObject(helper.handle,5000);throw PortalError(500,"updateOperationFailed");}
                    if(action=="Apply"&&std::filesystem::exists(path/L"ready.json")) {
                        const auto ready=read_json(path/L"ready.json");if(!ready.value("ok",false))throw PortalError(500,"updateOperationFailed");
                        // Let the HTTP acknowledgement reach the browser before stopping listeners.
                        std::this_thread::sleep_for(std::chrono::milliseconds(750));stopping=true;handed_off=true;break;
                    }
                }
                if(handed_off)return; // The owned apply helper must outlive this process.
                const auto result=read_json(path/L"result.json");
                if(!result.value("ok",false))throw std::runtime_error(result.value("error",std::string("updateOperationFailed")));
                DWORD exit_code=1;
                if(!GetExitCodeProcess(helper.handle,&exit_code)||exit_code!=0||action=="Apply")throw PortalError(500,"updateOperationFailed");
                std::lock_guard lock(mutex);
                if(action=="Check") {
                    state["latest"]=result.at("latest");state["checkedAt"]=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
                    last_check=Clock::now();next_check=last_check+std::chrono::hours(6);
                    const auto& latest=state["latest"];
                    if(latest.value("available",false)&&latest.at("version")!=announced){announced=latest.at("version").get<std::string>();std::cout<<"[Updater] "<<tr("updateAvailable")<<" "<<announced<<" — "<<latest.at("url").get<std::string>()<<'\n'<<std::flush;}
                    else std::cout<<"[Updater] "<<tr("updateChecked")<<" ("<<latest.at("version").get<std::string>()<<")\n"<<std::flush;
                } else {state["package"]=result.at("package");prepared_folder=path;state["package"]["source"]=action=="Download"?"github":"upload";}
                state["status"]=prepared_folder.empty()?"idle":"ready";state["busy"]=false;
            } catch(const std::exception& error) {
                std::lock_guard lock(mutex);state["busy"]=false;state["status"]=prepared_folder.empty()?"failed":"ready";
                const std::string message=error.what();state["error"]=message.starts_with("update")?message:"updateOperationFailed";
                std::cerr<<"[Updater] "<<tr(state["error"].get<std::string>())<<'\n'<<std::flush;
            }
        }
    }
};
Updater::Updater(const std::filesystem::path& directory,const std::filesystem::path& data,const std::filesystem::path& network,
                 const std::vector<std::wstring>& arguments,std::atomic_bool& stopping,bool automatic_check)
    :impl_(std::make_unique<Impl>(directory,data,network,arguments,stopping,automatic_check)){}
Updater::~Updater()=default;
Json Updater::describe() const {std::lock_guard lock(impl_->mutex);return impl_->state;}
Json Updater::summary() const {std::lock_guard lock(impl_->mutex);const auto& state=impl_->state;const auto& latest=state["latest"];
    return {{"available",latest.is_object()&&latest.value("available",false)},{"version",latest.is_object()?latest.value("version",std::string{}):""},
            {"url",latest.is_object()?latest.value("url",std::string{}):""}};}
Json Updater::command(const Json& value) {
    auto& p=*impl_;std::lock_guard lock(p.mutex);const auto action=value.value("action",std::string{});
    if(p.state["busy"].get<bool>()&&p.state["status"]!="uploading")throw PortalError(409,"updateBusy");
    if(action=="cancelUpload") {p.upload_id.clear();p.state["busy"]=false;p.state["status"]=p.prepared_folder.empty()?"idle":"ready";return p.state;}
    if(!p.upload_id.empty()) {
        if(action!="finishUpload"||value.value("id",std::string{})!=p.upload_id||p.uploaded!=p.upload_size)throw PortalError(409,"updateUploadOrder");
        p.upload_id.clear();p.prepared_folder.clear();p.state["package"]=nullptr;p.queue("Prepare",p.upload_folder);return p.state;
    }
    if(action=="check") {
        if(Clock::now()-p.last_check<std::chrono::seconds(60))throw PortalError(429,"updateCheckCooldown");p.last_check=Clock::now();p.queue("Check",p.folder());
    } else if(action=="download") {p.prepared_folder.clear();p.state["package"]=nullptr;p.queue("Download",p.folder());}
    else if(action=="install") {
        if(p.prepared_folder.empty()||!value.value("confirm",false))throw PortalError(400,"updateNotPrepared");p.queue("Apply",p.prepared_folder);
    } else if(action=="beginUpload") {
        if(!value.contains("size")||!value["size"].is_number_integer()||value["size"]<=0||value["size"]>max_zip)throw PortalError(413,"updateTooLarge");
        p.upload_folder=p.folder();p.upload_id=Portal::random_token();p.uploaded=0;p.upload_size=value["size"].get<std::uint64_t>();
        {std::ofstream file(p.upload_folder/L"package.zip",std::ios::binary);if(!file)throw PortalError(500,"updateOperationFailed");}
        p.state["status"]="uploading";p.state["busy"]=true;p.state["error"]="";
        return {{"id",p.upload_id},{"chunkSize",max_chunk}};
    } else throw PortalError(400,"portalInvalid");
    return p.state;
}
Json Updater::upload(std::string_view id,std::uint64_t offset,std::string_view bytes) {
    auto& p=*impl_;std::lock_guard lock(p.mutex);
    if(p.upload_id.empty()||id!=p.upload_id||offset!=p.uploaded)throw PortalError(409,"updateUploadOrder");
    if(bytes.empty()||bytes.size()>max_chunk||bytes.size()>p.upload_size-p.uploaded)throw PortalError(413,"updateTooLarge");
    no_reparse(p.upload_folder/L"package.zip");if(std::filesystem::file_size(p.upload_folder/L"package.zip")!=p.uploaded)throw PortalError(409,"updateUploadOrder");
    std::ofstream output(p.upload_folder/L"package.zip",std::ios::binary|std::ios::app);
    output.write(bytes.data(),static_cast<std::streamsize>(bytes.size()));output.close();if(!output)throw PortalError(500,"updateOperationFailed");
    p.uploaded+=bytes.size();return {{"received",p.uploaded},{"total",p.upload_size}};
}
}
