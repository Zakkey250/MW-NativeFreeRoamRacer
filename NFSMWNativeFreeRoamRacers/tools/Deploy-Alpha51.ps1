param([ValidateSet('Prepare','Deploy')][string]$Phase='Prepare',
    [string]$GameDirectory=$(Join-Path $(if($env:NFS_GAME_ROOT){$env:NFS_GAME_ROOT}elseif($env:COMPUTERNAME -eq 'ZAKKEY'){'E:\EA GAMES'}else{'\\ZAKKEY\EA GAMES'}) 'Need for Speed Most Wanted Main'))
$ErrorActionPreference='Stop'
$project=Split-Path $PSScriptRoot;$workspace=Split-Path $project
$game=(Resolve-Path -LiteralPath $GameDirectory).Path
$evidence=Join-Path $workspace 'diagnostics/custom-ai-20260915-alpha51'
$backup=Join-Path $workspace 'rollback/2026-09-15-pre-alpha51-custom-ai'
$installed=Join-Path $game 'scripts/NFSMWNativeFreeRoamRacers.asi'
$ini=Join-Path $game 'scripts/NFSMWNativeFreeRoamRacers.ini'
$artifact=Join-Path $project 'artifacts/Release/NFSMWNativeFreeRoamRacers.asi'
$oldHash='24BB75458CE931805CB9EEBFF33722D2F8BCBE6D1171C1FCBA7B762809055767'
$newHash='81ACA1FA5D47FAD9BE824F5DCA49AD587CB5A4F2E12B18BD256E96434A5D34C6'
function Hash([string]$p){(Get-FileHash -LiteralPath $p).Hash}
function Stopped {
    if(Get-CimInstance Win32_Process | Where-Object { $_.Name -in @('speed.exe','nfsMW.exe') }) {
        throw 'Game running: do not operate or terminate it; deployment deferred.'
    }
}
Stopped
if((Hash $artifact) -ne $newHash -or (Hash $installed) -ne $oldHash -or
   (Hash (Join-Path $backup 'Release/NFSMWNativeFreeRoamRacers.asi')) -ne $oldHash) {throw 'ASI identity mismatch'}
$target=Get-Content (Join-Path $workspace 'integration/NFSMW-ExecutableTarget.json') -Raw | ConvertFrom-Json
if((Hash (Join-Path $game $target.executable)) -ne $target.sha256) {throw 'Game target differs'}
$config=Get-Content (Join-Path $game 'Launch-SV-Safe.json') -Raw | ConvertFrom-Json
$state=Get-Content -LiteralPath $config.StatePath -Raw | ConvertFrom-Json
if($state.GameDirectory -ne $game) {throw 'Production SV belongs to another installation'}
function Snapshot {
    $files=@(
        Get-ChildItem -LiteralPath $game -File | Where-Object {$_.Extension -in '.exe','.dll','.ini','.json'}
        Get-ChildItem -LiteralPath (Join-Path $game 'scripts') -Recurse -File | Where-Object {$_.Extension -in '.asi','.dll','.ini','.json','.tpk','.wav','.mp3' -or $_.Name -eq 'VehicleCatalog.cache.bin'}
        Get-ChildItem -LiteralPath $game -Directory | Where-Object {$_.Name -like 'SVSaves_*'} | ForEach-Object {Get-ChildItem -LiteralPath $_.FullName -Recurse -File}
        Get-ChildItem -LiteralPath $state.VaultProfileDirectory -Recurse -File
        Get-Item -LiteralPath $config.StatePath,$state.ManifestPath,$state.OriginalSourceFile
    ) | Sort-Object FullName -Unique
    $result=@{};foreach($file in $files){$result[$file.FullName]=Hash $file.FullName};return $result
}
New-Item -ItemType Directory -Path $evidence -Force | Out-Null
$beforePath=Join-Path $evidence 'deployment-before.json'
if($Phase -eq 'Prepare') {
    if(Test-Path -LiteralPath $beforePath) {throw 'Preflight evidence already exists; do not overwrite'}
    if((Hash $ini) -ne (Hash (Join-Path $backup 'installed.ini'))) {throw 'INI differs from rollback copy'}
    & (Join-Path $workspace 'tools/Verify-NFSPatcherRuntime.ps1') -GameDirectory $game | Tee-Object (Join-Path $evidence 'runtime-before.txt')
    Snapshot | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath $beforePath -Encoding utf8
    'PASS preflight; only use apply_patch to add the two approved INI settings before Deploy.'
    return
}
$before=Get-Content -LiteralPath $beforePath -Raw | ConvertFrom-Json -AsHashtable
function IniValues([string]$path) {
    $values=@{};$section=''
    foreach($line in Get-Content -LiteralPath $path) {
        $text=$line.Trim();if(!$text -or $text.StartsWith(';') -or $text.StartsWith('#')){continue}
        if($text -match '^\[(.+)\]$'){$section=$matches[1];continue}
        if($text -match '^([^=]+)=(.*)$'){$values[$section+'/'+$matches[1].Trim()]=($matches[2] -split '[;#]',2)[0].Trim()}
    };return $values
}
$old=IniValues (Join-Path $backup 'installed.ini');$new=IniValues $ini
foreach($key in $old.Keys) {if($old[$key] -ne $new[$key]) {throw "Existing setting changed: $key"}}
if($new.Count -ne $old.Count+2 -or $new['Encounter/AIMode'] -ne 'Custom AI' -or $new['Encounter/CustomAILeaderPowerScale'] -ne '1.25') {
    throw 'Only the two Custom AI settings may be added'
}
$preCopy=Snapshot
foreach($key in $before.Keys) {if($key -ne $ini -and $preCopy[$key] -ne $before[$key]){throw "Unexpected pre-copy change: $key"}}
if($before.Count -ne $preCopy.Count){throw 'Unexpected pre-copy additions/removals'}
Stopped
Copy-Item -LiteralPath $artifact -Destination $installed -Force
if((Hash $installed) -ne $newHash){throw 'Installed hash mismatch'}
& (Join-Path $workspace 'tools/Verify-NFSPatcherRuntime.ps1') -GameDirectory $game | Tee-Object (Join-Path $evidence 'runtime-after.txt')
$after=Snapshot
$after | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $evidence 'deployment-after.json') -Encoding utf8
$changed=@($before.Keys | Where-Object {$after[$_] -ne $before[$_]})
if($changed.Count -ne 2 -or $installed -notin $changed -or $ini -notin $changed -or $before.Count -ne $after.Count) {
    throw 'Unexpected installation diff; inspect evidence before play'
}
"PASS audited=$($before.Count) changed=2 (ASI, own INI only) unchanged=$($before.Count-2); existing settings preserved"
"PASS installed Custom AI; ASI=$newHash; no game was launched"
