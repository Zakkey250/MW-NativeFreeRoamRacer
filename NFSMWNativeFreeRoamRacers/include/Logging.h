#pragma once

#include <Windows.h>

namespace native_freeroam {

enum class LogLevel { Info, Warning, Error };

void InitializeLogging(HMODULE module) noexcept;
void Log(LogLevel level, const char* format, ...) noexcept;

// Reuse only the current thread's file handle for a short synchronous report.
// No queued messages or process-lifetime handle; each Log still writes at once.
class LogBatch final {
public:
    LogBatch() noexcept;
    ~LogBatch() noexcept;
    LogBatch(const LogBatch&)=delete;
    LogBatch& operator=(const LogBatch&)=delete;
};

}  // namespace native_freeroam
