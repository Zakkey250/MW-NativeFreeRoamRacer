param([Parameter(Mandatory)][string]$GameDirectory)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'Verify-NFSMWNativeFreeRoamRacers.ps1') -GameDirectory $GameDirectory
$workspace=Split-Path $projectRoot
$baseline=Join-Path $workspace 'rollback/2026-09-15-pre-alpha57-rules'
$allowed=@('src/Runtime.cpp','src/Plugin.cpp','src/EncounterBattle.inl','src/EncounterSignal.inl','include/EncounterBattleModel.h','include/EncounterText.h')
foreach($folder in 'src','include','config') {
    foreach($file in Get-ChildItem -LiteralPath (Join-Path $baseline ('NFSMWNativeFreeRoamRacers/'+$folder)) -File) {
        $relative=$folder+'/'+$file.Name
        if($relative -in $allowed){continue}
        if((Get-FileHash -LiteralPath $file.FullName).Hash -ne (Get-FileHash -LiteralPath (Join-Path $projectRoot $relative)).Hash){throw "Unexpected NFR change: $relative"}
    }
}
foreach($file in Get-ChildItem -LiteralPath (Join-Path $baseline 'MWArmsAssist/src') -File) {
    if($file.Name -eq 'Plugin.cpp'){continue}
    if((Get-FileHash -LiteralPath $file.FullName).Hash -ne (Get-FileHash -LiteralPath (Join-Path $workspace ('MWArmsAssist/src/'+$file.Name))).Hash){throw "Unexpected Arms Assist change: $($file.Name)"}
}
$count=0
foreach($name in 'EncounterBattle.inl','EncounterPowerBoost.inl','EncounterCustomDrive.inl','EncounterCustomSpeed.inl') {
    $text=Get-Content (Join-Path $projectRoot ('src/'+$name)) -Raw
    foreach($m in [regex]::Matches($text,'\{(0x[0-9A-Fa-f]+),"((?:\\x[0-9A-Fa-f]{2})+)",(\d+)\}')) {
        $va=[Convert]::ToUInt32($m.Groups[1].Value.Substring(2),16)
        $expected=($m.Groups[2].Value -replace '^\\x','' -replace '\\x','-').ToUpperInvariant()
        if((ReadTargetBytes $va ([int]$m.Groups[3].Value)) -ne $expected){throw "ABI mismatch: $va"};++$count
    }
}
if($count -ne 49){throw "Unexpected guard count: $count"}
if((ReadTargetBytes 0x6F8F10 4) -ne '8B-41-28-C3'){throw 'Current IPlayer HUD offset mismatch'}
'PASS alpha57: alpha56 Custom/Stable steering, speed hooks, thresholds, power and population unchanged'
'PASS alpha57: existing Arms Assist police/race/repair policies and 49 battle ABI guards preserved; current HUD offset verified'
