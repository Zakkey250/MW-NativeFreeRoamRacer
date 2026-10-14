$ErrorActionPreference='Stop'
$project=Split-Path $PSScriptRoot;$workspace=Split-Path $project
$game=$(Join-Path $(if($env:NFS_GAME_ROOT){$env:NFS_GAME_ROOT}elseif($env:COMPUTERNAME -eq 'ZAKKEY'){'E:\EA GAMES'}else{'\\ZAKKEY\EA GAMES'}) 'Need for Speed Most Wanted')
$installed=Join-Path $game 'scripts\NFSMWNativeFreeRoamRacers.asi'
$artifact=Join-Path $project 'artifacts\Release\NFSMWNativeFreeRoamRacers.asi'
$backup=Join-Path $workspace 'rollback\2026-09-14-pre-alpha49-frontend-lifetime\NFSMWNativeFreeRoamRacers.asi'
$evidence=Join-Path $workspace 'diagnostics\portable-catalog-20260914-alpha49'
$oldHash='9F68726DD94E97D7626F928147F048D2D7AD4CFBF9DB9A7AC2824FC02D905209'
$newHash='AA57DE88E0FD57F8F02A5F390343E38847268507C17C9C7EFBF4A5304FBD7632'
function Hash([string]$p){(Get-FileHash -LiteralPath $p).Hash}
function Stopped {if(Get-CimInstance Win32_Process -Filter "Name='speed.exe'"){throw 'User game running; deployment deferred'}}
Stopped
if((Hash $installed) -ne $oldHash -or (Hash $backup) -ne $oldHash -or (Hash $artifact) -ne $newHash){throw 'Deployment/rollback identity mismatch'}
if((Hash (Join-Path $game 'scripts\NFSMWNativeFreeRoamRacers.tpk')) -ne 'CA929293186A10867B65729CE78AF89C5F5DE880239A402EC68E35E0169D32D1'){throw 'Icon pack mismatch'}
$manifest=Get-Content (Join-Path $workspace 'integration\NFSMW-ExecutableTarget.json') -Raw | ConvertFrom-Json
if((Hash (Join-Path $game 'speed.exe')) -ne $manifest.sha256){throw 'EXE mismatch'}
$config=Get-Content (Join-Path $game 'Launch-SV-Safe.json') -Raw | ConvertFrom-Json
$state=Get-Content $config.StatePath -Raw | ConvertFrom-Json
function Snapshot {
 $files=@(
  Get-ChildItem $game -File | Where-Object {$_.Extension -in '.exe','.dll','.ini','.json'}
  Get-ChildItem (Join-Path $game 'scripts') -Recurse -File | Where-Object {$_.Extension -in '.asi','.dll','.ini','.json','.tpk'}
  Get-ChildItem (Join-Path $game 'scripts\NativeFreeRoamRacers\encounter') -Recurse -File
  Get-ChildItem (Split-Path $state.ShadowProfileDirectory) -Recurse -File
  Get-ChildItem $state.VaultProfileDirectory -Recurse -File
  Get-Item $config.StatePath,$state.ManifestPath,$state.OriginalSourceFile
 ) | Sort-Object FullName -Unique
 $result=@{};foreach($f in $files){$result[$f.FullName]=Hash $f.FullName};return $result
}
New-Item -ItemType Directory -Path $evidence -Force | Out-Null
& (Join-Path $workspace 'tools\Verify-NFSPatcherRuntime.ps1') | Tee-Object (Join-Path $evidence 'runtime-before.txt')
$before=Snapshot
$before | ConvertTo-Json -Depth 4 | Set-Content (Join-Path $evidence 'deployment-before.json') -Encoding utf8
Stopped
Copy-Item -LiteralPath $artifact -Destination $installed -Force
& (Join-Path $workspace 'tools\Verify-NFSPatcherRuntime.ps1') | Tee-Object (Join-Path $evidence 'runtime-after.txt')
$after=Snapshot
$after | ConvertTo-Json -Depth 4 | Set-Content (Join-Path $evidence 'deployment-after.json') -Encoding utf8
$changed=@($before.Keys | Where-Object {!$after.ContainsKey($_) -or $after[$_] -ne $before[$_]})
$added=@($after.Keys | Where-Object {!$before.ContainsKey($_)})
if($changed.Count -ne 1 -or $changed[0] -ne $installed -or $added.Count -ne 0 -or $after[$installed] -ne $newHash){throw 'Unexpected deployment difference; inspect evidence before play'}
"PASS audited=$($before.Count) changed=1 unchanged=$($before.Count-1) added=0 removed=0"
'PASS all Shadow profiles, other mods, INIs, audio, TPK and EXE unchanged'
