param([string]$GameDirectory=$(Join-Path $(if($env:NFS_GAME_ROOT){$env:NFS_GAME_ROOT}elseif($env:COMPUTERNAME -eq 'ZAKKEY'){'E:\EA GAMES'}else{'\\ZAKKEY\EA GAMES'}) 'Need for Speed Most Wanted Main'))
$ErrorActionPreference='Stop'
$project=Split-Path $PSScriptRoot
$workspace=Split-Path $project
$game=(Resolve-Path -LiteralPath $GameDirectory).Path
$evidence=Join-Path $workspace 'diagnostics/encounter-20260920-alpha75'
$backup=Join-Path $workspace 'rollback/2026-09-20-pre-alpha75/installed'
function Hash([string]$path){(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash}
function Stopped {
    if(Get-CimInstance Win32_Process | Where-Object {$_.Name -in 'speed.exe','nfsMW.exe'}){throw 'User game running; defer, never terminate.'}
}
Stopped
$config=Get-Content -LiteralPath (Join-Path $game 'Launch-SV-Safe.json') -Raw | ConvertFrom-Json
$state=Get-Content -LiteralPath $config.StatePath -Raw | ConvertFrom-Json
if($state.GameDirectory -ne $game){throw 'SV installation mismatch'}
$targets=@(
    @{Name='NFSMWNativeFreeRoamRacers.asi';Artifact=(Join-Path $project 'artifacts/Release/NFSMWNativeFreeRoamRacers.asi');Old='64B7A5A4B41828212FA578ADFBAB4F1ED15AF6BCD1478CC188342E565F74C9DA';New='832BCE3AD8B6E86A1E3A2577C552C71DB974891580DBDC24FF6E074A8A645511'},
    @{Name='NFSMWLimitBreaker.ini';Artifact=(Join-Path $evidence 'NFSMWLimitBreaker.test.ini');Old=(Hash (Join-Path $backup 'NFSMWLimitBreaker.ini'));New=(Hash (Join-Path $evidence 'NFSMWLimitBreaker.test.ini'))}
)
# Only enable the existing optional audio switch in the installed test profile.
$oldIni=Get-Content -LiteralPath (Join-Path $backup 'NFSMWLimitBreaker.ini') -Raw
$testIni=Get-Content -LiteralPath $targets[1].Artifact -Raw
if(([regex]::Matches($oldIni,'(?m)^LBEngineAudioExpansion = 0\r?$')).Count -ne 1 -or
    $oldIni.Replace('LBEngineAudioExpansion = 0','LBEngineAudioExpansion = 1').Replace("`r`n","`n") -cne $testIni.Replace("`r`n","`n")){
    throw 'Test INI contains unexpected changes'
}
foreach($t in $targets){
    $t.Installed=Join-Path $game ('scripts/'+$t.Name)
    if((Hash $t.Installed) -ne $t.Old -or (Hash (Join-Path $backup $t.Name)) -ne $t.Old -or (Hash $t.Artifact) -ne $t.New){throw 'Unexpected target, backup or build'}
}
function Snapshot {
    $files=@(
        Get-ChildItem -LiteralPath $game -File | Where-Object {$_.Extension -in '.exe','.dll','.ini','.json'}
        Get-ChildItem -LiteralPath (Join-Path $game 'scripts') -Recurse -File | Where-Object {$_.Extension -in '.asi','.dll','.ini','.json','.tpk','.wav','.mp3','.session' -or $_.Name -eq 'VehicleCatalog.cache.bin'}
        Get-ChildItem -LiteralPath $game -Directory | Where-Object {$_.Name -like 'SVSaves_*'} | ForEach-Object {Get-ChildItem -LiteralPath $_.FullName -Recurse -File}
        Get-ChildItem -LiteralPath (Join-Path $game 'SaveVirtualizer') -Recurse -File | Where-Object {$_.FullName -match '\\vault\\' -or $_.Name -eq 'state.json'}
        Get-Item -LiteralPath $config.StatePath,$state.ManifestPath
        Get-ChildItem -LiteralPath (Split-Path (Split-Path $state.OriginalSourceFile)) -Recurse -File
    ) | Sort-Object FullName -Unique
    $r=@{};foreach($file in $files){$r[$file.FullName]=Hash $file.FullName};return $r
}
if(Test-Path -LiteralPath (Join-Path $evidence 'deployment-before.json')){throw 'Existing deployment audit; do not overwrite'}
$before=Snapshot
$before | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $evidence 'deployment-before.json') -Encoding utf8
Stopped
foreach($t in $targets){if((Hash $t.Installed) -ne $t.Old -or (Hash $t.Artifact) -ne $t.New){throw 'Concurrent target change'}}
try {
    foreach($t in $targets){Copy-Item -LiteralPath $t.Artifact -Destination $t.Installed -Force;if((Hash $t.Installed) -ne $t.New){throw 'Copy mismatch'}}
}catch{
    Stopped
    foreach($t in $targets){Copy-Item -LiteralPath (Join-Path $backup $t.Name) -Destination $t.Installed -Force}
    throw
}
$global:LASTEXITCODE=0
& (Join-Path $workspace 'tools/Verify-NFSPatcherRuntime.ps1') -GameDirectory $game | Tee-Object (Join-Path $evidence 'verify-after.txt')
if($LASTEXITCODE){throw 'Post-deployment target validation failed'}
& (Join-Path $project 'tools/Verify-NFSMWNativeFreeRoamRacers.ps1') -GameDirectory $game | Tee-Object (Join-Path $evidence 'nfrr-after.txt')
$after=Snapshot
$after | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $evidence 'deployment-after.json') -Encoding utf8
$changed=@($before.Keys | Where-Object {$after[$_] -ne $before[$_]})
if($before.Count -ne $after.Count -or $changed.Count -ne $targets.Count -or @($changed | Where-Object {$_ -notin $targets.Installed}).Count){throw 'Unexpected installed changes'}
"PASS audited=$($before.Count); only NFRR ASI and LB audio opt-in INI changed"
foreach($t in $targets){"PASS $($t.Name) SHA256=$($t.New)"}
"NFRR INI, LB binary, all other assets/profiles preserved. No runtime acceptance claimed."
