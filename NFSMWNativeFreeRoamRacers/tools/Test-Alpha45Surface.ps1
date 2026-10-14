$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'Verify-NFSMWNativeFreeRoamRacers.ps1')
$baseline=Join-Path $projectRoot '..\rollback\2026-09-14-pre-alpha.45'
foreach($folder in 'src','include'){
 foreach($f in Get-ChildItem (Join-Path $baseline $folder) -File){
  if($f.Name -in 'Runtime.cpp','Plugin.cpp','EncounterBattle.inl','EncounterRoute.inl'){continue}
  if((Get-FileHash $f.FullName).Hash -ne (Get-FileHash (Join-Path $projectRoot ($folder+'\'+$f.Name))).Hash){throw "Unrelated alpha44 source changed: $($f.Name)"}
 }
}
$runtime=Get-Content (Join-Path $projectRoot 'src\Runtime.cpp') -Raw
$runtime=$runtime.Replace('alpha.45-road-commitment','alpha.44-moving-path-bounded-pass') -replace '#include "EncounterCommittedPath.h"\r?\n','' -replace '#include <NFSPluginSDK/Game.MW05/Types/WRoadNav.h>\r?\n',''
if($runtime -cne (Get-Content (Join-Path $baseline 'src\Runtime.cpp') -Raw)){throw 'Unrelated runtime change'}
$source=Get-Content (Join-Path $projectRoot 'src\EncounterBattle.inl') -Raw
$old=Get-Content (Join-Path $baseline 'src\EncounterBattle.inl') -Raw
$normalize={param($s)
 $s=[regex]::Replace($s,'(?s)bool NativeEncounterFollow\(.*?(?=void\* EncounterProfile\(\))','FOLLOWER_ADAPTER')
 $s=$s -replace '(?m)^static_assert\(offsetof\(NFSPluginSDK::MW05::WRoadNav,.*\r?\n',''
 foreach($line in '    Vec3 routeOrigin{};','    int routeOriginSegment=-1;','    float routeProgress=0,routeEndpointDistance=0;',
 '    g_battle.routeProgress=Distance(g_battle.routeOrigin,rival->position);','    g_battle.routeEndpointDistance=Distance(g_battle.lastDestination,rival->position);'){
  $s=$s -replace ([regex]::Escape($line)+'\r?\n'),''
 }
 $s
}
if((& $normalize $old) -cne (& $normalize $source)){throw 'Battle changed outside follower and commitment telemetry'}
$guards=[regex]::Matches($source,'\{(0x[0-9A-Fa-f]+),"((?:\\x[0-9A-Fa-f]{2})+)",(\d+)\}')
if($guards.Count -ne 34){throw 'Native guard count changed'}
foreach($g in $guards){
 $va=[Convert]::ToUInt32($g.Groups[1].Value.Substring(2),16)
 $expected=$g.Groups[2].Value.Replace('\x',' ').Trim().ToUpperInvariant()
 if((ReadTargetBytes $va ([int]$g.Groups[3].Value)).Replace('-',' ') -ne $expected){throw 'Native battle ABI differs'}
}
if(!$source.Contains('if(busy) return true;') -or !$source.Contains('ai.nav,&position,nullptr,false)') -or !$source.Contains('encounter_route::CommitRoute(')){throw 'Native query safeguards missing'}
$route=Get-Content (Join-Path $projectRoot 'src\EncounterRoute.inl') -Raw
foreach($guard in 'caller!=Address(0x00428BCF)','g_encounterAdapterThread.load(std::memory_order_acquire)!=GetCurrentThreadId()',
'g_battle.model.phase()!=battle::Phase::Active','g_battle.model.leader()!=battle::Leader::Player',
'!g_battle.routeHint.valid','!ReadEncounterAI(g_battle.rival,ai)||ai.nav!=nav'){
 if(!$route.Contains($guard)){throw "Native renewal ownership guard missing: $guard"}
}
$policy=Get-Content (Join-Path $projectRoot 'include\EncounterCommittedPath.h') -Raw
if($policy -match 'reinterpret_cast|WriteProcessMemory|VirtualProtect|__thiscall'){throw 'Commit policy must be pure logic'}
'PASS 34 existing native ABI guards; no new hook or native function'
'PASS alpha44 population, audio, tuning, boost, police, HUD, markers, passing, input and outcome logic unchanged'
'PASS native renewal remains caller/thread/vehicle/phase scoped; invalid nav and consumed goal are not latched'
