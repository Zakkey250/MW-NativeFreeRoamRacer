// Execute the production logger with deterministic Win32 IO fault injection.
// No game process or real log file is accessed.
#include <Windows.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <cstdio>
#include <mutex>
#include <set>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace fixture {
std::mutex mutex;
std::set<HANDLE> live;
std::vector<std::string> rows;
unsigned opens=0,closes=0,errors=0,next=1;
bool failOpen=false,failWrite=false,shortWrite=false;
HANDLE WINAPI Open(LPCWSTR,DWORD access,DWORD share,LPSECURITY_ATTRIBUTES,DWORD creation,DWORD flags,HANDLE) {
    std::lock_guard<std::mutex> lock(mutex);++opens;
    if(access!=FILE_APPEND_DATA || share!=(FILE_SHARE_READ|FILE_SHARE_WRITE) ||
       creation!=OPEN_ALWAYS || flags!=FILE_ATTRIBUTE_NORMAL)++errors;
    if(failOpen){failOpen=false;return INVALID_HANDLE_VALUE;}
    const auto h=reinterpret_cast<HANDLE>(static_cast<UINT_PTR>(next++));live.insert(h);return h;
}
BOOL WINAPI Write(HANDLE h,LPCVOID bytes,DWORD length,LPDWORD written,LPOVERLAPPED overlap) {
    std::lock_guard<std::mutex> lock(mutex);
    if(!live.count(h)||overlap){++errors;*written=0;return FALSE;}
    if(failWrite){failWrite=false;*written=0;return FALSE;}
    if(shortWrite){shortWrite=false;*written=length/2;return TRUE;}
    rows.emplace_back(static_cast<const char*>(bytes),length);*written=length;return TRUE;
}
BOOL WINAPI Close(HANDLE h) {
    std::lock_guard<std::mutex> lock(mutex);++closes;
    if(live.erase(h)!=1)++errors;
    return TRUE;
}
void Reset() {live.clear();rows.clear();opens=closes=errors=0;failOpen=failWrite=shortWrite=false;}
}
#define CreateFileW fixture::Open
#define WriteFile fixture::Write
#define CloseHandle fixture::Close
#include "../src/Logging.cpp"
#undef CloseHandle
#undef WriteFile
#undef CreateFileW

namespace native_freeroam {
namespace {
// Only the settings/clock storage needed by the actual production report code.
struct {bool frameTimingDiagnostics=true,adaptiveSpawnPacing=false;
    float telemetryIntervalSeconds=5.0f;} g_settings;
LARGE_INTEGER g_frequency{};
#include "../src/PerformanceDiagnostics.inl"
// Execute the exact production cadence/batch wrapper with a deterministic row
// producer. Native vehicle reads are covered separately by source equivalence.
struct VehicleSnapshot {unsigned id=0;};
float g_telemetryTimer=0.0f;
unsigned telemetryReports=0;
bool throwTelemetry=false;
void LogTelemetryRows(const VehicleSnapshot& player,const std::vector<VehicleSnapshot>& vehicles) {
    ++telemetryReports;
    Log(LogLevel::Info,"AUDIO_POOL_HEALTH readOnly=1");
    Log(LogLevel::Info,"TELEMETRY player=%u vehicles=%u",player.id,static_cast<unsigned>(vehicles.size()));
    if(throwTelemetry)throw std::runtime_error("telemetry fixture");
    Log(LogLevel::Info,"CACHE_RETENTION wanted=1");
    for(const auto& v:vehicles)Log(LogLevel::Info,"RACER_STATE vehicle=%u",v.id);
}
#include "../src/TelemetryDispatch.inl"
}
}

int main() {
    using namespace native_freeroam;
    unsigned failures=0,passes=0;
    auto expect=[&](bool ok,const char* name){std::printf("%s %s\n",ok?"PASS":"FAIL",name);ok?++passes:++failures;};
    {const LogBatch empty;Log(LogLevel::Info,"disabled");}
    expect(fixture::opens==0&&g_logBatchDepth==0,"uninitialized logger and empty batch do no IO");
    wcscpy_s(g_logPath,L"offline-fixture.log");
    {const LogBatch empty;}
    expect(fixture::opens==0,"initialized but empty report opens no handle");
    for(unsigned i=0;i<19;++i)Log(LogLevel::Info,"row=%u",i);
    expect(fixture::opens==19&&fixture::closes==19&&fixture::rows.size()==19,"unbatched path retains per-row lifetime");
    const auto oldRows=fixture::rows;
    fixture::Reset();
    {
        const LogBatch report;
        for(unsigned i=0;i<19;++i){Log(LogLevel::Info,"row=%u",i);
            expect(fixture::rows.size()==i+1,"batched row written immediately before scope ends");}
        expect(fixture::opens==1&&fixture::closes==0&&fixture::live.size()==1,"19-row report reuses exactly one open handle");
    }
    expect(fixture::closes==1&&fixture::live.empty()&&g_logBatchDepth==0,"report closes handle immediately on return");
    bool equal=true;
    for(unsigned i=0;i<19;++i)equal&=oldRows[i].substr(23)==fixture::rows[i].substr(23);
    expect(equal,"message order severity CRLF and content unchanged");
    fixture::Reset();
    {
        const LogBatch outer;Log(LogLevel::Info,"outer");
        {const LogBatch inner;Log(LogLevel::Warning,"warning");Log(LogLevel::Error,"error");}
        expect(fixture::opens==1&&fixture::closes==0&&fixture::rows.size()==3,"nested scopes retain outer handle and immediate warning/error output");
    }
    expect(fixture::closes==1&&g_logBatchDepth==0,"nested scope closes once");
    fixture::Reset();
    try {const LogBatch unwind;Log(LogLevel::Info,"unwind");throw std::runtime_error("test");}catch(const std::runtime_error&){}
    expect(fixture::closes==1&&fixture::live.empty()&&g_logBatchDepth==0,"C++ unwind releases report handle");
    fixture::Reset();fixture::failOpen=true;
    {const LogBatch report;Log(LogLevel::Info,"unavailable");Log(LogLevel::Info,"recovered");Log(LogLevel::Info,"again");}
    expect(fixture::opens==2&&fixture::closes==1&&fixture::rows.size()==2,"failed open retries next row without permanent disable");
    for(bool partial:{false,true}) {
        fixture::Reset();fixture::failWrite=!partial;fixture::shortWrite=partial;
        {const LogBatch report;Log(LogLevel::Info,"failed");Log(LogLevel::Info,"recovered");}
        expect(fixture::opens==2&&fixture::closes==2&&fixture::rows.size()==1&&fixture::live.empty(),
            "failed or partial write closes handle and reopens next row without replay");
    }
    fixture::Reset();
    {
        const LogBatch mainReport;Log(LogLevel::Info,"main-before");
        std::thread other([]{Log(LogLevel::Info,"other-unbatched");});other.join();
        expect(fixture::opens==2&&fixture::closes==1&&fixture::live.size()==1,"another thread never borrows outer report handle");
        Log(LogLevel::Info,"main-after");
    }
    expect(fixture::opens==2&&fixture::closes==2&&fixture::errors==0,"cross-thread scopes remain independent");
    fixture::Reset();std::vector<std::thread> workers;
    for(unsigned t=0;t<4;++t)workers.emplace_back([t]{const LogBatch report;
        for(unsigned i=0;i<100;++i)Log(LogLevel::Info,"thread=%u row=%u",t,i);});
    for(auto& worker:workers)worker.join();
    expect(fixture::rows.size()==400&&fixture::opens==4&&fixture::closes==4&&fixture::errors==0&&fixture::live.empty(),
        "concurrent reports preserve every row and leave no handles or workers");
    std::set<std::string> unique;
    for(const auto& row:fixture::rows)unique.insert(row.substr(23));
    expect(unique.size()==400,"concurrent lines are distinct and never interleaved");
    fixture::Reset();
    {const LogBatch report;std::string longMessage(4096,'X');Log(LogLevel::Info,"%s",longMessage.c_str());}
    expect(fixture::rows.size()==1&&fixture::rows[0].size()<2304&&fixture::rows[0].ends_with("\r\n"),
        "oversized message remains bounded and newline terminated");
    expect(fixture::errors==0&&g_logBatchFile==INVALID_HANDLE_VALUE&&g_logBatchDepth==0,"no invalid handle use or surviving batch state");
    fixture::Reset();ResetPerformanceWindow(1);
    for(auto& histogram:g_performance)histogram.Add(100);
    ReportPerformanceWindow(30000000);
    expect(fixture::opens==0,"production report not due performs no IO");
    ReportPerformanceWindow(30000001);
    expect(fixture::opens==1&&fixture::closes==1&&fixture::rows.size()==19&&fixture::live.empty(),
        "actual production 19-stage report uses one scoped handle");
    expect(g_performanceWindowStart==30000001&&g_performance[0].count==0,
        "production report preserves histogram reset and next window");
    fixture::Reset();ReportPerformanceWindow(60000001);
    expect(fixture::opens==0,"production empty due window does not open file");
    for(unsigned count:{0u,6u,15u}) {
        fixture::Reset();telemetryReports=0;g_telemetryTimer=0.0f;
        const VehicleSnapshot player{42};std::vector<VehicleSnapshot> vehicles;
        for(unsigned i=0;i<count;++i)vehicles.push_back({i+1});
        LogTelemetryRows(player,vehicles);
        const auto expected=fixture::rows;
        expect(fixture::opens==count+3&&fixture::closes==count+3,
            "telemetry baseline uses one open per row");
        fixture::Reset();telemetryReports=0;
        LogTelemetry(player,vehicles,4.0f);
        expect(fixture::opens==0&&telemetryReports==0&&g_telemetryTimer==4.0f&&g_logBatchDepth==0,
            "actual telemetry wrapper not due performs no report or file IO");
        LogTelemetry(player,vehicles,1.0f);
        expect(fixture::opens==1&&fixture::closes==1&&fixture::rows.size()==count+3&&fixture::live.empty(),
            "actual telemetry wrapper uses one handle for zero six or fifteen racers");
        bool same=expected.size()==fixture::rows.size();
        for(std::size_t i=0;same&&i<expected.size();++i)same&=expected[i].substr(23)==fixture::rows[i].substr(23);
        expect(same&&telemetryReports==1&&g_telemetryTimer==0.0f,
            "telemetry content order and timer reset unchanged");
        LogTelemetry(player,vehicles,12.0f);
        expect(telemetryReports==2&&fixture::opens==2&&fixture::closes==2&&g_telemetryTimer==0.0f,
            "late telemetry emits once without catch-up burst or persistent handle");
    }
    fixture::Reset();g_telemetryTimer=0.0f;throwTelemetry=true;
    try{LogTelemetry({}, {},5.0f);}catch(const std::runtime_error&){}
    throwTelemetry=false;
    expect(fixture::opens==1&&fixture::closes==1&&fixture::live.empty()&&g_logBatchDepth==0,
        "actual telemetry wrapper closes handle on C++ exception");
    for(bool partial:{false,true}) {
        fixture::Reset();g_telemetryTimer=0.0f;fixture::failWrite=!partial;fixture::shortWrite=partial;
        LogTelemetry({}, {},5.0f);
        expect(fixture::opens==2&&fixture::closes==2&&fixture::rows.size()==2&&fixture::live.empty(),
            "actual telemetry wrapper reopens after failed or partial write without replay");
    }
    fixture::Reset();g_telemetryTimer=0.0f;fixture::failOpen=true;
    LogTelemetry({}, {},5.0f);
    expect(fixture::opens==2&&fixture::closes==1&&fixture::rows.size()==2&&fixture::live.empty(),
        "actual telemetry wrapper recovers from failed open on next row");
    std::printf("SUMMARY passes=%u failures=%u\n",passes,failures);return failures?1:0;
}
