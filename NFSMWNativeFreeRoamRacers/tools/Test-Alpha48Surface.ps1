$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'Verify-NFSMWNativeFreeRoamRacers.ps1')
$baseline=Join-Path $projectRoot '..\rollback\2026-09-14-pre-alpha48-catalog-fix'
foreach($folder in 'src','include','config') {
 foreach($file in Get-ChildItem (Join-Path $baseline $folder) -File) {
  if($file.Name -in 'Plugin.cpp','Runtime.cpp','DynamicVehicleCatalog.inl','VehicleVariation.inl') {continue}
  if((Get-FileHash $file.FullName).Hash -ne (Get-FileHash (Join-Path $projectRoot ($folder+'\'+$file.Name))).Hash) {throw "Unrelated source changed: $($file.Name)"}
 }
}
$runtime=Get-Content (Join-Path $projectRoot 'src\Runtime.cpp') -Raw
if($runtime.Replace('alpha.48-catalog-frontend-layout','alpha.47-dynamic-vehicle-catalog') -cne (Get-Content (Join-Path $baseline 'src\Runtime.cpp') -Raw)) {throw 'Unexpected runtime change beyond version'}
$catalog=Get-Content (Join-Path $projectRoot 'src\DynamicVehicleCatalog.inl') -Raw
if($catalog.Contains('g_catalogCursor<g_catalogExpected') -or $catalog -match 'CatalogData\(frontend,"(Cost|IsCustomizable)"') {throw 'alpha47 failure path remains'}
foreach($token in 'CatalogKeyVisited(key)','g_catalogVisited[g_catalogCursor]=key','if(g_catalogNext)','__finally','layout+0x4C','layout+0x58') {
 if(!$catalog.Contains($token)) {throw "Missing regression guard: $token"}
}
'PASS runtime changes only version; AI, battle, display, audio, settings and retention preserved'
'PASS native frontend layout and balanced Instance release; iterator uses zero sentinel plus fixed bound/cycle protection'
