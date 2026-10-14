# NFSMW Limit Breaker bundled with NFRR 0.1.2

Companion build: **0.1.3-resource-headroom.2**. This is the current resource-headroom implementation, supplied as an optional companion to NFRR.

- Preserves the guarded logical 32 / physical 64 vehicle-capacity implementation and capability-based population/audio interfaces.
- Adds optional native skin resources from installed textures when Unlimiter is absent. With Unlimiter installed, it keeps ownership; third-party settings are not overwritten.
- Adds the native car-part model pool setting `PartModelSlots=4096`; 72 KiB more reservation than stock 1024. Set 0 to leave the existing owner untouched.
- Fresh defaults: `AdditionalSkins=1`, `PartModelSlots=4096`, `LBEngineAudioExpansion=0`, `Logging=0`. Retain existing opted-in audio settings when upgrading.
- Coordinates with optional Event Loader use. Event Loader, leaderboards, cutscenes, maps and game texture banks are not supplied by this companion.

The skin, part-model, vehicle and engine-audio budgets are independent. More capacity is not proof of complete 16-car or all-custom-car compatibility. Known resource headroom improvements do not resolve every possible cause of invisible bodies.

同梱版は **0.1.3-resource-headroom.2**。車両枠32／物理格納64と能力APIを維持し、任意の追加スキン資源と改造パーツモデル枠4096を追加しています。Unlimiter導入時はその割当を優先。音声拡張とログは新規既定OFFです。既存の音声ON設定は保持できます。Event Loaderは任意・非同梱で、順位表・カットシーン・マップ資源を含みません。各資源枠は別々であり、16台・全車種・透明化の全原因への対応を保証しません。
