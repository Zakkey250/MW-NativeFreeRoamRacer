$ErrorActionPreference='Stop'
$project=Split-Path $PSScriptRoot
$baseline=Join-Path (Split-Path $project) 'rollback/2026-09-22-pre-alpha81/source'
foreach($file in @('PopulationBridge.inl','ConstructionLiveSet.inl','AnchorRoadSeed.inl','SpawnPreparation.inl','VehiclePerformance.inl','PerformanceRetry.inl','AudioPoolSupport.inl','AudioExpansion.inl','EncounterBattle.inl')) {
    if((Get-FileHash (Join-Path $project "src/$file")).Hash -ne (Get-FileHash (Join-Path $baseline "src/$file")).Hash){throw "Guard changed: $file"}
    "PASS unchanged $file"
}
if((Get-FileHash (Join-Path $project 'include/RefillPolicy.h')).Hash -ne (Get-FileHash (Join-Path $baseline 'include/RefillPolicy.h')).Hash){throw 'Ranking/cadence policy changed'}
$old=(Get-Content (Join-Path $baseline 'src/Runtime.cpp') -Raw).Replace("`r`n","`n")
$new=(Get-Content (Join-Path $project 'src/Runtime.cpp') -Raw).Replace("`r`n","`n")
$begin='bool SpawnAtAnchor('; $end='void RemoveRacer('
$a=$old.Substring($old.IndexOf($begin),$old.IndexOf($end)-$old.IndexOf($begin))
$b=$new.Substring($new.IndexOf($begin),$new.IndexOf($end)-$new.IndexOf($begin))
if($a -cne $b){throw 'Native construction/admission changed'}
'PASS SpawnAtAnchor byte-equivalent; fresh identity, road, capacity and lifecycle verification preserved'
if(!$new.Contains('return GetTrafficRoad(vehicle.pointer,&road)&&CaptureAnchorRoadSeed(road,seed);')){throw 'Selected candidate road validation missing'}
if(!$new.Split('void ClearWorld(')[-1].Contains('g_anchorSearch.Clear();')){throw 'Missing scratch cleanup'}
'PASS selected candidate uses existing validator; world cleanup present'
