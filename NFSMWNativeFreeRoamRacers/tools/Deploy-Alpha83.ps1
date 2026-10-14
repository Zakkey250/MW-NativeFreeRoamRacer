param([string]$GameDirectory=$(Join-Path $(if($env:NFS_GAME_ROOT){$env:NFS_GAME_ROOT}elseif($env:COMPUTERNAME -eq 'ZAKKEY'){'E:\EA GAMES'}else{'\\ZAKKEY\EA GAMES'}) 'Need for Speed Most Wanted Main'))
$ErrorActionPreference='Stop'
$project=Split-Path $PSScriptRoot
$workspace=Split-Path $project
$game=(Resolve-Path -LiteralPath $GameDirectory).Path
$evidence=Join-Path $workspace 'diagnostics/encounter-20260922-alpha83'
$backup=Join-Path $workspace 'rollback/2026-09-22-pre-alpha83/installed'
function Hash([string]$path){(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash}
function Stopped {
    if(Get-CimInstance Win32_Process | Where-Object {$_.Name -in 'speed.exe','nfsMW.exe'}){throw 'User game running; defer, never terminate.'}
}
Stopped
$config=Get-Content -LiteralPath (Join-Path $game 'Launch-SV-Safe.json') -Raw | ConvertFrom-Json
$state=Get-Content -LiteralPath $config.StatePath -Raw | ConvertFrom-Json
if($state.GameDirectory -ne $game){throw 'SV installation mismatch'}
$old='5E11E8183621B89F4EE9AEBDD5B516A8626F48EA607F1BF76950763341F2D47A'
$new='78521E7F177EB05AF91DC62DB302878C770E3464611FAF52F0B8B760F29D1EF0'
$installed=Join-Path $game 'scripts/NFSMWNativeFreeRoamRacers.asi'
$artifact=Join-Path $project 'artifacts/Release/NFSMWNativeFreeRoamRacers.asi'
$rollback=Join-Path $backup 'NFSMWNativeFreeRoamRacers.asi'
if((Hash $installed) -ne $old -or (Hash $rollback) -ne $old -or (Hash $artifact) -ne $new){throw 'Unexpected target, backup or build'}
if((Hash (Join-Path $game 'scripts/NFSMWNativeFreeRoamRacers.ini')) -ne (Hash (Join-Path $backup 'NFSMWNativeFreeRoamRacers.ini'))){throw 'INI changed since backup; review before deployment'}
$iniInstalled=Join-Path $game 'scripts/NFSMWNativeFreeRoamRacers.ini'
$iniArtifact=Join-Path $evidence 'NFSMWNativeFreeRoamRacers.test.ini'
$iniBackup=Join-Path $backup 'NFSMWNativeFreeRoamRacers.ini'
$iniOld=Hash $iniBackup
$iniNew=Hash $iniArtifact
$addition=@'
; Experimental, restart required. Reuse completed appearance for up to 15s (OFF: 5s).
; Career, anchor and capacity are still checked fresh. No LB required or held reservation.
; 実験機能・再起動必須。完成済み外見の保持を最大15秒へ延長（0=従来5秒）。
; キャリア・アンカー・空き枠は生成前に再検査。LB不要・予約延長なし。
RetainPreparedAppearance = 1
'@
$beforeIni=(Get-Content -LiteralPath $iniBackup -Raw).Replace("`r`n","`n").TrimEnd()
$afterIni=(Get-Content -LiteralPath $iniArtifact -Raw).Replace("`r`n","`n").TrimEnd()
$addition=$addition.Replace("`r`n","`n")
if($beforeIni.Contains('RetainPreparedAppearance') -or
   ([regex]::Matches($afterIni,'(?m)^RetainPreparedAppearance = 1$')).Count -ne 1 -or
   $afterIni.Replace($addition+"`n",'') -cne $beforeIni){throw 'Unexpected INI setting changes'}
function Snapshot {
    $files=@(
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
if((Hash $installed) -ne $old -or (Hash $artifact) -ne $new -or (Hash $iniInstalled) -ne $iniOld -or (Hash $iniArtifact) -ne $iniNew){throw 'Concurrent target change'}
try {
    Copy-Item -LiteralPath $artifact -Destination $installed -Force
    if((Hash $installed) -ne $new){throw 'Copy mismatch'}
    Copy-Item -LiteralPath $iniArtifact -Destination $iniInstalled -Force
    if((Hash $iniInstalled) -ne $iniNew){throw 'INI copy mismatch'}
    $global:LASTEXITCODE=0
    & (Join-Path $workspace 'tools/Verify-NFSPatcherRuntime.ps1') -GameDirectory $game | Tee-Object (Join-Path $evidence 'verify-after.txt')
    if($LASTEXITCODE){throw 'Post-deployment target validation failed'}
    & (Join-Path $project 'tools/Verify-NFSMWNativeFreeRoamRacers.ps1') -GameDirectory $game | Tee-Object (Join-Path $evidence 'nfrr-after.txt')
    $after=Snapshot
    $after | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $evidence 'deployment-after.json') -Encoding utf8
    $changed=@($before.Keys | Where-Object {$after[$_] -ne $before[$_]})
    if($before.Count -ne $after.Count -or $changed.Count -ne 2 -or @($changed | Where-Object {$_ -notin @($installed,$iniInstalled)}).Count){throw 'Unexpected installed changes'}
}catch{
    Stopped
    Copy-Item -LiteralPath $rollback -Destination $installed -Force
    Copy-Item -LiteralPath $iniBackup -Destination $iniInstalled -Force
    throw
}
"PASS audited=$($before.Count); only NFRR ASI and one new INI opt-in changed; SHA256=$new"
"Existing INI values, LB, other audited assets/profiles preserved. Runtime acceptance pending."
