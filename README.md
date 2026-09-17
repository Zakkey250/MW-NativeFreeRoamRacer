# MW-NativeFreeRoamRacer

Roaming AI racers and one-on-one encounter battles for **Need for Speed Most Wanted (2005)**. Includes the source of the optional **NFSU2 Encounter Audio Extractor**.

フリーロームを走るAIレーサーと、追走から始まる1対1のエンカウンターバトルを追加します。別添の音声抽出アプリのソースも収録しています。

## Download and instructions / 配布・説明書

- [Releases / 配布ZIP](https://github.com/Zakkey250/MW-NativeFreeRoamRacer/releases)
- [日本語説明書](NFSMWNativeFreeRoamRacers/docs/GUIDE_JA.md) / [English guide](NFSMWNativeFreeRoamRacers/docs/GUIDE_EN.md)
- [Audio extractor / 音声抽出アプリ](NFSU2EncounterAudioExtractor/README.md)
- [Build / ビルド](BUILD.md) / [Validation and limits / 検証範囲](VALIDATION.md)
- [Development blog (Japanese) / 開発ブログ：Native Free Roam Racerができるまで](NFSMWNativeFreeRoamRacers/docs/development/STORY_JA.md)

## Features / 機能

- Independent roaming racers; default 6, configurable 1–15. Current distribution defaults: population radius 600 m, minimap radius 200 m.
- Price/performance-based vehicle selection, career-limited upgrades and random appearance. Up to 5 model types including the player's model; 4 free-roam engine-audio slots.
- Automatic vehicle metadata calibration, persisted until related game data changes.
- Follow a racer and use the game's configured Join Event action. The leader chooses the route; a 300 m gap decides the battle. Stable AI pays 1,000 cash; Custom AI pays 300–3,000 by career Blacklist rank.
- Switch between Stable and Custom AI in the INI. Custom AI follows the player's recorded direction and attacks to overtake within 15 m; it is the shipping default.
- Lead gauge, challenge notification, trailing-player GPS, optional 2D encounter voices and background police detection.

- 独立走行AIは既定6台、1～15台に設定可能。現在の既定値は生成範囲600 m、マーカー表示200 mです。
- 価格・性能の近い車種を選び、キャリア進行に従った性能チューンとランダム外見を適用。自車を含む最大5車種、フリーローム用エンジン音4枠。
- 車両情報を自動構築・保存し、関連ゲームデータが変わるまで再利用。
- 追走中にゲーム内の「イベント参加」で開始。先頭車がルートを選び、300 m差で決着。報酬はStable AIで1,000固定、Custom AIでBLに応じて300～3,000。
- INIでStableとCustom AIを切替。Custom AIはプレイヤーの軌跡方向を追走し、15m以内で追い抜きへ移行します。配布既定値はCustom AIです。
- リードゲージ、開始通知、後追い時のGPS、任意の2D音声、背景での警察追跡。

## Compatibility / 対応範囲

**alpha.59**, Windows, ASI loader, compatible Visual C++ x86 runtime. Exact target: NFSPatcher English 1.3 + LAA `speed.exe`, 6,029,312 bytes, SHA-256:

```text
B248271BF8EAC8C9B283B8C95E3ADD672B713BF529B05F1780E58268493B9D06
```

The filename alone does not establish compatibility. Vehicle discovery no longer requires a particular add-on roster, but gameplay has been validated on the development installation only; vanilla and other car/mod combinations remain unverified. Native AI may take poor routes or become stuck. This remains an alpha release.

ファイル名が同じだけでは互換性を保証しません。特定の追加車種一覧への依存は除去しましたが、実走確認は開発環境のみです。バニラ・他の車両/MOD構成は未検証です。純正AIの経路選択や障害物で詰まる問題は残ります。アルファ版として配布します。

Game files, ASI loader, car assets, saves, generated caches and extracted audio are **not** included. Optional voices must be extracted from an owned NFS Underground 2 copy. Japanese extraction has been tested; other supported language-bank layouts are structurally supported but not fully runtime-validated.

ゲーム本体・ローダー・車両素材・セーブ・生成キャッシュ・抽出音声は同梱しません。任意の音声は所有するUG2から抽出してください。日本語版の抽出を確認済み、他言語版は構造対応のみで全抽出経路は未検証です。

## Latest changes / 最新の変更

See [alpha.59 patch notes](PATCH_NOTES_alpha59.md). Normal cruising now defaults to 60% of the native AI target speed, and challenges require a speed difference within ±10 km/h; both are configurable. Startup update notices check public GitHub metadata only, with no download or installation feature. Optional integrations remain optional and are not required to play.

[alpha.59パッチノート](PATCH_NOTES_alpha59.md)を参照してください。通常巡航の目標速度は既定60％、開始時の速度差は既定±10km/h以内となり、いずれもINIで調整できます。起動時の更新通知はGitHubの公開情報のみ確認し、DL・自動導入は行いません。任意連携はゲームプレイの必須要件ではありません。

## Source use / ソース利用条件

Research, exchange of ideas and personal use (including local modifications/builds) are permitted. Redistribution and distribution of modified builds require prior express permission. See [LICENSE.md](LICENSE.md); third-party licenses and previously granted rights are preserved.

研究・意見交換・個人利用（個人用の改変・ビルドを含む）は可能です。再配布および改変物のビルド配布には事前の明示的な許可が必要です。[利用条件](LICENSE.md)を確認してください。第三者ライセンスと過去に付与済みの権利は維持します。
