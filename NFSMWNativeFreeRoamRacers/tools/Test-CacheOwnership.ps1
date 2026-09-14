$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$install = & $vswhere -latest -products '*' -requires Microsoft.Component.MSBuild -property installationPath
if (-not $install) { throw 'Visual Studio build tools not found' }
$devcmd = Join-Path $install 'Common7\Tools\VsDevCmd.bat'
Push-Location $projectRoot
try {
    New-Item -ItemType Directory -Path 'artifacts\Tests' -Force | Out-Null
    $command = '"' + $devcmd + '" -no_logo -arch=x86 -host_arch=x64 && cl /nologo /std:c++20 /EHsc /O2 /MD /DWIN32_LEAN_AND_MEAN /DNOMINMAX /D_CRT_SECURE_NO_WARNINGS /Iinclude /Ithird_party\minhook\include /Ithird_party\NFSPluginSDK tools\CacheOwnershipTests.cpp artifacts\obj\Release\Logging.obj artifacts\obj\Release\buffer.obj artifacts\obj\Release\hook.obj artifacts\obj\Release\trampoline.obj artifacts\obj\Release\hde32.obj /Foartifacts\Tests\CacheOwnershipTests.obj /Feartifacts\Tests\CacheOwnershipTests.exe /link /LTCG /LARGEADDRESSAWARE /SUBSYSTEM:CONSOLE Advapi32.lib'
    & cmd /d /c $command
    if ($LASTEXITCODE -ne 0) { throw 'Offline test build failed; build Release Win32 first' }
    & '.\artifacts\Tests\CacheOwnershipTests.exe'
    if ($LASTEXITCODE -ne 0) { throw "Offline cache tests failed ($LASTEXITCODE)" }
} finally {
    Pop-Location
}
