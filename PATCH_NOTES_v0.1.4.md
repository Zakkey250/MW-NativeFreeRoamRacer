# NFRR 0.1.4 — W2C to MW RC4 compatibility

Changes from NFRR 0.1.3 / NFRR 0.1.3からの変更点

## English

- **Fixed an issue that prevented Encounter Battles from starting with [W2C to MW RC4 Preview](https://github.com/Zakkey250/W2C-to-MW-Converter/releases/tag/v2.0.0-rc4).** W2C’s safehouse and shop handling uses the same in-game input as NFRR. NFRR now checks the Join Event input after the game’s input handler returns. When a facility handles that input, NFRR leaves it alone.
- Updated route guidance and speed control for supported W2C/RPD AI modes. The interceptor speed adjustment respects native stopping and obstacle braking. Updated the optional AI integration and tightened checks on vehicle data used during spawning.
- Reduced diagnostic work when logging is disabled. Diagnostics remain off by default.
- Updated the optional **Limit Breaker** to `0.1.3-resource-headroom.6`. With a supported LAA executable and the recognized Unlimiter car arena, it can raise the 115 MiB vehicle streaming budget to 256 MiB by default, with 160, 192 and 224 MiB alternatives. The logical vehicle limit remains 32 and audio expansion remains off by default.
- W2C to MW and Event Loader are optional and distributed separately from this package.

**Updating from 0.1.3:** Close the game and replace `scripts/NFSMWNativeFreeRoamRacers.asi` and, if you use the companion, `scripts/NFSMWLimitBreaker.asi`. Keep your INIs, controls and saves. Compare the supplied LB INI for the new streaming options, then fully restart the game.

## 日本語

- **[W2C to MW RC4 Preview](https://github.com/Zakkey250/W2C-to-MW-Converter/releases/tag/v2.0.0-rc4)との併用時に、エンカウンターバトルを開始できない不具合を修正しました。** セーフハウスやショップとNFRRが同じ決定入力を使うため、NFRRはゲーム側の入力処理が戻った後に「イベント参加」を確認するようにしました。施設側が処理済みの入力では、バトルを開始しません。
- 対応するW2C/RPDのAIモードで、ルート誘導と速度制御を更新しました。迎撃車の速度補正でも、ゲーム本来の停止や障害物に対するブレーキは維持します。任意のAI連携と、車両生成時のデータ確認も更新しました。
- 診断ログがOFFのときに行う処理を削減しました。ログは引き続き既定OFFです。
- 同梱の任意MOD **Limit Breaker** を `0.1.3-resource-headroom.6` に更新しました。対応LAA実行ファイルと所定のUnlimiter車両メモリ構成では、車両ストリーミング領域を115 MiBから既定256 MiBへ拡張します。160、192、224 MiBも選択できます。論理車両上限は32台、音声拡張は既定OFFのままです。
- W2C to MWとEvent Loaderは任意で、このパッケージには含まれません。

**0.1.3からの更新方法：** ゲームを終了し、`scripts/NFSMWNativeFreeRoamRacers.asi` と、LBを使っている場合は `scripts/NFSMWLimitBreaker.asi` を差し替えてください。既存のINI・操作設定・セーブは保持し、必要に応じて同梱INIと新しいストリーミング設定を比較してください。反映にはゲームの完全再起動が必要です。
