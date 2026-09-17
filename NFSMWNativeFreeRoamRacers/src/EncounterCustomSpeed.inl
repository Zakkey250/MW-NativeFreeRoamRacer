// Exact AIActionRace SetDriveSpeed call, after its final SetDriveTarget.
using CustomSpeedFn=void(__thiscall*)(void*,float);
CustomSpeedFn g_originalCustomSpeed=nullptr;
bool g_customSpeedInstalled=false;
std::atomic<unsigned> g_customSpeedCalls{0},g_customSpeedStops{0};
float g_customNativeSpeed=-1,g_customAppliedSpeed=-1;
static_assert(offsetof(NFSPluginSDK::MW05::AIVehicle,mPursuit)==0xBC);
bool CruiseSpeedEligible(void* ai) noexcept {
    // The same verified SetDriveSpeed call serves both modes. Only this MOD's
    // currently retained racer generation is eligible, never traffic/cops/player.
    if(!g_settings.enabled||!g_streamRetentionEnabled.load(std::memory_order_acquire)||
       g_encounterAdapterThread.load(std::memory_order_acquire)!=GetCurrentThreadId()||
       g_settings.cruisingSpeedPercent>=100||!BattleVtable(ai,kRacecarIVehicleAIVtable)) return false;
    const auto address=reinterpret_cast<std::uintptr_t>(ai);
    if(address<0x4C) return false;
    void* vehicle=nullptr;void* backReference=nullptr;void* pursuit=nullptr;
    if(!AudioRead(reinterpret_cast<void*>(address-4),&vehicle)||!vehicle) return false;
    for(std::size_t i=0;i<kMaximumRacersLimit;++i) {
        if(g_protectedVehicles[i].load(std::memory_order_acquire)!=reinterpret_cast<std::uintptr_t>(vehicle)) continue;
        SpawnIdentity identity{};
        if(!CaptureSpawnIdentity(vehicle,&identity)||identity.key!=g_protectedKeys[i].load()||
           reinterpret_cast<std::uintptr_t>(identity.simable)!=g_protectedSimables[i].load()||
           !AudioRead(static_cast<unsigned char*>(vehicle)+0x54,&backReference)||backReference!=ai||
           !BattleVtable(reinterpret_cast<void*>(address-0x4C),kRacecarPrimaryVtable)||
           !AudioRead(reinterpret_cast<void*>(address-0x4C+0xBC),&pursuit)||pursuit) return false;
        if(EncounterBattleBusy()&&g_battle.rival.vehicle==reinterpret_cast<std::uintptr_t>(vehicle)&&
           g_battle.rival.simable==reinterpret_cast<std::uintptr_t>(identity.simable)&&g_battle.rival.key==identity.key) return false;
        return true;
    }
    return false;
}
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
    if(g_customSpeedInstalled&&caller==Address(0x00428FFA)&&CruiseSpeedEligible(ai)) {
        result=roaming_pace::CruiseSpeed(native,g_settings.cruisingSpeedPercent);
        static ULONGLONG nextLog=0;
        if(result!=native&&std::isfinite(result)&&now>=nextLog) {
            nextLog=now+5000;
            Log(LogLevel::Info,"ROAMING_PACE applied ai=%p native=%.1fkmh target=%.1fkmh percent=%.1f",ai,native*3.6f,result*3.6f,g_settings.cruisingSpeedPercent);
        }
    }
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
    // Cruising is independent of battle mode and SignalEnabled. Preserve the
    // existing Custom AI branch even when cruise reduction is disabled.
    if(!g_settings.enabled)return;
    auto* address=reinterpret_cast<void*>(Address(0x00431C80));
    if(ValidateCustomSpeedSurface()&&MH_CreateHook(address,&CustomSpeedHook,reinterpret_cast<void**>(&g_originalCustomSpeed))==MH_OK) {
        g_customSpeedInstalled=MH_EnableHook(address)==MH_OK;
        if(!g_customSpeedInstalled)MH_RemoveHook(address);
    }
    Log(g_customSpeedInstalled?LogLevel::Info:LogLevel::Warning,
        "ENCOUNTER_CUSTOM speedHook=%u callerScoped=00428FFA nativeStopPreserved=1 roamingPace=1 failedFallback=native-speed",unsigned(g_customSpeedInstalled));
}
