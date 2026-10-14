$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'Verify-NFSMWNativeFreeRoamRacers.ps1')
$baseline=Join-Path $projectRoot '..\rollback\2026-09-15-pre-alpha50-spawn-eligibility'
foreach($folder in 'src','include','config') {
 foreach($file in Get-ChildItem (Join-Path $baseline $folder) -File) {
  if($file.Name -in 'Plugin.cpp','Runtime.cpp','VehiclePerformance.inl') {continue}
  if((Get-FileHash $file.FullName).Hash -ne (Get-FileHash (Join-Path $projectRoot ($folder+'\'+$file.Name))).Hash) {throw "Unrelated source changed: $($file.Name)"}
 }
}
$source=Get-Content (Join-Path $projectRoot 'src\VehiclePerformance.inl') -Raw
$guards=[regex]::Matches($source,'\{(0x[0-9A-Fa-f]+),\{([^}]+)\}\}')
if($guards.Count -ne 7){throw 'Unexpected performance guard count'}
foreach($guard in $guards) {
 $va=[Convert]::ToUInt32($guard.Groups[1].Value.Substring(2),16)
 $expected=($guard.Groups[2].Value.Split(',') | ForEach-Object { '{0:X2}' -f [int]$_ }) -join ' '
 if((ReadTargetBytes $va 8).Replace('-',' ') -ne $expected){throw "Performance guard mismatch $va"}
 'PASS performance read/apply native guard {0:X8}' -f $va
}
if($source -match 'SetMaximum|NosAdded|WritePrivateProfile|MH_CreateHook'){throw 'Unexpected unlock or native hook write'}
if(!$source.Contains('(2,type,shop,0,false)') -or !$source.Contains('__finally')){throw 'Career filter / release missing'}
$selection=Get-Content (Join-Path $projectRoot 'src\SpawnEligibility.inl') -Raw
if($selection -match 'TERZO|55A612D2|WriteFile|CreateFile|MakeAppearance|ConstructTrafficSeed'){throw 'Selection must be generic, allocation-free of game vehicles and not persisted'}
if(!$selection.Contains('g_palette.size()>5') -or !$selection.Contains('std::remove_if')){throw 'Selection bound / stale bag filtering missing'}
$runtime=Get-Content (Join-Path $projectRoot 'src\Runtime.cpp') -Raw
$spawn=$runtime.Substring($runtime.IndexOf('bool SpawnAtAnchor('))
if($spawn.IndexOf('SelectPreparedSpawnVehicle(') -gt $spawn.IndexOf('MakeAppearance(')){throw 'Eligibility must precede appearance'}
if(!$runtime.Contains('if(candidateWait)') -or !$runtime.Contains('g_consecutiveSpawnFailures=0;g_spawnFailureBackoffSeconds=0.0f;return;')){throw 'Eligibility waiting counted as failure'}
'PASS native performance ABI and regular career locks retained; no unlock writes'
'PASS AI, battle, HUD, audio, calibration format/data and INIs unchanged from alpha49'
'PASS max-five generic eligibility filter before appearance; no-candidate wait separated from genuine spawn failures'
