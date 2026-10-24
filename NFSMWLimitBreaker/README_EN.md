# NFSMW Limit Breaker companion

Build **0.1.3-resource-headroom.5**, bundled with NFRR 0.1.3. Optional, recommended for retaining ordinary traffic alongside roaming racers and police. NFRR remains usable without LB.

## Installation and limits

With the game closed, copy `NFSMWLimitBreaker.asi` and `NFSMWLimitBreaker.ini` into `scripts`. Back up and merge new settings into an existing INI. Fully restart after changing resource options.

Logical vehicle stages are 20, 24, 28 and 32; default 32. Relocated storage has guarded physical capacity 64. This does not make every renderer, event type, sound pool or third-party spawner support 32 cars.

## Vehicle resource settings

```ini
[VehicleResources]
AdditionalSkins = 1
PartModelSlots = 8192
```

When Unlimiter is absent, `AdditionalSkins=1` prepares eight extra native skin/wheel/spinner sets from installed textures in memory. Stock writable slots 1–4 and precomposite slots 5–12 remain distinct; private extra slots are 13–20. No game texture bank is replaced. Unknown template formats fail closed. This is not a claim of 16 independently composited cars.

When NFSMWUnlimiter is loaded or present in the installation, LB does not install its own skin allocator. After validating an active 20-slot allocator and its actual texture resources, LB can publish unused slots 13–20 in the native free bitmap without replacing Unlimiter allocation/free hooks. Preserve the matching Unlimiter allocation/free implementation and its settings. Its `CarSkinCount` must match the DUMMY_SKIN, DUMMY_WHEEL and DUMMY_SPINNER sets actually available. Do not set 20 or 32 merely because LB allows 32 vehicles.

`PartModelSlots=8192` expands the native car-part eModel pool from stock 1024 with 168 KiB extra reservation. Supported values are 1024–16384; 0 opts out. Unknown patch ownership or an already-created pool prevents installation; another owner's larger reservation is not reduced. It leaves the Cars byte arena and game resource files unchanged.

Resource headroom can improve stability with multiple customized vehicles. It does not resolve all causes of invisible bodies, or certify all-car/16-car endurance.

## Optional engine-audio integration

```ini
[Audio]
LBEngineAudioExpansion = 0
```

Fresh default OFF. Existing opted-in settings may be retained. ON provides an independent 8 MiB mixer arena capability, not an engine-slot count by itself. To request six slots, also enable NFRR's corresponding option. NFRR validates the ABI, capability and actual allocation, otherwise retaining its normal limit. No fixed release-version dependency is required.

## Event Loader advance support

Supports cooperating population/event consumers without requiring Event Loader. NFRR coordinates optional event-AI ownership separately. Event Loader, leaderboards, cutscenes, maps and game assets are not included. Vehicle, skin, part-model and audio capacities are separate.

## Diagnostics and source

`[Diagnostics] Logging=0` is the default; omitted, empty or malformed values also disable diagnostics. Only exact `1` enables them after restart. OFF bypasses diagnostic formatting, file creation and polling. Keep any temporary ON setting out of distribution profiles.

Build with Visual Studio 2022 C++ v143 / Windows SDK, Release Win32. Run the NativeHarness target for offline checks. The capability interfaces remain `NFSMWLB_QueryPopulationV1`, `NFSMWLB_ReservePopulationV1` and `NFSMWLB_QueryResourceLimitsV1`; clients must validate ABI/capabilities, not compare version strings.

Original source uses the MIT license in LICENSE.md. MinHook and its upstream notices retain their licenses. No game assets, executable or ASI loader are included.

