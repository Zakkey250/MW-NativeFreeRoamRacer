# Decoder setup / デコーダーの配置

The decoder is not included. Download a Windows CLI bundle from [official vgmstream releases](https://github.com/vgmstream/vgmstream/releases). Extract it and copy **all** its files here, including `vgmstream-cli.exe`, the accompanying DLLs and license notices. Do not rename DLLs or copy only the EXE. Do not put an extra directory level between this folder and the EXE.

デコーダーは付属しません。[vgmstream公式リリース](https://github.com/vgmstream/vgmstream/releases)からWindows用CLI配布物を取得し、展開した`vgmstream-cli.exe`・関連DLL・ライセンスをすべてこのフォルダーへ配置してください。DLLの名前変更やEXEだけのコピーはしないでください。EXEの前に余分なフォルダー階層を作らないでください。

```text
NFSU2EncounterAudioExtractor.exe
_internal/
tools/
  vgmstream/
    vgmstream-cli.exe
    (DLLs and upstream license notices)
```

For source execution, this folder is relative to `app.py` instead. The previously tested decoder version was `r2117-284-ge6afeaac`; newer releases may differ. The app requires the command-line decoder, not a player plug-in. Retain and follow the decoder's own terms. The missing-decoder error is expected until this setup is complete.

ソース版では`app.py`から同じ相対位置です。確認済みデコーダー版は`r2117-284-ge6afeaac`で、将来の版の動作は未検証です。プレイヤー用プラグインではなくコマンドライン版が必要です。デコーダー独自の利用条件に従ってください。配置が完了するまで「vgmstream-cli.exeが見つからない」というエラーになります。
