#include <Windows.h>
#include <tlhelp32.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>
#include "../src/Logging.cpp"
#undef NDEBUG
#include <cassert>
namespace native_freeroam {
namespace {
struct {float telemetryIntervalSeconds=5;} g_settings;
struct VehicleSnapshot {};
float g_telemetryTimer=0;
unsigned traversals=0;
void LogTelemetryRows(const VehicleSnapshot&,const std::vector<VehicleSnapshot>&){++traversals;}
#include "../src/TelemetryDispatch.inl"
}
}
unsigned ThreadCount(){
    const auto snap=CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD,0);assert(snap!=INVALID_HANDLE_VALUE);
    THREADENTRY32 row{};row.dwSize=sizeof(row);unsigned count=0;
    if(Thread32First(snap,&row))do{count+=row.th32OwnerProcessID==GetCurrentProcessId();}while(Thread32Next(snap,&row));
    CloseHandle(snap);return count;
}
int main(int argc,char** argv){
    assert(argc==2);const std::string mode=argv[1];const bool on=mode=="on";
    wchar_t module[MAX_PATH]{};GetModuleFileNameW(nullptr,module,MAX_PATH);
    const std::filesystem::path exe=module;auto ini=exe;ini.replace_extension(".ini");auto log=exe;log.replace_extension(".log");
    assert(!std::filesystem::exists(ini)&&!std::filesystem::exists(log));
    if(mode!="missing"){std::ofstream f(ini);f<<"[Diagnostics]\nLogging="<<(on?"1":mode=="off"?"0":mode=="invalid"?"1garbage":"")<<"\nLogFlushIntervalMs=1000\n";}
    const auto threads=ThreadCount();
    native_freeroam::InitializeLogging(GetModuleHandleW(nullptr));
    assert(native_freeroam::LoggingEnabled()==on);
    native_freeroam::Log(native_freeroam::LogLevel::Info,"format bypass: %s",on?"enabled":reinterpret_cast<const char*>(1));
    native_freeroam::LogTelemetry({}, {}, 10);
    if(on){Sleep(1500);assert(std::filesystem::exists(log));assert(std::filesystem::file_size(log)>0);assert(native_freeroam::traversals==1);}
    else{assert(!std::filesystem::exists(log));assert(ThreadCount()==threads);assert(native_freeroam::traversals==0);}
    std::cout<<"PASS diagnostic logging "<<mode<<": file, formatting, worker and telemetry gates\n";
}
