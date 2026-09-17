# Build / ビルド

Personal research/builds are allowed under [the source terms](LICENSE.md); distributing modified builds requires prior express permission. Third-party components retain their own licenses.

研究・個人利用のビルドは可能です。改変ビルドの配布には事前の明示的な許可が必要です。第三者ライブラリのライセンスは維持します。

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
