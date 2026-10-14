$ErrorActionPreference='Stop'
$project=Split-Path $PSScriptRoot
$baseline=Join-Path (Split-Path $project) 'rollback/2026-09-22-pre-alpha85/source'
function Read([string]$p){(Get-Content -LiteralPath $p -Raw).Replace("`r`n","`n")}
$old=Read (Join-Path $baseline 'src/Runtime.cpp');$new=Read (Join-Path $project 'src/Runtime.cpp')
if($old.Replace('alpha.84-telemetry-log-io','alpha.85-performance-reference-guard') -cne $new){throw 'Unexpected runtime/construction change'}
foreach($folder in @('src','include','config')) {
    foreach($file in Get-ChildItem -LiteralPath (Join-Path $baseline $folder) -Recurse -File) {
        $relative=$file.FullName.Substring($baseline.Length+1)
        if($relative -in 'src\Runtime.cpp','src\VehiclePerformance.inl','include\Version.h'){continue}
        if((Get-FileHash $file.FullName).Hash -ne (Get-FileHash (Join-Path $project $relative)).Hash){throw "Unexpected change $relative"}
    }
}
$source=Read (Join-Path $project 'src/VehiclePerformance.inl')
$oldPerformance=Read (Join-Path $baseline 'src/VehiclePerformance.inl')
$start=$source.IndexOf('void WriteCareerPerformancePlan(')
if($source.Substring($start) -cne $oldPerformance.Substring($oldPerformance.IndexOf('void WriteCareerPerformancePlan('))){throw 'Performance write/readback changed'}
if($source.IndexOf('ValidateNativePerformanceReferences(key,&instance,*plan)') -gt $source.IndexOf('success=true;')){throw 'Reference validation must precede success'}
$spawn=$new.Substring($new.IndexOf('bool SpawnAtAnchor('))
if($spawn.IndexOf('ReadCareerPerformancePlan(vehicleKey,&performancePlan)') -gt $spawn.IndexOf('ConstructTrafficSeed(')){throw 'Fresh plan gate must precede allocation'}
'PASS runtime, constructor SEH, final fresh gates, performance writes/readback, career locks, LB/audio/AI and all INIs preserved'
