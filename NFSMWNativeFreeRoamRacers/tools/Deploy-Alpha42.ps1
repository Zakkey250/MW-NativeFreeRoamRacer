$ErrorActionPreference='Stop'
$project=Split-Path $PSScriptRoot;$workspace=Split-Path $project
$game=$(Join-Path $(if($env:NFS_GAME_ROOT){$env:NFS_GAME_ROOT}elseif($env:COMPUTERNAME -eq 'ZAKKEY'){'E:\EA GAMES'}else{'\\ZAKKEY\EA GAMES'}) 'Need for Speed Most Wanted')
$installed=Join-Path $game 'scripts\NFSMWNativeFreeRoamRacers.asi'
$texture=Join-Path $game 'scripts\NFSMWNativeFreeRoamRacers.tpk'
$artifact=Join-Path $project 'artifacts\Release\NFSMWNativeFreeRoamRacers.asi'
$asset=Join-Path $project 'artifacts\Release\NFSMWNativeFreeRoamRacers.tpk'
$backup=Join-Path $workspace 'rollback\2026-09-14-pre-alpha.42\Release\NFSMWNativeFreeRoamRacers.asi'
$evidence=Join-Path $workspace 'diagnostics\encounter-prompt-20260914-alpha42'
$oldHash='A315793EFB749876F369FD4932A4A1D9015F3B24FA2C0FF249E887E0645CDE36'
$newHash='E4FE34ABC6C7C44101A29EF4DBD2FE648746AD1D2E311E259ED76885827801B8'
$assetHash='CA929293186A10867B65729CE78AF89C5F5DE880239A402EC68E35E0169D32D1'
function Hash([string]$p){(Get-FileHash -LiteralPath $p).Hash}
function Stopped {if(Get-CimInstance Win32_Process -Filter "Name='speed.exe'"){throw 'User game running; deployment deferred'}}
Stopped
if((Hash $installed) -ne $oldHash -or (Hash $backup) -ne $oldHash -or (Hash $artifact) -ne $newHash -or (Hash $asset) -ne $assetHash){throw 'Deployment/rollback identity mismatch'}
if(Test-Path $texture){throw 'New asset path already exists; do not overwrite unknown asset'}
$manifest=Get-Content (Join-Path $workspace 'integration\NFSMW-ExecutableTarget.json') -Raw | ConvertFrom-Json
if((Hash (Join-Path $game 'speed.exe')) -ne $manifest.sha256){throw 'EXE mismatch'}
$config=Get-Content (Join-Path $game 'Launch-SV-Safe.json') -Raw | ConvertFrom-Json
$state=Get-Content $config.StatePath -Raw | ConvertFrom-Json
function Snapshot {
 $files=@(
  Get-ChildItem $game -File | Where-Object {$_.Extension -in '.exe','.dll','.ini','.json'}
  Get-ChildItem (Join-Path $game 'scripts') -Recurse -File | Where-Object {$_.Extension -in '.asi','.dll','.ini','.json','.tpk'}
  Get-ChildItem (Join-Path $game 'scripts\NativeFreeRoamRacers\encounter') -Recurse -File
  # Protect ALL profiles, not only state.ProfileName (ZAKKEY).
  Get-ChildItem (Split-Path $state.ShadowProfileDirectory) -Recurse -File
  Get-ChildItem $state.VaultProfileDirectory -Recurse -File
  Get-Item $config.StatePath,$state.ManifestPath,$state.OriginalSourceFile
 ) | Sort-Object FullName -Unique
 $result=@{};foreach($f in $files){$result[$f.FullName]=Hash $f.FullName};return $result
}
$before=Snapshot
$before | ConvertTo-Json -Depth 4 | Set-Content (Join-Path $evidence 'deployment-before.json') -Encoding utf8
Stopped
Copy-Item -LiteralPath $asset -Destination $texture
Copy-Item -LiteralPath $artifact -Destination $installed -Force
$after=Snapshot
$after | ConvertTo-Json -Depth 4 | Set-Content (Join-Path $evidence 'deployment-after.json') -Encoding utf8
$changed=@($before.Keys | Where-Object {!$after.ContainsKey($_) -or $after[$_] -ne $before[$_]})
$added=@($after.Keys | Where-Object {!$before.ContainsKey($_)})
if($changed.Count -ne 1 -or $changed[0] -ne $installed -or $added.Count -ne 1 -or $added[0] -ne $texture -or $after[$installed] -ne $newHash -or $after[$texture] -ne $assetHash){throw 'Unexpected deployment difference; inspect evidence before play'}
"PASS audited=$($before.Count) changed=1 unchanged=$($before.Count-1) added=1 removed=0"
"PASS all Shadow profiles, including repaired nanaSHiN, other mods, INIs, audio and EXE unchanged"
