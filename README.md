# MW Native Free Roam Racer — v0.1.1 Hotfix

Roaming AI racers and Underground 2-style encounter battles for **Need for Speed Most Wanted (2005)**. NFRR is the main mod; **NFSMW Limit Breaker v0.1.1 is included as an optional, recommended companion**. NFRR also works without LB, within normal vehicle/audio limits.

ロックポートに自由走行レーサーとエンカウンターバトルを追加します。主体はNFRR、LBは任意の付属MODとして同梱・併用推奨です。LBを外しても通常の車両・音声制限内で動作します。

## Download / 説明書

- [v0.1.1 Hotfix release](https://github.com/Zakkey250/MW-NativeFreeRoamRacer/releases/tag/v0.1.1)
- [English guide](NFSMWNativeFreeRoamRacers/docs/GUIDE_EN.md) / [日本語説明書](NFSMWNativeFreeRoamRacers/docs/GUIDE_JA.md)
- [Changes since 0.1.0 / パッチノート](PATCH_NOTES_v0.1.1.md)
- [Validation / 検証範囲](VALIDATION.md) / [Build / ビルド](BUILD.md)
- [LB companion source and guide](NFSMWLimitBreaker/README_EN.md) / [LB日本語](NFSMWLimitBreaker/README_JA.md)
- [Optional voice extractor / 音声抽出器](NFSU2EncounterAudioExtractor/README.md)
- [Development story / 開発記事](NFSMWNativeFreeRoamRacers/docs/development/STORY_JA.md)

## Features / 機能

- Independent racers: default 6, configurable 1–15; management radius 600 m, markers 200 m.
- Price/performance-based car selection, career-aware tuning, random appearance and persistent catalog calibration.
- Use the configured Join Event action while following a rival. The leader chooses the route; a 300 m gap decides the result.
- Custom AI follows your trajectory's general direction and attacks to overtake nearby; Stable AI remains selectable. Custom rewards range from 300 to 3,000 by Blacklist rank; Stable pays 1,000.
- Battle HUD, GPS assistance, background police activity, optional 2D UG2 voices and notification-only update checking.
- Recommended LB expands the total vehicle budget to 32, allowing more ordinary traffic alongside racers and police for more technical races. Actual density is conditional, not guaranteed.
- Optional LB audio support requests six engine slots and enables six model types only after verified allocation; otherwise normal limits apply. Integration uses API capabilities, not a fixed LB version.

既定6台（1～15台設定可）、管理600m・マーカー200m。価格・性能に応じた車種選択、キャリア準拠の性能チューン、外見抽選とキャリブレーションを搭載。追走中に「イベント参加」で挑戦し、300m差で決着します。Custom AIとStableを選択可能。LB併用時は最大32台の共用枠で一般交通・警察を残しやすくなり、よりテクニカルなレースになります。LB能力・実割り当て確認時のみ音声と車種を拡張し、利用できない場合は通常制限へ戻ります。

## Compatibility / 対応範囲

Windows, ASI loader and Visual C++ x86 runtime. Exact NFRR target: NFSPatcher English 1.3 + LAA `speed.exe`, 6,029,312 bytes, SHA-256:

```text
B248271BF8EAC8C9B283B8C95E3ADD672B713BF529B05F1780E58268493B9D06
```

The filename alone is insufficient. No specific car pack is required; arbitrary modded assets and hook combinations are not guaranteed safe. Native navigation can still struggle with junctions and obstacles. More traffic can increase load. See the guides and validation notes for limits, installation and settings. Preserve existing INIs and saves when updating; optional integration providers are not prerequisites.

ファイル名だけで互換性は判断できません。特定車種パックは不要ですが、すべての追加車両・MOD構成を保証しません。分岐・障害物でのAIの迷いや交通増加による負荷は残ります。更新時は既存INIとセーブを保管してください。

No game executable, car assets, saves, generated caches or extracted voices are included. The extractor is a separate no-decoder package; supply your own UG2 data and decoder as instructed. No weapon MOD is bundled. Existing optional integrations remain in NFRR.

ゲーム本体・車両素材・セーブ・キャッシュ・抽出音声は非同梱。抽出器はデコーダー別途配置版です。任意連携コードは維持し、武器MODは同梱しません。

## Rights / 利用条件

See [LICENSE.md](LICENSE.md): research, discussion and personal use are permitted; redistribution and distribution of modified builds require express permission. Third-party rights and previously granted licenses are preserved. Unofficial fan project; not affiliated with or endorsed by Electronic Arts.
