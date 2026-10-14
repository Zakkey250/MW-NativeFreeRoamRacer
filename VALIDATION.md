# NFRR 0.1.2 validation

Release preparation on 2026-10-14. Baseline: published NFRR 0.1.1. Latest development sources were staged separately; the release version was not installed into Main or W2C Sandbox.

## Build and offline checks

- NFRR 0.1.2: clean Release Win32 build from the public source layout, including minimal optional-provider headers.
- LB: current 0.1.3-resource-headroom.2 source rebuilt successfully; 7,934 native checks passed. The bundled ASI is the accepted companion artifact from the 2026-10-14 handoff, SHA-256 `EBE0D21EE0995BE6C2989A72629DDD0E51C667AD9B46D3E4F747742A49EE5590`. Local rebuilds may differ in layout/debug records/toolchain and do not replace the accepted runtime artifact.
- NFRR aggregate cache/AI/geometry/audio/population fixture suite: 9,274 checks passed.
- Reward/wager helper checks: 1,500 Blacklist/streak cases, plus cash limits, reset and police/rival identity checks.
- Marker/prompt guard tests: six racers over 120 seconds of continuous allocation failure produced 18 shared probes and four warnings; native and recognized adapter surfaces accepted, unrelated/private/unreadable destinations rejected.
- Update-notice tests: 68 passed, including numeric release selection, prerelease-to-stable progression, language choice and serialized notification clients.
- Diagnostic logger tests: missing/empty/malformed/OFF configurations create no log, skip formatting, create no logging worker and skip telemetry. ON creates output.
- Actual shipping NFRR/LB DLLs loaded in isolated unsupported test hosts: missing/invalid/OFF created no diagnostic log; ON created diagnostic output. These hosts intentionally fail game identity checks and do not install game hooks.
- ZIPs and SHA-256 manifests are checked against staged file contents. Source archives contain no compiled binaries, installed catalogs, saves, generated caches or game assets.

## Gameplay evidence and limits

The underlying development functionality has previous user gameplay acceptance, including the win-streak system and current LB resource improvements. The final version-stamped NFRR build has not been run in Main/Sandbox in this task, as requested for update-notice testing. Offline and unsupported-host load checks do not certify new in-game icon visibility, audio, every car pack or all race transitions.

Event Loader support is advance compatibility. Event Loader itself is not part of this release. Skin/model/audio/vehicle budgets remain separate, and general invisible-body or 16-car/all-MOD compatibility is not guaranteed.

## 日本語

公開0.1.1を基点とし、配布用の独立ソースからビルド・オフライン検査を実施しました。NFRR 9,274件、LB 7,934件、更新通知68件、報酬・掛け金1,500ケースなどが成功。ログOFF時の非作成とON時の出力も、実ロガーおよび配布DLLの隔離読み込みで確認しました。

同梱LBは引き継ぎで指定された実走採用済み成果物です。再ビルドは成功していますが、ローカル再ビルドを採用済みバイナリと同一ハッシュだとは扱いません。

更新通知の確認を妨げないため、バージョンを打ったNFRRはMain・Sandboxへ導入していません。既存のユーザー実走確認と今回のビルド検査を区別し、最終ビルドの新たな実走・全車種・全MOD構成の保証は行いません。Event Loader本体は非同梱です。
