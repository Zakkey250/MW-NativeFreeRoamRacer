$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'Test-EncounterSignalSurface.ps1')
$source=Get-Content (Join-Path $projectRoot 'src\EncounterBattle.inl') -Raw
$matches=[regex]::Matches($source,'\{(0x[0-9A-Fa-f]+),"((?:\\x[0-9A-Fa-f]{2})+)",(\d+)\}')
if($matches.Count -ne 34) {throw 'Unexpected native battle guard count'}
foreach($match in $matches) {
    $va=[Convert]::ToUInt32($match.Groups[1].Value.Substring(2),16)
    $size=[int]$match.Groups[3].Value
    $hex=$match.Groups[2].Value.Replace('\x',' ').Trim().ToUpperInvariant()
    $actual=(ReadTargetBytes $va $size).Replace('-',' ')
    if($hex -ne $actual) {throw ('Battle surface mismatch {0:X8}' -f $va)}
    'PASS native battle surface {0:X8}: {1}' -f $va,$actual
}
# This call site may be hooked by WidescreenFix at runtime: prove the original
# EXE's D3D device/EndScene mapping here without rejecting a legitimate hook.
if((ReadTargetBytes 0x006E75AC 17).Replace('-',' ') -ne 'A1 DC 2B 98 00 8B 10 83 C4 04 50 FF 92 A8 00 00 00') {throw 'D3D device call site mismatch'}
'PASS game D3D9 device global 00982BDC and EndScene slot 42'
if($source -match 'SetDriveTarget\)\(' -or $source -match 'DoDriving\)\(') {throw 'Direct steering adapter prohibited'}
'PASS scoped native road-path adapter; no direct steering or fake global event'
foreach($guard in @(@(0x004B5A7E,'8B 15 A8 1F 91 00 8B 42 24 D9 40 04 D8 08'),@(0x004AC280,'D9 41 04 D8 09 C3'))) {
    if((ReadTargetBytes $guard[0] ($guard[1].Split(' ').Count)).Replace('-',' ') -ne $guard[1]) {throw 'Native voice volume surface mismatch'}
    'PASS native active speech-volume chain {0:X8}' -f $guard[0]
}
if($source -match 'else if\(g_battle\.\w+\).*NativeEncounterRestore') {throw 'QuitRace must not be used for a mid-battle role switch'}
$voice=Get-Content (Join-Path $projectRoot 'src\EncounterVoice.inl') -Raw
$volume=Get-Content (Join-Path $projectRoot 'src\EncounterVolume.inl') -Raw
if(($voice+$volume) -match 'GetPrivateProfile|waveOutSetVolume|SetMasterVolume|WritePrivateProfile') {throw 'Encounter voice must follow native settings without INI or device-volume writes'}
'PASS game-only voice volume uses private PCM; no INI dependency or device volume writes'
$anchorGuards=@(
    @(0x00422690,'53 56 8B F1 8B 06 57 FF 50 48'),
    @(0x00777660,'8B 44 24 04 56 8B F1 3B C6'),
    @(0x0077776C,'5E C2 08 00')
)
foreach($guard in $anchorGuards) {
    if((ReadTargetBytes $guard[0] ($guard[1].Split(' ').Count)).Replace('-',' ') -ne $guard[1]) {throw 'Anchor copy ABI mismatch'}
    'PASS anchor scalar copy ABI {0:X8}' -f $guard[0]
}
