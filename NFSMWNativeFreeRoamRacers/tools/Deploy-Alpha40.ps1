$ErrorActionPreference='Stop'
$project=Split-Path $PSScriptRoot
$workspace=Split-Path $project
$game=$(Join-Path $(if($env:NFS_GAME_ROOT){$env:NFS_GAME_ROOT}elseif($env:COMPUTERNAME -eq 'ZAKKEY'){'E:\EA GAMES'}else{'\\ZAKKEY\EA GAMES'}) 'Need for Speed Most Wanted')
$exe=Join-Path $game 'speed.exe'
$installed=Join-Path $game 'scripts\NFSMWNativeFreeRoamRacers.asi'
$artifact=Join-Path $project 'artifacts\Release\NFSMWNativeFreeRoamRacers.asi'
$evidence=Join-Path $workspace 'diagnostics\encounter-battle-20260914-alpha40'
$backup=Join-Path $workspace 'rollback\2026-09-14-pre-alpha.40\Release\NFSMWNativeFreeRoamRacers.asi'
$ini=Join-Path $game 'scripts\NFSMWNativeFreeRoamRacers.ini'
$oldIni=Join-Path $workspace 'rollback\2026-09-14-pre-alpha.40\installed.ini'
$nextIni=Join-Path $evidence 'installed-next.ini'
function AssertStopped {
    if(Get-CimInstance Win32_Process -Filter "Name='speed.exe'" | Where-Object {!$_.ExecutablePath -or $_.ExecutablePath -eq $exe}) {
        throw 'Game running or path unknown; preserve process and defer deployment.'
    }
}
AssertStopped
$oldHash='F7390385A8EC6C65FBB5F6EDF4C0370957F2817BEE0607856534C688E38D6FE4'
$newHash='7E87F99C3F334CDEB12FBEBAFFE60BC2CBC2B0A5DC99D8A8B70A62C5B4439343'
if((Get-FileHash $installed).Hash -ne $oldHash -or (Get-FileHash $backup).Hash -ne $oldHash -or
    (Get-FileHash $artifact).Hash -ne $newHash) {throw 'Deployment / rollback identity mismatch'}
if((Get-FileHash $ini).Hash -ne (Get-FileHash $oldIni).Hash){throw 'User INI changed since backup; rebase before deployment'}
$oldIniText=(Get-Content $oldIni -Raw).Replace("`r`n","`n")
$nextIniText=(Get-Content $nextIni -Raw).Replace("`r`n","`n")
$addition=@'
; Trailing rival positive drive-output multipliers. Restart game after editing.
; 1.00 disables correction for that tier; range 1.00..2.00.
PowerScale0To100 = 1.15
PowerScale100To200 = 1.30
PowerScale200To300 = 1.50
'@
$addition=$addition.Replace("`r`n","`n")+"`n"
if(!$nextIniText.Contains($addition) -or $nextIniText.Replace($addition,'') -cne $oldIniText){throw 'INI must contain only the requested additive settings'}
if(([regex]::Matches($nextIniText,'(?m)^PowerScale(?:0To100|100To200|200To300)\s*=')).Count -ne 3){throw 'Duplicate or missing power key'}
$nextIniHash=(Get-FileHash $nextIni).Hash
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
Copy-Item -LiteralPath $nextIni -Destination $ini -Force
$after=Snapshot
$after | ConvertTo-Json -Depth 4 | Set-Content (Join-Path $evidence 'deployment-after.json') -Encoding utf8
$changed=@($before.Keys | Where-Object {!$after.ContainsKey($_) -or $after[$_] -ne $before[$_]})
$added=@($after.Keys | Where-Object {!$before.ContainsKey($_)})
if($changed.Count -ne 2 -or $installed -notin $changed -or $ini -notin $changed -or $added.Count -ne 0 -or $after[$installed] -ne $newHash -or $after[$ini] -ne $nextIniHash) {
    throw 'Unexpected protected-file change; inspect snapshots.'
}
"PASS deployment audited=$($before.Count) changed=2 unchanged=$($before.Count-2) added=0 removed=0 SHA256=$newHash"
"PASS ASI plus exactly three additive INI keys; all prior settings, unused Setting.ini, WAVs, other mods, EXE and saves unchanged"
