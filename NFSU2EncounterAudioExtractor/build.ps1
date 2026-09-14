param([string]$Python = 'python')
$ErrorActionPreference = 'Stop'
$root = $PSScriptRoot
$run = Join-Path $root ('artifacts/build-' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
if (Test-Path -LiteralPath $run) { throw 'Build output already exists.' }
New-Item -ItemType Directory -Path $run | Out-Null
Push-Location $root
try {
    & $Python -m unittest discover -s tests -v
    if ($LASTEXITCODE) { throw 'Tests failed.' }
    & $Python -m PyInstaller --noconfirm --distpath (Join-Path $run 'dist') --workpath (Join-Path $run 'work') 'NFSU2EncounterAudioExtractor.spec'
    if ($LASTEXITCODE) { throw 'PyInstaller failed; install the version documented in BUILD.md.' }
    $app = Join-Path $run 'dist/NFSU2EncounterAudioExtractor'
    foreach ($name in @('README.md', 'LICENSE.txt', 'THIRD_PARTY_NOTICES.txt')) {
        Copy-Item -LiteralPath (Join-Path $root $name) -Destination (Join-Path $app $name)
    }
    Copy-Item -LiteralPath (Join-Path $root 'Licenses') -Destination (Join-Path $app 'Licenses') -Recurse
    $decoder = Join-Path $app 'tools/vgmstream'
    New-Item -ItemType Directory -Path $decoder -Force | Out-Null
    Copy-Item -LiteralPath (Join-Path $root 'tools/vgmstream/README.md') -Destination (Join-Path $decoder 'README.md')
    Write-Output "Built (decoder not included): $app"
    Write-Output 'If your Python/runtime versions differ, update third-party license notices before using the build.'
} finally { Pop-Location }
