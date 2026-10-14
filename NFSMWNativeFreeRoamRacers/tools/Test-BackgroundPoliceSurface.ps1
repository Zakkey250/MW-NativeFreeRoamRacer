$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'Test-EncounterPowerSurface.ps1')
$source=Get-Content (Join-Path $projectRoot 'src/BackgroundPolice.inl') -Raw
$guards=[regex]::Matches($source,'\{(0x[0-9A-F]{8}),\{((?:0x[0-9A-F]{2},?){8})\}\}')
if($guards.Count -ne 12){throw 'Expected 12 background police guards'}
foreach($g in $guards){
    $va=[Convert]::ToUInt32($g.Groups[1].Value.Substring(2),16)
    $expected=($g.Groups[2].Value.Split(',')|ForEach-Object {$_.Substring(2)}) -join '-'
    if((ReadTargetBytes $va 8) -ne $expected){throw ('Police surface mismatch {0:X8}' -f $va)}
    Write-Output ('PASS background police exact bytes {0:X8}' -f $va)
}
$baseline=Join-Path $projectRoot '../rollback/2026-09-14-pre-alpha.41'
foreach($folder in 'src','include'){
    foreach($file in Get-ChildItem (Join-Path $baseline $folder) -File){
        if($file.Name -in 'Runtime.cpp','Plugin.cpp','EncounterHudFrame.inl','EncounterSignal.inl','EncounterBattle.inl','EncounterRoute.inl'){continue}
        if((Get-FileHash $file.FullName).Hash -ne (Get-FileHash (Join-Path $projectRoot ($folder+'/'+$file.Name))).Hash){throw "Accepted alpha40 changed: $($file.Name)"}
    }
}
foreach($forbidden in 'MH_CreateHook','StartPursuit(','EndPursuit(','KillVehicle(','SetGoal(','SpawnCop(','0x0060AAC0','0x0042AB80'){
    if($source.Contains($forbidden)){throw "Forbidden background action $forbidden"}
}
Write-Output 'PASS accepted alpha40 non-follower gameplay preserved; prompt/follower changes checked separately; background adapter has no hooks, forced pursuit, cop spawn, goal or vehicle teardown'
