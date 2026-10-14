param([Parameter(Mandatory)][string]$GameDirectory)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'Test-Alpha53Surface.ps1') -GameDirectory $GameDirectory
$text=Get-Content (Join-Path $projectRoot 'src/EncounterCustomSpeed.inl') -Raw
$count=0
foreach($m in [regex]::Matches($text,'\{(0x[0-9A-Fa-f]+),"((?:\\x[0-9A-Fa-f]{2})+)",(\d+)\}')) {
    $va=[Convert]::ToUInt32($m.Groups[1].Value.Substring(2),16)
    $expected=($m.Groups[2].Value -replace '^\\x','' -replace '\\x','-').ToUpperInvariant()
    if((ReadTargetBytes $va ([int]$m.Groups[3].Value)) -ne $expected){throw "Custom speed ABI mismatch at $va"}
    ++$count
}
if($count -ne 3){throw "Unexpected custom speed guards: $count"}
$baseline=Join-Path $projectRoot '../rollback/2026-09-15-pre-alpha54-chase-speed'
$allowed=@('src/Runtime.cpp','src/Plugin.cpp','src/EncounterBattle.inl')
foreach($folder in 'src','include','config') {
    foreach($file in Get-ChildItem -LiteralPath (Join-Path $baseline $folder) -File) {
        $relative=$folder+'/'+$file.Name
        if($relative -in $allowed){continue}
        if((Get-FileHash -LiteralPath $file.FullName).Hash -ne (Get-FileHash -LiteralPath (Join-Path $projectRoot $relative)).Hash){throw "Out-of-scope change: $relative"}
    }
}
'PASS alpha54 native speed setter, vtable and exact race call match EXE; x86 RET4 surface verified'
'PASS alpha54 directional guidance, stable follow, power tiers, outcome, generation, audio and INI preserved'
