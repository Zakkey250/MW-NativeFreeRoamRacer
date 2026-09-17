# alpha.59 — cruising controls and startup update notices

## English

- Added a normal cruising-speed setting, default **60%** of the native AI's requested speed. This is not a fixed 60 km/h limit. Set `[Population] CruisingSpeedPercent` to 1–100; 100 disables the reduction. Both AI modes are supported. Battle opponents and police-pursued racers retain their normal pace.
- Added configurable speed matching for challenges: `[Encounter] StartSpeedToleranceKmh = 10` requires a speed difference within **±10 km/h**. The rear 1–60 m envelope and other geometry checks remain. The speed check also applies when pressing Join Event.
- Added a once-per-launch GitHub update notice. It compares numeric versions and requires a published release containing the MOD's binary ZIP. Drafts, extractor-only/source-only releases and same/older versions are ignored.
- **No download or installation feature.** No browser is launched. Offline/errors silently continue, and notifications do not interrupt gameplay. Set `[Updates] Enabled = 0` to disable both checking and notifications.
- Japanese/English notices: `[Updates] Language = auto` follows the existing MOD language. Japanese uses Japanese text; all other/unknown languages use English.
- Shared per-process notification coordination is available for participating MODs, avoiding simultaneous update dialogs and load-order dependencies.
- Existing encounter behavior and optional integrations are retained. The audio extractor remains the unchanged v1.1.0 no-decoder package; no game audio is included.

**Updating:** Exit the game and back up files. Replace the ASI/TPK; preserve your INI and add the new keys if you wish to tune them. Missing keys use the defaults above. The alpha.58 cruising changes received user gameplay acceptance; alpha.59 update handling passed offline, live-metadata and standalone dialog tests. Actual game-start notification behavior and other installations remain unverified.

## 日本語

- 通常巡航速度を調整可能にしました。既定は純正AIの要求速度の**60％**で、60km/h固定ではありません。`[Population] CruisingSpeedPercent`は1～100、100で制限なし。両AIモード対応。バトル相手・警察追跡中のレーサーは制限対象外です。
- 開始時の速度合わせを追加。`[Encounter] StartSpeedToleranceKmh = 10`で、速度差**±10km/h以内**を要求します。後方1～60mなど既存の位置条件は維持し、イベント参加操作時にも再確認します。
- 起動時にGitHubの公開リリースを確認する更新通知を追加。版番号を数値比較し、本MODの配布ZIPがある新版だけを通知します。下書き・抽出器のみ・ソースのみ・同版・旧版は除外します。
- **DL・インストール機能はありません。** ブラウザーも起動しません。通信失敗時はそのまま続行し、走行中には通知しません。`[Updates] Enabled = 0`で通信と通知を無効化できます。
- 通知は日英対応。`[Updates] Language = auto`で既存のMOD言語に連動し、日本語以外・判定不能時は英語になります。
- 対応MOD同士で通知を1枚ずつ表示する共通方式を用意。読み込み順に依存しません。
- 既存バトル機能・任意連携は維持。抽出アプリは既存v1.1.0（デコーダー別途配置）のままです。ゲーム音声は同梱しません。

**更新手順：** ゲーム終了後にバックアップし、ASI/TPKを更新してください。既存INIは保持し、調整する場合は新項目を追記します。省略時も上記既定値で動作します。alpha.58の巡航変更はユーザー実走確認済み。alpha.59はオフライン試験・実通信・独立ウィンドウ試験済みで、実ゲーム起動時の表示と他環境は未検証です。
