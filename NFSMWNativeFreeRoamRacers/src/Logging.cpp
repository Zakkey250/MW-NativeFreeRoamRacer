#include "Logging.h"

#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <mutex>

namespace native_freeroam {
namespace {

std::mutex g_logMutex;
wchar_t g_logPath[MAX_PATH]{};
thread_local unsigned g_logBatchDepth=0;
thread_local HANDLE g_logBatchFile=INVALID_HANDLE_VALUE;

const char* LevelName(const LogLevel level) noexcept {
    switch (level) {
        case LogLevel::Warning: return "WARN";
        case LogLevel::Error: return "ERROR";
        default: return "INFO";
    }
}

}  // namespace

LogBatch::LogBatch() noexcept { ++g_logBatchDepth; }
LogBatch::~LogBatch() noexcept {
    if(--g_logBatchDepth==0 && g_logBatchFile!=INVALID_HANDLE_VALUE) {
        CloseHandle(g_logBatchFile);
        g_logBatchFile=INVALID_HANDLE_VALUE;
    }
}

void InitializeLogging(const HMODULE module) noexcept {
    wchar_t path[MAX_PATH]{};
    if (GetModuleFileNameW(module, path, MAX_PATH) == 0) return;
    wchar_t* extension = wcsrchr(path, L'.');
    if (extension != nullptr)
        wcscpy_s(extension, MAX_PATH - (extension - path), L".log");
    wcscpy_s(g_logPath, path);
}

void Log(const LogLevel level, const char* format, ...) noexcept {
    if (g_logPath[0] == L'\0') return;
    char message[2048]{};
    va_list arguments;
    va_start(arguments, format);
    vsnprintf_s(message, sizeof(message), _TRUNCATE, format, arguments);
    va_end(arguments);

    SYSTEMTIME time{};
    GetLocalTime(&time);
    char line[2304]{};
    snprintf(line, sizeof(line),
             "%04u-%02u-%02u %02u:%02u:%02u.%03u [%s] %s\r\n",
             time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute,
             time.wSecond, time.wMilliseconds, LevelName(level), message);

    std::lock_guard<std::mutex> lock(g_logMutex);
    HANDLE file = g_logBatchDepth?g_logBatchFile:INVALID_HANDLE_VALUE;
    if(file==INVALID_HANDLE_VALUE) file = CreateFileW(g_logPath, FILE_APPEND_DATA,
                              FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                              OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return;
    if(g_logBatchDepth) g_logBatchFile=file;
    DWORD written = 0;
    const auto length=static_cast<DWORD>(strlen(line));
    const bool complete=WriteFile(file,line,length,&written,nullptr)!=FALSE && written==length;
    // Do not replay partially written rows. On failure discard this handle so
    // the next row can retry opening, just as the original per-row logger did.
    if(!g_logBatchDepth || !complete) {
        CloseHandle(file);
        if(g_logBatchDepth) g_logBatchFile=INVALID_HANDLE_VALUE;
    }
}

}  // namespace native_freeroam
