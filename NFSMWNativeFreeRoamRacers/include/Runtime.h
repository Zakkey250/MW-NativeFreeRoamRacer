#pragma once

namespace native_freeroam {

bool VerifyHostExecutable() noexcept;
bool InstallRuntime() noexcept;
void StopEncounterVoiceForProcessExit() noexcept;

}  // namespace native_freeroam
