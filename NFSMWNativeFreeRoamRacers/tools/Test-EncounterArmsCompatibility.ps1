$ErrorActionPreference='Stop'
$project=Split-Path $PSScriptRoot
$workspace=Split-Path $project
$game=$(Join-Path $(if($env:NFS_GAME_ROOT){$env:NFS_GAME_ROOT}elseif($env:COMPUTERNAME -eq 'ZAKKEY'){'E:\EA GAMES'}else{'\\ZAKKEY\EA GAMES'}) 'Need for Speed Most Wanted')
$manifest=Get-Content (Join-Path $workspace 'integration\NFSMW-ExecutableTarget.json') -Raw | ConvertFrom-Json
$exe=Join-Path $game 'speed.exe'
if((Get-FileHash $exe).Hash -ne $manifest.sha256){throw 'Unexpected EXE identity'}
$source=Get-Content (Join-Path $project 'src\EncounterMinimap.inl') -Raw
if($source -match 'MH_CreateHook|MH_EnableHook|g_originalEncounterMapActive' -or
    !$source.Contains('PatchEncounterMapCall(reinterpret_cast<void*>(Address(0x0057A164))')) {
    throw 'Minimap must patch its dedicated call site only, not a shared entry point'
}
$arms=Get-Content (Join-Path $workspace 'MWArmsAssist\src\Plugin.cpp') -Raw
$guards=[Collections.Generic.HashSet[uint32]]::new()
foreach($match in [regex]::Matches($arms,'for\(uintptr_t va:\{([^}]+)\}\)if\(!Guard')) {
    foreach($address in [regex]::Matches($match.Groups[1].Value,'0x[0-9A-Fa-f]+')){
        [void]$guards.Add([Convert]::ToUInt32($address.Value.Substring(2),16))
    }
}
foreach($name in @('FrameVA','DestructorVA','RoadblockVA')) {
    $match=[regex]::Match($arms,$name+'=(0x[0-9A-Fa-f]+)')
    if(!$match.Success){throw "Missing Arms guard constant $name"}
    [void]$guards.Add([Convert]::ToUInt32($match.Groups[1].Value.Substring(2),16))
}
if(!$guards.Contains(0x00688200) -or $guards.Count -lt 45){throw 'Arms guard inventory incomplete'}
$before=[IO.File]::ReadAllBytes($exe)
$after=[byte[]]$before.Clone()
$site=0x0057A164-0x00400000
if([BitConverter]::ToString($before,$site,6) -ne 'FF-92-88-00-00-00'){throw 'Native map call mismatch'}
# Arbitrary relocated wrapper destination: only the exact six-byte write range
# matters to the independent Arms Assist guard-span check.
$patch=[byte[]](0x90,0xE8,0x11,0x22,0x33,0x44)
[Array]::Copy($patch,0,$after,$site,6)
# alpha39 GetCatchupCheat prologue is the only new detour. Model the full
# seven-byte stolen span conservatively for the independent Arms guards.
[Array]::Copy([byte[]](0xE9,0x11,0x22,0x33,0x44,0x90,0x90),0,$after,0x9390,7)
foreach($va in ($guards | Sort-Object)) {
    $offset=$va-0x00400000
    if([BitConverter]::ToString($before,$offset,12) -ne [BitConverter]::ToString($after,$offset,12)){
        throw ('Arms initialization guard overlaps map patch: {0:X8}' -f $va)
    }
    'PASS Arms initialization guard untouched {0:X8}' -f $va
}
'PASS all {0} Arms guard spans unchanged by map and alpha39 power detour; global IsActive entry is not detoured' -f $guards.Count
'NOTE offline overlap proof only; actual Arms READY / player spike / police EMP require next game session'
