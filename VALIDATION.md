# Validation / 検証範囲

## alpha.59 (current)

- The distributed ASI matches the installed alpha.59 binary: SHA-256 `6867D86980E8585B8EED2724B062552C6B048DAF5C7367567AE9C1EAB66DC17B`. Runtime source and public source are compared byte-for-byte; build paths may change a rebuild's binary hash.
- Release Win32 builds successfully. The core offline harness passes 1,396 checks, with 0 failures and one optional owned-voice check skipped. Update-notice tests pass 55 checks offline; live-metadata and standalone-dialog modes each pass 58 (including repeated base checks).
- Standalone Japanese/English test dialogs were created and auto-closed. Numeric versions, release/asset filtering, notification serialization, bounded queues and gameplay cancellation are tested. This is not screenshot QA or an in-game startup/combined-MOD acceptance test.
- The alpha.58 cruising and speed-matching changes received user gameplay confirmation. No additional game run is implied by packaging alpha.59. The new startup notice still needs in-game acceptance when an eligible newer release exists.
- Effective INI values match the current installed configuration. The unchanged icon has SHA-256 `CA929293186A10867B65729CE78AF89C5F5DE880239A402EC68E35E0169D32D1`. Packaging does not modify the installation or saves.
- Optional integration code is retained; no separate integration MOD binaries are bundled. Missing/unavailable interfaces disable only the dependent features. Other executable/car/mod configurations, extended-duration behavior and difficult AI routes remain unverified.

配布ASIは導入済みalpha.59と同一です。公開ソースとの一致・Win32ビルド・本体1,396件と通知55件のオフライン試験を確認。実通信・独立ウィンドウ試験は各58件（基礎試験を含む）です。alpha.58の巡航・開始条件はユーザー実走確認済みですが、alpha.59の実ゲーム起動時通知と他MODを組み合わせた通知表示は未検証です。INI実効値を維持し、梱包でゲーム本体やセーブは変更しません。

## alpha.57 (previous release / 過去の記録)

- The distributed ASI is the exact installed binary accepted by the user after the Japanese text hotfix: SHA-256 `DB9B94D5A830A923F00B8568AF5A3D55C23512D99695502354A33579CF3859C7`.
- Runtime acceptance is the user's report of no noticeable issues on the development installation. No new gameplay run is implied by release preparation.
- Public source builds as Release Win32. The offline harness passes 1,338 checks with 0 failures; one optional owned-audio check is skipped. Source/compiled behavior and native-ABI fixtures are tested, not every gameplay situation.
- The icon remains unchanged: SHA-256 `CA929293186A10867B65729CE78AF89C5F5DE880239A402EC68E35E0169D32D1`.
- INI values match the accepted installed configuration. `Language = ja` and `[BackgroundPolice] Enabled = 1` explicitly expose already-active defaults. Bilingual comments and layout were reorganized; the game installation is not changed by packaging.
- No Arms Assist source or build is distributed. New bridge-dependent behavior was developed with a private compatible build and must not be presented as available with all public installations. Unsupported/missing weapon integration leaves normal encounters available.
- The exact executable guard is unchanged. Other game/car/mod configurations, long-duration resource limits and all unusual obstacle cases remain unverified.

配布物は日本語文面修正後にユーザーが「目立った不具合無し」と確認したASIそのものです。公開ソースのビルド・1,338件のオフライン試験と、実ゲームのユーザー確認は区別します。Arms Assistのソース・ビルドは非同梱・非公開で、新連携は対応版の公開待ちです。INIでは省略されていた日本語と背景警察ONを明示しただけで、既存値は維持しています。

## alpha.50 (previous release / 過去の記録)

- The shipped ASI is the accepted gameplay binary, not a new gameplay revision: 241,152 bytes; SHA-256 `24BB75458CE931805CB9EEBFF33722D2F8BCBE6D1171C1FCBA7B762809055767`.
- Challenge icon TPK SHA-256: `CA929293186A10867B65729CE78AF89C5F5DE880239A402EC68E35E0169D32D1`.
- The accepted session loaded the persistent catalog (512 frontend records / 82 model types), recorded 61 spawns and repeatedly reached 6/6 racers, with no logged ERROR/WARN or spawn failures.
- The offline harness covers 1,039 checks. These cover guards, catalog/cache ownership and invalidation, spawning and encounter logic; they are not proof of all in-game cases.
- Distribution INI key/value defaults match the accepted installation; comments are rewritten in Japanese/English. Population 600 m, markers 200 m, six racers, five model types, four free-roam audio slots.
- Other installations/car rosters and changed-data recalibration are not yet gameplay-validated. Unsupported executables/hooks are rejected rather than assumed compatible.

配布ASIは実走で受け入れ済みのalpha.50そのものです。確認セッションでは61回生成、6/6台を繰り返し維持し、ERROR/WARN・生成失敗はありませんでした。INIの値は確認済みの導入環境と同じで、コメントのみ日英整理しています。1,039項目のオフライン試験は実走の代替ではありません。他環境・改変検知後の再キャリブレーションの実走検証は未実施です。

## Extractor v1.1.0

- The packaged executable is unchanged from the accepted v1.1.0 build. SHA-256: `9DAB134DAADC854A64511A516531CD0BD5672EFDAA5125B3B60F00124069564F`.
- This distribution updates documentation/license notices, not extraction logic. Previously granted MIT rights for earlier copies are not revoked.
- Five source unit tests cover archive structure, bounds, selectors, language discovery and language-neutral filenames.
- Earlier full Japanese retail extraction produced 599 valid WAVs (260 start, 186 victory, 153 defeat). Other language archives have not received the same full runtime test.
- The public archive contains no game archive, extracted voices or personal extraction manifest. Decoder binaries are excluded; users must separately install an official vgmstream CLI bundle. The earlier full extraction validation used that external decoder.

抽出アプリは動作確認済みv1.1.0のEXEを維持し、説明書・利用条件を整理しています。以前のMIT版の取得者に与えられた権利は取り消しません。日本語版599件の抽出実績と、他言語の構造対応を区別しています。

Neither package is digitally signed. These checks are not an antivirus/security certification. No gameplay acceptance is inferred merely from compilation or packaging.

両配布物ともデジタル署名はありません。これらの確認はセキュリティ認証ではなく、ビルド・梱包の成功を実走確認と同一視しません。
