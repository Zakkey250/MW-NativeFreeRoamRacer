$ErrorActionPreference='Stop'
$project=Split-Path $PSScriptRoot
$baseline=Join-Path (Split-Path $project) 'rollback/2026-09-22-pre-alpha82/source'
function Read([string]$p){(Get-Content -LiteralPath $p -Raw).Replace("`r`n","`n")}
$old=Read (Join-Path $baseline 'src/PopulationBridge.inl')
$new=Read (Join-Path $project 'src/PopulationBridge.inl')
function Segment([string]$text,[string]$start,[string]$end){
    $at=$text.IndexOf($start);if($at -lt 0){throw "Missing start $start"}
    $to=$text.IndexOf($end,$at);if($to -lt 0){throw "Missing end $end"}
    $text.Substring($at,$to-$at)
}
if((Segment $old 'bool ReclaimableTraffic(' 'bool ReclaimOneTraffic(') -cne
   (Segment $new 'bool ReclaimableTraffic(' 'unsigned CountMovingOrdinaryTraffic(')){throw 'Geometry guards changed'}
if((Segment $old 'VehicleSnapshot current;' '++moving;') -cne
   (Segment $new 'VehicleSnapshot current;' '++moving;')){throw 'Fresh traffic-count criteria changed'}
if((Segment $old '        void* simable=GetSimablePointer' '    const auto now=GetTickCount64();') -cne
   (Segment $new '        void* simable=GetSimablePointer' '    const auto now=GetTickCount64();')){throw 'Final victim checks, floor, or release changed'}
if(!$new.Contains('if(g_settings.lbNonRacerSlots&&!movingMeasured)') -or
   !$new.Contains('moving=countMoving();movingMeasured=true;')){throw 'Missing lazy per-attempt census'}
'PASS unchanged geometry, fresh count criteria, final identity/geometry/floor/release guards'
$oldRuntime=Read (Join-Path $baseline 'src/Runtime.cpp')
$newRuntime=Read (Join-Path $project 'src/Runtime.cpp')
if($oldRuntime.Replace('alpha.81-ranked-anchor-search','alpha.82-lazy-traffic-census') -cne $newRuntime){throw 'Runtime behavior outside bridge changed'}
foreach($file in @('include/PopulationPolicy.h','include/AnchorSearch.h','src/ConstructionLiveSet.inl','src/VehiclePerformance.inl','src/PerformanceRetry.inl','src/SpawnPreparation.inl','src/AudioExpansion.inl','src/AudioPoolSupport.inl','src/EncounterBattle.inl')) {
    if((Get-FileHash (Join-Path $project $file)).Hash -ne (Get-FileHash (Join-Path $baseline $file)).Hash){throw "Unexpected mutation $file"}
}
'PASS main runtime unchanged except version label; population/capability, anchor, audio, performance and battle preserved'
