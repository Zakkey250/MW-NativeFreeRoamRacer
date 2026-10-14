# NFRR 0.1.2

Changes from the published **NFRR 0.1.1 Hotfix**. Includes the latest optional NFSMW Limit Breaker companion.

## English

### New: win-streak wager battles

Turn on `[Encounter] WinStreakWager = 1` to add a risk-and-reward system to **Custom AI** battles. This option defaults OFF; Stable AI keeps its fixed $1,000 reward.

- The first battle uses the normal Blacklist-based reward. Each consecutive win adds **5% of the base reward** to the next stake: $300 → $315 → $330, for example. The increase is additive, without rounding or a gameplay multiplier cap.
- Win to receive the amount fixed at the start; lose or forfeit to lose the same amount. Insufficient cash blocks the challenge before it starts.
- Losing, changing profile, or leaving free roam for a safehouse, shop or normal event resets the streak. Pausing does not reset it.

### Smoother operation and fixes

- Failed rival-marker allocation now uses a shared 1–8 second retry delay and aggregated warnings, instead of retrying and writing an error every management update. A full native icon table is checked before allocation. This reduces unnecessary work; it does not expand the game's icon capacity.
- Restored the challenge notification's central icon when compatible Event Loader resource hooks are present, while preserving its existing resource-loading chain.
- Separated editor-only settings from normal NFRR settings so editor installation does not disable roaming racers or encounter battles. Existing enabled population settings take priority.
- Corrected optional weapon-forfeit detection: a confirmed hit on the battle rival qualifies; hitting police or another vehicle does not.
- Improved shared vehicle-model lifetime handling. Detailed diagnostic logging is opt-in and defaults OFF; OFF skips log-file creation, formatting, diagnostic telemetry and the logging worker. Functional catalog caching remains active.

### Updated optional Limit Breaker

The bundle includes **LB 0.1.3-resource-headroom.2**, retaining the 32-vehicle logical budget and guarded storage expansion.

- Adds native car-part model capacity up to **4,096 slots** (`[VehicleResources] PartModelSlots = 4096`), with only 72 KiB additional reservation over the stock 1,024 slots.
- When Unlimiter is absent, `[VehicleResources] AdditionalSkins = 1` prepares additional skin/wheel/spinner resources from the user's installed data in memory, without replacing game files. With Unlimiter installed, its skin allocation retains ownership.
- These are separate resource limits: a 32-vehicle budget does not mean 32 independent skins, models or engine sounds. The changes provide headroom, not a guarantee against every cause of invisible vehicle bodies.
- Fresh LB configurations default engine-audio expansion OFF. Existing users can retain their opted-in setting; six-slot audio requires both LB and NFRR options plus successful capability/allocation checks. Never raise Unlimiter's skin count beyond the texture sets actually installed.

### Advance compatibility: Event Loader

Includes advance support for the optional **NFSMW Event Loader**: editor-settings isolation, event AI ownership coordination and notification-resource hook compatibility. Event Loader is **not bundled or required**. NFRR works without it; LB remains optional. This is compatibility groundwork, not a release of Event Loader, expanded leaderboards or new race types.

Close the game before updating. Back up and merge new INI entries rather than replacing your customized settings. Restart after changing resource options. No game assets, extracted voices, saves or generated caches are included. The voice extractor remains the separate no-decoder package.

## 日本語

### 新機能：連勝ボーナス付き賭けバトル

`[Encounter] WinStreakWager = 1` で、**Custom AI専用**の賭けバトルを有効にできます。既定OFFです。Stable AIは従来どおり報酬1,000固定です。

- 初戦はブラックリスト順位に応じた通常報酬。以降は連勝1回ごとに**基本報酬の5%**を加算します。例：300 → 315 → 330。複利・端数切捨て・ゲーム上の倍率上限はありません。
- 勝利で開始時に確定した額を獲得し、敗北・反則負けで同額を失います。掛け金不足なら開始前に拒否されます。
- 敗北、プロフィール変更、セーフハウス・ショップ・通常イベントなどへのフリーローム離脱で連勝リセット。一時停止だけではリセットしません。

### 動作改善・不具合修正

- ライバルマーカー確保失敗時に共有の1～8秒の再試行待機と警告集約を導入。管理更新ごとの再確保・エラーログ連打を抑え、純正アイコン枠が満杯なら確保呼び出しを省略します。アイコン枠そのものを増やす機能ではありません。
- 対応Event Loaderの素材読み込みフックがある場合、開始通知の中央アイコンが消える問題を修正。既存の読み込み処理を維持して連携します。
- エディタ専用設定を通常NFRR設定から分離し、エディタ導入によってレーサー生成・バトルが無効化される問題を修正。有効な通常生成設定を優先します。
- 任意連携のウェポン反則判定を修正。対戦相手への命中を対象とし、警察や別車両への命中では反則負けにしません。
- 共有車両モデルの解放処理の安全性を改善。詳細診断ログは任意ON・既定OFFとし、OFFではログ作成・整形・診断テレメトリ・ログ用スレッドを省略します。機能上必要な車種キャッシュは継続します。

### 任意同梱Limit Breakerの更新

同梱LBを **0.1.3-resource-headroom.2** に更新。論理32台の車両枠と、安全検査付きの格納領域拡張を維持しています。

- 改造パーツのネイティブモデル枠を最大**4,096枠**へ拡張（`[VehicleResources] PartModelSlots = 4096`）。純正1,024枠との差は72KiBです。
- Unlimiterがない環境では、`AdditionalSkins = 1` で導入済み資源から追加スキン・ホイール・スピナーをメモリ内に準備。ゲームファイルは置換しません。Unlimiter導入時はその割当処理を優先します。
- 車両枠・スキン・モデル・エンジン音は別の制限です。車両32台の設定だけで、すべてが32枠になるわけではありません。資源の余裕を増やす改善であり、透明化の全原因を解消する保証ではありません。
- 新規LB設定では音声枠拡張は既定OFF。従来ONにしていた利用者は既存設定を維持できます。音声6枠は両MODの設定と能力・実割当の確認が必要です。Unlimiterのスキン数は実装済みテクスチャ資源数に合わせてください。

### 先行対応：Event Loader

任意MOD **NFSMW Event Loader** に向け、エディタ設定の分離・イベントAIの所有権調整・通知素材フックの共存を先行対応しました。Event Loaderは**非同梱・必須ではありません**。NFRRは未導入でも動作し、LBも任意です。Event Loader本体、順位表拡張、新レース種目の公開を含むものではありません。

更新時はゲームを終了し、既存INIをバックアップして新項目を追記してください。資源設定変更後は再起動が必要です。ゲーム素材・抽出音声・セーブ・生成キャッシュは非同梱。音声抽出器は引き続きデコーダー別途配置版です。
