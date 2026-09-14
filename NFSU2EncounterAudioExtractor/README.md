# NFSU2 Encounter Audio Extractor

Small, portable Windows app that extracts the encounter-call voices from an owned PC copy of *Need for Speed Underground 2*.

Windows用の小型ポータブルアプリです。所有しているPC版 *Need for Speed Underground 2* の `sdat.viv` から、エンカウンター通話音声を抽出します。

## What it extracts / 抽出内容

- Encounter start / エンカウンター開始
- Player victory / プレイヤー勝利時
- Player defeat / プレイヤー敗北時

The app recognizes the retail speech-bank suffixes for English, French, German, Italian, Japanese and Spanish. It detects the language actually present in the selected archive; it does not translate audio.

英語・フランス語・ドイツ語・イタリア語・日本語・スペイン語の製品版音声バンク名を認識します。選択したアーカイブ内の言語を検出するもので、音声の翻訳は行いません。

Compatibility is implemented for the known retail suffixes `EN` (no suffix), `_FR`, `_GR`, `_IT`, `_JP`, and `_SP`. The complete extraction path has been runtime-tested with a Japanese retail archive (599 valid WAV files). Other language archives were not available for the included test, so the app validates their BIG/IDX/EVT/SCHl structure and stops safely if it differs.

既知の製品版接尾辞 `EN`（接尾辞なし）、`_FR`、`_GR`、`_IT`、`_JP`、`_SP` に対応しています。日本語製品版では全抽出経路を実行し、有効なWAV 599件を確認済みです。他言語の実データは今回の試験環境になかったため、BIG/IDX/EVT/SCHl構造を検証し、構造が異なる場合は安全に停止します。

## Use / 使い方

**One-time decoder setup is required.** This ZIP contains the app and Python runtime, but no vgmstream/codec binaries. Download the Windows CLI bundle from [official vgmstream releases](https://github.com/vgmstream/vgmstream/releases) and copy its EXE, DLLs and notices into `tools/vgmstream` next to this app. See [the setup guide](tools/vgmstream/README.md). No Python installation is required for the packaged app.

**初回にデコーダーの配置が必要です。** ZIPにはアプリとPythonランタイムを含みますが、vgmstream・コーデックは含みません。[vgmstream公式配布](https://github.com/vgmstream/vgmstream/releases)のWindows CLI一式（EXE・DLL・告知）をアプリの`tools/vgmstream`へ置いてください。[配置ガイド](tools/vgmstream/README.md)を参照。配布アプリを使うためのPythonインストールは不要です。

1. Start `NFSU2EncounterAudioExtractor.exe`.
2. Select the game's `SDATA\sdat.viv`.
3. Select an existing output folder and click **Analyze / 解析**.
4. Confirm the detected language, then click **Extract WAV / WAV抽出**.

1. `NFSU2EncounterAudioExtractor.exe` を起動します。
2. ゲームの `SDATA\sdat.viv` を選択します。
3. 既存の出力先フォルダーを選び、**Analyze / 解析** を押します。
4. 検出言語を確認し、**Extract WAV / WAV抽出** を押します。

The app creates a new timestamped folder each run and never modifies `sdat.viv`. Its ready-to-copy layout is `NativeFreeRoamRacers\encounter\start`, `player_victory`, and `player_defeat`. Copy the generated `NativeFreeRoamRacers` folder into the Most Wanted `scripts` folder. `manifest.csv` records every extracted file, and `summary.json` records the source SHA-256 and totals.

毎回新しい日時付きフォルダーを作り、`sdat.viv` は変更しません。出力はそのままコピーできる `NativeFreeRoamRacers\encounter\start`、`player_victory`、`player_defeat` 構成です。生成された `NativeFreeRoamRacers` フォルダーを Most Wanted の `scripts` へコピーしてください。`manifest.csv` に全ファイル、`summary.json` に元ファイルのSHA-256と集計を記録します。

Output WAV filenames never contain a language code or language name. If an unusual archive contains multiple complete language banks and `ALL` is selected, each language is placed above its own `NativeFreeRoamRacers` folder to prevent collisions; filenames remain language-neutral.

出力WAVのファイル名には言語コードと言語名を含めません。複数の完全な言語バンクを持つ特殊なアーカイブで `ALL` を選んだ場合だけ、衝突防止のため各言語フォルダーの下に `NativeFreeRoamRacers` を作ります。WAV名自体は共通です。

## Notes / 注意

- Use only with game data you legally own. / 正規に所有しているゲームデータで使用してください。
- Game archives and extracted audio are not included. / ゲームデータと抽出音声は同梱していません。
- The categories come from the Outrun event groups and selector records in the matching `.idx`/`.evt`, not from filename guessing.
- 分類はファイル名の推測ではなく、対応する `.idx`/`.evt` の Outrun イベントグループと選択レコードに基づきます。
- Runtime selection of a particular variant remains the game's responsibility; speaker and cue identifiers are preserved in filenames and the manifest.
- 個々のバリエーションを実際に選ぶ条件はゲーム側の処理です。話者ID・キューID等はファイル名とマニフェストへ保持します。

## Source diagnostics / ソース版の診断機能

```text
python app.py --sdat "D:\Game\SDATA\sdat.viv" --analyze
python app.py --sdat "D:\Game\SDATA\sdat.viv" --output "D:\Extracted" --language ALL
```

Language codes: `EN`, `FR`, `GR`, `IT`, `JP`, `SP`.

The packaged EXE is intended for GUI use. These commands are for developers running the source tree.

配布EXEはGUIで使用してください。上記コマンドはソースツリーを実行する開発者向けです。

## Third-party software / 第三者ソフトウェア

WAV decoding is provided by a separately installed vgmstream bundle. See `THIRD_PARTY_NOTICES.txt`, `Licenses`, and the decoder's own notices after installation.

## Source use / ソース利用条件

Research, exchange of ideas and personal use are permitted under [LICENSE.txt](LICENSE.txt). Redistribution and distribution of modified builds require prior express permission. Third-party licenses and rights already granted for earlier MIT-licensed copies are preserved. This publication is source-available, not an open-source license.

[LICENSE.txt](LICENSE.txt)に従い研究・意見交換・個人利用を許可します。再配布・改変物のビルド配布には事前の明示的な許可が必要です。第三者ライセンスと、以前のMIT版で付与済みの権利は維持します。ソース公開であり、オープンソースライセンスではありません。
