param(
    [string]$GameDirectory='C:\Program Files (x86)\EA GAMES\Need for Speed Most Wanted Main'
)
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$scripts=Join-Path $GameDirectory 'scripts'
if(-not (Test-Path -LiteralPath $scripts)){throw "Scripts directory not found: $scripts"}
& (Join-Path $PSScriptRoot 'Verify-Static.ps1') -GameExe (Join-Path $GameDirectory 'speed.exe')
$stamp=Get-Date -Format 'yyyyMMdd-HHmmss'
$backup=Join-Path $root "runtime-backups\pre-install-$stamp"
New-Item -ItemType Directory -Path $backup -Force | Out-Null
foreach($name in 'NFSMWLimitBreaker.asi','NFSMWLimitBreaker.ini','NFSMWLimitBreaker.log'){
    $target=Join-Path $scripts $name
    if(Test-Path -LiteralPath $target){Copy-Item -LiteralPath $target -Destination $backup}
}
Copy-Item -LiteralPath (Join-Path $root 'bin\NFSMWLimitBreaker.asi') -Destination (Join-Path $scripts 'NFSMWLimitBreaker.asi') -Force
if(-not (Test-Path -LiteralPath (Join-Path $scripts 'NFSMWLimitBreaker.ini'))){
    Copy-Item -LiteralPath (Join-Path $root 'NFSMWLimitBreaker.ini') -Destination (Join-Path $scripts 'NFSMWLimitBreaker.ini')
}
Write-Output "PASS installed; rollback=$backup"
