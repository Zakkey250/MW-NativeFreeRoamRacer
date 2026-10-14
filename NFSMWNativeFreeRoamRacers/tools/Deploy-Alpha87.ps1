param([string]$GameDirectory=$(Join-Path $(if($env:NFS_GAME_ROOT){$env:NFS_GAME_ROOT}elseif($env:COMPUTERNAME -eq 'ZAKKEY'){'E:\EA GAMES'}else{'\\ZAKKEY\EA GAMES'}) 'Need for Speed Most Wanted Main'))
$ErrorActionPreference='Stop'
$project=Split-Path $PSScriptRoot
$workspace=Split-Path $project
$game=(Resolve-Path -LiteralPath $GameDirectory).Path
$evidence=Join-Path $workspace 'diagnostics/encounter-20260922-alpha87'
$backup=Join-Path $workspace 'rollback/2026-09-22-pre-alpha87/installed'
function Hash([string]$path){(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash}
function Stopped {
    if(Get-CimInstance Win32_Process | Where-Object {$_.Name -in 'speed.exe','nfsMW.exe'}){throw 'User game running; defer, never terminate.'}
}
Stopped
$config=Get-Content -LiteralPath (Join-Path $game 'Launch-SV-Safe.json') -Raw | ConvertFrom-Json
$state=Get-Content -LiteralPath $config.StatePath -Raw | ConvertFrom-Json
if($state.GameDirectory -ne $game){throw 'SV installation mismatch'}
$old='C782C85317CCEFEB5AE7646F70662EF9A62620D26924AFAB38CA0DEEE2278BB0'
$new='5AE641C021EFA93213CF91868CB1A9F1A050261126F0F3196B73DD098766063F'
$installed=Join-Path $game 'scripts/NFSMWNativeFreeRoamRacers.asi'
$artifact=Join-Path $project 'artifacts/Release/NFSMWNativeFreeRoamRacers.asi'
$rollback=Join-Path $backup 'NFSMWNativeFreeRoamRacers.asi'
if((Hash $installed) -ne $old -or (Hash $rollback) -ne $old -or (Hash $artifact) -ne $new){throw 'Unexpected target, backup or build'}
if((Hash (Join-Path $game 'scripts/NFSMWNativeFreeRoamRacers.ini')) -ne (Hash (Join-Path $backup 'NFSMWNativeFreeRoamRacers.ini'))){throw 'INI changed since backup; review before deployment'}
function Snapshot {
    $files=@(
        Get-Item -LiteralPath (Join-Path $game 'CARS/CIVICFN2/GEOMETRY.BIN'),(Join-Path $game 'CARS/SPOILER_HATCH/GEOMETRY.BIN')
        Get-ChildItem -LiteralPath (Join-Path $game 'GLOBAL') -Recurse -File
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
if((Hash $installed) -ne $old -or (Hash $artifact) -ne $new){throw 'Concurrent target change'}
try {
    Copy-Item -LiteralPath $artifact -Destination $installed -Force
    if((Hash $installed) -ne $new){throw 'Copy mismatch'}
    $global:LASTEXITCODE=0
    & (Join-Path $workspace 'tools/Verify-NFSPatcherRuntime.ps1') -GameDirectory $game | Tee-Object (Join-Path $evidence 'verify-after.txt')
    if($LASTEXITCODE){throw 'Post-deployment target validation failed'}
    & (Join-Path $project 'tools/Verify-NFSMWNativeFreeRoamRacers.ps1') -GameDirectory $game | Tee-Object (Join-Path $evidence 'nfrr-after.txt')
    $after=Snapshot
    $after | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $evidence 'deployment-after.json') -Encoding utf8
    $changed=@($before.Keys | Where-Object {$after[$_] -ne $before[$_]})
    if($before.Count -ne $after.Count -or $changed.Count -ne 1 -or $changed[0] -ne $installed){throw 'Unexpected installed changes'}
}catch{
    Stopped
    Copy-Item -LiteralPath $rollback -Destination $installed -Force
    throw
}
"PASS audited=$($before.Count); only NFRR ASI changed; SHA256=$new"
"All INIs, LB, other audited assets/profiles preserved. Runtime acceptance pending."
