# Optional population reservation API (experimental.9)

Experimental.10 preserves this V1 ABI unchanged while the provider's reported
logical limit advances from 28 to 32. Consumers must continue to use the
returned status fields and capability bits instead of comparing MOD versions.

`NFSMWLB_ReservePopulationV1` is an undecorated cdecl export. See
`src/PopulationApi.h` for its fixed 24-byte output and capability bits.
Consumers discover it on the already-loaded ASI; no import library, release
comparison, load order assumption, or mandatory dependency is required.

Only call on the game's simulation/management thread. A nonzero owner can
reserve 0..15 missing vehicles. Zero releases; nonzero expires after 2000 ms
without refresh. Another active owner receives Busy. Available requires the
relocated vectors, guards, manager storage and traffic hook to remain intact.
The reported free count is the minimum vacancy across all 14 fixed vectors.
No storage or logical limit is increased by this API.

The native traffic-capacity predicate still runs. Reservations may only
restrict a native true result, never turn a native rejection into admission.
No cars are removed by LB. With no consumer, admission is unchanged.

NFR alpha.64 checked the API at 20 Hz and reserved the entire racer deficit.
That consumer policy was superseded by alpha.65: it can starve native traffic
cache reuse and remove the moving anchors needed for replenishment. The LB API
implementation is unchanged. Alpha.65 idle checks request ZERO slots. Only a
prepared construction requests ONE slot. A native retirement wait retains it
for at most 1500 ms, without extending the deadline, and retries next tick.
No anchor, a failed preparation, completion, timeout or world exit releases it.

NFR alpha.65 checks the API again immediately before construction.
If full, it can retire ONE distant ordinary traffic car that has been offscreen
for at least 3 seconds, excluding player, police, racers, advanced civilians,
managed cars and the chosen spawn anchor. It waits until a later spawn tick
and verifies real vacancy; it never constructs in the reclaim tick. Existing
construction protection of every remaining live car is unchanged.

Without this capability (absent, disabled, older LB), NFR keeps its conservative
21-live fallback guard. Unsafe or busy providers do not authorize reclamation
or construction above that boundary. The new high-population behavior still
requires user gameplay acceptance; offline ABI and boundary tests are not proof
of full-world stability.

MinHook is vendored under its existing BSD license in
`third_party/minhook/LICENSE.txt`; preserve it when distributing LB builds.
