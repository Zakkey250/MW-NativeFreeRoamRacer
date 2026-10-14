param([Parameter(Mandatory)][string]$GameDirectory)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'Verify-NFSMWNativeFreeRoamRacers.ps1') -GameDirectory $GameDirectory
$baseline=Join-Path $projectRoot '../rollback/2026-09-15-pre-alpha51-custom-ai'
$allowed=@('src/Runtime.cpp','src/Plugin.cpp','src/EncounterBattle.inl','src/EncounterRoute.inl',
    'src/EncounterPowerSettings.inl','src/EncounterPowerBoost.inl','config/NFSMWNativeFreeRoamRacers.ini')
foreach($folder in @('src','include','config')) {
    foreach($file in Get-ChildItem -LiteralPath (Join-Path $baseline $folder) -File) {
        $relative=$folder+'/'+$file.Name
        if($relative -in $allowed) {continue}
        if((Get-FileHash -LiteralPath $file.FullName).Hash -ne (Get-FileHash -LiteralPath (Join-Path $projectRoot $relative)).Hash) {
            throw "Out-of-scope source change: $relative"
        }
    }
}
$before=Get-Content (Join-Path $baseline 'src/EncounterBattle.inl') -Raw
$after=Get-Content (Join-Path $projectRoot 'src/EncounterBattle.inl') -Raw
function Extract-StableFollow([string]$text) {
    $begin=$text.IndexOf('bool NativeEncounterFollow(')
    $end=$text.IndexOf('void* EncounterProfile()', $begin)
    return (($text.Substring($begin,$end-$begin) -replace '#include "EncounterCustomFollow.inl"','').Trim() -replace "`r`n","`n")
}
if((Extract-StableFollow $before) -cne (Extract-StableFollow $after)) {throw 'Stable follow implementation changed'}
$guardFiles=@('EncounterBattle.inl','EncounterPowerBoost.inl')
$count=0
foreach($name in $guardFiles) {
    $text=Get-Content (Join-Path $projectRoot ('src/'+$name)) -Raw
    foreach($m in [regex]::Matches($text,'\{(0x[0-9A-Fa-f]+),"((?:\\x[0-9A-Fa-f]{2})+)",(\d+)\}')) {
        $va=[Convert]::ToUInt32($m.Groups[1].Value.Substring(2),16)
        $expected=($m.Groups[2].Value -replace '^\\x','' -replace '\\x','-').ToUpperInvariant()
        if((ReadTargetBytes $va ([int]$m.Groups[3].Value)) -ne $expected) {throw "Encounter ABI guard mismatch at $va"}
        ++$count
    }
}
if($count -ne 42) {throw "Unexpected encounter guard count $count"}
$custom=Get-Content (Join-Path $projectRoot 'src/EncounterCustomFollow.inl') -Raw
if($custom -match 'MH_CreateHook|WriteProcessMemory|SetDriveTarget|DoDriving|QuitRace|SetMaximum') {
    throw 'Unexpected new hook/direct drive/lifecycle mutation'
}
'PASS stable follow unchanged; generation, audio, HUD, lead/result model, career and persistence sources unchanged'
"PASS $count encounter/power ABI guards; custom adapter uses existing verified calls only"
