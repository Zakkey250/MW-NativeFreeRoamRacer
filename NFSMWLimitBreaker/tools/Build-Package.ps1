param(
    [string]$GameExe=$(Join-Path $(if($env:NFS_GAME_ROOT){$env:NFS_GAME_ROOT}elseif($env:COMPUTERNAME -eq 'ZAKKEY'){'E:\EA GAMES'}else{'\\ZAKKEY\EA GAMES'}) 'Need for Speed Most Wanted Main\speed.exe')
)
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$version='v0.1.3-resource-headroom.6'
& (Join-Path $PSScriptRoot 'Build.ps1') -GameExe $GameExe

$dist=Join-Path $root 'dist'
$stage=Join-Path $dist "NFSMWLimitBreaker-$version"
$sourceStage=Join-Path $dist "NFSMWLimitBreaker-$version-source"
$nfrOverlay=Join-Path $dist "NFRR-Bundle-Overlay-NFSMWLimitBreaker-$version"
function Assert-DirectChild([string]$parent,[string]$child){
    $parentFull=[IO.Path]::GetFullPath($parent).TrimEnd([IO.Path]::DirectorySeparatorChar)+[IO.Path]::DirectorySeparatorChar
    $childFull=[IO.Path]::GetFullPath($child)
    if(-not $childFull.StartsWith($parentFull,[StringComparison]::OrdinalIgnoreCase)){
        throw "Refusing path outside package directory: $childFull"
    }
}
Assert-DirectChild $dist $stage
Assert-DirectChild $dist $sourceStage
Assert-DirectChild $dist $nfrOverlay
if(Test-Path -LiteralPath $stage){Remove-Item -LiteralPath $stage -Recurse -Force}
if(Test-Path -LiteralPath $sourceStage){Remove-Item -LiteralPath $sourceStage -Recurse -Force}
if(Test-Path -LiteralPath $nfrOverlay){Remove-Item -LiteralPath $nfrOverlay -Recurse -Force}
New-Item -ItemType Directory -Path (Join-Path $stage 'scripts') -Force | Out-Null
New-Item -ItemType Directory -Path (Join-Path $stage 'sdk') -Force | Out-Null
New-Item -ItemType Directory -Path $sourceStage -Force | Out-Null

Copy-Item -LiteralPath (Join-Path $root 'bin\NFSMWLimitBreaker.asi') -Destination (Join-Path $stage 'scripts\NFSMWLimitBreaker.asi')
Copy-Item -LiteralPath (Join-Path $root 'NFSMWLimitBreaker.ini') -Destination (Join-Path $stage 'scripts\NFSMWLimitBreaker.ini')
foreach($name in 'README_EN.md','README_JA.md','RELEASE_NOTES.md','LICENSE.md'){
    Copy-Item -LiteralPath (Join-Path $root $name) -Destination $stage
    Copy-Item -LiteralPath (Join-Path $root $name) -Destination $sourceStage
}
Copy-Item -LiteralPath (Join-Path $root 'third_party\minhook\LICENSE.txt') -Destination (Join-Path $stage 'MINHOOK-LICENSE.txt')
Copy-Item -LiteralPath (Join-Path $root 'src\PopulationApi.h') -Destination (Join-Path $stage 'sdk\PopulationApi.h')
Copy-Item -LiteralPath (Join-Path $root 'src\ResourceApi.h') -Destination (Join-Path $stage 'sdk\ResourceApi.h')
Copy-Item -LiteralPath (Join-Path $root 'docs\Population-Reservation-V1.md') -Destination (Join-Path $stage 'sdk\Population-Reservation-V1.md')
Copy-Item -LiteralPath (Join-Path $root 'docs\Resource-Limits-V1.md') -Destination (Join-Path $stage 'sdk\Resource-Limits-V1.md')

$releaseAsi=Join-Path $stage 'scripts\NFSMWLimitBreaker.asi'
$releaseIni=Join-Path $stage 'scripts\NFSMWLimitBreaker.ini'
$iniText=Get-Content -LiteralPath $releaseIni -Raw
if($iniText -notmatch '(?m)^RpdStreamingPoolMiB\s*=\s*256\s*$' -or
   $iniText -notmatch '(?m)^Logging\s*=\s*0\s*$'){
    throw 'Release INI is missing the streaming and logging defaults.'
}
$asiBytes=[IO.File]::ReadAllBytes($releaseAsi)
$asiUnicode=[Text.Encoding]::Unicode.GetString($asiBytes)
if(!$asiUnicode.Contains($version.Substring(1))){
    throw 'Release ASI does not contain the expected version string.'
}

$manifestLines=Get-ChildItem -LiteralPath $stage -Recurse -File |
    Sort-Object FullName |
    ForEach-Object {
        $relative=$_.FullName.Substring($stage.Length+1).Replace('\','/')
        $hash=(Get-FileHash -Algorithm SHA256 -LiteralPath $_.FullName).Hash
        "$hash  $relative"
    }
[IO.File]::WriteAllLines((Join-Path $stage 'MANIFEST-SHA256.txt'),
    $manifestLines,[Text.UTF8Encoding]::new($false))

New-Item -ItemType Directory -Path (Join-Path $nfrOverlay 'scripts') -Force | Out-Null
$nfrDocs=Join-Path $nfrOverlay 'docs\NFSMWLimitBreaker'
New-Item -ItemType Directory -Path (Join-Path $nfrDocs 'sdk') -Force | Out-Null
Copy-Item -LiteralPath $releaseAsi -Destination (Join-Path $nfrOverlay 'scripts\NFSMWLimitBreaker.asi')
Copy-Item -LiteralPath $releaseIni -Destination (Join-Path $nfrOverlay 'scripts\NFSMWLimitBreaker.ini')
foreach($name in 'README_EN.md','README_JA.md','RELEASE_NOTES.md','LICENSE.md','MINHOOK-LICENSE.txt'){
    Copy-Item -LiteralPath (Join-Path $stage $name) -Destination $nfrDocs
}
Get-ChildItem -LiteralPath (Join-Path $stage 'sdk') -File | ForEach-Object {
    Copy-Item -LiteralPath $_.FullName -Destination (Join-Path $nfrDocs 'sdk')
}
$overlayManifestLines=Get-ChildItem -LiteralPath $nfrOverlay -Recurse -File |
    Sort-Object FullName |
    ForEach-Object {
        $relative=$_.FullName.Substring($nfrOverlay.Length+1).Replace('\','/')
        $hash=(Get-FileHash -Algorithm SHA256 -LiteralPath $_.FullName).Hash
        "$hash  $relative"
    }
[IO.File]::WriteAllLines((Join-Path $nfrOverlay 'MANIFEST-SHA256.txt'),
    $overlayManifestLines,[Text.UTF8Encoding]::new($false))
$sourceModule=Join-Path $sourceStage 'NFSMWLimitBreaker'
New-Item -ItemType Directory -Path $sourceModule -Force | Out-Null
foreach($name in 'NFSMWLimitBreaker.vcxproj','NFSMWLimitBreaker.ini','src','tests','tools','docs','third_party','README_EN.md','README_JA.md','RELEASE_NOTES.md','LICENSE.md'){
    Copy-Item -LiteralPath (Join-Path $root $name) -Destination $sourceModule -Recurse
}
$workspace=Split-Path -Parent $root
foreach($name in 'NFSMWNativeFreeRoamRacers/include/ExecutableTargets.h','integration/AppearanceSafety.h'){
    $destination=Join-Path $sourceStage $name
    New-Item -ItemType Directory -Path (Split-Path $destination) -Force | Out-Null
    Copy-Item -LiteralPath (Join-Path $workspace $name) -Destination $destination
}

$binaryZip=Join-Path $dist "NFSMWLimitBreaker-$version.zip"
$sourceZip=Join-Path $dist "NFSMWLimitBreaker-$version-source.zip"
$nfrOverlayZip=Join-Path $dist "NFRR-Bundle-Overlay-NFSMWLimitBreaker-$version.zip"
Assert-DirectChild $dist $binaryZip
Assert-DirectChild $dist $sourceZip
Assert-DirectChild $dist $nfrOverlayZip
if(Test-Path -LiteralPath $binaryZip){Remove-Item -LiteralPath $binaryZip -Force}
if(Test-Path -LiteralPath $sourceZip){Remove-Item -LiteralPath $sourceZip -Force}
if(Test-Path -LiteralPath $nfrOverlayZip){Remove-Item -LiteralPath $nfrOverlayZip -Force}
Compress-Archive -LiteralPath $stage -DestinationPath $binaryZip -CompressionLevel Optimal
Compress-Archive -LiteralPath $sourceStage -DestinationPath $sourceZip -CompressionLevel Optimal
Compress-Archive -LiteralPath $nfrOverlay -DestinationPath $nfrOverlayZip -CompressionLevel Optimal
Get-FileHash -Algorithm SHA256 -LiteralPath $binaryZip,$sourceZip,$nfrOverlayZip | Format-Table Path,Hash -AutoSize
