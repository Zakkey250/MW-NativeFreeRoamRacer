# v0.1.0-alpha.57 — Changes since alpha.50

## English

- Added **Custom AI**, selectable independently of the existing Stable AI mode. Rivals follow the general direction of your recorded trajectory, with improved progress tracking to reduce unnecessary turns and abandoned chases.
- Improved catch-up speed requests. Within **15 metres**, correctly aligned rivals transition into an overtaking attack while retaining speed assistance to reduce sudden slowing at the switch.
- Added a configurable Custom AI leader drive-output boost, default **1.25×**. This is not a speed multiplier.
- Added career-scaled rewards for Custom AI: **300 cash at BL15 to 3,000 at BL1**, rounded down to hundreds. **Stable AI still pays 1,000 cash.**
- Fixed lead/result notifications being suppressed during police pursuits and added bounded retries when the HUD is temporarily unavailable.
- Added optional hit-based disqualification for supported MWArsenal EMP/shockwave effects. Firing without hitting and ordinary collisions do not count as fouls.
- Replaced the Japanese word for weapon with **ウェポン** in the disqualification message to avoid a missing font glyph. English text is unchanged.
- Reorganized the bilingual INI by feature, with an explicit `Language = ja` / `en` selector. Shipping defaults now use **Custom AI** and retain the accepted population, marker, audio and power settings.

**Optional integration notice:** Arms Assist is neither bundled nor published by this release. Spike-hit reporting and racer EMP attacks against police require its compatible encounter-bridge build, whose public distribution is still pending. An older Arms Assist without that bridge disables weapon-hit observation to avoid conflicts. Normal roaming and encounters do not require Arms Assist or MWArsenal.

**Updating:** Back up your files and exit the game. Replace the mod ASI/TPK; merge settings or use the supplied INI. To enable Custom AI while keeping an older INI, add `AIMode = Custom AI` under `[Encounter]`. Set `Language = en` for English or `Language = ja` for Japanese battle messages. Restart after editing. No audio, saves, vehicle assets or generated caches are bundled. The optional audio extractor remains **v1.1.0**, with the decoder obtained separately.

**Known limits:** Complex junctions, medians and obstacles can still cause poor routes or stuck vehicles. The user reported no noticeable problems in the current development installation; other car/mod configurations remain unverified. This remains an alpha release.

## 日本語

- 従来のStable AIとは別に、INIで切り替える **Custom AI** を追加。プレイヤーの軌跡の進行方向を基準に追走し、不要な急旋回や追走中断を抑えるため進行管理を改良しました。
- 追い上げ時の要求速度を補正。**15m以内**で向きなどの条件を満たすと追い抜きへ移行し、切替中も速度補助を維持して急減速を抑えます。
- Custom AIの先導時に、既定 **1.25倍**の駆動出力補正を追加。INIで調整可能です。速度倍率ではありません。
- Custom AI限定でキャリア連動報酬を導入。**BL15で300～BL1で3,000**、100単位で切り捨て。**Stable AIは1,000固定**を維持します。
- 警察追跡中にリード・結果の汎用メッセージが表示されなくなる問題を修正。HUDを一時利用できない場合の再試行を追加しました。
- 対応するMWArsenalのEMP・ショックウェーブについて、実際の命中効果による反則負け判定を追加。発射だけ・空振り・通常接触は反則になりません。
- 日本語の反則メッセージを **「ウェポンを当てちまった…！／反則負けだ！」** に変更し、非対応文字による表示欠けを回避。英語は変更していません。
- 日英INIを機能別に整理し、`Language = ja`／`en`で言語を明示的に切り替えられるようにしました。配布既定値は **Custom AI** とし、生成台数・マーカー・音声枠・出力倍率は確認済み設定を維持します。

**任意連携について：** Arms Assistは今回同梱・公開しません。スパイク命中通知とレーサーの対警察EMPは、連携対応Arms Assistの配布待ちです。連携非対応の旧Arms Assist導入時は競合回避のため武器命中監視を無効化します。通常のフリーローム・バトルにはArms AssistもMWArsenalも不要です。

**更新方法：** ゲームを終了してバックアップ後、本MODのASI・TPKを更新し、設定は統合するか同梱INIを利用してください。旧INIを保持してCustom AIを使う場合は、`[Encounter]`へ`AIMode = Custom AI`を追加します。表示言語は日本語が`Language = ja`、英語が`Language = en`。変更後は再起動してください。音声・セーブ・車両素材・生成キャッシュは同梱しません。任意の音声抽出アプリは **v1.1.0据え置き**、デコーダーは別途配置です。

**既知の制限：** 複雑な分岐・中央分離帯・障害物での迷走や立ち往生は残ります。現在の開発環境ではユーザーから「目立った不具合無し」の確認を得ていますが、他の車両・MOD構成は未検証です。引き続きアルファ版として配布します。
