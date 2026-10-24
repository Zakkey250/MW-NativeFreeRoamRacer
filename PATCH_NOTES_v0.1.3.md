# NFRR 0.1.3 Hotfix

Changes from the published NFRR 0.1.2 / 公開NFRR 0.1.2からの変更点

## English

Fixes incorrect chase-target coordinates in Custom AI and improves compatibility with the upcoming Event Loader and vehicle generation. The optional bundled Limit Breaker has also been updated.

### NFRR

- **Fixed incorrect Custom AI chase coordinates.** An incorrect coordinate permutation when passing a chase target to the native AI could send the rival toward an unintended position and altitude.
- **Continued integration work for the upcoming Event Loader.** Updated integration with `MWEventLoader.asi`. NFRR verifies ownership of supported Event Loader hooks so its encounter battles and notifications can coexist with them. Event Loader is optional; NFRR also works on its own.
- **Improved add-on vehicle recognition.** Vehicle types are now queried through the game's own accessors, avoiding stale vehicle tables after add-on cars are installed. Stock cars without a separate rear-wheel part slot are no longer rejected for that reason.
- **Adjusted exterior-customization load with larger roaming populations.** When a new spawn brings the rival population to five or more, NFRR limits the proportion of cars with customized exterior geometry. Live population is checked again immediately before spawning. Existing cars are not rebuilt, and paint settings are preserved when a new car uses stock exterior geometry.
- **Improved Redux 3.04 compatibility.** Supported executable identification and hook validation remain in place. Compatibility with every additional MOD combination, including Redux installations, is not guaranteed.

### Optional bundled Limit Breaker

Updated from the `0.1.3-resource-headroom.2` build bundled with NFRR 0.1.2 to `0.1.3-resource-headroom.5`.

- **Fixed an AI dispatch-queue overflow.** Writes beyond the native queue's 24 entries could corrupt adjacent sound and spatial-query data. The affected queue now reserves 512 entries and can grow as required, while preserving native processing order.
- **Addressed skin-slot exhaustion with Unlimiter.** The game could expose only 12 free slots even when the allocator was configured for 20. After validating the required skin, wheel and spinner resources and the active allocator, LB makes unused slots 13–20 available. Occupied slots and Unlimiter's allocation/freeing routines retain ownership.
- **Fixed recoloring-cache handling across different skin resolutions.** Cached pixel positions are rebuilt when texture dimensions change, rather than being reused for a different resolution.
- **Increased the default part-model capacity.** `PartModelSlots` now defaults to 8,192 instead of 4,096. This reserves 168 KiB more than the stock 1,024 slots. Existing explicitly configured values can be retained.

The default logical vehicle limit remains 32. Skins, part models and audio have separate limits; this does not provide 32 slots for every vehicle resource. Engine-audio expansion remains OFF by default.

### Updating

Close the game before updating. Back up your existing INIs and merge any needed entries while preserving custom settings. Restart the game completely after changing resource-related settings.

## 日本語

Custom AIの追尾座標に関する不具合を修正し、公開準備中のEventLoaderとの互換性および車両生成処理を改善しました。同梱の任意MOD「Limit Breaker」も更新しています。

### NFRR本体

- **Custom AI追尾の座標不具合を修正**  
  追尾目標を純正AIへ渡す際に座標の並びを誤変換し、本来の走行ラインとは異なる位置・高度を目指してしまう問題を修正しました。

- **公開準備中のEventLoaderとの連携対応を進めています。**  
  MWEventLoader.asiとの連携処理を更新。EventLoaderが使用する対応フックの所有権を確認し、NFRRのバトル処理や通知表示と共存できるよう調整しました。EventLoaderは任意で、NFRR単体でも使用できます。

- **追加車両の認識を改善**  
  車種情報をゲーム側の取得処理から参照するよう変更し、追加車両導入後に古い車種テーブルを参照する問題へ対処しました。また、独立した後輪パーツ枠を持たない純正車両を、不適合として除外しないよう修正しました。

- **多数のライバル生成時の外装負荷を調整**  
  生成後のライバルが5台以上になる場合、カスタム外装車の割合を抑える処理を追加しました。生成直前にも実際の生存台数を再確認します。既に走行中の車両の外装は変更せず、純正外装へ戻す場合も塗装設定は保持します。

- **Redux 3.04対応を改善**  
  対応実行ファイルの識別とフック検査を維持しています。Reduxを含むすべての追加MOD構成での動作を保証するものではありません。

### 同梱Limit Breakerの更新

1.2同梱の0.1.3-resource-headroom.2から、現行の0.1.3-resource-headroom.5へ更新します。

- **AI処理キューの容量超過を修正**  
  純正の24件固定キューを超えた書き込みが、隣接する音声・空間判定用データを破損する問題を修正しました。対象キューに512件分の領域と必要時の追加拡張を用意し、純正の処理順序を維持します。

- **Unlimiter併用時のスキン枠不足へ対処**  
  設定上は20枠でも、ゲーム側の空き管理が12枠に留まる場合へ対処しました。必要なスキン・ホイール・スピナー資源と割当処理を検査したうえで、未使用の13～20枠を利用可能にします。使用中の枠やUnlimiterの割当・解放処理は維持します。

- **異なる解像度のスキン間での塗装キャッシュ処理を修正**  
  別解像度向けの画素位置キャッシュを再利用しないよう、サイズ変更時に再構築します。

- **部品モデル枠の新規設定値を拡大**  
  PartModelSlotsの既定値を4,096から8,192へ変更。純正1,024枠に対する追加メモリは168KiBです。既存INIで指定している値は、そのまま使用できます。

論理車両上限の既定値は引き続き32台です。スキン・部品モデル・音声にはそれぞれ別の制限があり、すべての車両資源が32台分へ拡張されるわけではありません。音声拡張は引き続き既定OFFです。

### 更新方法

ゲームを終了してから更新してください。既存INIをバックアップし、カスタム設定を保持したまま必要な項目を統合してください。資源関連設定の変更後は、ゲームの完全再起動が必要です。
