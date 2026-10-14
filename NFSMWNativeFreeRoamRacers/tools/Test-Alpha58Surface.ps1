param([Parameter(Mandatory)][string]$GameDirectory)
$ErrorActionPreference='Stop'
$projectRoot=Split-Path $PSScriptRoot
$manifest=Get-Content (Join-Path $projectRoot '../integration/NFSMW-ExecutableTarget.json') -Raw | ConvertFrom-Json
$exe=Join-Path $GameDirectory $manifest.executable
if((Get-FileHash -LiteralPath $exe).Hash -ne $manifest.sha256){throw 'Target EXE differs'}
$bytes=[IO.File]::ReadAllBytes($exe)
$pe=[BitConverter]::ToInt32($bytes,0x3c)
$count=[BitConverter]::ToUInt16($bytes,$pe+6)
$optional=[BitConverter]::ToUInt16($bytes,$pe+20)
function ReadTargetBytes([uint32]$va,[int]$length) {
    $rva=$va-0x400000
    for($i=0;$i -lt $count;$i++) {
        $s=$pe+24+$optional+40*$i
        $start=[BitConverter]::ToUInt32($bytes,$s+12)
        $size=[BitConverter]::ToUInt32($bytes,$s+16)
        $raw=[BitConverter]::ToUInt32($bytes,$s+20)
        if($rva -ge $start -and $rva+$length -le $start+$size){return [BitConverter]::ToString($bytes,$raw+$rva-$start,$length)}
    }
    throw 'Address outside file-backed section'
}
$text=Get-Content (Join-Path $projectRoot 'src/EncounterCustomSpeed.inl') -Raw
$guards=0
foreach($m in [regex]::Matches($text,'\{(0x[0-9A-Fa-f]+),"((?:\\x[0-9A-Fa-f]{2})+)",(\d+)\}')) {
    $va=[Convert]::ToUInt32($m.Groups[1].Value.Substring(2),16)
    $expected=($m.Groups[2].Value -replace '^\\x','' -replace '\\x','-').ToUpperInvariant()
    if((ReadTargetBytes $va ([int]$m.Groups[3].Value)) -ne $expected){throw "Speed guard mismatch: $va"}
    ++$guards
}
if($guards -ne 3){throw 'Expected three speed guards'}
$baseline=Join-Path $projectRoot '../rollback/2026-09-17-pre-alpha58-cruise'
$allowed=@('src/Runtime.cpp','src/Plugin.cpp','src/EncounterSignal.inl','src/EncounterCustomSpeed.inl','config/NFSMWNativeFreeRoamRacers.ini')
$unchanged=0
foreach($folder in 'src','include','config') {
    foreach($file in Get-ChildItem -LiteralPath (Join-Path $baseline $folder) -File) {
        $relative=$folder+'/'+$file.Name
        if($relative -in $allowed){continue}
        if((Get-FileHash -LiteralPath $file.FullName).Hash -ne (Get-FileHash -LiteralPath (Join-Path $projectRoot $relative)).Hash){throw "Out-of-scope source change: $relative"}
        ++$unchanged
    }
}
"PASS alpha58 guards=$guards executable=$($manifest.sha256) unchangedPriorFiles=$unchanged; optional integrations preserved"
