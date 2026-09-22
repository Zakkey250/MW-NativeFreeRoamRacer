# v0.1.0 — changes since public v0.1.0-alpha.59

## English

NFRR moves from Alpha to v0.1.0, with NFSMW Limit Breaker v0.1.0 included as an optional companion. NFRR remains the main mod and does not require LB.

- **A busier Rockport:** recommended LB integration expands the shared vehicle budget to up to 32, giving roaming racers room alongside ordinary traffic and police. Denser traffic brings more technical encounter races. Actual density still depends on native spawning and available resources.
- **Reliable replenishment and repeated battles with expanded vehicle limits:** revised capacity negotiation, spawn reservations, safe retirement and native race-start preparation address stalls that previously prevented replenishment or further challenges. Integration uses API capabilities rather than a fixed LB version or assumed vehicle limit.
- **More audio and model variety when supported:** optional LB audio-arena integration requests six engine-audio slots. A six-model palette, including the player's car, is enabled only after the required live allocation is verified. Without those capabilities, normal audio and model limits remain available.
- **Better coexistence with traffic and police:** configurable non-racer headroom leaves room for other vehicles; eligible traffic is considered conservatively rather than allowing racer replenishment to consume all capacity.
- **Less repeated work during spawning:** staged preparation, ranked anchor searches, deferred traffic scans, bounded reuse of prepared appearance and adaptive pacing reduce redundant work. Existing AI driving continues independently; this is not a universal FPS or zero-stutter guarantee.
- **Lower diagnostic overhead:** reduced deep audio observation and batched/reused log output. Detailed audio tracing remains opt-in.
- **Safer modded-vehicle handling:** career-performance references are validated without rejecting valid lazy-resolved data. Repeated performance-readback failures are isolated by model so other eligible cars can continue spawning. Shared-solid unload handling invalidates exact-pointer aliases instead of blacklisting one named car. This is not a guarantee against every malformed asset or CTD.
- **Clear optional integration settings:** LB-dependent options have their own INI section and normal operation remains available without LB. Existing optional weapon integration remains in the binary; no weapon mod is bundled or required.
- **Stable release versioning:** version is now `0.1.0`; its startup update notice considers newer stable releases only. No automatic downloads or installation.

Custom AI, career-based Custom rewards, Stable's fixed 1,000 reward, cruising-speed control and startup update notices were already present in public Alpha.59; they are retained, not presented as new v0.1.0 features. The supplied INI uses the current tested Main values, including six racers, 600 m management radius, 200 m markers and ±15 km/h challenge tolerance. Preserve and compare existing INIs when upgrading.

## 日本語

Alpha表記を外し、NFRR v0.1.0へ移行します。NFSMW Limit Breaker v0.1.0は任意の付属MODとして同梱。主体はNFRRで、LBを必須にはしません。

- **より賑やかなロックポート：** 推奨のLB連携で共用車両枠を最大32台へ拡張。一般交通・警察とレーサーを共存させやすくなり、交通量の多いテクニカルな対戦を楽しめます。実数は純正生成制御と資源状況に依存します。
- **拡張車両枠での補充・連戦を改善：** 容量確認、生成予約、安全な解放、純正レース開始準備を見直し、補充や次の挑戦が停止する問題に対処しました。特定LBバージョンや固定台数を前提とせず、API能力で連携します。
- **対応環境で音声枠・車種バリエーションを拡張：** 任意のLB音声連携でエンジン音6枠を要求。必要な実割り当てを確認できた場合のみ自車込み6車種へ拡張します。条件不足・LBなしでは通常の音声枠・車種制限へ戻ります。
- **一般交通・警察との共存：** NFRR以外の車両用枠を設定可能にし、レーサー補充で容量を使い切らないよう配慮。一般車の解放も安全条件を確認します。
- **生成時の重複処理を削減：** 準備の段階化、候補位置の優先探索、交通走査の必要時実行、準備済み外見の期限付き再利用、負荷に応じた生成待機を導入。既存AIの走行は独立して継続します。あらゆる環境でのFPS向上や引っ掛かりゼロを保証するものではありません。
- **診断負荷を軽減：** 詳細音声観測を抑え、ログ出力のまとめ処理・ファイル再利用を実施。詳細トレースは任意です。
- **追加車両データへの安全性を改善：** 遅延解決される正常データを誤拒否せず、キャリア性能参照を検証します。性能読み戻し失敗は車種単位で待機させ、他車種の生成を継続。特定車種の除外ではなく、共有モデル解放時に同一ポインターの残存参照を解除する処理を追加しました。不正資産全般やCTDの完全解消を保証するものではありません。
- **任意連携設定を整理：** LB依存項目をINIの専用セクションに集約。LBなしの通常動作も維持します。既存の任意ウェポン連携はバイナリに残しており、武器MODは必須でも同梱でもありません。
- **正式版の更新判定：** バージョンを `0.1.0` とし、起動時通知は新しい正式版のみを対象にします。自動DL・インストールは行いません。

Custom AI、CustomのBL連動報酬、Stableの1,000固定報酬、巡航速度設定、更新通知は公開済みAlpha.59から継続する機能であり、今回の新機能とは扱いません。同梱INIはMainの確認済み値を採用し、最大6台・管理600m・マーカー200m・開始速度差±15km/hです。更新時は既存INIを保管して差分を確認してください。
