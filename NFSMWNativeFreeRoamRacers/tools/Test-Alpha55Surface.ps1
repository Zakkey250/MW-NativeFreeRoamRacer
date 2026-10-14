param([Parameter(Mandatory)][string]$GameDirectory)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'Verify-NFSMWNativeFreeRoamRacers.ps1') -GameDirectory $GameDirectory
$count=0
foreach($name in 'EncounterBattle.inl','EncounterPowerBoost.inl','EncounterCustomDrive.inl','EncounterCustomSpeed.inl') {
    $text=Get-Content (Join-Path $projectRoot ('src/'+$name)) -Raw
    foreach($m in [regex]::Matches($text,'\{(0x[0-9A-Fa-f]+),"((?:\\x[0-9A-Fa-f]{2})+)",(\d+)\}')) {
        $va=[Convert]::ToUInt32($m.Groups[1].Value.Substring(2),16)
        $expected=($m.Groups[2].Value -replace '^\\x','' -replace '\\x','-').ToUpperInvariant()
        if((ReadTargetBytes $va ([int]$m.Groups[3].Value)) -ne $expected){throw "ABI guard mismatch at $va"}
        ++$count
    }
}
if($count -ne 49){throw "Unexpected guard count: $count"}
$baseline=Join-Path $projectRoot '../rollback/2026-09-15-pre-alpha55-attack-window'
$allowed=@('src/Runtime.cpp','src/Plugin.cpp','src/EncounterBattle.inl','src/EncounterCustomFollow.inl',
    'src/EncounterCustomSpeed.inl','include/EncounterPursuit.h','include/EncounterChaseSpeed.h')
foreach($folder in 'src','include','config') {
    foreach($file in Get-ChildItem -LiteralPath (Join-Path $baseline $folder) -File) {
        $relative=$folder+'/'+$file.Name
        if($relative -in $allowed){continue}
        if((Get-FileHash -LiteralPath $file.FullName).Hash -ne (Get-FileHash -LiteralPath (Join-Path $projectRoot $relative)).Hash){throw "Out-of-scope change: $relative"}
    }
}
function Extract-Stable([string]$text) {
    $start=$text.IndexOf('bool NativeEncounterFollow(')
    $end=$text.IndexOf('#include "EncounterCustomFollow.inl"',$start)
    return ($text.Substring($start,$end-$start).Trim() -replace "`r`n","`n")
}
if((Extract-Stable (Get-Content (Join-Path $baseline 'src/EncounterBattle.inl') -Raw)) -cne
    (Extract-Stable (Get-Content (Join-Path $projectRoot 'src/EncounterBattle.inl') -Raw))){throw 'Stable native follow changed'}
'PASS alpha55 49 existing ABI guards; no new native function or hook'
'PASS alpha55 Stable native follow, directional trail, outcome, power tiers, population, audio, HUD and INI preserved'
