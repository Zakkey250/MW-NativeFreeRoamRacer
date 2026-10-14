# MW Native Free Roam Racer — v0.1.2

Bring roaming AI racers and one-on-one encounter battles to Rockport in Need for Speed Most Wanted (2005).

NFRR is the main mod. **NFSMW Limit Breaker (LB) 0.1.3-resource-headroom.2 is included as an optional companion, and using them together is recommended.** NFRR also works without LB.

## Why use the included Limit Breaker?

LB expands the native vehicle budget from 20 to up to 32. This provides more room for roaming racers alongside ordinary traffic and police, so NFRR can operate without having to reduce normal traffic volume just to make room for racers. With LB active, traffic can be denser than usual: more obstacles and narrower opportunities to pass make encounter battles more technical.

This is extra capacity, not a guarantee of a fixed traffic count. Native traffic rules, pursuits, streaming, visibility, other mods and safe spawn locations still affect actual population. At capacity, NFRR may wait or reclaim eligible off-screen ordinary traffic. NFRR preserves a configured non-racer share and avoids retiring the last two moving ordinary traffic cars.

|Feature|With compatible, active LB|Without LB / unsupported capability|
|---|---|---|
|Roaming racers and encounter battles|Available|Available|
|Total vehicle budget|Up to 32 with supplied LB settings|Native 20-vehicle budget|
|NFRR maximum setting|1–15; supplied value 6|1–15; actual count constrained by native resources|
|Engine-audio support|Requests 6 slots when enabled and an 8 MiB arena is verified|Uses the normal 0–4-slot setting|
|Model variety|6 including the player only after actual 6-slot allocation is verified|Configured 1–5 including the player|

LB integration checks exported API capabilities and live capacity, not a particular release number. Unsupported optional capabilities fall back individually. An unsafe/inconsistent active provider can defer spawning instead of permitting an unsafe allocation. Six requested audio slots do not guarantee six audible engines in every game state. LB is not a general-purpose fix for damaged car assets.

## Requirements

- Windows, MW (2005) PC and an ASI loader; none are included.
- NFSPatcher English 1.3 + LAA executable, 6,029,312 bytes, SHA-256 `B248271BF8EAC8C9B283B8C95E3ADD672B713BF529B05F1780E58268493B9D06`. This is the NFRR target and the Main test target. LB alone also supports the non-LAA original, but that does not expand NFRR compatibility. A similarly named EXE is not sufficient.
- Microsoft Visual C++ x86 runtime compatible with the build.
- No particular add-on car pack is required. Cars are discovered from the installed database. Unsupported hooks or invalid records can be rejected; arbitrary mod combinations are not guaranteed compatible.

## Install / update

1. Fully exit the game. Back up saves and any files with matching names.
2. Copy this package's `scripts` folder into the game directory. Keep NFRR's `.asi`, `.ini` and `.tpk` together. The two `NFSMWLimitBreaker` files are the optional companion.
3. Existing users: preserve your INIs and compare the new settings rather than overwriting your preferences. Do not run older roaming-racer implementations alongside NFRR. Do not replace unrelated mods, Japanese resources or Save Virtualizer files.
4. First launch: remain in the safehouse for about two seconds, then enter career free roam and allow calibration/spawning to complete. `scripts/NativeFreeRoamRacers/VehicleCatalog.cache.bin` is reused until related game-data fingerprints change. Do not distribute this locally generated file.
5. To disable only LB, exit the game and move `scripts/NFSMWLimitBreaker.asi` outside the loader's folders. Keep its INI for later use. NFRR automatically uses its normal resource limits on the next launch. LB settings inside NFRR's INI alone do not disable LB's vehicle-budget expansion.

To remove NFRR, exit and remove only `NFSMWNativeFreeRoamRacers.asi`, `.ini` and `.tpk`. Optional generated voices/cache may be kept or removed separately. LB may remain installed independently.

## Play

Follow a racer from behind, approximately 1–60 m away, and match speed within the configured tolerance (supplied: ±15 km/h). When the upper-left prompt appears, press your configured **Join Event** action. Direction/alignment checks still apply.

The leader chooses the route. Overtake to take the lead; a 300 m separation decides the result. The upper-left distance gauge is green while you lead and red while you trail. Other NFRR racer markers are temporarily hidden during the battle. When trailing by at least 100 m, native GPS can guide you toward the opponent; a player-selected destination takes priority.

`Custom AI` follows the general direction of your recorded route and attacks to overtake within 15 m when alignment permits. `Stable` retains the separate native-based behavior. Junctions, medians, obstacles and shortcuts can still defeat navigation; no perfect route-following is promised.

Stable wins pay 1,000. Custom AI rewards follow career Blacklist rank:

|BL|15|14|13|12|11|10|9|8|7|6|5|4|3|2|1|
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
|Cash|300|400|600|800|1,000|1,200|1,400|1,600|1,800|2,000|2,200|2,400|2,600|2,800|3,000|

Unknown rank uses 300 in Custom AI. Defeats/disqualifications pay nothing. Police can discover racers in the background; detection is not guaranteed. Optional compatible MWArsenal integration can disqualify a player for a successful weapon hit during a battle. No weapon mod is included or required; unavailable integration leaves normal battles working.

## Settings and supplied defaults

Edit `scripts/NFSMWNativeFreeRoamRacers.ini` while the game is closed, then restart. **Language is near the top: `[Encounter] Language = en` for English, `ja` for Japanese.** The supplied value preserves the tested Main setting (`ja`). This does not change the game's language or the extracted voice language.

- Maximum racers: 6 (1–15), management radius 600 m, marker radius 200 m.
- Cruising request: 60% of native requested speed, not 60 km/h. Battle opponents and police-pursued racers are exempt.
- Challenge speed tolerance: ±15 km/h (old INIs without the key use ±10).
- Custom AI leading output: 1.25; trailing output: 1.50 / 1.75 / 2.00 at 0–100 / over 100–200 / over 200–300 m. These are drive-output, not top-speed multipliers.
- Model selection follows nearby price/performance ranks. Random paint/body/hood/spoiler, stock wheels; retry invalid choices, then use stock. Performance upgrades respect career unlocks, including nitrous availability.
- LB companion: `MaxVehicles = 32`, audio-arena expansion enabled. NFRR `[LimitBreaker]` groups optional audio/variety expansion (both 1) and non-racer share (6). Without verified capabilities, normal settings apply.
- Adaptive spawn pacing and prepared-appearance retention are enabled. Deep audio diagnostics are off; lightweight timing is on. These do not throttle the existing native AI tick.

## Voices and updates

No game audio is included. The separate **NFSU2EncounterAudioExtractor v1.1.0 no-decoder package** uses your own UG2 `SDATA/sdat.viv`; follow its instructions to supply the decoder separately. Copy the generated `NativeFreeRoamRacers` folder into MW's `scripts`. Keep `speaker**` filenames and `encounter/start`, `player_victory`, `player_defeat` folders. Voices are non-positional and follow game voice volume. No `Setting.ini` is needed; battles work without voices.

Startup update notices query public GitHub metadata without downloading/installing anything. Stable v0.1.2 considers newer stable releases, not Alpha builds. `[Updates] Enabled = 0` disables communication and notices. `Language = auto` follows the encounter language; Japanese uses Japanese, other languages use English. Offline/errors do not block play. Cooperating mods can serialize their notices.

## Limits, reporting and rights

Vehicle/resource limits and safe anchor availability may keep racers below your requested maximum. More traffic can increase CPU/streaming load. Reducing maximum racers or disabling LB is an available performance trade-off, not a guaranteed cure for unrelated stutters.

Shared-model lifetime checks and performance-reference validation improve safety, but do not repair every malformed vehicle mod. The latest pre-release gameplay was stable; actual shared-alias detachment was not observed in that run, so universal CTD prevention is not claimed.

Report version, executable hash, relevant mods/car setup, reproduction steps and `scripts/NFSMWNativeFreeRoamRacers.log` (plus LB log if used). Review logs for personal information before sharing. See `PATCH_NOTES_v0.1.2.md` for changes since public Alpha.59 and `docs/NFSMWLimitBreaker/` for companion details.

Original project code and documentation are licensed under the MIT License in `LICENSE.md`. Third-party rights remain unchanged. The project license grants no rights to game assets or extracted voices.

## 0.1.2 settings and upgrade

**Win-streak wager battles** are optional and Custom-AI-only. Set `[Encounter] WinStreakWager=1` (default 0). Previous consecutive wins add 5% of the Blacklist base reward each: 300, 315, 330. A loss costs the same amount fixed at the start. Cash must cover the stake; losses and leaving free roam reset the streak. Stable rewards remain 1,000. Native cash integer limits still apply.

NFRR keeps normal settings in `NFSMWNativeFreeRoamRacers.ini`; editor-only fallback uses `NFSMWNativeFreeRoamRacers.editor.ini`. Normal enabled population settings take priority. Event Loader support is advance compatibility, optional and not bundled; it is not needed for encounter battles.

The bundled LB adds `[VehicleResources] AdditionalSkins=1` and `PartModelSlots=4096`. Skin allocation stays with Unlimiter when installed, and its count must match installed texture sets. Read the companion guide before changing skin settings. Fresh LB `[Audio] LBEngineAudioExpansion=0` defaults OFF. Preserve an existing opted-in value when upgrading; six slots still require both modules' settings and capability/allocation checks. Never replace third-party INIs automatically.

Detailed logs default OFF: `[Diagnostics] Logging=0`. Only exact `1` opts in after restart; missing/invalid values stay OFF. Diagnostic telemetry/frame timing is skipped while OFF, but functional catalog caching continues. Keep temporary logging out of normal/distribution settings.

See [changes from 0.1.1](../../PATCH_NOTES_v0.1.2.md). Back up existing INIs and merge new entries with the game closed. The extractor remains a separate no-decoder asset; the event/weapon MODs are not included.
