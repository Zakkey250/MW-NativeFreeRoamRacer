# PVehicle list analysis (NFSPatcher English 1.3)

Target SHA-256: `B248271BF8EAC8C9B283B8C95E3ADD672B713BF529B05F1780E58268493B9D06` (LAA) / `80774C2E5D619B4F120B48D4462896FD504C263399D203A238769CFFDE1D253C` (base).

## Corrected layout

`0x0092CD18` is the first of ten fixed-vector objects with a `0x60` byte stride:

| Address | Meaning |
|---|---|
| `0x0092CD18` | allocator/vtable pointer (`0x008918D8`) |
| `0x0092CD1C` | data pointer, initially `0x0092CD28` |
| `0x0092CD20` | capacity, initially 20 |
| `0x0092CD24` | count |
| `0x0092CD28..0x0092CD77` | inline storage, 20 pointers |
| `0x0092CD78` | allocator/vtable pointer of the next vector |
| `0x0092CD7C` | data pointer of the next vector |
| `0x0092CD80` | capacity of the next vector |
| `0x0092CD84` | count of the next vector |

The allocator methods at `0x00424500..0x00424530` are fixed-capacity methods: they report 20, return `this + 0x10`, and do not free. Therefore changing budget immediates alone lets count 21 and 22 overwrite the next vector.

## Reference result

Raw and decoded operand scans found no direct reference to any individual address in `0x0092CD28..0x0092CD77`. Consumers obtain `data` and `count` through the container fields. Generic helpers index the ten-vector table from base `0x0092CD18` with a `0x60` stride. The creation path at `0x006893AD` reads capacity, calls the vector growth route when full, appends through the data pointer, and increments count. Removal/compaction uses `0x0067AD40` and the same fields.

This makes guarded data-pointer relocation sufficient for the confirmed consumers; no executable operand needs to point directly at a new allocation. Runtime acceptance later proved that relocating only the first table member was insufficient: the third member exceeded 20 and damaged the fourth member's object fields. All ten table vectors are therefore relocated together. The five separate budget calculations still need coordinated changes.

## AI-target companion vector

The first 20-plus runtime test proved that each live vehicle also contributes an `ISimable*` entry to the AI-target lookup vector:

| Address | Meaning |
|---|---|
| `0x0090D660` | fixed-vector allocator/vtable pointer (`0x008915B8`) |
| `0x0090D664` | data pointer, initially `0x0090D670` |
| `0x0090D668` | capacity, initially 20 |
| `0x0090D66C` | count |
| `0x0090D670..0x0090D6BF` | inline storage, 20 `ISimable*` pointers |
| `0x0090D6C0` | first adjacent AI-goal factory-registry node |

The append path at `0x00423750` consults the vector capacity but its fixed allocator cannot provide larger storage. In the captured failure, the vector count reached 23 and entries 21 through 23 overwrote `0x0090D6C0..0x0090D6C8`. A later factory lookup at `0x0043B554` followed the third overwritten pointer as a linked-list node and raised `0xC0000005` while reading address `0x0000016F`. At the same instant, the relocated primary vehicle vector held 22 valid pointers with intact guards and unused storage, proving that the first relocation itself was not corrupted.

`0.1.0-experimental.2` treated those two vectors as one transaction. Its second runtime test kept both relocated vectors healthy through count 22, but exposed further 20-slot companion vectors.

## Additional per-vehicle vectors found by the second CTD

The second dump failed at `0x0041C17B` while a destructor/erase path dereferenced a corrupted allocator pointer. The immediate overwrite originated from the 21st pointer in the fixed vector at `0x0092FE50`: its inline storage ends at `0x0092FEB0`, exactly where the adjacent dynamic registry begins. The dump also showed two other per-vehicle vectors already at count 22 and a non-primary `PVehicle*` table vector with evidence of an earlier overflow.

| Vector object | Expected vtable | Observed count | Role/evidence |
|---|---:|---:|---|
| `0x0092CD18 + n*0x60`, `n=0..9` | `0x008918D8` | up to 22 | ten-member `PVehicle*` table; member 2 damaged member 3 |
| `0x0090D660` | `0x008915B8` | 22 | AI-target `ISimable*` lookup |
| `0x00913DB0` | `0x00897774` | 22 | per-vehicle audio-related registry |
| `0x00913E70` | `0x008977A4` | 22 | active vehicle-component/physics-tick registry |
| `0x0092FE50` | `0x008AA454` | 21 | vehicle-component registry; overwrote the adjacent object at `0x0092FEB0` |

`0.1.0-experimental.3` treats all 14 vectors as one transaction. Each must have its exact expected vtable, zero count, and either its inline-ready state (`data = object + 0x10`, `capacity = 20`) or the observed constructor-complete cold state (`data = 0`, `capacity = 0`). Every data pointer and capacity is redirected before the budget patches are committed. A patch failure restores the exact previous fields in reverse order. Each relocated vector receives independent vtable, pointer, capacity, count, guard, and obsolete-inline-range monitoring.

## Third runtime result and diagnostic gate

The `experimental.3` runtime reached count 24 in the primary PVehicle, AI-target, vehicle-audio, and active-vehicle-component vectors, while the vehicle-component registry reached 23. No relocated-vector invariant or obsolete-inline-range monitor fired. Roughly ten seconds later the process exited with `0xC0000005`, but Windows produced neither an Application Error record nor a new local dump for that run. This rules out the two previously observed overwrites but does not identify the third exception address.

`0.1.0-experimental.4` therefore keeps the relocation unchanged and adds evidence collection instead of speculatively relocating more globals. When the primary vector first reaches 24, it scans the executable's committed static-data range for still-inline pointer vectors with capacity 20 and records their counts. A first-chance access violation at or beyond that threshold produces a minidump and register record before exception processing continues normally.

The next run captured the first exception at `0x00687956` (`mov edx,[ebp]`) with `ebp = 1`. The local victim-candidate vector at `esp+0x2C` reported `data = esp+0x3C`, `capacity = 24`, and `count = 24`. Its records are 24 bytes each, but the function's inline backing was compiled for only 20. Records 21 through 24 consequently overwrote 96 bytes of the SEH/saved-register/caller-frame area; the first function argument at `esp+0x244` had become the record flag value `1`. The later exceptions at address `0x27` were secondary effects of the same damaged return frame.

`0.1.0-experimental.5` preserved all 24 victim candidates without enlarging or re-indexing the function's stack frame. It replaced only the vtable immediate loaded into this local vector with a private six-entry copy. Its first implementation allocated `capacity * 0x18` bytes from the process heap. Runtime counters later proved that the compiler emits no normal destructor/free call for this originally fixed inline vector, so repeated calls accumulated small process-lifetime allocations. Experimental.7 replaces them with a bounded 16-slot reusable pool while retaining the private grow/max behavior and leaving the original game vtable at `0x008AA234` unchanged.

`0.1.0-experimental.6` also constrains the first-chance AV diagnostic. Runtime evidence at 24 vehicles showed a foreign ASI repeatedly raising and handling an AV; experimental.5 wrote four approximately 22 MiB dumps for that non-fatal sequence and caused visible stalls. The handler now accepts only exception addresses inside the verified main executable image and atomically permits one dump per process. Foreign-ASI exceptions continue through normal exception dispatch without logging or dumping.

## AI vehicle manager embedded vector

The final experimental.5 run ended at `0x0043EAFB` with `ESI = 0` while native pursuit management followed its request list. The full-memory dump showed that the AI vehicle manager's embedded vector at `manager + 0xC4` still used vtable `0x008918D8`, inline storage at `manager + 0xD4`, capacity 20, and count 22. Its 21st pointer overwrote `manager + 0x124`; its 22nd pointer overwrote the request-list head at `manager + 0x128` with `1`. This directly explains the later invalid list traversal and null dereference.

The constructor writes the fixed-vector vtable at `0x0043CA5F` (four-byte operand at `0x0043CA61`) and reserves 20 entries later in the same constructor. Experimental.7 changes only that operand to a private six-entry vtable. A bounded four-slot reusable pool supplies 32-pointer backing. The first reserve exposes capacity 20; the 21st append calls the private grow method, which returns 32 and reuses the same backing without discarding existing entries. The constructor operand, live manager vector, pool membership, capacity, count, and allocation failures are monitored. No NFRR or other third-party code is changed.

## Experimental.8 boundary

Experimental.7 completed the final pursuit at 24 vehicles without a CTD or a relocated-vector fault. Experimental.8 therefore moves all 14 container capacities and the five game budgets together to 28 while each allocation remains 32 pointers. The four extra slots are corruption containment, not an advertised 32-vehicle limit. The monitor records separate snapshots at the validated 24-vehicle point and at the configured limit, checks the active primary pointers for readable committed memory, and continues to detect container-field, vtable, obsolete-inline-range, and guard changes. Main-executable access-violation capture becomes eligible at 24 even when the configured limit is 28. A safe universal rejection hook for arbitrary third-party construction remains a later gate.

## Experimental.10 boundary

Experimental.9 added an optional, capability-discovered traffic reservation API without changing the 28-vehicle limit. The NFRR alpha.69 acceptance run reached 28, completed five encounter cycles and 165 bounded reservations, and produced no LB invariant, allocation, or exception fault. It also showed sustained `live=28 limit=28` periods during pursuit, while the vehicle-component registry independently reached 28. This is direct evidence that the coordinated limit, rather than an unobserved lower companion vector, was constraining replenishment.

Experimental.10 moves the 14 container capacities and five budget operands together to 32. All backing allocations, including the victim-candidate and AI vehicle-manager reusable pools, grow to 64 entries. Physical capacity is deliberately separated from the current 32 limit and the planned 48 ceiling: it leaves 32 containment slots now and 16 at 48. The accepted 28-vehicle point becomes the milestone snapshot; the configured-limit snapshot moves to 32. Main-executable AV capture remains eligible at 24 to retain earlier diagnostic coverage. `NFSMWLB_ReservePopulationV1` remains ABI-identical and reports the new logical limit through its existing status field. Values above 32 remain rejected pending staged acceptance at 32, 36, 40, 44, and finally 48, with renewed companion-limit scans at every stage.
