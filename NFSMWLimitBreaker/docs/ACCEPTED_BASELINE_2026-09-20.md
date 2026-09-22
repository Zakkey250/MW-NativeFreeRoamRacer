# Accepted vehicle-capacity baseline (2026-09-20)

## Accepted configuration

- Logical vehicle limit: 32
- Physical storage: 64 entries
- `MaxVehicles = 32`
- `LBCarLoaderExpansion = 0`
- Engine-audio expansion remains an independent, restart-gated option.

The run completed without the previously observed small-prop pass-through, pursuit-breaker trigger/animation failures, vehicle-generation failure, or approximately one-second stalls.

## CarLoader decision

The 256 MiB CarLoader stage was structurally ACTIVE and reported no invariant fault, but the affected gameplay run exhibited stalls on the order of one second. An A/B control kept the same ASI, 32/64 vehicle capacity, audio feature, NFRR build, and installation while disabling only CarLoader expansion. The control run was stable.

Therefore:

- 256 MiB is rejected for distribution.
- 384 and 512 MiB are frozen and must not be promoted.
- The distribution baseline must keep CarLoader expansion OFF.
- Prior collision/event-trigger degradation may be related, but this remains a hypothesis rather than a proven retrospective cause.

This decision does not reduce the accepted 32/64 vehicle capacity. Further vehicle-count expansion requires a design that does not enlarge or replace the CarLoader arena and must undergo fresh gameplay acceptance.
