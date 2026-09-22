# NFSMW Limit Breaker Resource Limits V1

`NFSMWLB_QueryResourceLimitsV1` is an optional, version-independent C export.
Consumers must resolve the export from the already-loaded
`NFSMWLimitBreaker.asi`; they must not infer support from a file version or
hard-coded Limit Breaker release number.

The caller supplies a writable `ResourceLimitsV1` and its byte size. A nonzero
return value is the published capability mask. Consumers must also validate
`structSize`, `abiVersion`, the corresponding capability bit, and the reported
resource size.

For engine-audio expansion, require all of the following:

- ABI version 1.
- `ResourceAudioMixerPoolExpanded` is set.
- `audioMixerPoolBytes >= 0x00800000`.

If any check fails, retain the game's native audio policy. The capability is
published only after the ASI verifies the live 8 MiB arena and allocator
descriptor. Configuration changes require a full game restart.

This API does not grant permission to patch another mod. It only reports a
verified game-owned resource capacity.
