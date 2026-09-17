#include "Logging.h"
#include "Runtime.h"
#include "UpdateNotice.h"
#include "Version.h"

#include <Windows.h>

namespace {

DWORD WINAPI InitializePlugin(void* context) noexcept {
    Sleep(500);
    native_freeroam::Log(
        native_freeroam::LogLevel::Info,
        "NFSMW Native Free Roam Racers %s initializing",native_freeroam::kVersion);
    if (!native_freeroam::VerifyHostExecutable()) return 0;
    if (!native_freeroam::InstallRuntime()) {
        native_freeroam::Log(native_freeroam::LogLevel::Error,
                             "Runtime installation failed; no hook remains active");
        return 0;
    }
    native_freeroam::CheckForStartupUpdate(static_cast<HMODULE>(context));
    return 0;
}

}  // namespace

BOOL APIENTRY DllMain(const HMODULE module, const DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_DETACH) native_freeroam::StopEncounterVoiceForProcessExit();
    if (reason != DLL_PROCESS_ATTACH) return TRUE;
    DisableThreadLibraryCalls(module);
    native_freeroam::InitializeLogging(module);
    const HANDLE thread =
        CreateThread(nullptr, 0, InitializePlugin, module, 0, nullptr);
    if (thread != nullptr) CloseHandle(thread);
    return TRUE;
}
