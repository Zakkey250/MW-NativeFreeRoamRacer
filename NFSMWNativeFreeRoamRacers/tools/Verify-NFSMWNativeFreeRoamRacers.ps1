[CmdletBinding()]
param(
    [string]$Artifact,
    [string]$Source,
    [string]$TargetManifest,
    [string]$GameDirectory=$(Join-Path $(if($env:NFS_GAME_ROOT){$env:NFS_GAME_ROOT}elseif($env:COMPUTERNAME -eq 'ZAKKEY'){'E:\EA GAMES'}else{'\\ZAKKEY\EA GAMES'}) 'Need for Speed Most Wanted')
)

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
if ([string]::IsNullOrWhiteSpace($Artifact)) { $Artifact = Join-Path $projectRoot 'artifacts\Release\NFSMWNativeFreeRoamRacers.asi' }
if ([string]::IsNullOrWhiteSpace($Source)) { $Source = Join-Path $projectRoot 'src\Runtime.cpp' }
if ([string]::IsNullOrWhiteSpace($TargetManifest)) { $TargetManifest = Join-Path $projectRoot '..\integration\NFSMW-ExecutableTarget.json' }

$failures = [System.Collections.Generic.List[string]]::new()
function Pass([string]$Message) { Write-Host "PASS $Message" }
function Fail([string]$Message) { Write-Host "FAIL $Message"; $failures.Add($Message) }

if (-not (Test-Path -LiteralPath $Artifact -PathType Leaf)) {
    Fail "artifact missing: $Artifact"
} else {
    $bytes = [System.IO.File]::ReadAllBytes((Resolve-Path -LiteralPath $Artifact))
    $peOffset = [BitConverter]::ToInt32($bytes, 0x3C)
    $signature = [BitConverter]::ToUInt32($bytes, $peOffset)
    $machine = [BitConverter]::ToUInt16($bytes, $peOffset + 4)
    $characteristics = [BitConverter]::ToUInt16($bytes, $peOffset + 22)
    if ($signature -eq 0x00004550) { Pass 'PE signature' } else { Fail 'invalid PE signature' }
    if ($machine -eq 0x014C) { Pass 'machine=I386' } else { Fail ("machine=0x{0:X4}" -f $machine) }
    if (($characteristics -band 0x2000) -ne 0) { Pass 'image is DLL/ASI' } else { Fail 'DLL characteristic absent' }
    Pass "artifact sha256=$((Get-FileHash -Algorithm SHA256 -LiteralPath $Artifact).Hash) size=$($bytes.Length)"
}

$sourceText = Get-Content -LiteralPath $Source -Raw
$targetHeader=Join-Path (Split-Path (Split-Path $Source)) 'include/ExecutableTargets.h'
if(Test-Path -LiteralPath $targetHeader){$sourceText += Get-Content -LiteralPath $targetHeader -Raw}
$variation = Get-Content -LiteralPath (Join-Path (Split-Path $Source) 'VehicleVariation.inl') -Raw
$manifest = Get-Content -LiteralPath $TargetManifest -Raw | ConvertFrom-Json
if ($sourceText.Contains($manifest.sha256) -and $sourceText.Contains([string]$manifest.size)) { Pass "fail-closed target matches $($manifest.id)" } else { Fail 'source target identity differs from manifest' }

$forbidden = @('kWRoadNavBuildPathToPoint', 'SetDriveSpeed])', 'kSlotAIDoDriving', 'kRacecarStartRace', 'kRacecarQuitRace')
foreach ($token in $forbidden) { if ($sourceText.Contains($token)) { Fail "forbidden external drive/navigation token: $token" } }
if ($sourceText.Contains('goalCreationCalls=%u actionSelectionCalls=%u externalPathWrites=0 externalDriveWrites=0')) { Pass 'native Goal creation and bounded Action selection declaration present' } else { Fail 'native Goal/Action declaration missing' }
if ($sourceText.Contains('kMaximumRacersLimit = 15') -and $sourceText.Contains('kDefaultMaximumRacers = 6') -and $sourceText.Contains('std::array<MarkerSlot, kMaximumRacersLimit>')) { Pass 'default six, supported limit fifteen, independent marker slots' } else { Fail 'population limits or marker slots missing' }
$legacySearch=$sourceText.Contains('std::uniform_int_distribution<std::size_t>(1, candidateCount)') -and $sourceText.Contains('if(rank<bestRank)')
$rankedSearch=$false
$searchPath=Join-Path $projectRoot 'include/AnchorSearch.h'
if($sourceText.Contains('g_anchorSearch.Select(g_random') -and (Test-Path -LiteralPath $searchPath)) {
    $search=Get-Content -LiteralPath $searchPath -Raw
    $rankedSearch=$sourceText.Contains('g_anchorSearch.Reserve(kInitialVehicleReserve)') -and
        $sourceText.Contains('g_anchorSearch.Clear();') -and
        $sourceText.Contains('return GetTrafficRoad(vehicle.pointer,&road)&&CaptureAnchorRoadSeed(road,seed);')
    foreach($token in @('std::array<std::vector<std::size_t>,4>', 'rank<tiers.size()', 'while(!tier.empty())',
        'std::uniform_int_distribution<std::size_t>(0,tier.size()-1)', 'tier.pop_back()',
        'if(validate(index))', 'Clear();return result;')) {
        if(!$search.Contains($token)){$rankedSearch=$false}
    }
}
if ($sourceText.Contains('ConsumeManagementTick') -and $sourceText.Contains('g_vehicleScratch.reserve(kInitialVehicleReserve)') -and ($legacySearch -or $rankedSearch)) { Pass 'bounded management rate, reused snapshot buffer, validated ranked random anchor sampling' } else { Fail 'population optimization missing' }
if ($sourceText.Contains('retry-with-bounded-backoff') -and $sourceText.Contains('permanentLatch=0') -and $sourceText.Contains('g_spawnFailureBackoffSeconds')) { Pass 'bounded population retry without permanent latch present' } else { Fail 'bounded population retry missing' }
if ($sourceText.Contains('kAIVehicleSetGoal = 0x00422480') -and $sourceText.Contains('kAIGoalRacerHash = 0x08D0D8A7')) { Pass 'exact native AIGoalRacer installation path present' } else { Fail 'native AIGoalRacer installation path missing' }
if ($sourceText.Contains('CacheQueryHook') -and $sourceText.Contains('g_protectedKeys') -and $sourceText.Contains('CACHE_RETENTION') -and $sourceText.Contains('kRaceStatusCacheQuery = 0x005E8A20')) { Pass 'alpha.8 cache ownership vote and generation checks present' } else { Fail 'cache ownership protection missing' }
foreach ($token in @('KillSimableHook', 'DeactivateVehicleHook', 'DeletingDestructorHook')) { if ($sourceText.Contains($token)) { Fail "unsafe lifecycle suppression present: $token" } }

# Check actual file-backed target bytes, not just declarations in the source.
$gameExe = Join-Path $GameDirectory $manifest.executable
$gameBytes = [IO.File]::ReadAllBytes($gameExe)
if ((Get-FileHash -LiteralPath $gameExe -Algorithm SHA256).Hash -ne $manifest.sha256) { Fail 'installed target SHA differs' }
$pe = [BitConverter]::ToInt32($gameBytes, 0x3c)
$sectionCount = [BitConverter]::ToUInt16($gameBytes, $pe + 6)
$optionalSize = [BitConverter]::ToUInt16($gameBytes, $pe + 20)
function ReadTargetBytes([uint32]$VA, [int]$Count) {
    $rva = $VA - 0x400000
    for ($s=0; $s -lt $sectionCount; ++$s) {
        $at = $pe + 24 + $optionalSize + 40*$s
        $start = [BitConverter]::ToUInt32($gameBytes, $at+12)
        $size = [BitConverter]::ToUInt32($gameBytes, $at+16)
        $raw = [BitConverter]::ToUInt32($gameBytes, $at+20)
        if ($rva -ge $start -and ($rva+$Count) -le ($start+$size)) {
            $offset = $raw + $rva - $start
            return [BitConverter]::ToString($gameBytes, $offset, $Count)
        }
    }
    throw "Target VA is not file backed: $VA"
}
$guards = @(
    @(0x4F3000, '51-A1-44-CE-92-00-53-55-8B-E9-8B-0D-C4-CF-92-00-03-C1-83-F8-04-BB-91-00-00-00-7C-05-B8-04-00-00-00'),
    @(0x4F3030, '85-C0-7E-45-56-89-44-24-0C'),
    @(0x4F3040, '53-6A-00-8B-CD-E8-26-29-FF-FF-8B-F8-8B-17-53-8B-CF-FF-52-04'),
    @(0x4E5CB9, '83-05-D4-1E-91-00-04-5B-C3'),
    @(0x4E5C93, 'A1-00-E0-91-00-C1-EB-02-80-E3-01-85-C0-74-08-8B-80-68-19-00-00-EB-0B-8B-0D-04-E0-91-00-E8-4B-6D-0F-00-85-C0-75-09'),
    @(0x5DCA00, '8B-41-2C-C3'),
    @(0x4F3079, 'C6-45-18-01-5D-5B-59-C2-04-00'),
    @(0x4D95E0, '8B-41-10-85-C0-74-0E-8A-48-38-84-C9-74-09-8B-40-04-85-C0-75-F2-33-C0-C2-04-00'),
    @(0x4F51D0, 'C7-41-34-00-00-00-00-E9-14-F9-FF-FF'),
    @(0x898D2C, '00-30-4F-00-E0-95-4D-00-60-BC-4E-00'),
    @(0x896FB4, 'D0-4F-4E-00'),
    @(0x896FC4, 'D0-51-4F-00'),
    @(0x4E6330, '83-EC-08-A1-74-3E-91-00-53-8B-D9-8B-0D-7C-3E-91-00'),
    @(0x4E636B, '8B-43-1C-39-85-28-02-00-00'),
    @(0x4E643E, '8B-16-55-8B-CE-FF-52-0C'),
    @(0x4C7FA0, '6A-FF-68-A6-BE-86-00-64-A1-00-00-00-00'),
    @(0x4C7FB6, '8B-44-24-14-8B-0D-58-E5-8E-00'),
    @(0x4C808E, 'E8-ED-D4-FF-FF'),
    @(0x4C809E, '83-C4-10-C3'),
    @(0x4C80AF, '83-C4-10-C3'),
    @(0x4B3CD0, '8B-44-24-04-56-50-8B-F1-E8-93-25-02-00-8B-4E-18-8B-11-FF-52-40-89-46-24-5E-C2-04-00'),
    @(0x4D629D, '89-51-18-89-51-14-C2-04-00'),
    @(0x4B3CF0, '53-56-8B-F1-33-DB-53-8D-4E-50'),
    @(0x4B3DF4, '5E-5B-C3'),
    @(0x4B3E0A, '5E-5B-C3'),
    @(0x8EE6F4, 'A0-7F-4C-00'),
    @(0x896C04, 'D0-3C-4B-00'),
    @(0x896C0C, 'F0-3C-4B-00'),
    @(0x4B3AD0, 'B8-E8-E6-8E-00-C3'),
    @(0x8EE6EC, '80-66-89-00'),
    @(0x896680, '53-46-58-43-54-4C-5F-45-6E-67-69-6E-65-00'),
    @(0x4AE463, '8B-4C-24-20-89-9E-DC-00-00-00-89-8E-94-00-00-00'),
    @(0x4B1642, '8B-80-48-02-00-00-50-8D-4C-24-14-E8-9E-CD-FF-FF'),
    @(0x4BC420, '55-8B-EC-83-E4-F0-83-EC-74-53-56-8B-F1'),
    @(0x4C8B40, '53-8B-5C-24-08-56-8B-F1-53-8D-8E-D4-00-00-00'),
    @(0x4BC5C6, '8B-56-14-8B-4E-18-8B-42-34-8B-49-74-89-88-48-02-00-00'),
    @(0x4C8CB4, '8B-4E-18-8B-56-14-8B-42-34-8B-49-74-5E-89-88-48-02-00-00'),
    @(0x4BC691, '5F-5E-5B-8B-E5-5D-C2-04-00'),
    @(0x4C8CC7, '5B-C2-04-00'),
    @(0x896C14, '20-C4-4B-00'),
    @(0x896D50, '40-8B-4C-00'),
    @(0x8974D0, '40-8B-4C-00'),
    @(0x694700, '56-57-8B-7C-24-0C-57-8B-F1-E8-52-FA-FF-FF-8B-86-98-00-00-00'),
    @(0x694746, '5F-5E-C2-04-00'),
    @(0x694210, 'D9-87-94-00-00-00-D8-4C-24-14-D8-44-24-18-D9-5E-7C-D9-5F-04'),
    @(0x8AC124, '00-47-69-00'),
    @(0x6A03A0, 'D9-81-2C-01-00-00-C3'),
    @(0x8AB6E4, 'A0-03-6A-00'),
    @(0x8ABF8C, 'A0-03-6A-00'),
    @(0x4D96B0, '8B-41-10-85-C0-74-15-8B-4C-24-04-EB-03-8D-49-00-3B-48-1C-74-09-8B-40-04-85-C0-75-F4-33-C0-C2-04-00'),
    @(0x6A3F00, '8B-41-08-50-83-C1-AC-E8-E4-AA-04-00-48-F7-D8-1B-C0-40-C3'),
    @(0x6EE9F0, '8B-4C-24-04-85-C9-74-0F-8A-41-0C-84-C0-75-08-8B-01-FF-50-08-C2-04-00-83-C8-FF-C2-04-00'),
    @(0x4B15B0, '8A-41-11-84-C0-74-17-8B-41-14-85-C0-74-10-8A-88-2D-02-00-00-84-C9-74-06-B8-01-00-00-00-C3-33-C0-C3'),
    @(0x8AC0E8, '00-3F-6A-00'),
    @(0x897748, 'B0-15-4B-00'),
    @(0x5E8A20, '55-8B-E9-83-BD-60-19-00-00-01'),
    @(0x8A3A68, '20-8A-5E-00'),
    @(0x6879CC, 'FF-52-08-85-C0-74-1B'),
    @(0x6879EE, 'C7-43-10-00-00-00-00'),
    @(0x687DA8, '8B-17-6A-01-8B-CF-FF-12'),
    @(0x4E4EA0, '6A-FF-68-98-CD-86-00-64'),
    @(0x45A430, '8B-41-04-85-C0-74-2A-8B'),
    @(0x67C570, '83-EC-40-55-8B-6C-24-48'),
    @(0x739A70, '8B-44-24-04-8B-D1-8A-4C'),
    @(0x7596E0, '51-8B-44-24-0C-53-55-8B'),
    @(0x747BB0, '53-55-56-57-8B-39-33-DB'),
    @(0x56F2F0, '53-55-8B-6C-24-0C-56-57'),
    @(0x581C20, '8B-D1-33-C0-53-55-56-8D'),
    @(0x689361, 'B9-66-00-00-00-8B-F8-F3'),
    @(0x455BC0, '8B-44-24-04-56-8B-71-04'),
    @(0x453FC0, '8B-41-08-8B-40-24-C3-CC'),
    @(0x456B00, '8B-41-08-83-C0-1C-6A-FF'),
    @(0x456B20, '8B-49-08-8B-44-24-04-56'),
    @(0x455960, '8B-44-24-04-56-8B-71-08'),
    @(0x454190, '8B-54-24-04-8D-44-24-04'),
    @(0x454640, '8B-4C-24-04-85-C9-74-23'),
    @(0x455FD0, 'A1-BC-DC-90-00-56-8B-70'),
    @(0x7594A0, '83-EC-10-53-55-56-57-8B'),
    @(0x756E90, '83-EC-44-53-55-56-8B-F1'),
    @(0x51E1A0, '6A-FF-68-58-E4-86-00-64'),
    @(0x581745, '8B-4C-24-0C-8B-71-4C-8D'),
    @(0x58164F, '8A-41-58-84-C0-74-08-8B')
)
foreach ($g in $guards) {
    $expected = $g[1]
    if ((ReadTargetBytes $g[0] ($expected.Split('-').Count)) -eq $expected) { Pass ('native target guard VA=0x{0:X8}' -f $g[0]) } else { Fail ('native target mismatch VA=0x{0:X8}' -f $g[0]) }
}
if ($sourceText.Contains('kSlotOffScreenTime = 24') -and $sourceText.Contains('RETIRE_DEFERRED') -and $sourceText.Contains('outside hard retirement radius')) { Pass 'visible retirement deferral and hard safety radius present' } else { Fail 'visible retirement deferral missing' }
if ($sourceText.Contains('MISSING_FINAL') -and $sourceText.Contains('pointerPresent=%u')) { Pass 'stream-loss identity diagnostics present' } else { Fail 'stream-loss identity diagnostics missing' }
foreach ($token in @('WindowStart', 'RankVehicles', 'ChooseAppearanceOrStock', 'DynamicVehicleCatalog.inl', 'g_vehicleBag', 'stockFallback=%u', 'usage == 0', '__finally')) {
    if ($variation.Contains($token)) { Pass "variation guard: $token" } else { Fail "variation guard missing: $token" }
}
$catalogText = Get-Content -LiteralPath (Join-Path (Split-Path $Source) 'DynamicVehicleCatalog.inl') -Raw
if(($sourceText+$variation+$catalogText) -match 'kInstalledVehicles|kCatalogAssets|CATALOG_ASSET_MISMATCH|#include "InstalledVehicleCatalog.h"') { Fail 'fixed installed car pack still required' }
else { Pass 'runtime catalog does not depend on compiled model list or GLOBAL hashes' }
foreach($token in @('kCatalogCollectionLimit=8192','kCatalogModelLimit=1024','ResetDynamicCatalog','ReadCatalogSource','PreferCatalogKey','g_catalogCursor<kCatalogCollectionLimit','CatalogKeyVisited','ReadCatalogFrontend')) {
    if($catalogText.Contains($token)) {Pass "dynamic catalog guard: $token"} else {Fail "dynamic catalog guard missing: $token"}
}

if ($failures.Count -ne 0) { throw "Verification failed with $($failures.Count) issue(s)." }
