// Native prompt/input adapter. Battle writes are deferred to the frame update.
// Native addresses are specific to the SHA-256-checked NFSPatcher LAA image.
constexpr std::uintptr_t kEncounterHudJoy = 0x0057C660;
constexpr std::uintptr_t kEncounterQueuePeek = 0x00633DC0;
constexpr std::uintptr_t kEncounterPeekReturn = 0x0057C742;
constexpr std::uintptr_t kEncounterMenuUpdate = 0x0057BBC0;
constexpr std::uintptr_t kEncounterMenuVtable = 0x008A2524;
constexpr std::uintptr_t kEncounterMenuHide = 0x0057BEA0;
constexpr std::uintptr_t kEncounterFindObject = 0x00524850;
constexpr std::uintptr_t kEncounterSetAnimation = 0x00514D10;
constexpr std::uint32_t kEncounterEngageAction = 46;
constexpr ULONGLONG kEncounterFollowGraceMs = 2000;
using EncounterJoyFn = void(__thiscall*)(void*, void*);
using EncounterPeekFn = void*(__thiscall*)(void*, void**);
// The native HUD walker pushes IPlayer* at 0058CE3B; Update ends RET 4 at
// 0057BD0C. The argument is unused by this widget but remains ABI-mandatory.
using EncounterMenuFn = void(__thiscall*)(void*, void*);
EncounterJoyFn g_encounterOriginalJoy = nullptr;
EncounterPeekFn g_encounterOriginalPeek = nullptr;
EncounterMenuFn g_encounterOriginalMenu = nullptr;
bool g_encounterEnabled = false;
bool g_encounterFaulted = false;
thread_local void* g_encounterJoyContext = nullptr;
void* g_encounterHud = nullptr;
ULONGLONG g_encounterHudTick = 0;
void* g_encounterOwnedWidget = nullptr; // Compared only against a live Update callback.
ULONGLONG g_encounterNextFrameAttempt = 0;

struct EncounterAction { std::uint32_t id; std::int32_t slot; float value; };
static_assert(sizeof(EncounterAction) == 12);
struct EncounterFollow {
    float distance = 0, ahead = 0, lateral = 0, speedDelta = 0;
};
struct EncounterCandidate {
    void* player = nullptr;
    void* rival = nullptr;
    void* simable = nullptr;
    std::uint32_t key = 0;
    float heldSeconds = 0;
    ULONGLONG sampleTick = 0;
    ULONGLONG qualifiedTick = 0;
    bool ready = false;
    bool holding = false;
};
EncounterCandidate g_encounterCandidate{};
ULONGLONG g_encounterCooldownUntil = 0;

const char* EncounterFollowReason(const VehicleSnapshot& player,
                                 const VehicleSnapshot& rival,
                                 EncounterFollow* output = nullptr,
                                 bool holding = false) noexcept {
    EncounterFollow f{};
    const float dx = rival.position.x - player.position.x;
    const float dz = rival.position.z - player.position.z;
    const float length = std::hypot(rival.heading.x, rival.heading.z);
    f.distance = Distance(player.position, rival.position);
    f.speedDelta = std::abs(player.speed - rival.speed);
    if (!std::isfinite(f.distance) || !std::isfinite(length) || length < 0.001f ||
        !std::isfinite(player.speed) || !std::isfinite(rival.speed)) return "invalid-snapshot";
    f.ahead = (dx * rival.heading.x + dz * rival.heading.z) / length;
    f.lateral = std::abs(dx * rival.heading.z - dz * rival.heading.x) / length;
    if (output) *output = f;
    if (!player.pointer || !rival.pointer || player.pointer == rival.pointer ||
        player.driverClass != kDriverHuman || rival.driverClass != kDriverRacer) return "vehicle-identity";
    // Hard boundaries are not extended by grace: never challenge ahead/alongside,
    // beyond 60 m, across a large vertical gap, or from the opposite direction.
    if (f.distance > 60.0f) return "distance-over-60m";
    if (f.ahead < 1.0f) return "not-behind-1m";
    if (f.lateral > 20.0f) return "lateral-over-20m";
    if (std::abs(player.position.y - rival.position.y) > eligibility::HeightLimit(dx,dz)) return "height-over-slope-limit";
    const auto heading = HorizontalHeadingDot(player.heading, rival.heading);
    if (!std::isfinite(heading) || heading <= 0.0f) return "oncoming-or-invalid-heading";
    if (player.speed < 0 || rival.speed < 0) return "invalid-speed";
    // Hard gate, including grace and the fresh button-press sample.
    if (!roaming_pace::SpeedMatched(player.speed,rival.speed,g_settings.startSpeedToleranceKmh)) return "speed-delta-over-tolerance";
    if (holding) return nullptr;
    if (heading < eligibility::headingDotMinimum) return "heading-dot-under-0.25";
    // No minimum speed: braking, a brief stop, or an EMP recovery must not make
    // the input window vanish. Only reverse-facing geometry remains excluded.
    // Matching two stopped vehicles is allowed; no absolute minimum speed.
    return nullptr;
}

bool IsEncounterFollowing(const VehicleSnapshot& player, const VehicleSnapshot& rival,
                          EncounterFollow* output = nullptr) noexcept {
    return !EncounterFollowReason(player, rival, output);
}

bool CanHoldEncounterFollowing(const EncounterCandidate& candidate,
                              const VehicleSnapshot& player, const VehicleSnapshot& rival,
                              ULONGLONG now, EncounterFollow* output = nullptr) noexcept {
    return candidate.ready && candidate.player == player.pointer &&
        candidate.rival == rival.pointer && candidate.key == rival.vehicleKey &&
        now - candidate.sampleTick <= 250 && now - candidate.qualifiedTick <= kEncounterFollowGraceMs &&
        !EncounterFollowReason(player, rival, output, true);
}

void ResetEncounterSignal() noexcept {
    g_encounterCandidate = {};
    g_encounterCooldownUntil = 0;
    g_encounterHud = nullptr;
    g_encounterHudTick = 0;
    g_encounterNextFrameAttempt = 0;
    // Do not dereference cached HUD/widget pointers here. A surviving widget's
    // next native Update removes our prompt only if it has no native notification.
}

bool EncounterHudAvailable(void* hud) noexcept {
    if (!hud || !IsFreeRoam()) return false;
    auto* bytes = static_cast<std::uint8_t*>(hud);
    std::uint64_t features = 0;
    std::uint8_t pursuit = 1;
    void* frontend = nullptr;
    std::uint16_t menuFlags = 0xffff;
    return SafeRead(bytes + 0x18, &features) && features != 0 &&
        SafeRead(bytes + 0x2BC, &pursuit) && pursuit == 0 &&
        SafeRead(reinterpret_cast<void*>(Address(0x0091CB20)), &frontend) && frontend &&
        SafeRead(static_cast<std::uint8_t*>(frontend) + 0x1E, &menuFlags) && menuFlags == 0;
}

bool EncounterWidgetFree(void* widget) noexcept {
    if (!widget) return false;
    auto* bytes = static_cast<std::uint8_t*>(widget);
    std::uintptr_t vtable = 0;
    std::uint32_t gate = 1, event = 1;
    std::uint8_t nativeVisible = 1, smsRequest = 1;
    std::int32_t smsTimer = 1;
    return SafeRead(bytes, &vtable) && vtable == Address(kEncounterMenuVtable) &&
        SafeRead(bytes + 0x44, &gate) && gate == 0 &&
        SafeRead(bytes + 0x48, &event) && event == 0 &&
        SafeRead(bytes + 0x4D, &nativeVisible) && nativeVisible == 0 &&
        SafeRead(bytes + 0x4C, &smsRequest) && smsRequest == 0 &&
        SafeRead(bytes + 0x50, &smsTimer) && smsTimer == 0;
}

#include "EncounterPromptAssets.inl"
#include "EncounterHudFrame.inl"

#include "EncounterGeometry.inl"

template<class GeometryReader>
void UpdateEncounterCandidateUsing(const VehicleSnapshot& legacyPlayer,
                              const std::vector<VehicleSnapshot>& vehicles,
                              const float dt,GeometryReader readGeometry) noexcept {
    const auto now = GetTickCount64();
    if (!g_encounterEnabled || g_encounterFaulted || EncounterBattleBusy() ||
        now < g_encounterCooldownUntil || now - g_encounterHudTick > 250 ||
        !EncounterHudAvailable(g_encounterHud)) {
        g_encounterCandidate = {};
        return;
    }
    VehicleSnapshot player{};
    if(!readGeometry(legacyPlayer,player)) {g_encounterCandidate={};return;}
    const ManagedRacer* chosen = nullptr;
    bool chosenQualified = false;
    const char* lostReason = "missing-or-recycled-rival";
    EncounterFollow lostFollow{};
    float nearest = std::numeric_limits<float>::infinity();
    for (const auto& racer : g_racers) {
        if (racer.missingSeconds > 0 || !racer.simable) continue;
        const auto live = std::find_if(vehicles.begin(), vehicles.end(), [&](const auto& v) {
            return v.pointer == racer.pointer && v.vehicleKey == racer.vehicleKey;
        });
        EncounterFollow follow{};
        if (live == vehicles.end()) continue;
        VehicleSnapshot rival{};
        if(!readGeometry(*live,rival)) continue;
        const auto reason = EncounterFollowReason(player, rival, &follow);
        const bool same = racer.pointer == g_encounterCandidate.rival &&
            racer.vehicleKey == g_encounterCandidate.key && racer.simable == g_encounterCandidate.simable;
        if (same) { lostReason = reason ? reason : "candidate-reset"; lostFollow = follow; }
        const bool held = same && CanHoldEncounterFollowing(g_encounterCandidate, player, rival, now, &follow);
        if (reason && !held) continue;
        // Lock the existing eligible opponent to prevent flicker when two cars run together.
        if (same) { chosen = &racer; chosenQualified = !reason; break; }
        if (follow.distance < nearest) { chosen = &racer; chosenQualified = !reason; nearest = follow.distance; }
    }
    if (!chosen) {
        if (g_encounterCandidate.ready) Log(LogLevel::Info,
            "ENCOUNTER_READY lost reason=%s distance=%.2fm ahead=%.2fm lateral=%.2fm speedDelta=%.1fkmh lastQualifiedAgeMs=%llu",
            lostReason, lostFollow.distance, lostFollow.ahead, lostFollow.lateral, lostFollow.speedDelta * 3.6f,
            now - g_encounterCandidate.qualifiedTick);
        g_encounterCandidate = {};
        return;
    }
    auto& c = g_encounterCandidate;
    if (c.rival != chosen->pointer || c.key != chosen->vehicleKey ||
        c.simable != chosen->simable || c.player != player.pointer || now - c.sampleTick > 250) {
        c = {};
        c.player = player.pointer; c.rival = chosen->pointer;
        c.simable = chosen->simable; c.key = chosen->vehicleKey;
    }
    c.sampleTick = now;
    if (chosenQualified) {
        c.qualifiedTick = now;
        c.heldSeconds += std::clamp(dt, 0.0f, 0.10f);
    }
    if (c.ready && c.holding == chosenQualified)
        Log(LogLevel::Info, "ENCOUNTER_READY hold=%u graceMs=2000 lastQualifiedAgeMs=%llu",
            unsigned(!chosenQualified), now - c.qualifiedTick);
    c.holding = !chosenQualified;
    if (!c.ready && c.heldSeconds >= eligibility::dwellSeconds) {
        c.ready = true;
        Log(LogLevel::Info, "ENCOUNTER_READY rival=%p key=%08X held=%.2fs nativePrompt=MenuZoneTrigger mappedAction=46 raceStarted=0",
            c.rival, c.key, c.heldSeconds);
    }
}

void UpdateEncounterCandidate(const VehicleSnapshot& player,const std::vector<VehicleSnapshot>& vehicles,float dt) noexcept {
    UpdateEncounterCandidateUsing(player,vehicles,dt,[](const auto& in,auto& out){return ReadEncounterGeometry(in,out);});
}

bool EncounterActionIsEngage(const EncounterAction& a) noexcept {
    // The native HUD dispatch uses the action ID, not a physical key or mData threshold.
    return a.id == kEncounterEngageAction;
}

#include "EncounterMessage.inl"

bool EncounterResultHudAvailable(void* hud) noexcept {
    if(!hud||!IsFreeRoam()) return false;
    std::uint64_t features=0;void* frontend=nullptr;std::uint16_t flags=0xffff;
    return AudioRead(static_cast<unsigned char*>(hud)+0x18,&features)&&features!=0&&
        AudioRead(reinterpret_cast<void*>(Address(0x0091CB20)),&frontend)&&frontend&&
        AudioRead(static_cast<unsigned char*>(frontend)+0x1E,&flags)&&flags==0;
}
void* CurrentEncounterResultHud() noexcept {
    // Native IPlayer::GetHud (006F8F10) is mov eax,[ecx+28]; ret.
    void* list=nullptr;void* player=nullptr;void* hud=nullptr;unsigned count=0;
    if(!AudioRead(reinterpret_cast<void*>(Address(0x0092D884)),&count)||count==0||count>8||
       !AudioRead(reinterpret_cast<void*>(Address(0x0092D87C)),&list)||!list||
       !AudioRead(list,&player)||!player||!AudioRead(static_cast<unsigned char*>(player)+0x28,&hud))return nullptr;
    return EncounterResultHudAvailable(hud)?hud:nullptr;
}
bool ShowEncounterMessageUnsafe(void* hud,encounter_text::Id id,const wchar_t* content) noexcept {
    void* object = nullptr;
    if (!SafeRead(static_cast<std::uint8_t*>(hud) + 4, &object) || !object) return false;
    using FindFn = void*(__thiscall*)(void*, void*);
    // Actual target ends with RET 18h: six arguments. Verify ABI independently.
    using RequestFn = bool(__thiscall*)(void*, const char*, bool, std::uint32_t,
                                      std::uint32_t, std::uint32_t, std::uint32_t);
    void* message = reinterpret_cast<FindFn>(Address(0x005D59F0))(
        object, reinterpret_cast<void*>(Address(0x005650B0)));
    if(!message || !reinterpret_cast<RequestFn>(Address(0x00568030))(
        message, " ", false, 0x8AB83EDB, 0, 0, 2)) return false;
    // Native Request retains its timing/animation. Replace only that message's
    // FEString using the verified UTF-16 bString setter, bypassing ANSI loss.
    void* engine=nullptr;const char* packageName=nullptr;
    if(!AudioRead(reinterpret_cast<void*>(Address(0x0091CADC)),&engine)||!engine||
        !AudioRead(static_cast<unsigned char*>(message)-0x20,&packageName)||!packageName) return false;
    void* package=reinterpret_cast<void*(__thiscall*)(void*,const char*)>(Address(0x00516AF0))(engine,packageName);
    if(!package) return false;
    void* text=reinterpret_cast<void*(__thiscall*)(void*,unsigned)>(Address(0x005B8550))(package,0x32A7A521);
    std::size_t written=0;
    const auto setter=reinterpret_cast<void(__thiscall*)(void*,const wchar_t*)>(Address(0x005BCD20));
    if(!WriteEncounterMessageTree(text,content,setter,written)) {
        Log(LogLevel::Warning,"ENCOUNTER_MESSAGE rejected-layout id=%u noUnicodeWrites=1",unsigned(id));return false;
    }
    Log(LogLevel::Info,"ENCOUNTER_MESSAGE id=%u language=%s unicode=1 textLeaves=%u groupWrites=0",unsigned(id),g_settings.encounterEnglish?"en":"ja",unsigned(written));
    return true;
}

bool TryEncounterMessage(void* hud,encounter_text::Id id,const wchar_t* content) noexcept {
    static bool disabled=false;
    if(disabled||!hud) return false;
    __try {return ShowEncounterMessageUnsafe(hud,id,content);}
    __except(EXCEPTION_EXECUTE_HANDLER) {
        disabled=true;
        Log(LogLevel::Warning,"ENCOUNTER_MESSAGE exception=%08X notificationsDisabled=1 battleUnaffected=1",GetExceptionCode());
        return false;
    }
}

struct EncounterPendingMessage {
    encounter_text::Id id=encounter_text::Id::Start;
    wchar_t text[192]{};ULONGLONG expires=0,next=0;
};
EncounterPendingMessage g_pendingEncounterMessage{};
void FlushEncounterMessage() noexcept {
    auto& m=g_pendingEncounterMessage;const auto now=GetTickCount64();
    if(!m.expires)return;
    if(now>=m.expires||!IsFreeRoam()){m={};return;}
    if(now<m.next)return;m.next=now+100;
    if(auto* hud=CurrentEncounterResultHud();hud&&TryEncounterMessage(hud,m.id,m.text))m={};
}
bool ShowEncounterMessage(void* hud,encounter_text::Id id,unsigned reward=1000) noexcept {
    (void)hud; // Never dereference a cached HUD across pursuit/package transitions.
    auto& m=g_pendingEncounterMessage;m={};m.id=id;
    const auto content=encounter_text::Format(id,g_settings.encounterEnglish,reward);
    wcsncpy_s(m.text,content.c_str(),_TRUNCATE);m.expires=GetTickCount64()+5000;
    FlushEncounterMessage();return !m.expires;
}

void ObserveEncounterAction(void* hud, const EncounterAction& action) noexcept {
    if (!g_encounterEnabled || g_encounterFaulted || !EncounterActionIsEngage(action)) return;
    auto& c = g_encounterCandidate;
    const auto now = GetTickCount64();
    void* widget = nullptr;
    const char* rejected = !c.ready ? "not-ready" : now < g_encounterCooldownUntil ? "cooldown" :
        now - c.sampleTick > 250 ? "stale-sample" : !EncounterHudAvailable(hud) ? "hud-unavailable" :
        !SafeRead(static_cast<std::uint8_t*>(hud) + 0x328, &widget) ? "widget-unreadable" :
        !EncounterWidgetFree(widget) ? "native-notification" :
        widget != g_encounterOwnedWidget ? "prompt-not-owned" : nullptr;
    if (rejected) {
        Log(LogLevel::Info, "ENCOUNTER_INPUT mappedAction=46 accepted=0 ready=%u reason=%s", unsigned(c.ready), rejected);
        return;
    }
    const auto owner = std::find_if(g_racers.begin(), g_racers.end(), [&](const auto& r) {
        return r.pointer == c.rival && r.simable == c.simable && r.vehicleKey == c.key && r.missingSeconds == 0;
    });
    VehicleSnapshot player{}, rival{};
    EncounterFollow follow{};
      if (owner == g_racers.end() || !ReadEncounterVehicle(c.player, &player) ||
          !ReadEncounterVehicle(c.rival, &rival) || rival.vehicleKey != c.key) {
        Log(LogLevel::Info, "ENCOUNTER_INPUT mappedAction=46 accepted=0 reason=live-identity"); c = {}; return;
    }
    const auto followReason = EncounterFollowReason(player, rival, &follow);
    const bool grace = followReason && CanHoldEncounterFollowing(c, player, rival, now, &follow);
    if (followReason && !grace) {
        Log(LogLevel::Info, "ENCOUNTER_INPUT mappedAction=46 accepted=0 reason=%s distance=%.2fm ahead=%.2fm lateral=%.2fm speedDelta=%.1fkmh",
            followReason, follow.distance, follow.ahead, follow.lateral, follow.speedDelta * 3.6f);
        c = {}; return;
    }
    Log(LogLevel::Info, "ENCOUNTER_INPUT mappedAction=46 accepted=1 grace=%u", unsigned(grace));
    const bool queued = QueueEncounterBattle(player,rival);
    Log(LogLevel::Info, "ENCOUNTER_SIGNAL rival=%p key=%08X distance=%.1fm ahead=%.1fm lateral=%.1fm speedDelta=%.1fkmh queued=%u mappedAction=46 nativeWritesDeferred=1",
        c.rival, c.key, follow.distance, follow.ahead, follow.lateral,
        follow.speedDelta * 3.6f, unsigned(queued));
    c = {};
    g_encounterCooldownUntil = now + 8000;
}

void EncounterFault() noexcept {
    if (g_encounterFaulted) return;
    g_encounterFaulted = true;
    g_encounterCandidate = {};
    Log(LogLevel::Error, "ENCOUNTER exception; signal module disabled, native forwarding/population unchanged");
}

bool IsEncounterPeekContext(const std::uintptr_t caller, void* queue, void* context) noexcept {
    return caller == Address(kEncounterPeekReturn) && context &&
        queue == static_cast<std::uint8_t*>(context) + 0x30;
}

void __fastcall EncounterJoyHook(void* hud, void*, void* player) {
    // Native routing/population of mActionQ happens INSIDE this function.
    void* previous = g_encounterJoyContext;
    g_encounterJoyContext = hud;
    if (player) { g_encounterHud = hud; g_encounterHudTick = GetTickCount64(); }
    g_encounterOriginalJoy(hud, player);
    g_encounterJoyContext = previous;
}

void* __fastcall EncounterPeekHook(void* queue, void*, void** output) {
    const auto caller = reinterpret_cast<std::uintptr_t>(_ReturnAddress());
    void* result = g_encounterOriginalPeek(queue, output);
    if (IsEncounterPeekContext(caller, queue, g_encounterJoyContext)) {
        __try {
            void* item = nullptr;
            EncounterAction action{};
            if (SafeRead(output, &item) && SafeRead(item, &action))
                ObserveEncounterAction(g_encounterJoyContext, action);
        } __except (EXCEPTION_EXECUTE_HANDLER) { EncounterFault(); }
    }
    // Do not drain/rewrite the queue or steal real event/safehouse input.
    return result;
}

void __fastcall EncounterMenuHook(void* widget, void*, void* player) {
    // Let native SMS/event updates settle first, preserving the verified RET4 ABI.
    g_encounterOriginalMenu(widget, player);
    __try {
        const bool free = EncounterWidgetFree(widget);
        const bool owned = widget == g_encounterOwnedWidget;
        void* hudWidget = nullptr;
        const bool want = g_encounterEnabled && !g_encounterFaulted && g_encounterCandidate.ready &&
            GetTickCount64() - g_encounterCandidate.sampleTick <= 250 &&
            GetTickCount64() - g_encounterHudTick <= 250 && EncounterHudAvailable(g_encounterHud) &&
            SafeRead(static_cast<std::uint8_t*>(g_encounterHud) + 0x328, &hudWidget) && hudWidget == widget;
        if (!free) {
            if (owned) {
                std::uint32_t gate = 0, event = 0;
                EncounterUiRead(widget, 0x44, &gate); EncounterUiRead(widget, 0x48, &event);
                if (!gate && !event) SetEncounterPromptVisible(widget, false); // SMS takes over separately.
                else RestoreEncounterIcon(widget,EncounterNativeUiApi());
                g_encounterFrame = {};
                g_encounterOwnedWidget = nullptr; // Never hide an incoming real event.
            }
        } else if (want) {
            EncounterIconReady();
            if (!owned && (g_promptAssetState!=1||g_promptAssetSlow) && GetTickCount64() >= g_encounterNextFrameAttempt) {
                if (SetEncounterPromptVisible(widget, true)) {
                    g_encounterOwnedWidget = widget;
                    PlayEncounterPromptSound();
                    Log(LogLevel::Info, "ENCOUNTER_PROMPT show widget=%p nativeFrame=3345911D nodes=%u nativeEventNode=0A729B1B customIcon=%u eventHash=0 smsWrites=0", widget, g_encounterFrame.count,unsigned(g_encounterFrame.icon!=nullptr));
                } else {
                    g_encounterNextFrameAttempt = GetTickCount64() + 5000;
                    Log(LogLevel::Warning, "ENCOUNTER_PROMPT frame unavailable widget=%p reason=layout-or-visibility-guard ownership=not-acquired retryMs=5000", widget);
                }
            } else if (owned && (!EncounterFrameLive(widget, g_encounterFrame) || !SetEncounterFrameBits(g_encounterFrame, true))) {
                g_encounterFrame = {}; g_encounterOwnedWidget = nullptr;
            } else if (owned&&!g_encounterFrame.icon&&g_promptAssetState==2) {
                static ULONGLONG nextIconAttempt=0;
                const auto now=GetTickCount64();
                if(now>=nextIconAttempt) {
                    nextIconAttempt=now+2000;
                    if(UpgradeEncounterIconWithApi(widget,EncounterNativeUiApi()))
                        Log(LogLevel::Info,"ENCOUNTER_ICON lateAttach=1 ringAnimationRestart=0 extraSound=0");
                }
            }
        } else if (owned) {
            SetEncounterPromptVisible(widget, false);
            g_encounterOwnedWidget = nullptr;
            Log(LogLevel::Info, "ENCOUNTER_PROMPT hide ownedOnly=1");
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) { EncounterFault(); }
}

bool ValidateEncounterSurface() noexcept {
    struct Guard { std::uintptr_t address; const char* bytes; std::size_t size; };
    const Guard guards[] = {
        {kEncounterHudJoy, "\x64\xA1\x00\x00\x00\x00\x6A\xFF", 8},
        {kEncounterQueuePeek, "\x83\x39\x00\x7E\x13\x8B\x41\x08", 8},
        {0x0057C73D, "\xE8\x7E\x76\x0B\x00", 5},
        {kEncounterMenuUpdate, "\x51\x56\x8B\xF1\x8A\x46\x4C", 7},
        {0x0058CE39, "\x8B\x11\x55\xFF\x52\x04", 6},
        {0x0057BD09, "\x5F\x5E\x59\xC2\x04\x00", 6},
        {kEncounterMenuHide, "\x33\xC0\x88\x41\x1D\x89\x41\x14", 8},
        {kEncounterFindObject, "\x8B\x44\x24\x04", 4},
        {kEncounterSetAnimation, "\x57\x8B\x7C\x24\x08\x85\xFF", 7},
        {0x00514C78, "\x8B\x50\x1C\x8B\x48\x18\x81\xCA\x01\x00\x40\x02", 12},
        {0x00514CD0, "\x83\xE1\xFE\x81\xC9\x00\x00\x40\x02", 9},
        {0x005D59F0, "\x83\xEC\x08\x56\x8B\x71\x08", 7},
        {0x00568030, "\x56\x8B\xF1\x8B\x46\x14\x57", 7},
        {0x00568145, "\xC2\x18\x00", 3},
    };
    for (const auto& guard : guards)
        if (std::memcmp(reinterpret_cast<void*>(Address(guard.address)), guard.bytes, guard.size)) return false;
    return *reinterpret_cast<std::uintptr_t*>(Address(kEncounterMenuVtable + 4)) == Address(kEncounterMenuUpdate);
}

void InstallEncounterSignalHooks() noexcept {
    if (!g_settings.encounterSignalEnabled) return;
    if (!ValidateEncounterSurface()) {
        Log(LogLevel::Warning, "ENCOUNTER exact surface rejected; stable free roam retained");
        return;
    }
    const std::uintptr_t addresses[] = {kEncounterHudJoy, kEncounterQueuePeek, kEncounterMenuUpdate};
    void* replacements[] = {reinterpret_cast<void*>(&EncounterJoyHook), reinterpret_cast<void*>(&EncounterPeekHook), reinterpret_cast<void*>(&EncounterMenuHook)};
    void** originals[] = {reinterpret_cast<void**>(&g_encounterOriginalJoy), reinterpret_cast<void**>(&g_encounterOriginalPeek), reinterpret_cast<void**>(&g_encounterOriginalMenu)};
    bool success = true;
    for (unsigned i = 0; i < 3 && success; ++i)
        success = MH_CreateHook(reinterpret_cast<void*>(Address(addresses[i])), replacements[i], originals[i]) == MH_OK;
    for (const auto address : addresses)
        if (success) success = MH_QueueEnableHook(reinterpret_cast<void*>(Address(address))) == MH_OK;
    if (success) success = MH_ApplyQueued() == MH_OK;
    if (!success) {
        for (const auto address : addresses) {
            MH_DisableHook(reinterpret_cast<void*>(Address(address)));
            MH_RemoveHook(reinterpret_cast<void*>(Address(address)));
        }
    }
    g_encounterEnabled = success;
    Log(LogLevel::Info, "ENCOUNTER installed=%u phase=battle-prototype distance=1-60m lateralMax=20m heightMax=slope-scaled-5-12m headingDotMin=0.25 speedMin=none deltaMax=configured hardSpeedGrace=0 dwell=0.15s followGraceMs=2000 hardRangeGrace=0 cooldown=8s mappedAction=46 physicalKeyPolling=0 nativeEventWrites=0",
        unsigned(success));
}
