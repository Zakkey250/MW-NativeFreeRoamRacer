param([string]$GameDirectory=$(Join-Path $(if($env:NFS_GAME_ROOT){$env:NFS_GAME_ROOT}elseif($env:COMPUTERNAME -eq 'ZAKKEY'){'E:\EA GAMES'}else{'\\ZAKKEY\EA GAMES'}) 'Need for Speed Most Wanted Main'))
$ErrorActionPreference='Stop'
$project=Split-Path $PSScriptRoot
$workspace=Split-Path $project
$game=(Resolve-Path -LiteralPath $GameDirectory).Path
$evidence=Join-Path $workspace 'diagnostics/encounter-20260921-alpha77'
$backup=Join-Path $workspace 'rollback/2026-09-21-pre-alpha77/installed'
function Hash([string]$path){(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash}
function Stopped {
    if(Get-CimInstance Win32_Process | Where-Object {$_.Name -in 'speed.exe','nfsMW.exe'}){throw 'User game running; defer, never terminate.'}
}
Stopped
$config=Get-Content -LiteralPath (Join-Path $game 'Launch-SV-Safe.json') -Raw | ConvertFrom-Json
$state=Get-Content -LiteralPath $config.StatePath -Raw | ConvertFrom-Json
if($state.GameDirectory -ne $game){throw 'SV installation mismatch'}
$targets=@(
    @{Name='NFSMWNativeFreeRoamRacers.asi';Artifact=(Join-Path $project 'artifacts/Release/NFSMWNativeFreeRoamRacers.asi');Old='066A23925DDDBCF7B4887485AF8CF0A97C185B95662FEBEBF3F3ACA3CFFBC2B5';New='25D0B7892C8373D457D63D4DA5A105AEA2F3E5232D1B69F4A547D327DAD44D44'},
    @{Name='NFSMWNativeFreeRoamRacers.ini';Artifact=(Join-Path $evidence 'NFSMWNativeFreeRoamRacers.test.ini');Old=(Hash (Join-Path $backup 'NFSMWNativeFreeRoamRacers.ini'));New=(Hash (Join-Path $evidence 'NFSMWNativeFreeRoamRacers.test.ini'))}
)
# Preserve every existing byte except newline normalization and the new opt-in block.
$oldIni=Get-Content -LiteralPath (Join-Path $backup 'NFSMWNativeFreeRoamRacers.ini') -Raw
$testIni=Get-Content -LiteralPath $targets[1].Artifact -Raw
$addition=@'
; Experimental, restart required. Non-NFRR share of LB total capacity (0=off, 1..64).
; Shared by player/traffic/police/other MODs, not extra leases or guaranteed empties.
; Also preserves the last two moving ordinary traffic cars from MOD retirement.
; 実験機能・再起動必須。LB総上限のうちNFRR以外に残す枠。0=従来動作。
; 自車・交通・警察・他MODの共用枠。空きや警察出現の保証ではありません。LBなしでは無効。
LBNonRacerSlots = 6
'@
$oldNormalized=$oldIni.Replace("`r`n","`n")
$newNormalized=$testIni.Replace("`r`n","`n")
$addition=$addition.Replace("`r`n","`n")
if($oldNormalized.Contains('LBNonRacerSlots') -or
    ([regex]::Matches($newNormalized,'(?m)^LBNonRacerSlots = 6$')).Count -ne 1 -or
    $newNormalized.Replace($addition+"`n",'') -cne $oldNormalized){throw 'Unexpected test INI changes'}
foreach($t in $targets){
    $t.Installed=Join-Path $game ('scripts/'+$t.Name)
    if((Hash $t.Installed) -ne $t.Old -or (Hash (Join-Path $backup $t.Name)) -ne $t.Old -or (Hash $t.Artifact) -ne $t.New){throw 'Unexpected target, backup or build'}
}
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
foreach($t in $targets){if((Hash $t.Installed) -ne $t.Old -or (Hash $t.Artifact) -ne $t.New){throw 'Concurrent target change'}}
try {
    foreach($t in $targets){Copy-Item -LiteralPath $t.Artifact -Destination $t.Installed -Force;if((Hash $t.Installed) -ne $t.New){throw 'Copy mismatch'}}
}catch{
    Stopped
    foreach($t in $targets){Copy-Item -LiteralPath (Join-Path $backup $t.Name) -Destination $t.Installed -Force}
    throw
}
$global:LASTEXITCODE=0
& (Join-Path $workspace 'tools/Verify-NFSPatcherRuntime.ps1') -GameDirectory $game | Tee-Object (Join-Path $evidence 'verify-after.txt')
if($LASTEXITCODE){throw 'Post-deployment target validation failed'}
& (Join-Path $project 'tools/Verify-NFSMWNativeFreeRoamRacers.ps1') -GameDirectory $game | Tee-Object (Join-Path $evidence 'nfrr-after.txt')
$after=Snapshot
$after | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $evidence 'deployment-after.json') -Encoding utf8
$changed=@($before.Keys | Where-Object {$after[$_] -ne $before[$_]})
if($before.Count -ne $after.Count -or $changed.Count -ne $targets.Count -or @($changed | Where-Object {$_ -notin $targets.Installed}).Count){throw 'Unexpected installed changes'}
"PASS audited=$($before.Count); only NFRR ASI and NFRR headroom opt-in INI changed"
foreach($t in $targets){"PASS $($t.Name) SHA256=$($t.New)"}
"Existing INI values, LB binary/config and all other assets/profiles preserved. No runtime acceptance claimed."
