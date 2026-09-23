param(
    [string]$GameExe='C:\Program Files (x86)\EA GAMES\Need for Speed Most Wanted Main\speed.exe'
)
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$install=& $vswhere -latest -products '*' -requires Microsoft.Component.MSBuild -property installationPath
if(-not $install){throw 'Visual Studio build tools were not found.'}
$msbuild=Join-Path $install 'MSBuild/Current/Bin/MSBuild.exe'
& (Join-Path $PSScriptRoot 'Verify-Static.ps1') -GameExe $GameExe
& $msbuild (Join-Path $root 'NFSMWLimitBreaker.vcxproj') /p:Configuration=Release /p:Platform=Win32 /v:minimal /nologo
if($LASTEXITCODE){throw 'ASI build failed.'}
& $msbuild (Join-Path $root 'NFSMWLimitBreaker.vcxproj') /p:Configuration=Release /p:Platform=Win32 /p:NativeHarness=true /v:minimal /nologo
if($LASTEXITCODE){throw 'Test build failed.'}
& (Join-Path $root 'bin\NFSMWLimitBreaker.Tests.exe')
if($LASTEXITCODE){throw 'Native tests failed.'}
Write-Output 'PASS build, executable signatures, and native tests'
