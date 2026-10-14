$ErrorActionPreference='Stop'
$projectRoot=Split-Path $PSScriptRoot
. (Join-Path $PSScriptRoot 'Verify-NFSMWNativeFreeRoamRacers.ps1')
foreach($entry in @(
 @(0x0065FD30,'6A FF 68 5B A1 87 00 64'),@(0x006616F0,'8B 44 24 04 56 8B F1'),
 @(0x00503400,'53 55 56 57 8B 7C 24 14'),@(0x00515B90,'8B 44 24 04 85 C0 74 13'),
 @(0x004AE8F0,'A1 F8 86 8F 00 85 C0'),
 @(0x0057BD9B,'8B 0D A8 1F 91 00 83 C4 0C 6A 13 E8 45 2B F3 FF')
)){
 $actual=(ReadTargetBytes $entry[0] ($entry[1].Split(' ').Count)).Replace('-',' ')
 if($actual -ne $entry[1]){throw ('Prompt exact surface mismatch {0:X8}' -f $entry[0])}
 'PASS prompt surface {0:X8}' -f $entry[0]
}
$baseline=Join-Path $projectRoot '..\rollback\2026-09-14-pre-alpha.42\src'
foreach($file in Get-ChildItem $baseline -File){
 if($file.Name -in 'Runtime.cpp','Plugin.cpp','EncounterHudFrame.inl','EncounterSignal.inl','EncounterBattle.inl','EncounterRoute.inl'){continue}
 if((Get-FileHash $file.FullName).Hash -ne (Get-FileHash (Join-Path $projectRoot ('src\'+$file.Name))).Hash){throw "Unrelated source changed: $($file.Name)"}
}
$old=Get-Content (Join-Path $baseline 'Runtime.cpp') -Raw
$new=(Get-Content (Join-Path $projectRoot 'src\Runtime.cpp') -Raw).Replace('alpha.43-icon-owner-pursuit-path','alpha.42-prompt-icon-sound').Replace('alpha.42-prompt-icon-sound','alpha.41-background-police')
$new=$new -replace '#include "EncounterPursuitPath.h"\r?\n',''
if($old -cne $new){throw 'Runtime changed beyond version label'}
'PASS alpha41 population, AI skill, catchup, audio, police, markers unchanged; follower checked by alpha43 verifier'
$asset=Join-Path $projectRoot 'artifacts\Release\NFSMWNativeFreeRoamRacers.tpk'
if((Get-FileHash $asset).Hash -ne 'CA929293186A10867B65729CE78AF89C5F5DE880239A402EC68E35E0169D32D1'){throw 'Icon pack hash mismatch'}
'PASS original one-texture TPK identity'
