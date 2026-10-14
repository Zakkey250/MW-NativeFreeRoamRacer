$ErrorActionPreference='Stop'
$project=Split-Path $PSScriptRoot
$baseline=Join-Path (Split-Path $project) 'rollback/2026-09-22-pre-alpha83/source'
function Read([string]$p){(Get-Content -LiteralPath $p -Raw).Replace("`r`n","`n")}
$old=Read (Join-Path $baseline 'src/Runtime.cpp');$new=Read (Join-Path $project 'src/Runtime.cpp')
function Segment([string]$text,[string]$start,[string]$end){
    $at=$text.IndexOf($start);if($at -lt 0){throw "Missing start $start"}
    $to=$text.IndexOf($end,$at);if($to -lt 0){throw "Missing end $end"}
    $text.Substring($at,$to-$at)
}
if((Segment $old 'bool SpawnAtAnchor(' 'void RemoveRacer(') -cne (Segment $new 'bool SpawnAtAnchor(' 'void RemoveRacer(')){throw 'Fresh construction/performance/lease checks changed'}
if((Segment $old 'void Replenish(' 'void LogTelemetry(') -cne (Segment $new 'void Replenish(' 'void LogTelemetry(')){throw 'Replenishment gates changed'}
if((Segment $old 'void ClearWorld(' 'void Update(') -cne (Segment $new 'void ClearWorld(' 'void Update(')){throw 'World cleanup changed'}
foreach($file in @('src/PopulationBridge.inl','src/ConstructionLiveSet.inl','src/VehiclePerformance.inl','src/PerformanceRetry.inl','src/VehicleVarietyExpansion.inl','src/AudioPoolSupport.inl','src/EncounterBattle.inl')) {
    if((Get-FileHash (Join-Path $project $file)).Hash -ne (Get-FileHash (Join-Path $baseline $file)).Hash){throw "Unexpected change $file"}
}
if(!$new.Contains('L"RetainPreparedAppearance",0,modulePath)==1')){throw 'Default OFF explicit opt-in missing'}
'PASS fresh performance/anchor/construction/lease gates, reset paths and unrelated native code unchanged; new opt-in default OFF'
