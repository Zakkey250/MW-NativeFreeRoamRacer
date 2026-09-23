# 0.1.1 Hotfix

## English

Changes from 0.1.0:

- Fixed a vehicle-render registry overflow in the bundled NFSMW Limit Breaker that could corrupt memory and cause a crash, including when finishing a race event. The registry now receives the same guarded expansion and integrity checks as the other vehicle registries; the configured vehicle limit is unchanged.
- Fixed repeated racer spawn rejection when an added car uses the same valid data for its stock and fully upgraded chassis. NFRR now accounts for the game's no-op upgrade behavior before spawning, while retaining strict post-spawn checks, career restrictions, and the nitrous requirement. Other upgrades are unaffected.
- Added a localized **Open GitHub** button to update notifications. It opens the release page only when selected; updates are not downloaded automatically.

Update both NFRR and the bundled Limit Breaker with the game closed. Preserve your existing INI settings. Limit Breaker remains optional. This hotfix does not claim to fix every cause of intermittent invisible vehicle bodies.

## 日本語

0.1.0からの変更点：

- 同梱NFSMW Limit Breakerの車両描画用一覧が容量超過し、メモリ破損によってレースイベント終了時などにCTDする問題を修正。ほかの車両一覧と同様に、安全検査付きの領域拡張と整合性監視を適用しました。設定上の車両上限は変更していません。
- 追加車両の純正シャシーと最大強化シャシーが同じ正常なデータを参照している場合、強化不一致としてレーサー生成を繰り返し拒否する問題を修正。ゲーム側で強化が変化しない仕様を生成前に考慮します。生成後の厳密な検査、キャリア制限、ニトロ必須は維持し、ほかの強化には影響しません。
- 更新通知に日英対応の**「GitHubを開く」ボタン**を追加。選択した場合のみリリースページを開き、自動ダウンロードは行いません。

ゲームを終了してから、NFRRと同梱Limit Breakerの両方を更新してください。既存のINI設定は保持してください。LBは引き続き任意です。低頻度の車体透明化について、すべての原因の解消を保証するものではありません。
