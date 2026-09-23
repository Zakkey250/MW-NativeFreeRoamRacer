# NFSMW Limit Breaker

NFSMW Limit Breaker v0.1.1 Hotfix is an independent ASI that safely raises Need for Speed: Most Wanted (2005)'s vehicle limit from 20 to at most 32. It relocates all ten `PVehicle*` table vectors, the companion AI-target `ISimable*` vector, and per-vehicle audio, physics, and component registries into separate 64-entry storage, then changes the related vehicle and traffic budgets as one unit.

Version 0.1.1 also expands the vehicle-render connection registry, fixing the omitted 20-entry store. See [hotfix notes](RELEASE_NOTES_v0.1.1.md).

## Compatibility and safety

- Targets the 6,029,312-byte English 1.3 NFSPatcher executable, with or without the user-applied 4 GB/LAA flag.
- Verifies the complete executable SHA-256 and every original instruction before changing memory.
- Does not patch or require Unlimiter, Extra Options, Native Free Roam Racers, or MW Advanced Civilian Vehicles.
- Uses process-lifetime 64-pointer storage for each of the 15 vectors. The release limit of 32 leaves 32 containment slots. This is not a claim of 64-vehicle support.
- Activates all targets as one transaction only while every vector has its expected type and is empty. Conflicts or late loading leave the module inactive.
- Monitors each relocated vector's type, data pointer, capacity, count, guards, and obsolete inline range. Diagnostics are written to `scripts/NFSMWLimitBreaker.log`.

Supported SHA-256 values:

- Base NFSPatcher English 1.3: `80774C2E5D619B4F120B48D4462896FD504C263399D203A238769CFFDE1D253C`
- Same executable with only the LAA flag/checksum changed: `B248271BF8EAC8C9B283B8C95E3ADD672B713BF529B05F1780E58268493B9D06`

## v0.1.1 Hotfix

The release contains the gameplay-accepted logical-32 / physical-64 vehicle feature and the optional, restart-gated, capability-negotiated 8 MiB audio-arena expansion. The only accepted INI vehicle limits are `20 / 24 / 28 / 32`; 32 is the distribution recommendation. No CarLoader expansion code or setting is included.

`LBEngineAudioExpansion = 1` expands the game-owned audio mixer arena to 8 MiB before initialization. A cooperating mod may use extra audio slots only after checking the ABI, capability bit, and live byte count through `NFSMWLB_QueryResourceLimitsV1`. If this feature is disabled or unavailable, the vehicle-limit feature continues normally.

## Development history

`0.1.0-experimental.3` relocates 14 fixed-capacity vectors as one transaction, based on two in-game CTDs. One previously unrelocated `PVehicle*` table member had corrupted the following member; two vehicle audio/physics vectors reached 22 entries; and the vehicle-component registry reached 21 entries. That final overflow overwrote an adjacent registry and caused the destructor-path crash at `0x0041C17B`. This release relocates those structures together with the AI-target vector and updates all five confirmed budget calculations.

`0.1.0-experimental.4` adds diagnostics to the same relocation. A third CTD occurred just after all principal relocated vectors reached 24 with no detected damage. At that threshold, this build enumerates remaining 20-slot fixed vectors and writes `scripts/NFSMWLimitBreaker-crash-<PID>-<number>.dmp` on an access violation. It does not suppress or recover from the exception.

`0.1.0-experimental.6` retains the stack-local victim-vector fix from experimental.5 and narrows crash diagnostics to the main executable. A handled first-chance access violation inside another ASI is ignored, and at most one qualifying dump is written per run. This prevents diagnostic dump bursts from causing gameplay stalls while preserving capture of an executable-side limit-breaker fault. It does not modify the game's original vtable or another mod.

`0.1.0-experimental.7` fixes the final pursuit CTD observed at 24 vehicles. The AI vehicle manager embeds another 20-pointer vector at offset `+0xC4`; entries 21 and 22 overwrote the pursuit-request fields at `+0x124/+0x128`, eventually causing the null read at `0x0043EAFB`. This release substitutes a private growing vtable only at the manager constructor operand `0x0043CA61` and backs it with a bounded 32-pointer reusable pool. It also changes the victim-candidate vector from per-call heap allocations to a bounded reusable pool, because runtime counters proved that the compiler emits no normal destructor call for that originally fixed inline vector. No third-party mod is patched.

`0.1.0-experimental.8` stages the next logical limit at 28 after experimental.7 completed the final pursuit at 24 vehicles. Physical capacity remains 32, leaving four containment slots. It records primary PVehicle pointer readability and remaining 20-slot static-vector candidates separately at the validated 24-vehicle milestone and the configured-limit milestone; executable-side access-violation capture also becomes eligible at 24 rather than waiting for 28. The AI vehicle manager monitor no longer reports a false invariant failure when its global temporarily refers to a different cold lifecycle object.

`0.1.0-experimental.9` adds the optional `NFSMWLB_ReservePopulationV1` capability API. Consumers discover a fixed export and capability bits from the already-loaded ASI, with no dependency on the Limit Breaker release number, internal addresses, or load order. A two-second lease can only restrict native traffic admission temporarily; it cannot relax the logical limit or remove vehicles. In-game testing with NFRR alpha.69 reached 28 vehicles, completed 165 reservation intervals and five starts/finishes, continued spawning during pursuit, and reported no LB invariant or pool failure.

`0.1.0-experimental.10` responds to that run spending long periods at `28/28`, which left too little headroom for ordinary traffic replenishment during pursuit. It advances the logical limit to 32 and pre-expands the 14 fixed-vector stores, victim-candidate pool, and AI vehicle-manager pool to 64 entries. This leaves 32 containment slots at the current limit. The reservation export name, ABI, capability bits, and lease behavior are unchanged. In-game stability of the 32/64 configuration has been accepted without CarLoader expansion. Values 36, 40, 44, and 48 are outside the distribution-safe range.

`0.1.0-experimental.10-audio.1` leaves the in-game validated experimental.10 32/64 vehicle implementation unchanged and adds only the optional `LBEngineAudioExpansion` as an independent feature in the same ASI. It defaults OFF. When enabled before launch, it expands the game-owned audio mixer arena to 8 MiB and publishes `NFSMWLB_QueryResourceLimitsV1` capability only after the live arena and allocator descriptor both verify. Failure of this optional feature does not disable the vehicle-limit feature. It does not itself raise an engine-audio slot count; a cooperating mod must negotiate the capability separately. A complete game restart is required after changing the setting.

`0.1.0-experimental.10-audio.2` keeps the feature and 8 MiB capacity unchanged while optimizing only the added audio diagnostics. It samples the audio-arena invariant every two seconds instead of every 100 ms and coalesces allocator low-water logs in 256 KiB rather than 64 KiB steps. The experimental.10 vehicle monitor cadence, 32/64 vehicle implementation, capability ABI, and audio allocation behavior are unchanged.

`0.1.0-experimental.10-audio.2-carloader.1` was a rejected CarLoader-arena investigation. In-game testing with the 256 MiB stage produced stalls on the order of one second; an A/B run with the same ASI and only CarLoader disabled returned to stable behavior. v0.1.1 Hotfix removes this feature from both code and configuration.

It does not claim that physics, police, rendering, streaming, audio, or third-party spawn logic can all support more than 20 vehicles. In particular, NFSMW's free-roam engine-audio pool has a separate limit.

The game normally removes/defer vehicles before the list reaches its logical capacity. A universal interception path that can safely reject arbitrary construction beyond the configured limit has not yet been proven, so values above 32 are deliberately rejected by the INI parser.

## Installation

Copy `NFSMWLimitBreaker.asi` and `NFSMWLimitBreaker.ini` to the game's `scripts` directory. The distribution-safe baseline is logical 32 / physical 64 with `MaxVehicles = 32`. Enable `[Audio] LBEngineAudioExpansion = 1` only when the independent audio feature is wanted, then fully restart the game. NFRR negotiates this capability and safely falls back to its original audio-slot behavior when the requirements are not met.

Static/startup validation is not in-game acceptance. Test transitions, races, pursuits, busted state, safehouse entry/exit, frontend return, and vehicle destruction before treating the experiment as gameplay-safe.

