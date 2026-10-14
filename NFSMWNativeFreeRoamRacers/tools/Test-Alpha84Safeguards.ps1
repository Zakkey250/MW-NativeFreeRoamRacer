$ErrorActionPreference='Stop'
$project=Split-Path $PSScriptRoot
$baseline=Join-Path (Split-Path $project) 'rollback/2026-09-22-pre-alpha84/source'
function Read([string]$p){(Get-Content -LiteralPath $p -Raw).Replace("`r`n","`n")}
$old=Read (Join-Path $baseline 'src/Runtime.cpp');$new=Read (Join-Path $project 'src/Runtime.cpp')
$oldHeader=@'
void LogTelemetry(const VehicleSnapshot& player,
                  const std::vector<VehicleSnapshot>& vehicles,
                  const float dt) {
    g_telemetryTimer += dt;
    if (g_telemetryTimer < g_settings.telemetryIntervalSeconds) return;
    g_telemetryTimer = 0.0f;
'@
$newHeader=@'
// Keep the native SEH probe in a separate function from the RAII batch scope.
void LogTelemetryRows(const VehicleSnapshot& player,
                      const std::vector<VehicleSnapshot>& vehicles) {
'@
$oldHeader=$oldHeader.Replace("`r`n","`n");$newHeader=$newHeader.Replace("`r`n","`n")
if(!$old.Contains($oldHeader) -or !$new.Contains($newHeader)){throw 'Expected report headers missing'}
$expected=$old.Replace($oldHeader,$newHeader).Replace('alpha.83-prepared-appearance-retention','alpha.84-telemetry-log-io')
$expected=$expected.Replace('template<typename T> bool AudioRead(const void* address,T* output) noexcept {',"#include `"TelemetryDispatch.inl`"`n`ntemplate<typename T> bool AudioRead(const void* address,T* output) noexcept {")
if($expected -cne $new){throw 'Unexpected Runtime change beyond report wrapper and version'}
foreach($folder in @('src','include','config')) {
    foreach($file in Get-ChildItem -LiteralPath (Join-Path $baseline $folder) -Recurse -File) {
        $relative=$file.FullName.Substring($baseline.Length+1)
        if($relative -in 'src\Runtime.cpp','include\Version.h'){continue}
        if((Get-FileHash $file.FullName).Hash -ne (Get-FileHash (Join-Path $project $relative)).Hash){throw "Unexpected change $relative"}
    }
}
'PASS telemetry reads/log content unchanged; all existing gameplay/audio/LB/config source unchanged; only report wrapper and version modified'
