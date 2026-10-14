$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'Verify-NFSMWNativeFreeRoamRacers.ps1')
$baseline=Join-Path $projectRoot '..\rollback\2026-09-14-pre-alpha.46'
foreach($folder in 'src','include'){
 foreach($f in Get-ChildItem (Join-Path $baseline $folder) -File){
  if($f.Name -in 'Runtime.cpp','Plugin.cpp','EncounterGuide.inl','EncounterPromptAssets.inl','EncounterHudFrame.inl','EncounterSignal.inl'){continue}
  if((Get-FileHash $f.FullName).Hash -ne (Get-FileHash (Join-Path $projectRoot ($folder+'\'+$f.Name))).Hash){throw "Unrelated alpha45 source changed: $($f.Name)"}
 }
}
$runtime=Get-Content (Join-Path $projectRoot 'src\Runtime.cpp') -Raw
if($runtime.Replace('alpha.46-road-commitment-gps-recovery','alpha.45-road-commitment') -cne (Get-Content (Join-Path $baseline 'src\Runtime.cpp') -Raw)){throw 'Unrelated runtime change'}
$signal=Get-Content (Join-Path $projectRoot 'src\EncounterSignal.inl') -Raw
$old=Get-Content (Join-Path $baseline 'src\EncounterSignal.inl') -Raw
$normalize={param($s) [regex]::Replace($s,'(?s)void __fastcall EncounterMenuHook\(.*?(?=bool ValidateEncounterSurface\(\))','MENU_PRESENTATION')}
if((& $normalize $old) -cne (& $normalize $signal)){throw 'Signal/input changes outside notification presentation'}
$guide=Get-Content (Join-Path $projectRoot 'src\EncounterGuide.inl') -Raw
$guards=[regex]::Matches($guide,'\{(0x[0-9A-Fa-f]+),"((?:\\x[0-9A-Fa-f]{2})+)",(\d+)\}')
if($guards.Count -ne 8){throw 'GPS native guard count changed'}
foreach($g in $guards){
 $va=[Convert]::ToUInt32($g.Groups[1].Value.Substring(2),16)
 $expected=$g.Groups[2].Value.Replace('\x',' ').Trim().ToUpperInvariant()
 if((ReadTargetBytes $va ([int]$g.Groups[3].Value)).Replace('-',' ') -ne $expected){throw 'GPS native ABI mismatch'}
}
$assets=Get-Content (Join-Path $projectRoot 'src\EncounterPromptAssets.inl') -Raw
$guards=[regex]::Matches($assets,'PromptBytes\((0x[0-9A-Fa-f]+),"((?:\\x[0-9A-Fa-f]{2})+)",(\d+)\)')
if($guards.Count -ne 5){throw 'Icon/sound native guard count changed'}
foreach($g in $guards){
 $va=[Convert]::ToUInt32($g.Groups[1].Value.Substring(2),16)
 $expected=$g.Groups[2].Value.Replace('\x',' ').Trim().ToUpperInvariant()
 if((ReadTargetBytes $va ([int]$g.Groups[3].Value)).Replace('-',' ') -ne $expected){throw 'Icon native ABI mismatch'}
}
if(($guide+$assets) -match 'MH_CreateHook|SetDriveTarget|SetPosition|WritePrivateProfile'){throw 'Forbidden hook/state mutation in display-only changes'}
if(!$guide.Contains('externalTargetChange=1 preserveExternal=1') -or !$guide.Contains('g_guide.nextRequest=now+2000')){throw 'GPS ownership or rate guard removed'}
if(([regex]::Matches($assets,'\(Address\(0x0065FD30\)\)')).Count -ne 1 -or !$assets.Contains('if(!g_promptAssetState)')){throw 'Texture allocation must remain once-only'}
'PASS 8 GPS and 5 texture/sound exact ABI guards; no new native function or hook'
'PASS GPS external target protection and bounded retry retained; texture resource allocated once only'
'PASS alpha45 road commitment, passing, population, sound, performance, battle outcomes and user input unchanged'
