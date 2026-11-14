param(
    [string]$GameExe=$(Join-Path $(if($env:NFS_GAME_ROOT){$env:NFS_GAME_ROOT}elseif($env:COMPUTERNAME -eq 'ZAKKEY'){'E:\EA GAMES'}else{'\\ZAKKEY\EA GAMES'}) 'Need for Speed Most Wanted Main\speed.exe')
)
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if(-not (Test-Path -LiteralPath $vswhere)){throw 'Visual Studio Installer / vswhere is required.'}
$install=& $vswhere -latest -products '*' -requires Microsoft.Component.MSBuild -property installationPath
if(-not $install){throw 'Visual Studio C++ build tools were not found.'}
$msbuild=Join-Path $install 'MSBuild\Current\Bin\MSBuild.exe'
& (Join-Path $PSScriptRoot 'Verify-Static.ps1') -GameExe $GameExe
& $msbuild (Join-Path $root 'NFSMWLimitBreaker.vcxproj') /p:Configuration=Release /p:Platform=Win32 /m:1 /nr:false /v:minimal /nologo
if($LASTEXITCODE){throw 'ASI build failed.'}
& $msbuild (Join-Path $root 'NFSMWLimitBreaker.vcxproj') /p:Configuration=Release /p:Platform=Win32 /p:NativeHarness=true /m:1 /nr:false /v:minimal /nologo
if($LASTEXITCODE){throw 'Test build failed.'}
& (Join-Path $root 'bin\NFSMWLimitBreaker.Tests.exe')
if($LASTEXITCODE){throw 'Native tests failed.'}
Write-Output 'PASS build, executable signatures, and native tests'
