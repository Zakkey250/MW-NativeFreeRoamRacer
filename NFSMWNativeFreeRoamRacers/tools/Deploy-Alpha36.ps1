$ErrorActionPreference='Stop'
$project=Split-Path $PSScriptRoot
$workspace=Split-Path $project
$game=$(Join-Path $(if($env:NFS_GAME_ROOT){$env:NFS_GAME_ROOT}elseif($env:COMPUTERNAME -eq 'ZAKKEY'){'E:\EA GAMES'}else{'\\ZAKKEY\EA GAMES'}) 'Need for Speed Most Wanted')
$exe=Join-Path $game 'speed.exe'
$installed=Join-Path $game 'scripts\NFSMWNativeFreeRoamRacers.asi'
$artifact=Join-Path $project 'artifacts\Release\NFSMWNativeFreeRoamRacers.asi'
$evidence=Join-Path $workspace 'diagnostics\encounter-battle-20260914-alpha36'
$backup=Join-Path $workspace 'rollback\2026-09-14-pre-alpha.36\Release\NFSMWNativeFreeRoamRacers.asi'
function AssertStopped {
    if(Get-CimInstance Win32_Process -Filter "Name='speed.exe'" | Where-Object {!$_.ExecutablePath -or $_.ExecutablePath -eq $exe}) {
        throw 'Game running or path unknown; preserve process and defer deployment.'
    }
}
AssertStopped
$oldHash='C570ED2437CEB2FFD5ED4812613948F165FBF515F006178B8C1BCB920C97BABF'
$newHash='E17568126EE8D1D8A22384F3A9530A3D14954575E1194A2708D5BB670F91F7CC'
if((Get-FileHash $installed).Hash -ne $oldHash -or (Get-FileHash $backup).Hash -ne $oldHash -or
    (Get-FileHash $artifact).Hash -ne $newHash) {throw 'Deployment / rollback identity mismatch'}
$manifest=Get-Content (Join-Path $workspace 'integration\NFSMW-ExecutableTarget.json') -Raw | ConvertFrom-Json
if((Get-FileHash $exe).Hash -ne $manifest.sha256) {throw 'Executable identity mismatch'}
$config=Get-Content (Join-Path $game 'Launch-SV-Safe.json') -Raw | ConvertFrom-Json
$state=Get-Content $config.StatePath -Raw | ConvertFrom-Json
function Snapshot {
    $files=@(
        Get-ChildItem $game -File | Where-Object {$_.Extension -in '.exe','.dll','.ini','.json'}
        Get-ChildItem (Join-Path $game 'scripts') -File -Recurse | Where-Object {$_.Extension -in '.asi','.dll','.ini','.json','.tpk'}
        Get-ChildItem (Join-Path $game 'scripts\NativeFreeRoamRacers\encounter') -File -Recurse
        Get-ChildItem $state.ShadowProfileDirectory -File -Recurse
        Get-ChildItem $state.VaultProfileDirectory -File -Recurse
        Get-Item $config.StatePath,$state.ManifestPath,$state.OriginalSourceFile
    ) | Sort-Object FullName -Unique
    $result=@{}
    foreach($file in $files){$result[$file.FullName]=(Get-FileHash -LiteralPath $file.FullName).Hash}
    return $result
}
$before=Snapshot
$before | ConvertTo-Json -Depth 4 | Set-Content (Join-Path $evidence 'deployment-before.json') -Encoding utf8
AssertStopped
Copy-Item -LiteralPath $artifact -Destination $installed -Force
$after=Snapshot
$after | ConvertTo-Json -Depth 4 | Set-Content (Join-Path $evidence 'deployment-after.json') -Encoding utf8
$changed=@($before.Keys | Where-Object {!$after.ContainsKey($_) -or $after[$_] -ne $before[$_]})
$added=@($after.Keys | Where-Object {!$before.ContainsKey($_)})
if($changed.Count -ne 1 -or $changed[0] -ne $installed -or $added.Count -ne 0 -or $after[$installed] -ne $newHash) {
    throw 'Unexpected protected-file change; inspect snapshots.'
}
"PASS deployment audited=$($before.Count) changed=1 unchanged=$($before.Count-1) added=0 removed=0 SHA256=$newHash"
"PASS all INIs including unused Setting.ini, user WAVs, other mods, EXE, Shadow, Vault and original saves unchanged"
