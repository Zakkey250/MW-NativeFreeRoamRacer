param([string]$GameDirectory=$(Join-Path $(if($env:NFS_GAME_ROOT){$env:NFS_GAME_ROOT}elseif($env:COMPUTERNAME -eq 'ZAKKEY'){'E:\EA GAMES'}else{'\\ZAKKEY\EA GAMES'}) 'Need for Speed Most Wanted Main'),[switch]$VerifyOnly)
$ErrorActionPreference='Stop'
$project=Split-Path $PSScriptRoot
$workspace=Split-Path $project
$game=(Resolve-Path -LiteralPath $GameDirectory).Path
$evidence=Join-Path $workspace 'diagnostics/encounter-20260920-alpha74'
$backup=Join-Path $workspace 'rollback/2026-09-20-pre-alpha74/installed'
$targets=@(
    @{Name='NFSMWNativeFreeRoamRacers.asi';Artifact=(Join-Path $project 'artifacts/Release/NFSMWNativeFreeRoamRacers.asi');Old='EE723908771F08BE39B53F8C2FE6276D3343083CB1680730FDE5F76B1B58FEE2';New='64B7A5A4B41828212FA578ADFBAB4F1ED15AF6BCD1478CC188342E565F74C9DA'}
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
    $expectedInstalled=if($VerifyOnly){$target.New}else{$target.Old}
    if((Hash $target.Installed) -ne $expectedInstalled -or (Hash (Join-Path $backup $target.Name)) -ne $target.Old -or (Hash $target.Artifact) -ne $target.New){throw 'Unexpected target/artifact/backup hash'}
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
if($VerifyOnly){
    $saved=Get-Content -LiteralPath (Join-Path $evidence 'deployment-before.json') -Raw | ConvertFrom-Json
    $before=@{};foreach($property in $saved.PSObject.Properties){$before[$property.Name]=$property.Value}
}else{
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
}
# A previous native command (e.g. git diff --no-index exit 1) is unrelated to
# the PowerShell hook validator. Clear only the inherited exit status.
$global:LASTEXITCODE=0
& (Join-Path $workspace 'tools/Verify-NFSPatcherRuntime.ps1') -GameDirectory $game | Tee-Object (Join-Path $evidence 'verify-after-complete.txt')
if($LASTEXITCODE){throw 'Target verification failed'}
$after=Snapshot
$after | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $evidence 'deployment-after.json') -Encoding utf8
$changed=@($before.Keys | Where-Object {$after[$_] -ne $before[$_]})
if($before.Count -ne $after.Count -or $changed.Count -ne $targets.Count -or @($changed | Where-Object {$_ -notin $targets.Installed}).Count){throw 'Unexpected installation changes'}
"PASS audited=$($before.Count) changed=1 NFR ASI only; all INIs, LB ASI, other MODs, all Shadow/Vault/original profiles unchanged"
foreach($target in $targets){"PASS installed $($target.Name) SHA256=$($target.New)"}
"No game launched; runtime activation and replenishment acceptance pending."
