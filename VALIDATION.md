# NFRR 0.1.3 Hotfix validation / 検証記録

Prepared 2026-10-25 against the published v0.1.2 source and assets. The release uses freshly rebuilt NFRR 0.1.3 and LB 0.1.3-resource-headroom.5 from the curated public source tree.

## Build, static and isolated checks

- Clean Release Win32 builds passed for NFRR and LB. Public source archives include the required support-header closure and upstream licenses.
- Focused Custom AI raw-coordinate/geometry/chase regression suite: 354 checks passed. The actual native argument layout, height and signed axes are tested; the W2C conversion branch is retained.
- LB native harness: 9,046 checks passed, including resource policies, skin ownership and dispatch-queue allocation/growth checks.
- Update-notice/executable identity tests: 68 passed. An obsolete test incorrectly rejected the legitimate same-size EN1.3/LAA pair; it now checks crossed pairs with different sizes, without relaxing runtime target validation.
- File-backed executable identities and native hook guards passed against the exact primary NFSPatcher EN1.3 executable and the supported Redux 3.04 executable. The stale static car-table assertion was replaced with checks of the current native-accessor validation path.
- Production logger: missing, empty, malformed, OFF and ON configurations passed. OFF bypasses formatting, file creation, the logging worker and telemetry traversal.
- Both actual shipping ASIs passed isolated unsupported-host load checks in all five logging modes. These hosts reject game identity and do not install game hooks. Ten ASI cases plus five production-logger cases passed.
- Source/default INIs ship diagnostic logging OFF. Deep audio and frame timing additionally require an explicit diagnostic opt-in while logging is ON. Functional catalog caching and safety checks remain enabled.
- Installed INIs, game resources, saves, profiles, Shadow and vault were not changed during release preparation. No game lab or full game copy was created.

## Earlier real-game evidence and limits

The 2026-10-25 chase investigation measured native target error above 4,468 m before the fix and below 2.77 m after it in bounded encounters, observing real body movement and native AI ownership. Different roads and vehicles were used; this is not a matched performance benchmark or full-battle acceptance. Evidence: `diagnostics/nfrr-custom-chase-20261025/FINAL-REPORT.md` in the development workspace.

LB .5 previously passed bounded actual-game tests with Event Loader, an eight-racer field, heat 5 and up to 31 total vehicles. That does not establish standalone NFRR chase acceptance or every combination of cars/MODs.

The final version-stamped release ASIs were rebuilt and checked in isolation in this publication task. They were not launched in a real game here. Earlier gameplay evidence is not presented as fresh gameplay acceptance of these exact binaries. The production Save Virtualizer verifier refused to run on the development host because it requires ZAKKEY and its local game paths; no production runtime acceptance is claimed.

The full aggregate legacy NFRR harness had a pre-existing catalog-fixture failure/crash in the chase investigation. It is not reported as passing. This release reports the focused chase suite separately.

Event Loader is in preparation, optional and not bundled. Redux support applies to the recognized executable and validated hook surfaces, not every Redux/MOD combination. Vehicle, skin, model and audio capacities remain separate; no universal invisible-body or 16-car guarantee is made.

## 日本語

NFRRとLBを配布ソースからクリーンビルドし、Custom AIの354件、LBの9,046件、更新通知68件、ロガー／実ASI読み込みの15ケースを検査しました。対応EXEの実バイトとフック面も静的検査しています。診断ログは既定OFFです。

今回、ゲームの起動や実ゲームへの配備は行っていません。以前の追尾修正・LB実走記録と、今回の最終バイナリのビルド／隔離検査は区別しています。旧集約ハーネスの既存失敗も合格件数に含めていません。公開はユーザーの明示依頼に基づきます。
