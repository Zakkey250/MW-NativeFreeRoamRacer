// Custom distance policy at the existing POSITIVE engine-output catchup term.
// The stock race controller remains untouched. Other cheat consumers (NOS,
// handling, etc.) and all other cars receive their original return value.
using EncounterCheatFn=float(__thiscall*)(void*);
EncounterCheatFn g_originalEncounterCheat=nullptr;
std::atomic<float> g_encounterPowerScale{1};
std::atomic<ULONGLONG> g_encounterPowerRefresh{0};
std::atomic<unsigned> g_encounterPowerCalls{0};
bool g_encounterPowerInstalled=false;
float EncounterPowerTier(bool active,bool playerLeads,float gap) noexcept {
    if(!active||!std::isfinite(gap)||gap<0||gap>300) return 1;
    if(!playerLeads) return g_settings.encounterAIMode==encounter_custom::Mode::Custom?
        g_settings.customAILeaderPowerScale:1;
    return g_settings.encounterPowerScales[gap<=100?0:(gap<=200?1:2)];
}
float ScaleEncounterPowerTerm(float native,float scale) noexcept {
    if(!std::isfinite(native)||native<0||native>1||!std::isfinite(scale)||scale<=1||scale>2.0f) return native;
    // 006B0016: output *= (1 + 0.5 * GetCatchupCheat()).
    // Multiply that output by scale, rather than treating scale as a cheat value.
    return (native+2.0f)*scale-2.0f;
}
void ClearEncounterPowerBoost() noexcept {
    g_encounterPowerScale.store(1,std::memory_order_release);
    g_encounterPowerRefresh.store(0,std::memory_order_release);
}
void UpdateEncounterPowerBoost(bool active,bool playerLeads,float gap) noexcept {
    const float scale=g_encounterPowerInstalled?EncounterPowerTier(active,playerLeads,gap):1;
    g_encounterPowerScale.store(scale,std::memory_order_release);
    g_encounterPowerRefresh.store(GetTickCount64(),std::memory_order_release);
}
float ApplyEncounterPowerBoost(void* cheater,std::uintptr_t caller,float native,ULONGLONG now) noexcept {
    if(caller!=Address(0x006B0016)) return native;
    if(!std::isfinite(native)||native<0||native>1) return native;
    const auto refreshed=g_encounterPowerRefresh.load(std::memory_order_acquire);
    const float scale=g_encounterPowerScale.load(std::memory_order_acquire);
    if(scale<=1||!refreshed||now<refreshed||now-refreshed>250) return native;
    const auto object=reinterpret_cast<std::uintptr_t>(cheater);
    if(object<0x720||!BattleVtable(cheater,0x008925C4)||
        !IsEncounterSkillOwner(reinterpret_cast<void*>(object-0x720))) return native;
    const float result=ScaleEncounterPowerTerm(native,scale);
    if(result!=native) ++g_encounterPowerCalls;
    return result;
}
float __fastcall EncounterPowerHook(void* cheater,void*) {
    const auto caller=reinterpret_cast<std::uintptr_t>(_ReturnAddress());
    const float native=g_originalEncounterCheat(cheater);
    return ApplyEncounterPowerBoost(cheater,caller,native,GetTickCount64());
}
void InstallEncounterPowerBoost() noexcept {
    if(!g_battleSurface||!g_settings.encounterSignalEnabled||!g_encounterSkillInstalled) return;
    struct Guard {std::uintptr_t va;const char* bytes;unsigned size;};
    const Guard guards[]={
        {0x00409390,"\x51\xd9\x41\x40\xd8\x41\x3c",7},
        {0x008925C8,"\x90\x93\x40\x00",4},
        {0x006B0005,"\x75\x23\x8b\x8e\x18\x01\x00\x00",8},
        {0x006B0011,"\x8b\x01\xff\x50\x04",5},
        {0x006B0016,"\xd8\x0d\xd4\x33\x89\x00\xd8\x05\x6c\x09\x89\x00\xd8\x4c\x24\x1c\xd9\x5c\x24\x1c",20},
        {0x008933D4,"\x00\x00\x00\x3f",4},
        {0x0089096C,"\x00\x00\x80\x3f",4},
        {0x006B3945,"\xc7\x46\x54\xe0\xb6\x8a\x00",7}
    };
    for(const auto& guard:guards) if(std::memcmp(reinterpret_cast<void*>(Address(guard.va)),guard.bytes,guard.size)) {
        Log(LogLevel::Warning,"ENCOUNTER_POWER surfaceRejected=%08X noBoost=1",unsigned(guard.va));return;
    }
    auto* target=reinterpret_cast<void*>(Address(0x00409390));
    if(MH_CreateHook(target,&EncounterPowerHook,reinterpret_cast<void**>(&g_originalEncounterCheat))==MH_OK) {
        g_encounterPowerInstalled=MH_EnableHook(target)==MH_OK;
        if(!g_encounterPowerInstalled) MH_RemoveHook(target);
    }
    Log(LogLevel::Info,"ENCOUNTER_POWER installed=%u policy=custom-distance positiveOutputOnly=1 tiers=%.3f/%.3f/%.3f nativeRaceControllerUnchanged=1",unsigned(g_encounterPowerInstalled),
        g_settings.encounterPowerScales[0],g_settings.encounterPowerScales[1],g_settings.encounterPowerScales[2]);
}
void LogEncounterPowerBoost() noexcept {
    Log(LogLevel::Info,"ENCOUNTER_POWER scale=%.3f appliedCalls=%u nativeCaller=006B0016 outputOnly=1",
        g_encounterPowerScale.load(),g_encounterPowerCalls.exchange(0));
}
