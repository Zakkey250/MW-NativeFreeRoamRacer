// alpha.18: mirror native pre-GRaceStatus event lookup at the allocation-count decision.
// Do not create/bind SFX objects from the display thread or change vehicle/race lists.
constexpr std::uintptr_t kAudioPoolCountSite=0x004F3030;
constexpr std::uintptr_t kAudioRacerPoolGlobal=0x00911F6C;
constexpr std::uintptr_t kAudioRacerPoolVtable=0x00898D24;
static_assert(offsetof(NFSPluginSDK::MW05::GRaceStatus,mRaceParms)==0x1968);
static_assert(offsetof(NFSPluginSDK::MW05::GRaceStatus,mPlayMode)==0x1960);
void* g_originalAudioPoolCount=nullptr;
bool g_audioPoolHookEnabled=false;
struct AudioPoolContext {
    unsigned valid=0;
    std::uintptr_t global=0,vt=0,head=0,raceStatus=0,raceParms=0,frontend=0;
    std::uint32_t type=0,kind=0,count=0,playMode=0,budget=0,countA=0,countB=0;
    std::uint8_t active=0,alternate=0;
};
AudioPoolContext ReadAudioPoolContext(std::uintptr_t pool) noexcept {
    AudioPoolContext c;
    if(AudioRead(Address(kAudioRacerPoolGlobal),0,&c.global)) c.valid|=1;
    if(AudioRead(pool,0,&c.vt)) c.valid|=2;
    if(AudioRead(pool,0x0C,&c.type)) c.valid|=4;
    if(AudioRead(pool,0x1C,&c.kind)) c.valid|=8;
    if(AudioRead(pool,0x10,&c.head)) c.valid|=16;
    if(AudioRead(pool,0x14,&c.count)) c.valid|=32;
    if(AudioRead(pool,0x18,&c.active)) c.valid|=64;
    if(AudioRead(Address(kRaceStatus),0,&c.raceStatus)) c.valid|=128;
    if(AudioRead(c.raceStatus,0x1968,&c.raceParms)) c.valid|=256;
    if(AudioRead(c.raceStatus,offsetof(NFSPluginSDK::MW05::GRaceStatus,mPlayMode),&c.playMode)) c.valid|=512;
    // 4E5C93..4E5CB5 uses [91E004]+2C when the race-status pointer is null.
    // 5DCA00 is just MOV EAX,[ECX+2C]; RET. Mirror the reads, do not call it.
    if((c.valid&128) && !c.raceStatus) {
        if(AudioRead(Address(0x0091E004),0,&c.frontend)) c.valid|=16384;
        if(AudioRead(c.frontend,0x2C,&c.raceParms)) c.valid|=32768;
    }
    if(AudioRead(Address(0x00911ED4),0,&c.budget)) c.valid|=1024;
    if(AudioRead(Address(0x0092CE44),0,&c.countA)) c.valid|=2048;
    if(AudioRead(Address(0x0092CFC4),0,&c.countB)) c.valid|=4096;
    if(AudioRead(Address(0x009142E7),0,&c.alternate)) c.valid|=8192;
    return c;
}
enum class AudioPoolReason : unsigned {Adjusted,Disabled,NativeNonzero,Unreadable,WrongPool,EventContext,NotEmpty,Budget,Counts};
int SelectAudioPoolCount(int nativeCount,std::uintptr_t pool,const AudioPoolContext& c,
                         bool enabled,unsigned requested,AudioPoolReason* reason) noexcept {
    *reason=AudioPoolReason::Disabled;
    if(!enabled || !requested) return nativeCount;
    *reason=AudioPoolReason::NativeNonzero;
    if(nativeCount!=0) return nativeCount;
    *reason=AudioPoolReason::Unreadable;
    if((c.valid&0x3CFF)!=0x3CFF) return nativeCount;
    if(c.raceStatus ? (c.valid&0x300)!=0x300 : (c.valid&0xC000)!=0xC000) return nativeCount;
    *reason=AudioPoolReason::WrongPool;
    if(!pool || c.global!=pool || c.vt!=Address(kAudioRacerPoolVtable) || c.type!=3 || c.kind!=1) return nativeCount;
    *reason=AudioPoolReason::EventContext;
    // Match 4E5C80's null race-parameters branch, not mRaceBin (which is +196C).
    // A present race status must be Roaming. Before it exists, require the native
    // alternate owner to be readable and report null parameters (not "assume Roaming").
    if(c.raceParms || (c.raceStatus ? c.playMode!=0 : !c.frontend)) return nativeCount;
    *reason=AudioPoolReason::NotEmpty;
    if(c.count || c.head) return nativeCount;
    *reason=AudioPoolReason::Budget;
    // 4E5C80's no-event branch reserves four native audio budget units; never alter it.
    if(c.budget<4 || c.budget>64 || c.alternate) return nativeCount;
    *reason=AudioPoolReason::Counts;
    if(c.countA || c.countB) return nativeCount;
    *reason=AudioPoolReason::Adjusted;
    return static_cast<int>(std::min(requested,4u));
}
std::atomic<unsigned> g_audioPoolAttempts{0},g_audioPoolAdjusted{0},g_audioPoolReason{0};
std::atomic<int> g_audioPoolNativeCount{0},g_audioPoolChosenCount{0};
std::atomic<std::uintptr_t> g_audioPoolLastManager{0};
std::atomic<unsigned> g_audioPoolLastValid{0};
std::atomic<std::uintptr_t> g_audioPoolLastRaceStatus{0},g_audioPoolLastFrontend{0},g_audioPoolLastParms{0};
int __cdecl ObserveAndSelectAudioPoolCount(int nativeCount,std::uintptr_t pool) noexcept {
    const auto c=ReadAudioPoolContext(pool);
    AudioPoolReason reason;
    const unsigned requested=std::min(g_settings.freeRoamAudioSlots,static_cast<unsigned>(g_settings.maximumRacers));
    const auto selected=SelectAudioPoolCount(nativeCount,pool,c,g_settings.enabled,requested,&reason);
    g_audioPoolAttempts.fetch_add(1,std::memory_order_relaxed);
    if(selected!=nativeCount) g_audioPoolAdjusted.fetch_add(1,std::memory_order_relaxed);
    g_audioPoolNativeCount.store(nativeCount,std::memory_order_relaxed);
    g_audioPoolChosenCount.store(selected,std::memory_order_relaxed);
    g_audioPoolLastManager.store(pool,std::memory_order_relaxed);
    g_audioPoolLastValid.store(c.valid,std::memory_order_relaxed);
    g_audioPoolLastRaceStatus.store(c.raceStatus,std::memory_order_relaxed);
    g_audioPoolLastFrontend.store(c.frontend,std::memory_order_relaxed);
    g_audioPoolLastParms.store(c.raceParms,std::memory_order_relaxed);
    g_audioPoolReason.store(static_cast<unsigned>(reason),std::memory_order_relaxed);
    return selected;
}
// EAX=count and EBP=manager at 4F3030. Restore all registers/flags except the selected EAX.
// The trampoline executes native TEST/JLE/PUSH ESI, including the untouched zero path.
__declspec(naked) void AudioPoolCountHook() {
    __asm {
        pushfd
        pushad
        push dword ptr [esp+8]
        push dword ptr [esp+32]
        call ObserveAndSelectAudioPoolCount
        add esp,8
        mov dword ptr [esp+28],eax
        popad
        popfd
        jmp dword ptr [g_originalAudioPoolCount]
    }
}
bool ValidateAudioPoolSurface() noexcept {
    const std::uint8_t entry[]={0x51,0xA1,0x44,0xCE,0x92,0,0x53,0x55,0x8B,0xE9,0x8B,0x0D,0xC4,0xCF,0x92,0,
        0x03,0xC1,0x83,0xF8,4,0xBB,0x91,0,0,0,0x7C,5,0xB8,4,0,0,0};
    const std::uint8_t site[]={0x85,0xC0,0x7E,0x45,0x56,0x89,0x44,0x24,0x0C};
    const std::uint8_t factory[]={0x53,0x6A,0,0x8B,0xCD,0xE8,0x26,0x29,0xFF,0xFF,0x8B,0xF8,0x8B,0x17,0x53,0x8B,0xCF,0xFF,0x52,4};
    const std::uint8_t budget[]={0x83,5,0xD4,0x1E,0x91,0,4,0x5B,0xC3};
    const std::uint8_t eventLookup[]={0xA1,0,0xE0,0x91,0,0xC1,0xEB,2,0x80,0xE3,1,0x85,0xC0,0x74,8,
        0x8B,0x80,0x68,0x19,0,0,0xEB,0x0B,0x8B,0x0D,4,0xE0,0x91,0,0xE8,0x4B,0x6D,0x0F,0,0x85,0xC0,0x75,9};
    const std::uint8_t eventGetter[]={0x8B,0x41,0x2C,0xC3};
    const std::uint8_t finish[]={0xC6,0x45,0x18,1,0x5D,0x5B,0x59,0xC2,4,0};
    const std::uint8_t empty[]={0x8B,0x41,0x10,0x85,0xC0,0x74,0x0E,0x8A,0x48,0x38,0x84,0xC9,0x74,9,0x8B,0x40,4,0x85,0xC0,0x75,0xF2,0x33,0xC0,0xC2,4,0};
    const std::uint8_t release[]={0xC7,0x41,0x34,0,0,0,0,0xE9,0x14,0xF9,0xFF,0xFF};
    std::uintptr_t init=0,acquire=0,update=0,bind=0,retire=0;
    return AudioCodeMatches(0x004F3000,entry) && AudioCodeMatches(kAudioPoolCountSite,site) &&
        AudioCodeMatches(0x004F3040,factory) && AudioCodeMatches(0x004E5CB9,budget) &&
        AudioCodeMatches(0x004E5C93,eventLookup) && AudioCodeMatches(0x005DCA00,eventGetter) &&
        AudioCodeMatches(0x004F3079,finish) && AudioCodeMatches(0x004D95E0,empty) && AudioCodeMatches(0x004F51D0,release) &&
        AudioRead(Address(kAudioRacerPoolVtable),8,&init) && init==Address(0x004F3000) &&
        AudioRead(Address(kAudioRacerPoolVtable),0x0C,&acquire) && acquire==Address(0x004D95E0) &&
        AudioRead(Address(kAudioRacerPoolVtable),0x10,&update) && update==Address(0x004EBC60) &&
        AudioRead(Address(0x00896FA8),0x0C,&bind) && bind==Address(0x004E4FD0) &&
        AudioRead(Address(0x00896FA8),0x1C,&retire) && retire==Address(0x004F51D0);
}
void InstallAudioPoolHook() noexcept {
    auto* site=reinterpret_cast<void*>(Address(kAudioPoolCountSite));
    if(ValidateAudioPoolSurface() && MH_CreateHook(site,&AudioPoolCountHook,&g_originalAudioPoolCount)==MH_OK) {
        g_audioPoolHookEnabled=MH_EnableHook(site)==MH_OK;
        if(!g_audioPoolHookEnabled) MH_RemoveHook(site);
    }
    Log(LogLevel::Info,"AUDIO_POOL_SUPPORT enabled=%u requestedSlots=%u countSite=004F3030 policy=empty-native-event-lookup-budgeted nativeMaximum=4 extraNativeCalls=0 raceListWrites=0",
        static_cast<unsigned>(g_audioPoolHookEnabled),g_settings.freeRoamAudioSlots);
}
struct AudioPoolMembership {int matched=-1;unsigned visited=0,active=0;const char* status="unknown";};
AudioPoolMembership ReadAudioPoolMembership(const AudioPoolContext& c,std::uintptr_t object) noexcept {
    AudioPoolMembership r;
    if((c.valid&62)!=62 || c.vt!=Address(kAudioRacerPoolVtable) || c.type!=3 || !object) return r;
    if(c.count>16) {r.status="limit";return r;}
    std::array<std::uintptr_t,16> seen{};
    auto node=c.head;
    int matched=0;
    while(node && r.visited<c.count) {
        for(unsigned i=0;i<r.visited;++i) if(seen[i]==node) {r.status="cycle";return r;}
        seen[r.visited++]=node;
        std::uintptr_t vt=0,next=0,bound=0;
        std::uint8_t active=0;
        if(!AudioRead(node,0,&vt) || vt!=Address(0x00896FA8) || !AudioRead(node,4,&next) ||
            !AudioRead(node,0x34,&bound) || !AudioRead(node,0x38,&active)) {r.status="unreadable-or-foreign";return r;}
        r.active+=active!=0;
        if(bound==object && active) matched=1;
        node=next;
    }
    if(node || r.visited!=c.count) {r.status="inconsistent";return r;}
    r.matched=matched;r.status=c.count?"enumerated":"empty";
    return r;
}
void LogAudioPool(const AudioSnapshot& s,const char* source,const char* phase,unsigned key) noexcept {
    std::uintptr_t pool=0;
    AudioRead(Address(kAudioRacerPoolGlobal),0,&pool);
    const auto c=ReadAudioPoolContext(pool);
    const auto membership=ReadAudioPoolMembership(c,s.object);
    Log(LogLevel::Info,"AUDIO_POOL phase=%s source=%s vehicle=%08X key=%08X object=%08X pool=%08X poolVT=%08X valid=%04X count=%u active=%u member=%d status=%s type=%u kind=%u ready=%u head=%08X raceParms=%08X playMode=%u budget=%u countA=%u countB=%u alternate=%u attempts=%u adjusted=%u lastNative=%d lastChosen=%d lastReason=%u lastManager=%08X lastValid=%04X lastRaceStatus=%08X lastFrontend=%08X lastParms=%08X",
        phase,source,static_cast<unsigned>(s.owner),key,static_cast<unsigned>(s.object),static_cast<unsigned>(pool),static_cast<unsigned>(c.vt),
        c.valid,c.count,membership.active,membership.matched,membership.status,c.type,c.kind,static_cast<unsigned>(c.active),
        static_cast<unsigned>(c.head),static_cast<unsigned>(c.raceParms),c.playMode,c.budget,c.countA,c.countB,static_cast<unsigned>(c.alternate),
        g_audioPoolAttempts.load(),g_audioPoolAdjusted.load(),g_audioPoolNativeCount.load(),g_audioPoolChosenCount.load(),g_audioPoolReason.load(),
        static_cast<unsigned>(g_audioPoolLastManager.load()),g_audioPoolLastValid.load(),
        static_cast<unsigned>(g_audioPoolLastRaceStatus.load()),static_cast<unsigned>(g_audioPoolLastFrontend.load()),
        static_cast<unsigned>(g_audioPoolLastParms.load()));
}
