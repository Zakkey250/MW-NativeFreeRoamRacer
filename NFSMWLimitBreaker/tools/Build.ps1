param(
    [string]$GameExe='C:\Program Files (x86)\EA GAMES\Need for Speed Most Wanted Main\speed.exe'
)
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$msbuild='C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe'
if(-not (Test-Path -LiteralPath $msbuild)){throw 'Visual Studio 2022 MSBuild was not found.'}
& (Join-Path $PSScriptRoot 'Verify-Static.ps1') -GameExe $GameExe
& $msbuild (Join-Path $root 'NFSMWLimitBreaker.vcxproj') /p:Configuration=Release /p:Platform=Win32 /v:minimal /nologo
if($LASTEXITCODE){throw 'ASI build failed.'}
& $msbuild (Join-Path $root 'NFSMWLimitBreaker.vcxproj') /p:Configuration=Release /p:Platform=Win32 /p:NativeHarness=true /v:minimal /nologo
if($LASTEXITCODE){throw 'Test build failed.'}
& (Join-Path $root 'bin\NFSMWLimitBreaker.Tests.exe')
if($LASTEXITCODE){throw 'Native tests failed.'}
Write-Output 'PASS build, executable signatures, and native tests'
