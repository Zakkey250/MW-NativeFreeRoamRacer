// alpha.16: SFXCTL_Engine factory, parent binding and initialization observers.
// Descriptor 8EE6E8 names SFXCTL_Engine and points to the cdecl factory (one stack argument).
constexpr std::uintptr_t kAudioLifeFunctions[] = {0x004C7FA0,0x004B3CD0,0x004B3CF0};
using AudioLifeFactoryFn = std::uintptr_t(__cdecl*)(std::uintptr_t);
using AudioLifeBindFn = std::uintptr_t(__thiscall*)(void*,std::uintptr_t);
using AudioLifeInitFn = std::uintptr_t(__thiscall*)(void*);
AudioLifeFactoryFn g_originalAudioLifeFactory = nullptr;
AudioLifeBindFn g_originalAudioLifeBind = nullptr;
AudioLifeInitFn g_originalAudioLifeInit = nullptr;
bool g_audioLifeEnabled[3]{};
std::atomic<unsigned> g_audioLifeCalls[3]{},g_audioLifeReturns[3]{},g_audioLifeDrops{0},g_audioLifeUnknown{0};
SRWLOCK g_audioLifeLock = SRWLOCK_INIT;
struct AudioLifeSnapshot {
    AudioSourceIdentity id{};
    unsigned valid = 0;
    std::uintptr_t parentVT=0,data=0,context=0,helper=0;
    std::uint32_t parentClass=0,mode=0;
};
struct AudioLifeRecord {
    AudioLifeSnapshot snapshot{};
    unsigned generation=0,counts[3]{},lastStage=0;
    std::uintptr_t factoryArg=0,bindArg=0;
    ULONGLONG lastEvent=0;
};
std::array<AudioLifeRecord,128> g_audioLifeRecords{};
unsigned g_audioLifeGeneration=0,g_audioLifeEvictions=0;

bool ReadAudioLife(std::uintptr_t source,AudioLifeSnapshot* output) noexcept {
    AudioLifeSnapshot s; s.id.source=source;
    if (!AudioRead(source,0,&s.id.vt) || !AudioSourceType(0,s.id.vt)) return false;
    if (AudioRead(source,0x14,&s.id.parent)) s.valid|=1;
    if (AudioRead(source,0x18,&s.id.state)) s.valid|=2;
    if (AudioRead(s.id.parent,0x34,&s.id.object)) s.valid|=4;
    if (AudioRead(s.id.parent,0,&s.parentVT)) s.valid|=8;
    if (AudioRead(s.id.parent,0x14,&s.parentClass)) s.valid|=16;
    if (AudioRead(source,0x0C,&s.data)) s.valid|=32;
    if (AudioRead(source,0x24,&s.mode)) s.valid|=64;
    if (AudioRead(source,0x28,&s.context)) s.valid|=128;
    if (AudioRead(source,0x34,&s.helper)) s.valid|=256;
    *output=s;
    return true; // Unbound/partially initialized is useful, not an error.
}
void StoreAudioLife(unsigned stage,const AudioLifeSnapshot& s,std::uintptr_t arg,ULONGLONG now) noexcept {
    if(stage>=3) return;
    auto slot=g_audioLifeRecords.size();
    for(std::size_t i=0;i<g_audioLifeRecords.size();++i)
        if(g_audioLifeRecords[i].generation && g_audioLifeRecords[i].snapshot.id.source==s.id.source) {slot=i;break;}
    const bool found=slot<g_audioLifeRecords.size();
    if(!found) {
        slot=0;
        for(std::size_t i=0;i<g_audioLifeRecords.size();++i) {
            if(!g_audioLifeRecords[i].generation) {slot=i;break;}
            if(g_audioLifeRecords[i].lastEvent<g_audioLifeRecords[slot].lastEvent) slot=i;
        }
        if(g_audioLifeRecords[slot].generation) ++g_audioLifeEvictions;
    }
    auto& r=g_audioLifeRecords[slot];
    // A factory result starts a new observed lifetime, even at a recycled address.
    if(!found || stage==0 || (stage==1 && r.snapshot.id.parent && r.snapshot.id.parent!=s.id.parent)) {
        r={};
        if(++g_audioLifeGeneration==0) ++g_audioLifeGeneration;
        r.generation=g_audioLifeGeneration;
    }
    r.snapshot=s; ++r.counts[stage]; r.lastStage=stage; r.lastEvent=now;
    if(stage==0) r.factoryArg=arg;
    if(stage==1) r.bindArg=arg;
}
void ObserveAudioLife(unsigned stage,std::uintptr_t source,std::uintptr_t arg) noexcept {
    AudioLifeSnapshot s;
    if(!ReadAudioLife(source,&s)) {g_audioLifeUnknown.fetch_add(1,std::memory_order_relaxed);return;}
    if(TryAcquireSRWLockExclusive(&g_audioLifeLock)) {
        StoreAudioLife(stage,s,arg,GetTickCount64());
        ReleaseSRWLockExclusive(&g_audioLifeLock);
    } else g_audioLifeDrops.fetch_add(1,std::memory_order_relaxed);
}
std::uintptr_t __cdecl AudioLifeFactoryHook(std::uintptr_t arg) {
    g_audioLifeCalls[0].fetch_add(1,std::memory_order_relaxed);
    const auto result=g_originalAudioLifeFactory(arg);
    g_audioLifeReturns[0].fetch_add(1,std::memory_order_relaxed);
    ObserveAudioLife(0,result,arg);
    return result;
}
std::uintptr_t __fastcall AudioLifeBindHook(void* source,void*,std::uintptr_t arg) {
    g_audioLifeCalls[1].fetch_add(1,std::memory_order_relaxed);
    const auto result=g_originalAudioLifeBind(source,arg);
    g_audioLifeReturns[1].fetch_add(1,std::memory_order_relaxed);
    ObserveAudioLife(1,reinterpret_cast<std::uintptr_t>(source),arg);
    return result;
}
std::uintptr_t __fastcall AudioLifeInitHook(void* source,void*) {
    g_audioLifeCalls[2].fetch_add(1,std::memory_order_relaxed);
    const auto result=g_originalAudioLifeInit(source);
    g_audioLifeReturns[2].fetch_add(1,std::memory_order_relaxed);
    ObserveAudioLife(2,reinterpret_cast<std::uintptr_t>(source),0);
    return result;
}
bool ValidateAudioLifeSurface() noexcept {
    const std::uint8_t factory[]={0x6A,0xFF,0x68,0xA6,0xBE,0x86,0,0x64,0xA1,0,0,0,0};
    const std::uint8_t factoryArg[]={0x8B,0x44,0x24,0x14,0x8B,0x0D,0x58,0xE5,0x8E,0};
    const std::uint8_t ctor[]={0xE8,0xED,0xD4,0xFF,0xFF};
    const std::uint8_t exitFactory[]={0x83,0xC4,0x10,0xC3};
    const std::uint8_t bind[]={0x8B,0x44,0x24,4,0x56,0x50,0x8B,0xF1,0xE8,0x93,0x25,2,0,
        0x8B,0x4E,0x18,0x8B,0x11,0xFF,0x52,0x40,0x89,0x46,0x24,0x5E,0xC2,4,0};
    const std::uint8_t baseBind[]={0x89,0x51,0x18,0x89,0x51,0x14,0xC2,4,0};
    const std::uint8_t init[]={0x53,0x56,0x8B,0xF1,0x33,0xDB,0x53,0x8D,0x4E,0x50};
    const std::uint8_t exitInit[]={0x5E,0x5B,0xC3};
    std::uintptr_t factoryPtr=0,bindPtr=0,initPtr=0;
    return AudioCodeMatches(kAudioLifeFunctions[0],factory) && AudioCodeMatches(0x004C7FB6,factoryArg) &&
        AudioCodeMatches(0x004C808E,ctor) && AudioCodeMatches(0x004C809E,exitFactory) &&
        AudioCodeMatches(0x004C80AF,exitFactory) && AudioCodeMatches(kAudioLifeFunctions[1],bind) &&
        AudioCodeMatches(0x004D629D,baseBind) && AudioCodeMatches(kAudioLifeFunctions[2],init) &&
        AudioCodeMatches(0x004B3DF4,exitInit) && AudioCodeMatches(0x004B3E0A,exitInit) &&
        AudioRead(Address(0x008EE6E8),0x0C,&factoryPtr) && factoryPtr==Address(kAudioLifeFunctions[0]) &&
        AudioRead(Address(0x00896BF0),0x14,&bindPtr) && bindPtr==Address(kAudioLifeFunctions[1]) &&
        AudioRead(Address(0x00896BF0),0x1C,&initPtr) && initPtr==Address(kAudioLifeFunctions[2]);
}
void InstallAudioLifeHooks() noexcept {
    // Validate before any entry is patched; each diagnostic fails independently.
    const bool valid=g_audioSourceEnabled[0] && ValidateAudioLifeSurface();
    void* hooks[]={reinterpret_cast<void*>(&AudioLifeFactoryHook),reinterpret_cast<void*>(&AudioLifeBindHook),reinterpret_cast<void*>(&AudioLifeInitHook)};
    void** originals[]={reinterpret_cast<void**>(&g_originalAudioLifeFactory),reinterpret_cast<void**>(&g_originalAudioLifeBind),reinterpret_cast<void**>(&g_originalAudioLifeInit)};
    for(unsigned stage=0;stage<3;++stage) {
        auto* target=reinterpret_cast<void*>(Address(kAudioLifeFunctions[stage]));
        if(valid && MH_CreateHook(target,hooks[stage],originals[stage])==MH_OK) {
            g_audioLifeEnabled[stage]=MH_EnableHook(target)==MH_OK;
            if(!g_audioLifeEnabled[stage]) MH_RemoveHook(target);
        }
        Log(LogLevel::Info,"AUDIO_LIFECYCLE_DIAGNOSTICS stage=%u enabled=%u hook=%08X type=SFXCTL_Engine capacity=128 gameStateWrites=0 extraNativeCalls=0",
            stage,static_cast<unsigned>(g_audioLifeEnabled[stage]),static_cast<unsigned>(kAudioLifeFunctions[stage]));
    }
}
struct AudioLifeMatch {AudioLifeRecord record{}; const char* status="not-observed"; unsigned evictions=0;};
AudioLifeMatch FindAudioLife(std::uintptr_t object) noexcept {
    AudioLifeMatch result;
    if(!object) {result.status="no-object";return result;}
    std::array<AudioLifeRecord,128> records;
    if(!TryAcquireSRWLockShared(&g_audioLifeLock)) {result.status="busy";return result;}
    records=g_audioLifeRecords; result.evictions=g_audioLifeEvictions;
    ReleaseSRWLockShared(&g_audioLifeLock);
    for(const auto& r:records) {
        if(!r.generation || !r.snapshot.id.parent) continue;
        const auto& id=r.snapshot.id;
        if(id.object && id.object!=object) continue;
        AudioSourceIdentity live;
        const bool linked=ReadAudioSourceIdentity(0,id.source,&live) && live.parent==id.parent &&
            live.state==id.state && live.object==object;
        if((id.object==object || linked) && (!result.record.generation || r.lastEvent>=result.record.lastEvent)) {
            result.record=r;
            result.status=linked?"live-link":"historical-link";
        }
    }
    return result;
}
void LogAudioLife(const AudioSnapshot& s,const char* source,const char* phase,unsigned key) noexcept {
    if(!g_audioLifeEnabled[0] && !g_audioLifeEnabled[1] && !g_audioLifeEnabled[2]) return;
    const auto match=FindAudioLife(s.object);
    const auto& r=match.record; const auto& v=r.snapshot; const auto& id=v.id;
    Log(LogLevel::Info,"AUDIO_LIFECYCLE phase=%s source=%s vehicle=%08X key=%08X object=%08X status=%s producer=%08X parent=%08X state=%08X eventObject=%08X generation=%u created=%u bound=%u initialized=%u lastStage=%u ageMs=%llu valid=%03X parentVT=%08X parentClass=%08X data0C=%08X mode24=%08X context28=%08X helper34=%08X factoryArg=%08X bindArg=%08X totalCalls=%u/%u/%u totalReturns=%u/%u/%u lockDrops=%u unknown=%u evictions=%u",
        phase,source,static_cast<unsigned>(s.owner),key,static_cast<unsigned>(s.object),match.status,
        static_cast<unsigned>(id.source),static_cast<unsigned>(id.parent),static_cast<unsigned>(id.state),static_cast<unsigned>(id.object),
        r.generation,r.counts[0],r.counts[1],r.counts[2],r.lastStage,r.generation?GetTickCount64()-r.lastEvent:0ULL,v.valid,
        static_cast<unsigned>(v.parentVT),v.parentClass,static_cast<unsigned>(v.data),v.mode,static_cast<unsigned>(v.context),static_cast<unsigned>(v.helper),
        static_cast<unsigned>(r.factoryArg),static_cast<unsigned>(r.bindArg),
        g_audioLifeCalls[0].load(),g_audioLifeCalls[1].load(),g_audioLifeCalls[2].load(),
        g_audioLifeReturns[0].load(),g_audioLifeReturns[1].load(),g_audioLifeReturns[2].load(),
        g_audioLifeDrops.load(),g_audioLifeUnknown.load(),match.evictions);
}
