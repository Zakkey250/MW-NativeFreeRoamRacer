$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'Test-CareerPerformanceSurface.ps1')
foreach($guard in @(
    @(0x00428B53,'83 78 78 03'),
    @(0x00787E77,'66 89 87 DA 02 00 00'),
    @(0x00787E85,'C6 87 D8 02 00 00 00'),
    @(0x00428AE6,'A1 00 E0 91 00 85 C0'),
    @(0x00428AF3,'83 B8 60 19 00 00 01'),
    @(0x00770073,'83 7E 78 03 75 07 C7 46 78 02 00 00 00')
)) {
    if((ReadTargetBytes $guard[0] ($guard[1].Split(' ').Count)).Replace('-',' ') -ne $guard[1]){throw 'RoadNav state ABI mismatch'}
    'PASS RoadNav request/mode/goal and native event-only renewal gate {0:X8}' -f $guard[0]
}
$body=Get-Content (Join-Path $projectRoot 'src\EncounterBattle.inl') -Raw
$start=$body.IndexOf('bool NativeEncounterFollow(');$end=$body.IndexOf('void* EncounterProfile()')
$follow=$body.Substring($start,$end-$start)
if($follow -match 'Address\(0x(?:00416900|00423010)\)|(?:SetGoal|DoDriving|SetDriveTarget)\('){throw 'Follow recovery must not restart/quit/reset native AI'}
$passEnd=$follow.IndexOf('// Use the existing native target object')
if($passEnd -lt 0 -or $follow.Substring($passEnd).Contains('Address(0x00770060)')){throw 'Path cancellation only permitted on one-time pass transition'}
if(!$follow.Contains('if(!g_battle.passActive)') -or !$follow.Contains('g_battle.passActive=true;')){throw 'Pass transition must be edge triggered'}
if(!$follow.Contains('if(busy) return true;')){throw 'Pending path requests must not be cancelled'}
'PASS current-player pursuit at 1Hz, one-time path release for pass only; no native race or steering reset'
