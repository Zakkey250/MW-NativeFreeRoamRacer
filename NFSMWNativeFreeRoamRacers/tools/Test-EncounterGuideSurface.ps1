$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'Test-RouteRecoverySurface.ps1')
$source=Get-Content (Join-Path $projectRoot 'src\EncounterGuide.inl') -Raw
$guards=[regex]::Matches($source,'\{(0x[0-9A-Fa-f]+),"((?:\\x[0-9A-Fa-f]{2})+)",(\d+)\}')
if($guards.Count -ne 8){throw 'Unexpected GPS guard count'}
foreach($match in $guards) {
    $va=[Convert]::ToUInt32($match.Groups[1].Value.Substring(2),16)
    $expected=$match.Groups[2].Value.Replace('\x',' ').Trim().ToUpperInvariant()
    if((ReadTargetBytes $va ([int]$match.Groups[3].Value)).Replace('-',' ') -ne $expected){throw 'Native GPS ABI mismatch'}
    'PASS native GPS guard {0:X8}' -f $va
}
foreach($guard in @(
    @(0x00409390,'51 D9 41 40 D8 41 3C'),
    @(0x008925C8,'90 93 40 00'),
    @(0x0043D08E,'8D BE 6C 07 00 00'),
    @(0x00429582,'A1 00 E0 91 00'),
    @(0x005FFA5A,'8B 8E 68 19 00 00 85 C9')
)) {
    if((ReadTargetBytes $guard[0] ($guard[1].Split(' ').Count)).Replace('-',' ') -ne $guard[1]){throw 'Catchup ABI/gating mismatch'}
    'PASS stock catchup input / event dependency {0:X8}' -f $guard[0]
}
$probe=Get-Content (Join-Path $projectRoot 'src\EncounterCatchupProbe.inl') -Raw
if($probe -match 'MH_CreateHook|WriteMemory|VirtualProtect|memcpy|1\.15|1\.30|1\.50'){throw 'Stock probe must not modify catchup or physics'}
if($source -match 'MH_CreateHook|SetDriveTarget|SetPosition|WritePrivateProfile'){throw 'GPS must not add steering, physics or global hooks'}
'PASS GPS lease, bounded native requests; stock catchup observation only, no guessed multiplier or fabricated race state'
