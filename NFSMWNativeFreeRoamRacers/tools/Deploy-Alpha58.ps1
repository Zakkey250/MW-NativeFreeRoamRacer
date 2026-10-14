param([string]$GameDirectory=$(Join-Path $(if($env:NFS_GAME_ROOT){$env:NFS_GAME_ROOT}elseif($env:COMPUTERNAME -eq 'ZAKKEY'){'E:\EA GAMES'}else{'\\ZAKKEY\EA GAMES'}) 'Need for Speed Most Wanted Main'))
$ErrorActionPreference='Stop'
$project=Split-Path $PSScriptRoot;$workspace=Split-Path $project
$game=(Resolve-Path -LiteralPath $GameDirectory).Path
$evidence=Join-Path $workspace 'diagnostics/encounter-20260917-alpha58'
$backup=Join-Path $workspace 'rollback/2026-09-17-pre-alpha58-cruise/installed'
$installed=Join-Path $game 'scripts/NFSMWNativeFreeRoamRacers.asi'
$ini=Join-Path $game 'scripts/NFSMWNativeFreeRoamRacers.ini'
$artifact=Join-Path $project 'artifacts/Release/NFSMWNativeFreeRoamRacers.asi'
$iniSource=Join-Path $evidence 'NFSMWNativeFreeRoamRacers.ini'
$oldHash='DB9B94D5A830A923F00B8568AF5A3D55C23512D99695502354A33579CF3859C7'
$newHash='EF71DE0EF331D5537231DDA983997CCC6B39D70E38CCB09D5A31DB849972329C'
function Hash([string]$path){(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash}
function Stopped {
    if(Get-CimInstance Win32_Process | Where-Object {$_.Name -in 'speed.exe','nfsMW.exe'}) {
        throw 'User game running: defer deployment; do not terminate it.'
    }
}
function IniValues([string]$path) {
    $result=@{};$section=''
    foreach($line in Get-Content -LiteralPath $path) {
        if($line -match '^\s*\[([^\]]+)\]'){$section=$Matches[1]}
        elseif($line -match '^\s*([^;#=]+?)\s*=\s*([^;#]*)'){$result["$section/$($Matches[1].Trim())"]=$Matches[2].Trim()}
    }
    return $result
}
Stopped
if((Hash $installed) -ne $oldHash -or (Hash $artifact) -ne $newHash){throw 'Unexpected NFR build'}
if(Test-Path -LiteralPath $backup){throw 'Deployment backup already exists'}
$oldValues=IniValues $ini;$newValues=IniValues $iniSource
foreach($key in $oldValues.Keys){if($newValues[$key] -ne $oldValues[$key]){throw "Existing setting changed: $key"}}
if($newValues.Count -ne $oldValues.Count+2 -or $newValues['Population/CruisingSpeedPercent'] -ne '60' -or
    $newValues['Encounter/StartSpeedToleranceKmh'] -ne '10'){throw 'INI additions differ'}
& (Join-Path $workspace 'tools/Verify-NFSPatcherRuntime.ps1') -GameDirectory $game | Tee-Object (Join-Path $evidence 'runtime-before.txt')
$config=Get-Content -LiteralPath (Join-Path $game 'Launch-SV-Safe.json') -Raw | ConvertFrom-Json
$state=Get-Content -LiteralPath $config.StatePath -Raw | ConvertFrom-Json
if($state.GameDirectory -ne $game){throw 'SV game directory mismatch'}
function Snapshot {
    $files=@(
        Get-ChildItem -LiteralPath $game -File | Where-Object {$_.Extension -in '.exe','.dll','.ini','.json'}
        Get-ChildItem -LiteralPath (Join-Path $game 'scripts') -Recurse -File | Where-Object {$_.Extension -in '.asi','.dll','.ini','.json','.tpk','.wav','.mp3' -or $_.Name -eq 'VehicleCatalog.cache.bin'}
        Get-ChildItem -LiteralPath $game -Directory | Where-Object {$_.Name -like 'SVSaves_*'} | ForEach-Object {Get-ChildItem -LiteralPath $_.FullName -Recurse -File}
        Get-ChildItem -LiteralPath $state.VaultProfileDirectory -Recurse -File
        Get-Item -LiteralPath $config.StatePath,$state.ManifestPath,$state.OriginalSourceFile
    ) | Sort-Object FullName -Unique
    $result=@{};foreach($file in $files){$result[$file.FullName]=Hash $file.FullName};return $result
}
$before=Snapshot
$before | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $evidence 'deployment-before.json') -Encoding utf8
New-Item -ItemType Directory -Path $backup | Out-Null
Copy-Item -LiteralPath $installed,$ini -Destination $backup
if((Hash (Join-Path $backup 'NFSMWNativeFreeRoamRacers.asi')) -ne $oldHash){throw 'Backup mismatch'}
Stopped
if((Hash $installed) -ne $before[$installed] -or (Hash $ini) -ne $before[$ini]){throw 'Target changed during audit'}
Copy-Item -LiteralPath $artifact -Destination $installed -Force
Copy-Item -LiteralPath $iniSource -Destination $ini -Force
if((Hash $installed) -ne $newHash -or (Hash $ini) -ne (Hash $iniSource)){throw 'Installed hash mismatch'}
& (Join-Path $workspace 'tools/Verify-NFSPatcherRuntime.ps1') -GameDirectory $game | Tee-Object (Join-Path $evidence 'runtime-after.txt')
$after=Snapshot
$after | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $evidence 'deployment-after.json') -Encoding utf8
$changed=@($before.Keys | Where-Object {$after[$_] -ne $before[$_]})
if($before.Count -ne $after.Count -or $changed.Count -ne 2 -or @($changed | Where-Object {$_ -notin @($installed,$ini)}).Count){throw 'Unexpected installation changes'}
"PASS audited=$($before.Count) changed=2 (NFR ASI + INI only); other mods, saves and production SV unchanged"
"PASS installed alpha.58 ASI=$newHash; no game launched; actual driving not verified"
