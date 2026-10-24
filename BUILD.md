# Build / ビルド

Original project code and documentation are licensed under the [MIT License](LICENSE.md). Third-party components retain their own licenses.

本プロジェクトの独自コードと文書は[MIT License](LICENSE.md)で公開します。第三者ライブラリには、それぞれのライセンスが引き続き適用されます。

## MW mod

Install Visual Studio 2022 C++ build tools (v143) with a Windows SDK. From the repository root, in PowerShell:

```powershell
& ./NFSMWNativeFreeRoamRacers/tools/Build.ps1
& ./NFSMWNativeFreeRoamRacers/tools/Test-CacheOwnership.ps1
& ./NFSMWNativeFreeRoamRacers/tools/Test-UpdateNotice.ps1
```

The project uses `Release|Win32` (the solution maps `Release|x86`). Output: `NFSMWNativeFreeRoamRacers/artifacts/Release/NFSMWNativeFreeRoamRacers.asi`. Vendored MinHook and NFSPluginSDK are included. The offline harness does not require the game. Building successfully is not gameplay validation. Debug paths/toolchain changes can change the resulting binary hash.

Visual Studio 2022のC++ツール（v143）とWindows SDKが必要です。プロジェクトは`Release|Win32`、ソリューションは`Release|x86`です。オフライン試験にゲームは不要ですが、成功しても実走確認を代替しません。

The original challenge icon is generated without game assets. Python with Pillow is required (verified with Python 3.14.5 / Pillow 12.2.0):

```powershell
python ./NFSMWNativeFreeRoamRacers/tools/Build-EncounterIcon.py
python ./NFSMWNativeFreeRoamRacers/tools/Test-EncounterIcon.py
```

## Update notice / 更新通知

The updater additionally uses the vendored MIT-licensed nlohmann/json header and the Windows WinHTTP library. `Test-UpdateNotice.ps1` runs offline policy/queue tests by default; optional `-Live` accesses GitHub metadata, and `-Dialog` briefly displays auto-closing Japanese/English test windows. Neither option starts or operates the game. Preserve the per-file `/utf-8` option for `UpdateNotice.cpp`.

更新通知は同梱のnlohmann/json（MIT）とWindows標準WinHTTPを使用します。通知テストの`-Live`はGitHub実通信、`-Dialog`は自動で閉じる日英テスト画面を表示します。通常はオフライン試験のみで、ゲームは起動しません。

## Audio extractor / 音声抽出アプリ

Use 64-bit Python with Tkinter (the accepted portable build used Python 3.14), PyInstaller 6.16.0, and an unmodified Windows vgmstream CLI bundle from [vgmstream](https://github.com/vgmstream/vgmstream). Do not copy game archives into the source tree.

```powershell
cd NFSU2EncounterAudioExtractor
python -m unittest discover -s tests -v
python -m pip install pyinstaller==6.16.0
./build.ps1
```

The build excludes decoder binaries. For extraction, separately obtain `vgmstream-cli.exe`, its DLLs and license notices. The script writes a new timestamped build directory under `artifacts`; it does not erase previous packages. Keep the generated EXE, `_internal` and `tools` directories together. Source execution uses `python app.py`; put the decoder bundle in `tools/vgmstream` for the default relative lookup.

64bit Python（Tkinter付き）・PyInstallerでビルドします。デコーダーは同梱せず、実際の抽出時に別途配置します。ビルドは`artifacts`配下に日時別で作成し、既存配布物は削除しません。EXEと`_internal`・`tools`は一緒に配置してください。ソース版の既定検索先は`tools/vgmstream`です。


## 0.1.3 source layout

The `NFSMWEventLoader/src`, `World2MW/src/NeoAI`, `World2MW/src/NativeLoader` and `integration` support headers in this repository are the minimal compile-time dependency closure for NFRR. Keep their relative paths. They do not include the Event Loader/World2MW runtime or game data; those modules remain optional runtime providers.

Logger coverage uses `tools/LoggingModeTests.cpp` (link/include the NFRR include directory), `tools/ASILoggingLoadTests.cpp`, and isolated directories for each OFF/ON case. Historical `LoggingBatchTests.cpp` describes the old synchronous logger and is not the current logger's acceptance suite. See LOGGING_COVERAGE.md and VALIDATION.md for release checks. Build Release Win32 first before the aggregate cache/AI fixture harness.

Event Loader・World2MW・integrationの同梱サポートヘッダーはNFRRの最小ビルド依存関係です。相対配置を維持してください。本体・ゲーム素材は含まれず、ランタイム連携は任意です。新ロガーの試験はLoggingModeTests/ASILoggingLoadTestsを使用します。旧LoggingBatchTestsは旧方式の履歴です。
