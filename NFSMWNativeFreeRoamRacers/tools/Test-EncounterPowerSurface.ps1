$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'Test-EncounterGuideSurface.ps1')
$boost=Get-Content (Join-Path $projectRoot 'src\EncounterPowerBoost.inl') -Raw
$guards=[regex]::Matches($boost,'\{(0x[0-9A-Fa-f]+),"((?:\\x[0-9A-Fa-f]{2})+)",(\d+)\}')
if($guards.Count -ne 8){throw 'Unexpected power guard count'}
foreach($match in $guards) {
    $va=[Convert]::ToUInt32($match.Groups[1].Value.Substring(2),16)
    $expected=$match.Groups[2].Value.Replace('\x',' ').Trim().ToUpperInvariant()
    if((ReadTargetBytes $va ([int]$match.Groups[3].Value)).Replace('-',' ') -ne $expected){throw 'Native positive-output boost ABI mismatch'}
    'PASS native positive-output boost guard {0:X8}' -f $va
}
foreach($required in @('caller!=Address(0x006B0016)','now-refreshed>250','IsEncounterSkillOwner','g_originalEncounterCheat(cheater)','(native+2.0f)*scale-2.0f')) {
    if(!$boost.Contains($required)){throw "Missing boost restriction $required"}
}
if($boost -match 'SetDriveTarget|SetPosition|SetSpeed|SetVelocity|WritePrivateProfile|SetMaximum|NosAdded'){throw 'Boost must not steer, move, unlock or persist tuning'}
'PASS boost scoped to identified opponent and one positive-output caller; standard catchup controller and other cheat consumers unchanged'
