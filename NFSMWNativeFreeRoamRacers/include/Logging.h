#pragma once

#include <Windows.h>

namespace native_freeroam {

enum class LogLevel { Info, Warning, Error };

void InitializeLogging(HMODULE module) noexcept;
void Log(LogLevel level, const char* format, ...) noexcept;

}  // namespace native_freeroam
