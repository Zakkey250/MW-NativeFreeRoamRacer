#pragma once
#include <Windows.h>
namespace native_freeroam {
// Called only after successful runtime installation, outside the loader lock.
void CheckForStartupUpdate(HMODULE module) noexcept;
bool StartupNoticeAllowed() noexcept;
}
