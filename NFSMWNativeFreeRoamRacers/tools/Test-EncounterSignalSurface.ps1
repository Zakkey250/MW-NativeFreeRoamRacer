$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot
# Existing verifier resolves exact PE sections and checks SHA-256, assets and baseline hooks.
. (Join-Path $PSScriptRoot 'Verify-NFSMWNativeFreeRoamRacers.ps1')
$signal = Get-Content -LiteralPath (Join-Path $projectRoot 'src\EncounterSignal.inl') -Raw
$guards = @(
    @(0x0057C660, '64 A1 00 00 00 00 6A FF'),
    @(0x00633DC0, '83 39 00 7E 13 8B 41 08'),
    @(0x0057C73D, 'E8 7E 76 0B 00'),
    @(0x0057BBC0, '51 56 8B F1 8A 46 4C'),
    @(0x0058CE39, '8B 11 55 FF 52 04'),
    @(0x0057BD09, '5F 5E 59 C2 04 00'),
    @(0x0057BEA0, '33 C0 88 41 1D 89 41 14'),
    @(0x00524850, '8B 44 24 04'),
    @(0x00514D10, '57 8B 7C 24 08 85 FF'),
    @(0x00514C78, '8B 50 1C 8B 48 18 81 CA 01 00 40 02'),
    @(0x00514CD0, '83 E1 FE 81 C9 00 00 40 02'),
    @(0x005D59F0, '83 EC 08 56 8B 71 08'),
    @(0x00568030, '56 8B F1 8B 46 14 57'),
    @(0x00568145, 'C2 18 00'),
    @(0x008A2528, 'C0 BB 57 00')
)
foreach ($guard in $guards) {
    $expected = $guard[1]
    $actual = (ReadTargetBytes $guard[0] ($expected.Split(' ').Count)).Replace('-', ' ')
    if ($actual -ne $expected) { throw ('Encounter guard mismatch {0:X8}: {1}' -f $guard[0], $actual) }
    Write-Output ('PASS encounter exact bytes {0:X8}: {1}' -f $guard[0], $actual)
}
foreach ($forbiddenSignal in @('GetAsyncKeyState', 'XINPUT_GAMEPAD', 'SetGoal(', 'StartRace(', 'GiveCash(', 'bytes[0x4C] =', 'std::memcpy(bytes + 0x50')) {
    if ($signal.Contains($forbiddenSignal)) { throw "Encounter forbidden write/poll: $forbiddenSignal" }
}
Write-Output 'PASS signal adapter uses converted action 46 and defers battle mutation to frame update; no physical key or SMS writes'
$baseline = Join-Path $projectRoot '..\rollback\2026-09-14-pre-alpha.40\src'
if (Test-Path -LiteralPath $baseline) {
    foreach ($file in Get-ChildItem -LiteralPath $baseline -File) {
        # alpha42 prompt files are separately checked against accepted alpha41.
        if ($file.Name -in @('Runtime.cpp', 'Plugin.cpp', 'EncounterBattle.inl', 'EncounterPowerBoost.inl','EncounterHudFrame.inl','EncounterSignal.inl','EncounterRoute.inl')) { continue }
        $current = Join-Path $projectRoot ('src\' + $file.Name)
        if ((Get-FileHash -LiteralPath $current).Hash -ne (Get-FileHash -LiteralPath $file.FullName).Hash) {
            throw "Accepted baseline source changed: $($file.Name)"
        }
    }
    $oldBattle=Get-Content (Join-Path $baseline 'EncounterBattle.inl') -Raw
    $newBattle=Get-Content (Join-Path $projectRoot 'src\EncounterBattle.inl') -Raw
    if($newBattle.Contains('!EncounterHudAvailable(g_encounterHud)) {') -or $newBattle.Contains('if(!NativeEncounterFollow(player,!g_battle.destinationSet)) {EndEncounterBattle')) {throw 'Presentation/path failure must not end active battle'}
    $normalizeBattle={param($text)
        # alpha43 follower is validated separately against pre-alpha43; keep
        # start/outcome/reward/restoration and every other adapter byte strict.
        $text=[regex]::Replace($text,'(?s)bool NativeEncounterFollow\(.*?(?=void\* EncounterProfile\(\))','FOLLOWER_ADAPTER')
        $text=$text.Replace('float routeAge=0,pathSeconds=0;','float routeAge=0;')
        $text=$text -replace '(?m)^    g_battle.pathSeconds\+=dt;\r?\n',''
        foreach($line in @('#include "EncounterPowerBoost.inl"',
            '    ClearEncounterPowerBoost();','        ClearEncounterPowerBoost();',
            '    UpdateEncounterPowerBoost(true,g_battle.model.leader()==battle::Leader::Player,g_battle.model.gap());',
            '        LogEncounterPowerBoost();')) {
            $text=$text -replace ('(?m)^'+[regex]::Escape($line)+'\r?\n'),''
        }
        $text
    }
    if((& $normalizeBattle $newBattle) -cne (& $normalizeBattle $oldBattle)){throw 'Battle changed outside new power policy calls'}
    $oldRuntime=Get-Content (Join-Path $baseline 'Runtime.cpp') -Raw
    $newRuntime=Get-Content (Join-Path $projectRoot 'src\Runtime.cpp') -Raw
    $newRuntime=$newRuntime.Replace('alpha.43-icon-owner-pursuit-path','alpha.42-prompt-icon-sound').Replace('alpha.42-prompt-icon-sound','alpha.41-background-police').Replace('alpha.41-background-police','alpha.40-configurable-power')
    $newRuntime=$newRuntime -replace '#include "EncounterPursuitPath.h"\r?\n',''
    foreach($line in @('    bool backgroundPoliceEnabled = true;',
        '    g_settings.backgroundPoliceEnabled = GetPrivateProfileIntW(L"BackgroundPolice", L"Enabled", 1, modulePath) != 0;',
        '#include "BackgroundPolice.inl"','    ResetBackgroundPolice();',
        '    UpdateBackgroundPolice(*player, vehicles, dt);','    InitializeBackgroundPolice();')) {
        $newRuntime=$newRuntime -replace ('(?m)^'+[regex]::Escape($line)+'\r?\n'),''
    }
    $newRuntime=$newRuntime.Replace('alpha.40-configurable-power','alpha.39-distance-power-boost')
    $newRuntime=$newRuntime -replace '(?m)^    std::array<float,3> encounterPowerScales\{1.15f,1.30f,1.50f\};\r?\n',''
    $newRuntime=$newRuntime -replace '(?m)^    LoadEncounterPowerSettings\(modulePath\);\r?\n',''
    $newRuntime=$newRuntime -replace '#include "EncounterPowerSettings.inl"\r?\n\r?\n',''
    if($oldRuntime -cne $newRuntime){throw 'Population/identity runtime changed beyond version'}
    foreach($file in Get-ChildItem (Join-Path (Split-Path $baseline) 'include') -File) {
        if((Get-FileHash $file.FullName).Hash -ne (Get-FileHash (Join-Path $projectRoot ('include\'+$file.Name))).Hash){throw "Accepted model/header changed: $($file.Name)"}
    }
    $oldPower=Get-Content (Join-Path $baseline 'EncounterPowerBoost.inl') -Raw
    $newPower=Get-Content (Join-Path $projectRoot 'src\EncounterPowerBoost.inl') -Raw
    $newPower=$newPower.Replace('return g_settings.encounterPowerScales[gap<=100?0:(gap<=200?1:2)];','return gap<=100?1.15f:(gap<=200?1.30f:1.50f);').Replace('scale>2.0f','scale>1.5f').Replace('scale=%.3f','scale=%.2f')
    $normalizeLog={param($text) [regex]::Replace($text,'(?s)    Log\(LogLevel::Info,"ENCOUNTER_POWER installed=.*?\);','INSTALL_LOG')}
    if((& $normalizeLog $newPower) -cne (& $normalizeLog $oldPower)){throw 'Power changed outside configured values, allowed ceiling and diagnostics'}
    Write-Output 'PASS alpha39 runtime and power guards preserved; only INI scales, ceiling and diagnostics changed'
}
