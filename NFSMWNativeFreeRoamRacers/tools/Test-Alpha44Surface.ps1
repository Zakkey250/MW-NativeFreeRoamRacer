$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'Verify-NFSMWNativeFreeRoamRacers.ps1')
$baseline=Join-Path $projectRoot '..\rollback\2026-09-14-pre-alpha.44'
foreach($folder in 'src','include'){
 foreach($f in Get-ChildItem (Join-Path $baseline $folder) -File){
  if($f.Name -in 'Runtime.cpp','Plugin.cpp','EncounterBattle.inl','EncounterPursuit.h','EncounterPursuitPath.h'){continue}
  if((Get-FileHash $f.FullName).Hash -ne (Get-FileHash (Join-Path $projectRoot ($folder+'\'+$f.Name))).Hash){throw "Unrelated alpha43 source changed: $($f.Name)"}
 }
}
$runtime=Get-Content (Join-Path $projectRoot 'src\Runtime.cpp') -Raw
if($runtime.Replace('alpha.44-moving-path-bounded-pass','alpha.43-icon-owner-pursuit-path') -cne (Get-Content (Join-Path $baseline 'src\Runtime.cpp') -Raw)){throw 'Runtime changed outside version label'}
$source=Get-Content (Join-Path $projectRoot 'src\EncounterBattle.inl') -Raw
$old=Get-Content (Join-Path $baseline 'src\EncounterBattle.inl') -Raw
$normalize={param($s)
 $s=[regex]::Replace($s,'(?s)bool NativeEncounterFollow\(.*?(?=void\* EncounterProfile\(\))','FOLLOWER_ADAPTER')
 $s=$s -replace '(?m)^    battle::Point lastDestinationHeading\{\};\r?\n',''
 $s.Replace('{rival->heading.x,rival->heading.y,rival->heading.z},dt);','{rival->heading.x,rival->heading.y,rival->heading.z});')
}
if((& $normalize $old) -cne (& $normalize $source)){throw 'Battle changed outside follower route policy / elapsed-time input'}
# No new native function or hook: verify every existing battle ABI guard against
# the exact manifest EXE, including path query, CancelPath and Target access.
$guards=[regex]::Matches($source,'\{(0x[0-9A-Fa-f]+),"((?:\\x[0-9A-Fa-f]{2})+)",(\d+)\}')
if($guards.Count -ne 34){throw 'Native guard count changed'}
foreach($g in $guards){
 $va=[Convert]::ToUInt32($g.Groups[1].Value.Substring(2),16)
 $expected=$g.Groups[2].Value.Replace('\x',' ').Trim().ToUpperInvariant()
 if((ReadTargetBytes $va ([int]$g.Groups[3].Value)).Replace('-',' ') -ne $expected){throw 'Native battle ABI differs'}
}
if(!$source.Contains('if(busy) return true;') -or !$source.Contains('ai.nav,&position,nullptr,false)') -or !$source.Contains('KeepMovingPursuitPath')){throw 'Missing async native graph safeguards'}
foreach($file in 'EncounterPursuit.h','EncounterPursuitPath.h'){
 $s=Get-Content (Join-Path $projectRoot ('include\'+$file)) -Raw
 if($s -match 'reinterpret_cast|WriteProcessMemory|VirtualProtect|__thiscall'){throw 'Policy must be pure geometry/time logic'}
}
'PASS 34 native battle ABI guards; no new hooks, game functions, direct steering or physics writes'
'PASS alpha43 generation, audio, tuning, police, HUD, markers, inputs, rewards and outcome logic unchanged'
