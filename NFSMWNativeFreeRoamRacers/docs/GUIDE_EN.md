# MW Native Free Roam Racer

Adds roaming AI racers and one-on-one encounter battles to Need for Speed Most Wanted (2005).

**alpha.50 is an alpha distribution, gameplay-validated on the development installation. Other car/mod combinations remain unverified.**

On first use, remain in the safehouse for about 2 seconds, then enter free roam and allow the initial vehicle scan and spawning to finish. Successful vehicle, price and stock-performance metadata is saved to `scripts/NativeFreeRoamRacers/VehicleCatalog.cache.bin`. Later launches reuse it when related data SHA-256 fingerprints match. Changed vehicle definitions/Unlimiter configuration or invalid caches trigger rebuilding. Modify game data only while the game is closed. Do not redistribute generated caches.

## Requirements and compatibility

- Windows, the PC edition of MW (2005), and an ASI loader.
- This build targets NFSPatcher English 1.3 + LAA `speed.exe`, 6,029,312 bytes, SHA-256 `B248271BF8EAC8C9B283B8C95E3ADD672B713BF529B05F1780E58268493B9D06`. Renaming another EXE does not make it compatible. The game, EXE and loader are not included.
- A compatible Microsoft Visual C++ x86 runtime.
- Cars are discovered from the loaded vehicle database. No particular add-on car pack, compiled roster or matching GLOBAL-file hashes are required. Car assets are not included.
- Uses original part functions or compatible entrypoint hooks owned by Unlimiter. Unknown hook owners and invalid vehicle records are rejected. Compatibility with arbitrary mods or corrupt data is not guaranteed. Gameplay on vanilla and other Unlimiter configurations remains unverified.

## Install, update and remove

1. Exit the game. Back up existing files with the same names and your saves.
2. Extract the ZIP's `scripts` folder into the game directory. `NFSMWNativeFreeRoamRacers.asi`, `.ini` and `.tpk` belong together in `scripts`.
3. To retain your settings when updating, do not overwrite your existing INI. Do not load older experimental implementations alongside this mod. Do not replace other mods or Save Virtualizer files.
4. Enter career free roam. Catalog discovery and waiting for suitable distant traffic anchors mean racers may take time to appear.

To uninstall, exit the game and remove only those three mod files. Optional voice files are separate. Updating the mod does not modify saves or existing audio.

## Gameplay

- Follow a roaming racer. When the upper-left challenge HUD appears, use the game's configured **Join Event** action. No fixed keyboard key or D-pad binding is required.
- The leader chooses the road. Overtaking swaps the lead; separating the cars by 300 metres decides the battle. A win awards 1,000 cash.
- The upper-left gauge is green for a player lead and red for an opponent lead. Other racers managed by this mod temporarily lose their map markers during the battle.
- When trailing by at least 100 metres, native GPS can guide you toward the opponent. A different user-selected destination takes priority.
- Existing police patrols can discover roaming racers through native detection. A pursuit is not guaranteed on every encounter.

## Shipping defaults

|Setting|Default|
|---|---|
|Maximum racers|6 (configurable 1–15)|
|Population radius|600 m|
|Minimap radius|200 m|
|Model variety|5 including your car's model (1–5)|
|Free-roam audio slots|4 (0–4)|
|Spawn interval|5 seconds|
|Traffic anchor distance|INI: 350–700 m; effectively 350–600 m after population-radius clamping|
|Trailing AI drive output|0–100 m: 1.50; over 100–200 m: 1.75; over 200–300 m: 2.00|

Vehicle selection uses nearby price/performance ranks. Model variety is separate from vehicle count. Random appearance covers paint, body, hood and spoiler. Invalid selections are retried and then fall back to stock. Performance upgrades respect career unlocks; nitrous is equipped only when unlocked and supported by the car. Output multipliers are not speed or top-speed multipliers.

Restart after editing the INI. Add `Language = en` under `[Encounter]` for English battle messages; omission selects Japanese. To disable background police detection, add `Enabled = 0` under `[BackgroundPolice]`.

## Optional encounter voices

No game audio is distributed. Use the separately packaged **NFSU2EncounterAudioExtractor** with your own NFS Underground 2 `SDATA/sdat.viv`. The extractor has Japanese and English instructions.

Copy its generated `NativeFreeRoamRacers` folder into MW's `scripts`. Categories are `scripts/NativeFreeRoamRacers/encounter/start`, `player_victory` and `player_defeat`. Keep the generated filenames containing `speaker**`: the mod chooses an actor and randomly selects that actor's matching start/result lines.

Voices play as 2D audio and follow the game's voice-volume setting. No separate `Setting.ini` is required. Battles also work without optional voice files. Do not redistribute extracted voices or game archives.

## Limitations and reports

Native road navigation may slow down, choose a poor branch or get stuck near junctions, medians and obstacles. Game resource limits and traffic availability can keep the actual population below the configured maximum. This mod does not expand the audio pool to 6 or 15 slots.

Automatic discovery has safety bounds of 8,192 pvehicle records and 1,024 model types. Invalid data or unsupported hooks can stop processing. Initial discovery examines at most one record per management update.

## Source use

Research, exchange of ideas and personal use are permitted under the included LICENSE.md. Redistribution and distribution of modified builds require prior express permission. Third-party licenses and previously granted rights are preserved.

For reports include the mod version, EXE SHA-256, Unlimiter version, car setup, reproduction steps and `scripts/NFSMWNativeFreeRoamRacers.log`. Review logs for personal information before posting them.
