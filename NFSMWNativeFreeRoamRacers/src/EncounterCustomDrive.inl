// AIActionRace's final SetDriveTarget call (00428FEA -> return 00428FED).
// Replace its argument, not a steering/throttle input or global road data.
using CustomDriveVector=NFSPluginSDK::MW05::UMath::Vector3;
static_assert(offsetof(CustomDriveVector,y)==0&&offsetof(CustomDriveVector,z)==4&&offsetof(CustomDriveVector,x)==8);
using CustomDriveFn=void(__thiscall*)(void*,const CustomDriveVector*);
extern bool g_encounterPathInstalled;
CustomDriveFn g_originalCustomDrive=nullptr;
bool g_customDriveInstalled=false;
std::atomic<unsigned> g_customDriveCalls{0},g_customRoadQueries{0};
bool CustomDirectPhase() noexcept {
    return g_customDriveInstalled&&g_settings.encounterAIMode==encounter_custom::Mode::Custom&&
        g_encounterAdapterThread.load(std::memory_order_acquire)==GetCurrentThreadId()&&
        g_battle.model.phase()==battle::Phase::Active&&g_battle.model.leader()==battle::Leader::Player&&
        g_battle.customDirect&&!g_battle.pursuit.passing();
}
bool CustomDriveHint(void* ai,std::uintptr_t caller,ULONGLONG now,Vec3& aim) noexcept {
    if(caller!=Address(0x00428FED)||!CustomDirectPhase()||!g_battle.routeHint.valid||
        now<g_battle.customHintAt||now-g_battle.customHintAt>250) return false;
    EncounterNativeAI owner{};
    if(!ReadEncounterAI(g_battle.rival,owner)||static_cast<unsigned char*>(owner.primary)+0x4C!=ai) return false;
    const auto p=g_battle.routeHint.position;
    if(!battle::Finite(p)) return false;
    aim={p.x,p.y,p.z};return true;
}
void DispatchCustomDrive(void* ai,std::uintptr_t caller,const CustomDriveVector* native,ULONGLONG now) {
    Vec3 aim{};
    if(caller==Address(0x00428FED)&&W2CDirectDriveHint(ai,now,aim)) {
        if(caller==Address(0x00428FED)&&native){for(auto& row:g_w2cCommands)if(row.ai==ai&&row.valid){row.nativeAim={native->x,native->y,native->z};break;}}
        // The legacy W2C provider returns SDK-labelled YZX values.
        const auto encoded=ToUMath(aim);++g_customDriveCalls;g_originalCustomDrive(ai,&encoded);
    } else if(CustomDriveHint(ai,caller,now,aim)) {
        // EncounterGeometry and DirectionalTrail use physical XYZ. 00405310
        // copies raw +0/+4/+8; ToUMath would permute this to YZX a second time.
        CustomDriveVector encoded{};
        static_assert(sizeof(encoded)==sizeof(aim));
        std::memcpy(&encoded,&aim,sizeof(encoded));
        ++g_customDriveCalls;g_originalCustomDrive(ai,&encoded);
    } else g_originalCustomDrive(ai,native);
}
void __fastcall CustomDriveHook(void* ai,void*,const CustomDriveVector* native) {
    DispatchCustomDrive(ai,reinterpret_cast<std::uintptr_t>(_ReturnAddress()),native,GetTickCount64());
}
bool BlockCustomRoadQuery(void* nav,std::uintptr_t caller) noexcept {
    if(caller!=Address(0x00428BCF)&&caller!=Address(0x00428A76))return false;
    if(EventNeoRoadBlocked(nav)||W2CBlockRoadQuery(nav)){++g_customRoadQueries;return true;}
    if(!CustomDirectPhase()) return false;
    EncounterNativeAI owner{};
    if(!ReadEncounterAI(g_battle.rival,owner)||owner.nav!=nav) return false;
    ++g_customRoadQueries;return true;
}
bool ValidateCustomDriveSurface() noexcept {
    struct Guard {std::uintptr_t va;const char* bytes;unsigned size;};
    const Guard guards[]={
        {0x00405310,"\x8b\x44\x24\x04\x8b\x10\x83\xc1\x3c\x89\x11\x8b\x50\x04\x89\x51\x04\x8b\x40\x08\x89\x41\x08\xc2\x04\x00",26},
        {0x0089267C,"\x10\x53\x40\x00",4},
        {0x00428FE0,"\x8b\x4e\x40\x8b\x11\x8d\x44\x24\x54\x50\xff\x52\x3c",13},
        {0x00428A71,"\xe8\xba\x04\x36\x00",5}
    };
    for(const auto& guard:guards) if(std::memcmp(reinterpret_cast<void*>(Address(guard.va)),guard.bytes,guard.size)) return false;
    return true;
}
void InstallCustomDriveHook() noexcept {
    auto* address=reinterpret_cast<void*>(Address(0x00405310));
    if(g_battleSurface&&ValidateCustomDriveSurface()&&
        MH_CreateHook(address,&CustomDriveHook,reinterpret_cast<void**>(&g_originalCustomDrive))==MH_OK) {
        g_customDriveInstalled=MH_EnableHook(address)==MH_OK;
        if(!g_customDriveInstalled) MH_RemoveHook(address);
    }
    if(!g_customDriveInstalled) g_settings.encounterAIMode=encounter_custom::Mode::Stable;
    Log(g_customDriveInstalled?LogLevel::Info:LogLevel::Warning,
        "ENCOUNTER_CUSTOM driveHook=%u callerScoped=00428FED failedFallback=Stable",unsigned(g_customDriveInstalled));
    InstallW2CCurvatureHook();
}


