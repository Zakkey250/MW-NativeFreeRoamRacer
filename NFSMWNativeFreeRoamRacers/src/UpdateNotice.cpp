#include "UpdateNotice.h"
#include "UpdateRelease.h"
#include "ModUpdateDialog.h"
#include "Version.h"
#include "Logging.h"
#include <winhttp.h>
#include <atomic>
#include <filesystem>
#pragma comment(lib,"winhttp.lib")
#pragma comment(lib,"user32.lib")
#pragma comment(lib,"advapi32.lib")

namespace native_freeroam { namespace {
constexpr char repository[]="Zakkey250/MW-NativeFreeRoamRacer";
constexpr char assetPrefix[]="MW-NativeFreeRoamRacer-";
constexpr wchar_t apiPath[]=L"/repos/Zakkey250/MW-NativeFreeRoamRacer/releases?per_page=100";
std::atomic<bool> checked{false};
mod_update::KernelHandle checkedEvent;
struct InternetHandle {
    HINTERNET value;
    explicit InternetHandle(HINTERNET v):value(v){}
    ~InternetHandle(){if(value)WinHttpCloseHandle(value);}
    InternetHandle(const InternetHandle&)=delete;
    InternetHandle& operator=(const InternetHandle&)=delete;
};
bool FetchReleases(std::string& output) {
    const auto deadline=GetTickCount64()+10000;
    InternetHandle session(WinHttpOpen(L"MW-NativeFreeRoamRacer-UpdateNotice/1",WINHTTP_ACCESS_TYPE_NO_PROXY,
                                     WINHTTP_NO_PROXY_NAME,WINHTTP_NO_PROXY_BYPASS,0));
    if(!session.value||!WinHttpSetTimeouts(session.value,1500,2000,2000,2000))return false;
    InternetHandle connection(WinHttpConnect(session.value,L"api.github.com",INTERNET_DEFAULT_HTTPS_PORT,0));
    if(!connection.value)return false;
    InternetHandle request(WinHttpOpenRequest(connection.value,L"GET",apiPath,nullptr,WINHTTP_NO_REFERER,
                                              WINHTTP_DEFAULT_ACCEPT_TYPES,WINHTTP_FLAG_SECURE));
    if(!request.value)return false;
    DWORD redirects=WINHTTP_OPTION_REDIRECT_POLICY_NEVER,auth=WINHTTP_AUTOLOGON_SECURITY_LEVEL_HIGH;
    DWORD disabled=WINHTTP_DISABLE_COOKIES;
    if(!WinHttpSetOption(request.value,WINHTTP_OPTION_REDIRECT_POLICY,&redirects,sizeof(redirects))||
       !WinHttpSetOption(request.value,WINHTTP_OPTION_AUTOLOGON_POLICY,&auth,sizeof(auth))||
       !WinHttpSetOption(request.value,WINHTTP_OPTION_DISABLE_FEATURE,&disabled,sizeof(disabled)))return false;
    constexpr wchar_t headers[]=L"Accept: application/vnd.github+json\r\nX-GitHub-Api-Version: 2022-11-28\r\n";
    if(!WinHttpSendRequest(request.value,headers,static_cast<DWORD>(-1),WINHTTP_NO_REQUEST_DATA,0,0,0)||
       GetTickCount64()>=deadline||!WinHttpReceiveResponse(request.value,nullptr))return false;
    DWORD status=0,size=sizeof(status);
    if(!WinHttpQueryHeaders(request.value,WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,WINHTTP_HEADER_NAME_BY_INDEX,
                            &status,&size,WINHTTP_NO_HEADER_INDEX)||status!=200)return false;
    output.clear();char buffer[8192];
    while(GetTickCount64()<deadline) {
        DWORD read=0;
        if(!WinHttpReadData(request.value,buffer,sizeof(buffer),&read))return false;
        if(!read)return !output.empty();
        if(output.size()+read>2*1024*1024)return false;
        output.append(buffer,read);
    }
    return false;
}
bool UseJapanese(const std::filesystem::path& ini,const std::filesystem::path& scripts) {
    wchar_t language[256]{};
    GetPrivateProfileStringW(L"Updates",L"Language",L"auto",language,256,ini.c_str());
    if(_wcsicmp(language,L"auto")!=0)return mod_update::Japanese(language);
    // Follow this MOD's existing language choice first; no second setting is
    // required for players who already selected English/Japanese battle text.
    GetPrivateProfileStringW(L"Encounter",L"Language",L"",language,256,ini.c_str());
    if(language[0])return mod_update::Japanese(language);
    const auto widescreen=scripts/L"NFSMostWanted.WidescreenFix.ini";
    GetPrivateProfileStringW(L"LANGUAGE",L"Language",L"",language,256,widescreen.c_str());
    if(language[0])return mod_update::Japanese(language);
    HKEY key=nullptr;
    if(RegOpenKeyExW(HKEY_LOCAL_MACHINE,L"SOFTWARE\\EA Games\\Need for Speed Most Wanted",0,KEY_QUERY_VALUE|KEY_WOW64_32KEY,&key)==ERROR_SUCCESS) {
        DWORD size=sizeof(language),type=0;
        if(RegQueryValueExW(key,L"Language",nullptr,&type,reinterpret_cast<BYTE*>(language),&size)!=ERROR_SUCCESS||type!=REG_SZ)language[0]=0;
        language[255]=0;RegCloseKey(key);
    }
    return mod_update::Japanese(language); // Unknown and all non-Japanese -> English.
}
} // namespace

void CheckForStartupUpdate(HMODULE module) noexcept {
    if(checked.exchange(true))return;
    try {
        wchar_t path[32768]{};const DWORD count=GetModuleFileNameW(module,path,32768);
        if(!count||count>=32768)return;
        const auto scripts=std::filesystem::path(path).parent_path();
        auto ini=std::filesystem::path(path);ini.replace_extension(L".ini");
        if(!GetPrivateProfileIntW(L"Updates",L"Enabled",1,ini.c_str())) {Log(LogLevel::Info,"UPDATE_NOTICE disabled");return;}
        const auto eventName=mod_update::QueueName(GetCurrentProcessId())+L".Checked.MW-NativeFreeRoamRacer";
        checkedEvent.value=CreateEventW(nullptr,TRUE,TRUE,eventName.c_str());
        if(!checkedEvent.value||GetLastError()==ERROR_ALREADY_EXISTS)return;
        if(!StartupNoticeAllowed())return;
        std::string payload;
        if(!FetchReleases(payload)){Log(LogLevel::Info,"UPDATE_NOTICE unavailable (offline/HTTP/timeout); startup unaffected");return;}
        const auto update=mod_update::FindUpdate(payload,kVersion,repository,assetPrefix);
        if(!update){Log(LogLevel::Info,"UPDATE_NOTICE no eligible newer release current=%s",kVersion);return;}
        if(!StartupNoticeAllowed()){Log(LogLevel::Info,"UPDATE_NOTICE deferred until next launch; gameplay active");return;}
        const bool japanese=UseJapanese(ini,scripts);
        const std::wstring current(kVersion,kVersion+std::char_traits<char>::length(kVersion));
        const std::wstring latest(update->tag.begin(),update->tag.end());
        std::wstring body=japanese?L"新しいバージョンが公開されています。\r\n\r\n現在: ":L"A newer version is available.\r\n\r\nInstalled: ";
        body+=current+(japanese?L"\r\n公開版: ":L"\r\nAvailable: ")+latest;
        body+=japanese?L"\r\n\r\nダウンロード・インストールは行いません。\r\n更新する場合はゲーム終了後に公開ページをご確認ください。":
            L"\r\n\r\nNothing will be downloaded or installed.\r\nTo update, visit the release page after closing the game.";
        body+=L"\r\n\r\ngithub.com/Zakkey250/MW-NativeFreeRoamRacer/releases";
        const bool shown=mod_update::ShowSerializedNotice(module,
            japanese?L"MW Native Free Roam Racer — 更新通知":L"MW Native Free Roam Racer — Update available",
            body.c_str(),japanese?L"閉じて続行":L"Close and continue",StartupNoticeAllowed);
        Log(LogLevel::Info,"UPDATE_NOTICE current=%s available=%s shown=%u language=%s downloads=0",kVersion,update->tag.c_str(),unsigned(shown),japanese?"ja":"en");
    }catch(...){Log(LogLevel::Info,"UPDATE_NOTICE skipped after internal error; startup unaffected");}
}
} // namespace native_freeroam
