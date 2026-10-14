param([Parameter(Mandatory)][string]$GameDirectory)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'Test-Alpha51Surface.ps1') -GameDirectory $GameDirectory
$text=Get-Content (Join-Path $projectRoot 'src/EncounterCustomDrive.inl') -Raw
$count=0
foreach($m in [regex]::Matches($text,'\{(0x[0-9A-Fa-f]+),"((?:\\x[0-9A-Fa-f]{2})+)",(\d+)\}')) {
    $va=[Convert]::ToUInt32($m.Groups[1].Value.Substring(2),16)
    $expected=($m.Groups[2].Value -replace '^\\x','' -replace '\\x','-').ToUpperInvariant()
    if((ReadTargetBytes $va ([int]$m.Groups[3].Value)) -ne $expected){throw "Custom drive ABI mismatch at $va"}
    ++$count
}
if($count -ne 4){throw "Unexpected custom guard count: $count"}
$baseline=Join-Path $projectRoot '../rollback/2026-09-15-pre-alpha53-directional-trail'
$allowed=@('src/Runtime.cpp','src/Plugin.cpp','src/EncounterBattle.inl','src/EncounterRoute.inl','src/EncounterCustomFollow.inl')
foreach($folder in 'src','include','config') {
    foreach($file in Get-ChildItem -LiteralPath (Join-Path $baseline $folder) -File) {
        $relative=$folder+'/'+$file.Name
        if($relative -in $allowed){continue}
        if((Get-FileHash -LiteralPath $file.FullName).Hash -ne (Get-FileHash -LiteralPath (Join-Path $projectRoot $relative)).Hash){throw "Out-of-scope change: $relative"}
    }
}
$custom=Get-Content (Join-Path $projectRoot 'src/EncounterCustomFollow.inl') -Raw
if($custom -match '0x00788F30|0x00423860'){throw 'Custom adapter still submits road chase/path'}
'PASS alpha53 native target setter, vtable, exact race caller and second road-query call match target EXE'
'PASS alpha53 Stable AI, pure outcome, population, HUD, audio, boosts, config and cache sources preserved'
