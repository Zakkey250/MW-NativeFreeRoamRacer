// Exact AIActionRace SetDriveSpeed call, after its final SetDriveTarget.
using CustomSpeedFn=void(__thiscall*)(void*,float);
CustomSpeedFn g_originalCustomSpeed=nullptr;
bool g_customSpeedInstalled=false;
std::atomic<unsigned> g_customSpeedCalls{0},g_customSpeedStops{0};
float g_customNativeSpeed=-1,g_customAppliedSpeed=-1;
bool CustomSpeedEligible(void* ai,ULONGLONG now) noexcept {
    Vec3 aim{};
    if(CustomDriveHint(ai,Address(0x00428FED),now,aim)) return true;
    // Attack frees steering to native passing, but keeps its speed advantage.
    if(!g_customDriveInstalled||g_settings.encounterAIMode!=encounter_custom::Mode::Custom||
        g_encounterAdapterThread.load(std::memory_order_acquire)!=GetCurrentThreadId()||
        g_battle.model.phase()!=battle::Phase::Active||g_battle.model.leader()!=battle::Leader::Player||
        !g_battle.pursuit.passing()||!g_battle.passActive||now<g_battle.customHintAt||now-g_battle.customHintAt>250) return false;
    EncounterNativeAI owner{};
    return ReadEncounterAI(g_battle.rival,owner)&&static_cast<unsigned char*>(owner.primary)+0x4C==ai;
}
void DispatchCustomSpeed(void* ai,std::uintptr_t caller,float native,ULONGLONG now) {
    float result=native;
    if(g_customSpeedInstalled&&caller==Address(0x00428FFA)&&
        CustomSpeedEligible(ai,now)) {
        result=encounter_custom::RaiseChaseSpeed(native,g_battle.customSpeedDemand);
        g_battle.customLastSpeedRequest=std::isfinite(result)&&result>=0?result:-1;
        g_customNativeSpeed=native;g_customAppliedSpeed=result;
        if(result>native) ++g_customSpeedCalls;
        if(std::isfinite(native)&&native<=.1f) ++g_customSpeedStops;
    }
    g_originalCustomSpeed(ai,result);
}
void __fastcall CustomSpeedHook(void* ai,void*,float native) {
    DispatchCustomSpeed(ai,reinterpret_cast<std::uintptr_t>(_ReturnAddress()),native,GetTickCount64());
}
bool ValidateCustomSpeedSurface() noexcept {
    struct Guard {std::uintptr_t va;const char* bytes;unsigned size;};
    const Guard guards[]={
        {0x00431C80,"\x8b\x44\x24\x04\x89\x41\x38\xc2\x04\x00",10},
        {0x00892678,"\x80\x1c\x43\x00",4},
        {0x00428FED,"\x8b\x4e\x40\x8b\x44\x24\x24\x8b\x11\x50\xff\x52\x38",13}
    };
    for(const auto& guard:guards)if(std::memcmp(reinterpret_cast<void*>(Address(guard.va)),guard.bytes,guard.size))return false;
    return true;
}
void InstallCustomSpeedHook() noexcept {
    if(g_settings.encounterAIMode!=encounter_custom::Mode::Custom||!g_customDriveInstalled)return;
    auto* address=reinterpret_cast<void*>(Address(0x00431C80));
    if(ValidateCustomSpeedSurface()&&MH_CreateHook(address,&CustomSpeedHook,reinterpret_cast<void**>(&g_originalCustomSpeed))==MH_OK) {
        g_customSpeedInstalled=MH_EnableHook(address)==MH_OK;
        if(!g_customSpeedInstalled)MH_RemoveHook(address);
    }
    Log(g_customSpeedInstalled?LogLevel::Info:LogLevel::Warning,
        "ENCOUNTER_CUSTOM speedHook=%u callerScoped=00428FFA nativeStopPreserved=1 failedFallback=alpha53-speed",unsigned(g_customSpeedInstalled));
}
