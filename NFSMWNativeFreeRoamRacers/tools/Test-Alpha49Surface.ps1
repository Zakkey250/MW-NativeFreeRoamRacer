$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'Verify-NFSMWNativeFreeRoamRacers.ps1')
$baseline=Join-Path $projectRoot '..\rollback\2026-09-14-pre-alpha49-frontend-lifetime'
foreach($folder in 'src','include','config') {
 foreach($file in Get-ChildItem (Join-Path $baseline $folder) -File) {
  if($file.Name -in 'Plugin.cpp','Runtime.cpp','DynamicVehicleCatalog.inl') {continue}
  if((Get-FileHash $file.FullName).Hash -ne (Get-FileHash (Join-Path $projectRoot ($folder+'\'+$file.Name))).Hash) {throw "Unrelated source changed: $($file.Name)"}
 }
}
$runtime=Get-Content (Join-Path $projectRoot 'src\Runtime.cpp') -Raw
$normalized=$runtime.Replace('alpha.49-frontend-lifetime-cache','alpha.48-catalog-frontend-layout')
$normalized=$normalized -replace '(?m)^    CaptureFrontendCatalog\(\); // Must run before the free-roam-only guard\.\r?\n',''
$normalized=$normalized -replace '(?m)^    try \{ PrepareCatalogPersistence\(\); \}\r?\n    catch\(\.\.\.\) \{ Log\(LogLevel::Warning,"CALIBRATION_CACHE_DISABLED initialization=failed"\); \}\r?\n',''
if(($normalized -replace '\r\n',"`n") -cne ((Get-Content (Join-Path $baseline 'src\Runtime.cpp') -Raw) -replace '\r\n',"`n")) {throw 'Unexpected runtime changes'}
if($runtime.IndexOf('CaptureFrontendCatalog();') -gt $runtime.IndexOf('if (!IsFreeRoam())')) {throw 'Menu capture behind free-roam guard'}
$cache=Get-Content (Join-Path $projectRoot 'src\PersistentVehicleCatalog.inl') -Raw
foreach($token in 'header.identity!=identity','digest!=header.payloadHash','MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH','sourceChanged=1','liveModelValidation=1') {
 if(!$cache.Contains($token)){throw "Cache guard missing: $token"}
}
'PASS AI, battle, HUD, audio, INIs and retention source unchanged from alpha48'
'PASS menu calibration before free-roam guard; persistent values are versioned, source-bound and corruption-checked'
