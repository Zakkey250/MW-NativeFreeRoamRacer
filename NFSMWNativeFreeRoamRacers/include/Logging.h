#pragma once

#include <Windows.h>

namespace native_freeroam {

enum class LogLevel { Info, Warning, Error };

void InitializeLogging(HMODULE module) noexcept;
bool LoggingEnabled() noexcept;
void Log(LogLevel level, const char* format, ...) noexcept;

// Compatibility scope marker. Buffered diagnostics are drained by one writer;
// gameplay callbacks never open/write/close a diagnostic file.
class LogBatch final {
public:
    LogBatch() noexcept;
    ~LogBatch() noexcept;
    LogBatch(const LogBatch&)=delete;
    LogBatch& operator=(const LogBatch&)=delete;
};

}  // namespace native_freeroam
