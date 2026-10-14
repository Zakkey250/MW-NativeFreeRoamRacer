# Diagnostic logging coverage for NFRR 0.1.2

This inventory covers the authored runtime modules shipped in this release, and the unchanged optional audio-extractor asset. Game data, third-party loaders and unrelated installed MODs are outside this package.

| Component | Switch and OFF default | Disabled behavior | Verification |
| --- | --- | --- | --- |
| NFRR | `[Diagnostics] Logging=0`; only exact `1` opts in | No diagnostic file, formatting or logging worker; telemetry and render-resource traversal bypassed; deep audio/frame diagnostics require opt-in | Production logger tests: missing, empty, malformed, OFF and ON; release-binary load checks |
| Limit Breaker | `[Diagnostics] Logging=0`; only exact `1` opts in | Log function returns before formatting/file access; no diagnostic polling loop or diagnostic exception handler | Source gates, 7,934 native checks; release-binary missing/invalid/OFF/ON checks |
| Voice extractor | No diagnostic logger shipped | `manifest.csv` and `summary.json` are functional extraction results, not diagnostic logs | Existing 1.1.0 no-decoder asset retained; no new runtime logger |

OFF does not disable catalog calibration/cache, population APIs, safe allocation checks, update notices, or extracted-audio manifests. Those are functional operations.

The inspected Sandbox loader is Ultimate ASI Loader Win32 6.4.0, SHA-256 `CF4A28C41199031F0378A082BF6FACB2575CB13A1C9AB2F5590652AB0E1AC205`. Its exported interface was inventoried; no supported per-MOD diagnostic registration API was identified. This release retains loader ownership, does not invent an ABI and does not modify the loader. Detailed module logs remain explicitly opt-in.

Temporary ON configurations exist only under isolated release-test directories. The source/default INIs, source archives and runtime ZIP ship OFF. Existing evidence and installed user settings are preserved. Static/build and isolated load tests do not certify real-game icon visibility, audio or every race/MOD combination.
