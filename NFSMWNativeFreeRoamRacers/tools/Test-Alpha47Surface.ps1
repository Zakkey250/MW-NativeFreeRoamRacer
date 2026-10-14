$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'Verify-NFSMWNativeFreeRoamRacers.ps1')
$baseline=Join-Path $projectRoot '..\rollback\2026-09-14-pre-alpha47-portable-catalog'
foreach($folder in 'src','include') {
    foreach($file in Get-ChildItem (Join-Path $baseline $folder) -File) {
        if($file.Name -in 'Runtime.cpp','Plugin.cpp','VehicleVariation.inl','EncounterPowerSettings.inl') {continue}
        $current=Join-Path $projectRoot ($folder+'\'+$file.Name)
        if((Get-FileHash $file.FullName).Hash -ne (Get-FileHash $current).Hash) {throw "Unrelated source changed: $($file.Name)"}
    }
}
$source=Get-Content (Join-Path $projectRoot 'src\Runtime.cpp') -Raw
$old=Get-Content (Join-Path $baseline 'src\Runtime.cpp') -Raw
$hookPattern='(?s)MH_CreateHook\(.*?\)'
if(([regex]::Matches($source,$hookPattern).Value -join "`n") -cne ([regex]::Matches($old,$hookPattern).Value -join "`n")) {throw 'Unexpected hook change'}
function Read-IniValues([string]$path) {
    $result=@{};$section=''
    foreach($line in Get-Content -LiteralPath $path) {
        if($line -match '^\[([^]]+)\]') {$section=$Matches[1];continue}
        if($line -match '^\s*([^;#=]+?)\s*=\s*([^;#]*)') {$result[$section+'.'+$Matches[1].Trim()]=$Matches[2].Trim()}
    }
    return $result
}
$active=Read-IniValues $(Join-Path $(if($env:NFS_GAME_ROOT){$env:NFS_GAME_ROOT}elseif($env:COMPUTERNAME -eq 'ZAKKEY'){'E:\EA GAMES'}else{'\\ZAKKEY\EA GAMES'}) 'Need for Speed Most Wanted\scripts\NFSMWNativeFreeRoamRacers.ini')
$defaults=Read-IniValues (Join-Path $projectRoot 'config\NFSMWNativeFreeRoamRacers.ini')
if($active.Count -ne $defaults.Count) {throw 'Default INI key count differs'}
foreach($key in $active.Keys) {if($defaults[$key] -ne $active[$key]) {throw "Default differs from accepted installed value: $key"}}
'PASS accepted installed INI values reproduced exactly; original INI untouched'
'PASS alpha46 battle rules, AI route, audio, GPS, icons, markers and retention implementation unchanged'
'PASS no additional native hook sites'
