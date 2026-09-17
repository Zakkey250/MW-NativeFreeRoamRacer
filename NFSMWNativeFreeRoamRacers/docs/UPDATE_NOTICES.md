# alpha.59 — startup update notification

Checks public GitHub release metadata once per game process, after NFR's normal runtime installation on its existing initialization worker. It never downloads assets, launches a browser, installs files, reads credentials, or changes the game/ASI loader. No game-thread network calls or extra game hooks. No notification when current, offline, invalid metadata, or rate limited. The local development version is compared numerically, not by date or lexicographic tag order.

`[Updates] Enabled = 1` enables the check; `0` prevents all update-check network access and UI. `[Updates] Language = auto` follows `[Encounter] Language`; if absent, Widescreen Fix's game language and then the 32-bit game registry language are read. `ja`, `jp`, `Japanese`, `ja-JP` select Japanese; all other/unknown languages select English. Explicit `ja` / `en` overrides auto.

GitHub endpoint: `https://api.github.com/repos/Zakkey250/MW-NativeFreeRoamRacer/releases?per_page=100`. HTTPS certificate validation remains enabled; no redirects, authentication or cookies. Only public metadata is transmitted/requested, not saves, game paths or hardware IDs. Direct WinHTTP access (no proxy autodetection). Phase timeouts 1.5–2 seconds; 10-second receive budget with at most the in-flight operation's timeout beyond it; response capped at 2 MiB / depth 32 / 100 releases. No retries or background polling.

Release eligibility:

- Strict numeric `vMAJOR.MINOR.PATCH[-alpha.N|-beta.N|-rc.N]` tags.
- Drafts, missing publication metadata, malformed/duplicate JSON keys and conflicting prerelease flags rejected.
- Stable installations only notify stable releases. Alpha/beta/RC builds also consider newer prereleases.
- Exact repository release page and uploaded nonempty `MW-NativeFreeRoamRacer-<tag>.zip` asset required. Source-only releases and the bundled extractor are not MOD updates.
- Only the highest valid version newer than this installed build is shown. Release text and download URLs are never rendered/executed.

The Japanese/English dialog lists installed/available versions and the human-readable release-page address. It only has a close/continue button. It does not own or disable the game window. A queued/open notification is cancelled once gameplay begins; it closes automatically after 60 seconds. Skipped notifications are checked again next launch, not replayed mid-race. No persistent notification cache is written.

## Shared notification protocol v1 — EA TRAX integration contract

Copy/reuse `include/ModUpdateDialog.h` and optionally `include/UpdateRelease.h` (preserving the JSON dependency's license). Each MOD retains its own repository, asset prefix, current version, settings, language and startup-state callback. EA TRAX is **not modified by this change**.

1. Network access only on an initialization worker, outside DllMain/loader lock/game hooks. Install the MOD normally before running optional checks.
2. Keep the event `Local\Zakkey250.NFSMW.UpdateNotice.v1.<PID>.Checked.<unique-product-id>` alive until process exit to deduplicate copies of the same product. The NFR product ID is `MW-NativeFreeRoamRacer`. Other products MUST use a different ID.
3. All notification dialogs acquire the SAME named mutex `Local\Zakkey250.NFSMW.UpdateNotice.v1.<PID>`; no version, repository or MOD name is appended to this mutex name. PID isolates separate game instances.
4. Hold the mutex only for the bounded notification UI, never for HTTP, file generation, MOD initialization or a callback into another MOD. Release on close/failure; handle abandoned mutexes.
5. Recheck startup eligibility while queuing (100 ms polls), immediately before showing and while open. Default queue limit 90 seconds, visible limit 60 seconds. Continue normally if lock/window creation fails or gameplay has started.
6. No hook chaining, load-order dependency or linking to another MOD's binary. This protocol serializes participating update dialogs; unrelated legacy cache-generation dialogs do not automatically adopt it.

## 日本語

起動時にGitHubの公開リリースを確認し、導入版より新しい本体ZIPがある場合だけ通知します。DL・自動導入・ブラウザー起動はありません。通信失敗や最新版の場合は何も表示せずゲームを続行。通知言語は既存のMOD言語設定に連動し、日本語以外は英語です。

EA TRAX側にも共通の通知ロックを使用することで、起動順に依存せず更新通知を1枚ずつ表示できます。EA TRAX本体への追加実装は別作業です。ゲームプレイに入った後は割り込まず、次回起動時に再確認します。

## Third-party component

nlohmann/json 3.11.3 is copied unmodified from the locally available vendored single header. Its MIT license remains in `third_party/nlohmann/LICENSE.MIT` and the header. Include these files/license in source distributions; preserve the notice in binary distributions. This dependency is not subject to the project's original-source redistribution restrictions.

Official API reference: https://docs.github.com/en/rest/releases/releases#list-releases
