param([string]$GameDirectory=$(Join-Path $(if($env:NFS_GAME_ROOT){$env:NFS_GAME_ROOT}elseif($env:COMPUTERNAME -eq 'ZAKKEY'){'E:\EA GAMES'}else{'\\ZAKKEY\EA GAMES'}) 'Need for Speed Most Wanted Main'))
$ErrorActionPreference='Stop'
$project=Split-Path $PSScriptRoot
$workspace=Split-Path $project
$game=(Resolve-Path -LiteralPath $GameDirectory).Path
$evidence=Join-Path $workspace 'diagnostics/encounter-20260918-alpha65'
$backup=Join-Path $workspace 'rollback/2026-09-18-pre-alpha65-anchor-reservation/installed'
$targets=@(
    @{Name='NFSMWNativeFreeRoamRacers.asi';Artifact=(Join-Path $project 'artifacts/Release/NFSMWNativeFreeRoamRacers.asi');Old='783B5AF68D503DCC8E7123DEABA63D4904347EB28A30C2FB1580ED615545E519';New='88A0833469E3DD841D0CADD9C8D303E232B850190F2FA45B7AC26866BCD1F39E'}
)
function Hash([string]$path){(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash}
function Stopped {
    if(Get-CimInstance Win32_Process | Where-Object {$_.Name -in 'speed.exe','nfsMW.exe'}){throw 'User game running: defer deployment; never terminate it.'}
}
Stopped
$config=Get-Content -LiteralPath (Join-Path $game 'Launch-SV-Safe.json') -Raw | ConvertFrom-Json
$state=Get-Content -LiteralPath $config.StatePath -Raw | ConvertFrom-Json
if($state.GameDirectory -ne $game){throw 'SV installation mismatch'}
foreach($target in $targets){
    $target.Installed=Join-Path $game ('scripts/'+$target.Name)
    if((Hash $target.Installed) -ne $target.Old -or (Hash (Join-Path $backup $target.Name)) -ne $target.Old -or (Hash $target.Artifact) -ne $target.New){throw 'Unexpected target/artifact/backup hash'}
}
function Snapshot {
    $files=@(
        Get-ChildItem -LiteralPath $game -File | Where-Object {$_.Extension -in '.exe','.dll','.ini','.json'}
        Get-ChildItem -LiteralPath (Join-Path $game 'scripts') -Recurse -File | Where-Object {$_.Extension -in '.asi','.dll','.ini','.json','.tpk','.wav','.mp3' -or $_.Name -eq 'VehicleCatalog.cache.bin'}
        Get-ChildItem -LiteralPath $game -Directory | Where-Object {$_.Name -like 'SVSaves_*'} | ForEach-Object {Get-ChildItem -LiteralPath $_.FullName -Recurse -File}
        Get-ChildItem -LiteralPath (Join-Path $game 'SaveVirtualizer') -Recurse -File | Where-Object {$_.FullName -match '\\vault\\' -or $_.Name -eq 'state.json'}
        Get-Item -LiteralPath $config.StatePath,$state.ManifestPath
        Get-ChildItem -LiteralPath (Split-Path (Split-Path $state.OriginalSourceFile)) -Recurse -File
    ) | Sort-Object FullName -Unique
    $result=@{};foreach($file in $files){$result[$file.FullName]=Hash $file.FullName};return $result
}
$before=Snapshot
$before | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $evidence 'deployment-before.json') -Encoding utf8
Stopped
foreach($target in $targets){if((Hash $target.Installed) -ne $target.Old -or (Hash $target.Artifact) -ne $target.New){throw 'Target changed during audit'}}
try {
    foreach($target in $targets){
        Copy-Item -LiteralPath $target.Artifact -Destination $target.Installed -Force
        if((Hash $target.Installed) -ne $target.New){throw 'Installed hash mismatch'}
    }
} catch {
    Stopped
    foreach($target in $targets){Copy-Item -LiteralPath (Join-Path $backup $target.Name) -Destination $target.Installed -Force}
    throw
}
& (Join-Path $workspace 'tools/Verify-NFSPatcherRuntime.ps1') -GameDirectory $game | Tee-Object (Join-Path $evidence 'verify-after.txt')
if($LASTEXITCODE){throw 'Target verification failed'}
$after=Snapshot
$after | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $evidence 'deployment-after.json') -Encoding utf8
$changed=@($before.Keys | Where-Object {$after[$_] -ne $before[$_]})
if($before.Count -ne $after.Count -or $changed.Count -ne 1 -or @($changed | Where-Object {$_ -notin $targets.Installed}).Count){throw 'Unexpected installation changes'}
"PASS audited=$($before.Count) changed=1 NFR ASI only; LB, INIs, other MODs, all Shadow/Vault/original profiles unchanged"
foreach($target in $targets){"PASS installed $($target.Name) SHA256=$($target.New)"}
"No game launched; runtime activation and replenishment acceptance pending."
