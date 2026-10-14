$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'Test-BackgroundPoliceSurface.ps1')
. (Join-Path $PSScriptRoot 'Test-EncounterPromptSurface.ps1')
foreach($g in @(
 @(0x0057BD1C,'8B 7E 0C'),@(0x0057BD55,'6A 01 68 F8 C8 79 50 57 E8 AE 8F F9 FF'),
 @(0x00428A61,'6A 00 6A 00'),@(0x00428A71,'E8 BA 04 36 00'),
 @(0x0078852C,'8B 47 2C 83 78 7C 03 8B A8 E0 02 00 00'),
 @(0x00788F48,'68 FC 03 00 00'),@(0x0078862E,'C7 40 78 02 00 00 00')
)){
 if((ReadTargetBytes $g[0] ($g[1].Split(' ').Count)).Replace('-',' ') -ne $g[1]){throw 'alpha43 exact ABI mismatch'}
 'PASS alpha43 native evidence {0:X8}' -f $g[0]
}
$before=Join-Path $projectRoot '..\rollback\2026-09-14-pre-alpha.43'
foreach($folder in 'src','include'){
 foreach($f in Get-ChildItem (Join-Path $before $folder) -File){
  if($f.Name -in 'Runtime.cpp','Plugin.cpp','EncounterHudFrame.inl','EncounterBattle.inl','EncounterRoute.inl'){continue}
  if((Get-FileHash $f.FullName).Hash -ne (Get-FileHash (Join-Path $projectRoot ($folder+'\'+$f.Name))).Hash){throw "Unrelated alpha42 source changed: $($f.Name)"}
 }
}
$runtime=Get-Content (Join-Path $projectRoot 'src\Runtime.cpp') -Raw
$runtime=$runtime.Replace('alpha.43-icon-owner-pursuit-path','alpha.42-prompt-icon-sound') -replace '#include "EncounterPursuitPath.h"\r?\n',''
if($runtime -cne (Get-Content (Join-Path $before 'src\Runtime.cpp') -Raw)){throw 'Unrelated runtime change'}
$route=Get-Content (Join-Path $projectRoot 'src\EncounterRoute.inl') -Raw
$route=$route.Replace('g_originalEncounterPath(nav,&position,nullptr,flag)','g_originalEncounterPath(nav,&position,&heading,flag)')
if($route -cne (Get-Content (Join-Path $before 'src\EncounterRoute.inl') -Raw)){throw 'Route hook owner/caller scope changed'}
$battle=Get-Content (Join-Path $projectRoot 'src\EncounterBattle.inl') -Raw
$old=Get-Content (Join-Path $before 'src\EncounterBattle.inl') -Raw
$normalize={param($s)
 $s=[regex]::Replace($s,'(?s)bool NativeEncounterFollow\(.*?(?=void\* EncounterProfile\(\))','FOLLOWER_ADAPTER')
 $s=$s.Replace('float routeAge=0,pathSeconds=0;','float routeAge=0;')
 $s -replace '(?m)^    g_battle.pathSeconds\+=dt;\r?\n',''
}
if((& $normalize $old) -cne (& $normalize $battle)){throw 'Battle changed outside follower and path age counter'}
if(!$battle.Contains('ai.nav,&position,nullptr,false)') -or !$battle.Contains('encounter_route::KeepPursuitPath(')){throw 'Follower must use native unconstrained endpoint and bounded completed-path retention'}
'PASS alpha42 input, sound, texture loader, population, tuning, audio, boost, police, marker and outcomes unchanged'
'PASS follower uses native road graph; no race restart, direct steering, global race-state change or synchronous path pumping'
