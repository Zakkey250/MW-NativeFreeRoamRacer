#include "Logging.h"
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <algorithm>
namespace native_freeroam {
namespace {
std::mutex g_logMutex;
bool g_loggingEnabled=false;
HANDLE g_logFile=INVALID_HANDLE_VALUE,g_logWriter=nullptr;
DWORD g_flushMs=10000,g_maxLines=20;
std::string g_pending;
volatile LONG g_dropped=0;
constexpr size_t kMaxBytes=256*1024;
const char* LevelName(LogLevel level) noexcept {return level==LogLevel::Error?"ERROR":level==LogLevel::Warning?"WARN":"INFO";}
DWORD WINAPI Writer(void*) {
 std::string batch;batch.reserve(kMaxBytes);
 for(;;){
  Sleep(g_flushMs);
  {std::lock_guard lock(g_logMutex);batch.swap(g_pending);}
  if(!batch.empty()){DWORD written=0;WriteFile(g_logFile,batch.data(),DWORD(batch.size()),&written,nullptr);batch.clear();}
  // No retry/replay of partial writes, and no disk I/O on game callbacks.
  LONG dropped=InterlockedExchange(&g_dropped,0);
  if(dropped){char line[128];int n=sprintf_s(line,"DEBUG_LOG_DROPPED count=%ld bounded_queue=1\r\n",dropped);DWORD written;WriteFile(g_logFile,line,DWORD(n),&written,nullptr);}
 }
}
}
// Kept as API-compatible scope markers; the background writer owns the handle.
LogBatch::LogBatch() noexcept {}
LogBatch::~LogBatch() noexcept {}
bool LoggingEnabled() noexcept {return g_loggingEnabled;}
void InitializeLogging(HMODULE module) noexcept {
 wchar_t ini[MAX_PATH]{},path[MAX_PATH]{};
 if(!GetModuleFileNameW(module,ini,MAX_PATH))return;
 auto extension=wcsrchr(ini,L'.');if(!extension)return;
 wcscpy_s(extension,MAX_PATH-(extension-ini),L".ini");
 wchar_t loggingSetting[8]{};
 GetPrivateProfileStringW(L"Diagnostics",L"Logging",L"0",loggingSetting,8,ini);
 if(wcscmp(loggingSetting,L"1")!=0)return;
 wcscpy_s(path,ini);extension=wcsrchr(path,L'.');wcscpy_s(extension,MAX_PATH-(extension-path),L".log");
 g_flushMs=std::clamp<DWORD>(GetPrivateProfileIntW(L"Diagnostics",L"LogFlushIntervalMs",10000,ini),1000,60000);
 g_maxLines=std::clamp<DWORD>(GetPrivateProfileIntW(L"Diagnostics",L"LogMaxLinesPerSecond",20,ini),0,10000);
 try{g_pending.reserve(kMaxBytes);}catch(...){return;}
 g_logFile=CreateFileW(path,FILE_APPEND_DATA,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
 if(g_logFile==INVALID_HANDLE_VALUE)return;
 g_logWriter=CreateThread(nullptr,0,Writer,nullptr,0,nullptr);
 if(!g_logWriter){CloseHandle(g_logFile);g_logFile=INVALID_HANDLE_VALUE;return;}
 g_loggingEnabled=true;
}
void Log(LogLevel level,const char* format,...) noexcept {
 if(!g_loggingEnabled)return;
 std::unique_lock<std::mutex> lock(g_logMutex,std::try_to_lock);
 if(!lock.owns_lock()){InterlockedIncrement(&g_dropped);return;}
 DWORD now=GetTickCount();static DWORD window=0;static unsigned count=0;
 if(now-window>=1000){window=now;count=0;}
 if(level==LogLevel::Info&&g_maxLines&&count>=g_maxLines){InterlockedIncrement(&g_dropped);return;}
 char message[2048]{};va_list args;va_start(args,format);vsnprintf_s(message,sizeof(message),_TRUNCATE,format,args);va_end(args);
 SYSTEMTIME time{};GetLocalTime(&time);char line[2304]{};
 snprintf(line,sizeof(line),"%04u-%02u-%02u %02u:%02u:%02u.%03u [%s] %s\r\n",time.wYear,time.wMonth,time.wDay,time.wHour,time.wMinute,time.wSecond,time.wMilliseconds,LevelName(level),message);
 size_t bytes=strlen(line);if(g_pending.size()+bytes>kMaxBytes){InterlockedIncrement(&g_dropped);return;}
 g_pending.append(line,bytes);++count;
}
}
