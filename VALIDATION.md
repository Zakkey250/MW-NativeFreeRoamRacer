# NFRR 0.1.4 validation / 検証記録

The release uses the current development NFRR runtime and its build dependencies, with a 0.1.4 version stamp, and the NFRR companion branch of Limit Breaker 0.1.3-resource-headroom.6.

## Build and isolated checks

- Release Win32 builds passed for both mods. The public source includes the required support headers; the game and optional runtime providers are not bundled.
- NFRR: 71 focused encounter-input checks, 354 focused Custom AI checks and 68 update-notice/executable-identity checks passed. The input fixture checks the HUD return path, original Peek call, and the case where another handler consumes the action. The full historical aggregate harness is outside these results.
- LB: 9,046 native checks passed. The static verifier passed on the recognized NFSPatcher EN 1.3 LAA and Redux 3.04 LAA executables. These are file and signature checks.
- Logging: five production-logger tests and ten real ASI load checks on an isolated unsupported host passed (missing, empty, invalid, OFF and ON). Default package INIs keep diagnostics OFF.

## Runtime evidence and limits

- The exact release NFRR and LB binaries loaded in the isolated RC4 game with Ultimate ASI Loader Win32 10.0.2 and the recognized NFSPatcher EN 1.3 LAA executable. The game window remained active during the startup check. Live inspection confirmed that RC4 owns the shared input handler, the original call still targets it, and NFRR owns the HUD return hook. Neither mod created a diagnostic log with logging OFF.
- In the RC4 compatibility investigation, the development build started a battle using the mapped action in the HUD input path in an isolated game. The opponent moved 277 m over approximately 6.6 seconds. This was a bounded battle-start check, separate from verification of the version-stamped release binaries.
- Earlier LB development checks exercised the 224 and 256 MiB options in bounded eight-model RPD fixtures. They do not establish a fix for every rendering or streaming issue.
- W2C to MW and Event Loader are optional integrations. Executable checks do not establish support for every Redux or third-party MOD combination.

## 日本語

最新の開発ソースからNFRR 0.1.4とLB 0.1.3-resource-headroom.6をビルドしました。入力71件、Custom AI 354件、更新通知68件、LBのネイティブ試験9,046件、ログ関連15ケースに合格しています。配布する両ASIで、隔離RC4環境の起動・読み込み・入力フックの共存を確認しました。診断ログはOFFのまま生成されませんでした。

RC4対応時には、隔離ゲームで「イベント参加」の入力経路からバトル開始を確認し、相手車両の実移動を計測しました。これは開発ビルドでの開始検証です。旧集約ハーネス、すべてのMOD構成、バトル完走を合格として扱っていません。
