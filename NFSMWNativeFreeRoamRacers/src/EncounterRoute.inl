// Keep native AIActionRace's own path renewal on our committed endpoint while
// traversing the selected road. Never override another vehicle or caller.
// All other road queries (traffic, police, GPS, roaming racers) pass unchanged.
using EncounterPathFn=bool(__thiscall*)(void*,const Vec3*,const Vec3*,bool);
EncounterPathFn g_originalEncounterPath=nullptr;
std::atomic<unsigned> g_encounterNativeTrailRequests{0};
bool g_encounterPathInstalled=false;
bool EncounterPathHint(void* nav,std::uintptr_t caller,Vec3& position,Vec3& heading) noexcept {
    if(caller!=Address(0x00428BCF)||g_encounterAdapterThread.load(std::memory_order_acquire)!=GetCurrentThreadId()||
        g_battle.model.phase()!=battle::Phase::Active||
        g_battle.model.leader()!=battle::Leader::Player||!g_battle.routeHint.valid) return false;
    EncounterNativeAI ai{};
    if(!ReadEncounterAI(g_battle.rival,ai)||ai.nav!=nav) return false;
    auto hint=g_battle.routeHint;
    if(g_settings.encounterAIMode==encounter_custom::Mode::Custom) {
        position={hint.position.x,hint.position.y,hint.position.z};
        heading={hint.heading.x,hint.heading.y,hint.heading.z};return true;
    }
    unsigned mode=0;std::uint8_t valid=0,crossed=0;int edges=0;std::int16_t segment=-1;
    if(AudioRead(static_cast<unsigned char*>(nav)+0x78,&mode)&&
       AudioRead(static_cast<unsigned char*>(nav)+0x50,&valid)&&
       AudioRead(static_cast<unsigned char*>(nav)+0x2D8,&crossed)&&
       AudioRead(static_cast<unsigned char*>(nav)+0x2E0,&edges)&&
       AudioRead(static_cast<unsigned char*>(nav)+0x8E,&segment)&&
       encounter_route::Holding(encounter_route::CommitRoute(false,g_battle.routeAge>=4,
           g_battle.destinationSet,valid!=0,mode,crossed!=0,edges,g_battle.routeOriginSegment,
           segment,g_battle.pathSeconds,g_battle.routeProgress,g_battle.routeEndpointDistance))) {
        hint.position={g_battle.lastDestination.x,g_battle.lastDestination.y,g_battle.lastDestination.z};
        hint.heading=g_battle.lastDestinationHeading;
    }
    position={hint.position.x,hint.position.y,hint.position.z};
    heading={hint.heading.x,hint.heading.y,hint.heading.z};return true;
}
bool DispatchEncounterPath(void* nav,std::uintptr_t caller,const Vec3* destination,const Vec3* direction,bool flag) {
    if(BlockCustomRoadQuery(nav,caller)) return false;
    Vec3 position{},heading{};
    if(g_settings.encounterAIMode==encounter_custom::Mode::Custom&&!g_battle.routeHint.valid&&
        !g_battle.pursuit.passing()&&caller==Address(0x00428BCF)&&
        g_encounterAdapterThread.load(std::memory_order_acquire)==GetCurrentThreadId()&&
        g_battle.model.phase()==battle::Phase::Active&&g_battle.model.leader()==battle::Leader::Player) {
        EncounterNativeAI ai{};
        if(ReadEncounterAI(g_battle.rival,ai)&&ai.nav==nav) return false;
    }
    if(EncounterPathHint(nav,caller,position,heading)) {
        ++g_encounterNativeTrailRequests;
        return g_originalEncounterPath(nav,&position,
            g_settings.encounterAIMode==encounter_custom::Mode::Custom?&heading:nullptr,flag);
    }
    return g_originalEncounterPath(nav,destination,direction,flag);
}
bool __fastcall EncounterPathHook(void* nav,void*,const Vec3* destination,const Vec3* direction,bool flag) {
    return DispatchEncounterPath(nav,reinterpret_cast<std::uintptr_t>(_ReturnAddress()),destination,direction,flag);
}
void InstallEncounterPathHook() noexcept {
    if(!g_battleSurface||!g_settings.encounterSignalEnabled) return;
    auto* address=reinterpret_cast<void*>(Address(0x00788F30));
    if(MH_CreateHook(address,&EncounterPathHook,reinterpret_cast<void**>(&g_originalEncounterPath))==MH_OK) {
        g_encounterPathInstalled=MH_EnableHook(address)==MH_OK;
        if(!g_encounterPathInstalled) MH_RemoveHook(address);
    }
    Log(LogLevel::Info,"ENCOUNTER_ROUTE nativeRenewalHook=%u callerScoped=00428BCF directSteering=0",unsigned(g_encounterPathInstalled));
}
