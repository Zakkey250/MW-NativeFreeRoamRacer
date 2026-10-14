param(
    [Parameter(Mandatory=$true)][string]$ExpectedNewHash,
    [string]$GameDirectory=$(Join-Path $(if($env:NFS_GAME_ROOT){$env:NFS_GAME_ROOT}elseif($env:COMPUTERNAME -eq 'ZAKKEY'){'E:\EA GAMES'}else{'\\ZAKKEY\EA GAMES'}) 'Need for Speed Most Wanted Main')
)
$ErrorActionPreference='Stop'
$project=Split-Path $PSScriptRoot
$workspace=Split-Path $project
$game=(Resolve-Path -LiteralPath $GameDirectory).Path
$evidence=Join-Path $workspace 'diagnostics/encounter-20260918-alpha63'
$installed=Join-Path $game 'scripts/NFSMWNativeFreeRoamRacers.asi'
$artifact=Join-Path $project 'artifacts/Release/NFSMWNativeFreeRoamRacers.asi'
$backup=Join-Path $workspace 'rollback/2026-09-18-pre-alpha63-native-start/installed/NFSMWNativeFreeRoamRacers.asi'
$oldHash='0463D2BFBD229AF7D59D97613B7C52C7EF1609059EFC42E0316F36346E3FFBA6'
function Hash([string]$path){(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash}
function Stopped {
    if(Get-CimInstance Win32_Process | Where-Object {$_.Name -in 'speed.exe','nfsMW.exe'}) {
        throw 'User game running: defer deployment; never terminate it.'
    }
}
Stopped
if((Hash $installed) -ne $oldHash -or (Hash $backup) -ne $oldHash -or
    (Hash $artifact) -ne $ExpectedNewHash){throw 'Unexpected build or rollback hash'}
$config=Get-Content -LiteralPath (Join-Path $game 'Launch-SV-Safe.json') -Raw | ConvertFrom-Json
$state=Get-Content -LiteralPath $config.StatePath -Raw | ConvertFrom-Json
if($state.GameDirectory -ne $game){throw 'SV installation mismatch'}
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
Stopped
if((Hash $installed) -ne $oldHash -or (Hash $artifact) -ne $ExpectedNewHash){throw 'Target changed during audit'}
Copy-Item -LiteralPath $artifact -Destination $installed -Force
if((Hash $installed) -ne $ExpectedNewHash){throw 'Installed hash mismatch'}
& (Join-Path $workspace 'tools/Verify-NFSPatcherRuntime.ps1') -GameDirectory $game | Tee-Object (Join-Path $evidence 'verify-after.txt')
if($LASTEXITCODE){throw 'Target verification failed'}
$after=Snapshot
$after | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $evidence 'deployment-after.json') -Encoding utf8
$changed=@($before.Keys | Where-Object {$after[$_] -ne $before[$_]})
if($before.Count -ne $after.Count -or $changed.Count -ne 1 -or $changed[0] -ne $installed){throw 'Unexpected installation changes'}
"PASS audited=$($before.Count) changed=1 NFR ASI only; INIs, LB, other MODs, Shadow, Vault and original save unchanged"
"PASS installed alpha.63 SHA256=$ExpectedNewHash; no game launched; live acceptance pending"
