# NFSMW Limit Breaker v0.1.0 Release

## Included

- Gameplay-accepted logical vehicle limit of 32.
- Physical capacity of 64 entries for the relocated vehicle-related vectors and bounded local pools.
- Accepted INI vehicle stages: 20, 24, 28, and 32.
- `NFSMWLB_QueryPopulationV1` and `NFSMWLB_ReservePopulationV1` capability interfaces for cooperating population providers such as NFRR.
- Optional, restart-gated 8 MiB engine-audio arena expansion exposed through `NFSMWLB_QueryResourceLimitsV1`.
- English 1.3 NFSPatcher executable identity and patch-site validation, including the supported LAA/4 GB variant.

## Deliberately excluded

- CarLoader arena expansion. The rejected 256 MiB experiment caused approximately one-second stalls, while an otherwise identical A/B run was stable without it.
- Logical vehicle limits above 32.
- Patches to Unlimiter, Extra Options, NFRR, or other third-party MOD binaries.

## NFRR bundle policy

- Bundle `scripts/NFSMWLimitBreaker.asi` and `scripts/NFSMWLimitBreaker.ini` without renaming them.
- Keep `MaxVehicles = 32` for the accepted distribution profile.
- Keep the audio expansion default OFF. If a distribution enables it, both LB and NFRR settings must be enabled before launch, and NFRR must still negotiate ABI 1, the audio capability bit, and at least 8 MiB before requesting six slots.
- Do not infer compatibility from the release version. Consumers must use the exported capability interfaces and fail closed on incoherent provider data.
