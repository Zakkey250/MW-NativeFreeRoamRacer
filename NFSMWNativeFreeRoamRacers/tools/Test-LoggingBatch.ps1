$ErrorActionPreference='Stop'
$project=Split-Path $PSScriptRoot
$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$install=& $vswhere -latest -products '*' -requires Microsoft.Component.MSBuild -property installationPath
if(!$install){throw 'Build tools not found'}
$devcmd=Join-Path $install 'Common7\Tools\VsDevCmd.bat'
Push-Location $project
try {
    New-Item -ItemType Directory -Path artifacts/Tests -Force | Out-Null
    $command='"'+$devcmd+'" -no_logo -arch=x86 -host_arch=x64 && cl /nologo /std:c++20 /EHsc /O2 /MD /W4 /WX /DWIN32_LEAN_AND_MEAN /DNOMINMAX /D_CRT_SECURE_NO_WARNINGS /Iinclude tools\LoggingBatchTests.cpp /Foartifacts\Tests\LoggingBatchTests.obj /Feartifacts\Tests\LoggingBatchTests.exe /link /SUBSYSTEM:CONSOLE'
    & cmd /d /c $command
    if($LASTEXITCODE){throw 'Logging batch test build failed'}
    & artifacts/Tests/LoggingBatchTests.exe
    if($LASTEXITCODE){throw 'Logging batch tests failed'}
}finally{Pop-Location}
