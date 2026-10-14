param([string]$GameDirectory=$(Join-Path $(if($env:NFS_GAME_ROOT){$env:NFS_GAME_ROOT}elseif($env:COMPUTERNAME -eq 'ZAKKEY'){'E:\EA GAMES'}else{'\\ZAKKEY\EA GAMES'}) 'Need for Speed Most Wanted Main'))
$ErrorActionPreference='Stop'
$project=Split-Path $PSScriptRoot;$workspace=Split-Path $project
$game=(Resolve-Path -LiteralPath $GameDirectory).Path
$evidence=Join-Path $workspace 'diagnostics/custom-ai-20260915-alpha53'
$backup=Join-Path $workspace 'rollback/2026-09-15-pre-alpha53-directional-trail'
$installed=Join-Path $game 'scripts/NFSMWNativeFreeRoamRacers.asi'
$artifact=Join-Path $project 'artifacts/Release/NFSMWNativeFreeRoamRacers.asi'
$oldHash='2188BCE919B2FEF7555D9BE6B550661E3B68B6E0B38680C0783497093B971212'
$newHash='462ACE1A8F9028FBED8C2D314C42D72F08A9D211E5FC6E81C74C757317BE8D7C'
function Hash([string]$path){(Get-FileHash -LiteralPath $path).Hash}
function Stopped {
    if(Get-CimInstance Win32_Process | Where-Object {$_.Name -in 'speed.exe','nfsMW.exe'}) {
        throw 'User game running: deployment deferred; do not terminate it.'
    }
}
Stopped
if((Hash $artifact) -ne $newHash -or (Hash $installed) -ne $oldHash -or
    (Hash (Join-Path $backup 'Release/NFSMWNativeFreeRoamRacers.asi')) -ne $oldHash) {throw 'ASI identity mismatch'}
$target=Get-Content (Join-Path $workspace 'integration/NFSMW-ExecutableTarget.json') -Raw | ConvertFrom-Json
if((Hash (Join-Path $game $target.executable)) -ne $target.sha256){throw 'Target EXE differs'}
$config=Get-Content (Join-Path $game 'Launch-SV-Safe.json') -Raw | ConvertFrom-Json
$state=Get-Content -LiteralPath $config.StatePath -Raw | ConvertFrom-Json
if($state.GameDirectory -ne $game){throw 'Production SV belongs to another installation'}
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
$beforePath=Join-Path $evidence 'deployment-before.json'
if(Test-Path -LiteralPath $beforePath){throw 'Deployment evidence exists; do not overwrite'}
& (Join-Path $workspace 'tools/Verify-NFSPatcherRuntime.ps1') -GameDirectory $game | Tee-Object (Join-Path $evidence 'runtime-before.txt')
$before=Snapshot
$before | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath $beforePath -Encoding utf8
Stopped
Copy-Item -LiteralPath $artifact -Destination $installed -Force
if((Hash $installed) -ne $newHash){throw 'Installed hash mismatch'}
& (Join-Path $workspace 'tools/Verify-NFSPatcherRuntime.ps1') -GameDirectory $game | Tee-Object (Join-Path $evidence 'runtime-after.txt')
$after=Snapshot
$after | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $evidence 'deployment-after.json') -Encoding utf8
$changed=@($before.Keys | Where-Object {$after[$_] -ne $before[$_]})
if($changed.Count -ne 1 -or $changed[0] -ne $installed -or $before.Count -ne $after.Count) {
    throw 'Unexpected installation diff; inspect evidence before play'
}
"PASS audited=$($before.Count) changed=1 (own ASI only) unchanged=$($before.Count-1); INI and production SV preserved"
"PASS installed alpha.53 ASI=$newHash; no game launched"
