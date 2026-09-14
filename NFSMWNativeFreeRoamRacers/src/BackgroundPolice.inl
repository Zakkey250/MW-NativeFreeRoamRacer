// Reuse the native patrol -> visible offender -> pursuit-request pipeline.
// Do not call StartPursuit directly: AICopManager owns the pursuit activity,
// target references, contingent, escalation and teardown. No new hook or cop.
constexpr std::uintptr_t kPoliceDetectOffender = 0x00429EF0;
struct PoliceSurfaceGuard { std::uintptr_t address; std::array<std::uint8_t,8> bytes; };
constexpr PoliceSurfaceGuard kPoliceGuards[] = {
    {0x00429EF0,{0x6A,0xFF,0x68,0xA8,0x81,0x86,0x00,0x64}},
    {0x00426D10,{0x56,0x57,0x8B,0xF9,0x8B,0x87,0xD4,0x00}},
    {0x00410E20,{0x8A,0x41,0x60,0x84,0xC0,0x8A,0x54,0x24}},
    {0x005CC240,{0x8B,0x44,0x24,0x04,0x85,0xC0,0x74,0x23}},
    {0x005D59F0,{0x83,0xEC,0x08,0x56,0x8B,0x71,0x08,0x8B}},
    {0x00431D70,{0x8B,0x41,0x70,0xC3,0xCC,0xCC,0xCC,0xCC}},
    {0x00431D20,{0x8B,0x41,0x54,0xC3,0xCC,0xCC,0xCC,0xCC}},
    {0x00891978,{0x70,0x31,0x43,0x00,0x00,0x66,0x40,0x00}},
    {0x00892560,{0x90,0x52,0x43,0x00,0x00,0x66,0x40,0x00}},
    {0x00892480,{0x10,0x35,0x43,0x00,0xF0,0x52,0x40,0x00}},
    {0x008923E8,{0x30,0x35,0x43,0x00,0x20,0x33,0x42,0x00}},
    {0x00891900,{0x70,0x0D,0x43,0x00,0x20,0x0E,0x41,0x00}},
};
bool g_policeSurface=false, g_policeFaulted=false;
std::uint32_t g_patrolGoalHash=0;
float g_policeTimer=0, g_policeLogTimer=0;
std::size_t g_policeCandidateCursor=0;
unsigned g_policeAttempts=0, g_policeRequests=0;

template<class T> bool PoliceRead(std::uintptr_t base,std::size_t offset,T& out) noexcept {
    return base && SafeRead(reinterpret_cast<const void*>(base+offset),&out);
}
bool PoliceTable(std::uintptr_t object,std::uintptr_t expected) noexcept {
    std::uintptr_t table=0;
    return PoliceRead(object,0,table)&&table==Address(expected);
}
struct PatrolState {
    std::uintptr_t primary=0, target=0, targetSimable=0, pursuit=0;
    std::uint32_t goal=0;
    std::uint8_t inPursuit=0, targetValid=0;
};
bool ReadPatrol(const VehicleSnapshot& cop,PatrolState& out) noexcept {
    out={};
    if(cop.driverClass!=2 || !IsExpectedVehicle(cop.pointer)) return false;
    std::uintptr_t ai=0;
    if(!PoliceRead(reinterpret_cast<std::uintptr_t>(cop.pointer),0x54,ai)||ai<0x4C||
       !PoliceTable(ai,0x00892480)) return false; // CopCar only, not heli/support subclasses.
    const auto primary=ai-0x4C;
    if(!PoliceTable(primary,0x00892560)||!PoliceTable(primary+0x758,0x008923E8)||
       !PoliceRead(primary,0xA0,out.target)||!out.target||
       !PoliceRead(primary,0xBC,out.pursuit)||!PoliceRead(primary,0xC4,out.goal)||
       !PoliceRead(primary,0x760,out.inPursuit)||
       !PoliceRead(out.target,0x2C,out.targetValid)||!PoliceRead(out.target,0x1C,out.targetSimable)) return false;
    out.primary=primary;
    return true;
}
bool PoliceIdle(const PatrolState& cop) noexcept {
    return g_patrolGoalHash && cop.goal==g_patrolGoalHash && !cop.inPursuit &&
        !cop.pursuit && !cop.targetValid && !cop.targetSimable;
}
bool PolicePairEligible(const VehicleSnapshot& player,const VehicleSnapshot& cop,
                        const VehicleSnapshot& rival,const ManagedRacer& managed) noexcept {
    // Positions are all the same legacy permutation: 3-D distance is invariant.
    return cop.driverClass==2 && rival.driverClass==kDriverRacer &&
        rival.pointer!=player.pointer && rival.pointer==managed.pointer &&
        rival.vehicleKey==managed.vehicleKey && managed.simable &&
        managed.missingSeconds==0 && managed.ageSeconds>=5 &&
        std::isfinite(rival.speed) && rival.speed>=30.0f/3.6f &&
        Distance(player.position,cop.position)<=350 &&
        Distance(player.position,rival.position)<=g_settings.populationRadiusMeters &&
        Distance(cop.position,rival.position)<=150;
}
bool PoliceManagerAvailable() noexcept {
    std::uintptr_t manager=0,pending=0;
    std::uint32_t pursuits=0;
    std::uint8_t blocked=0;
    float lockout=0;
    if(!PoliceRead(Address(0x0092C67C),0,manager)||manager<0x54||
       !PoliceTable(manager,0x00891900)||!PoliceTable(manager-0x54,0x00891978)||
       !PoliceRead(manager-0x54,0x12C,pursuits)||pursuits!=0||
       !PoliceRead(manager-0x54,0xC0,pending)||pending||
       !PoliceRead(manager-0x54,0xB4,blocked)||blocked||
       !PoliceRead(manager-0x54,0xAC,lockout)||!std::isfinite(lockout)||lockout>0) return false;
    // Respect native police lockouts/spawn policy and actual player-pursuit state.
    return reinterpret_cast<bool(__thiscall*)(void*,bool)>(Address(0x00410E20))(
               reinterpret_cast<void*>(manager),false) &&
        !reinterpret_cast<bool(__thiscall*)(void*)>(Address(0x00426D10))(reinterpret_cast<void*>(manager));
}
bool PoliceTryDetect(const VehicleSnapshot& cop,const VehicleSnapshot& rival,
                     const ManagedRacer& managed) noexcept {
    __try {
        PatrolState patrol{};
        VehicleSnapshot freshCop{},freshRival{};
        // Fresh identities immediately before the native call. Nothing is retained
        // for later writes, and no pointer is used after calling the game here.
        if(!ReadVehicle(cop.pointer,&freshCop)||!ReadVehicle(rival.pointer,&freshRival)||
           freshCop.driverClass!=2||freshCop.vehicleKey!=cop.vehicleKey||
           freshRival.driverClass!=kDriverRacer||freshRival.vehicleKey!=managed.vehicleKey||
           GetSimablePointer(rival.pointer)!=managed.simable||
           !ReadPatrol(freshCop,patrol)||!PoliceIdle(patrol)) return false;
        // Fail closed for racers without the native perpetrator component.
        std::uintptr_t owner=0;
        if(!PoliceRead(reinterpret_cast<std::uintptr_t>(rival.pointer),4,owner)||!owner||
           !reinterpret_cast<void*(__thiscall*)(void*,std::uintptr_t)>(Address(0x005D59F0))(
               reinterpret_cast<void*>(owner),Address(0x004037E0))) return false;
        ++g_policeAttempts;
        return reinterpret_cast<bool(__thiscall*)(void*,void*)>(Address(kPoliceDetectOffender))(
            reinterpret_cast<void*>(patrol.primary),rival.pointer);
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        g_policeFaulted=true;
        Log(LogLevel::Error,"BACKGROUND_POLICE native detection fault=%08X; this feature disabled until restart",GetExceptionCode());
        return false;
    }
}
void ResetBackgroundPolice() noexcept {
    // Native manager owns already-started pursuits. No cached-object teardown.
    g_policeTimer=g_policeLogTimer=0;
    g_policeCandidateCursor=0;
    g_policeAttempts=g_policeRequests=0;
}
void InitializeBackgroundPolice() noexcept {
    g_policeSurface=false;
    if(!g_settings.backgroundPoliceEnabled) return;
    for(const auto& guard:kPoliceGuards) {
        std::array<std::uint8_t,8> bytes{};
        if(!SafeRead(reinterpret_cast<void*>(Address(guard.address)),&bytes)||bytes!=guard.bytes) {
            Log(LogLevel::Warning,"BACKGROUND_POLICE guard failed=%08X; feature disabled",static_cast<unsigned>(guard.address));
            return;
        }
    }
    g_patrolGoalHash=reinterpret_cast<std::uint32_t(__cdecl*)(const char*)>(Address(0x005CC240))("AIGoalPatrol");
    g_policeSurface=g_patrolGoalHash!=0;
    Log(LogLevel::Info,"BACKGROUND_POLICE enabled=%u nativeDetection=00429EF0 maxNewPursuits=1 pairRadius=150m copPlayerRadius=350m interval=1s newHooks=0 extraCopSpawns=0",g_policeSurface?1u:0u);
}
void UpdateBackgroundPoliceImpl(const VehicleSnapshot& player,const std::vector<VehicleSnapshot>& vehicles,float dt) {
    if(!g_settings.backgroundPoliceEnabled||!g_policeSurface||g_policeFaulted) return;
    if(!std::isfinite(dt)||dt<=0) return;
    g_policeTimer=std::max(0.f,g_policeTimer-dt);
    g_policeLogTimer+=dt;
    if(g_policeLogTimer>=5) {
        unsigned cops=0,idle=0,managedChases=0;
        for(const auto& vehicle:vehicles) {
            PatrolState patrol{};
            if(!ReadPatrol(vehicle,patrol)) continue;
            ++cops;if(PoliceIdle(patrol)) ++idle;
            if(!patrol.inPursuit||!patrol.pursuit||!patrol.targetValid) continue;
            for(const auto& racer:g_racers) {
                auto* live=FindLive(vehicles,racer);
                if(live && racer.simable==reinterpret_cast<void*>(patrol.targetSimable)&&
                   GetSimablePointer(live->pointer)==racer.simable) {++managedChases;break;}
            }
        }
        Log(LogLevel::Info,"BACKGROUND_POLICE state cops=%u idle=%u managedChases=%u attempts=%u acceptedRequests=%u battle=%u",cops,idle,managedChases,g_policeAttempts,g_policeRequests,EncounterBattleBusy()?1u:0u);
        g_policeLogTimer=0;
    }
    if(g_policeTimer>0) return;
    g_policeTimer=1;
    if(EncounterBattleBusy()||!PoliceManagerAvailable()||g_racers.empty()) return;
    for(const auto& vehicle:vehicles) {
        PatrolState pending{};
        if(ReadPatrol(vehicle,pending) && (pending.inPursuit||pending.pursuit||pending.targetValid||pending.targetSimable))
            return; // Includes a detected request not yet registered by the manager.
    }
    const auto start=g_policeCandidateCursor++%g_racers.size();
    for(std::size_t i=0;i<g_racers.size();++i) {
        const auto& racer=g_racers[(start+i)%g_racers.size()];
        const auto* rival=FindLive(vehicles,racer);
        if(!rival) continue;
        for(const auto& cop:vehicles) {
            PatrolState patrol{};
            if(!PolicePairEligible(player,cop,*rival,racer)||!ReadPatrol(cop,patrol)||!PoliceIdle(patrol)) continue;
            if(PoliceTryDetect(cop,*rival,racer)) {
                ++g_policeRequests;g_policeTimer=10;
                Log(LogLevel::Info,"BACKGROUND_POLICE request accepted cop=%08X racer=%08X key=%08X distance=%.1fm; awaiting native manager",static_cast<unsigned>(reinterpret_cast<std::uintptr_t>(cop.pointer)),static_cast<unsigned>(reinterpret_cast<std::uintptr_t>(rival->pointer)),rival->vehicleKey,Distance(cop.position,rival->position));
            }
            return; // At most one native detection attempt per second, fleet-wide.
        }
    }
}
void UpdateBackgroundPolice(const VehicleSnapshot& player,const std::vector<VehicleSnapshot>& vehicles,float dt) noexcept {
    __try {UpdateBackgroundPoliceImpl(player,vehicles,dt);}
    __except(EXCEPTION_EXECUTE_HANDLER) {
        g_policeFaulted=true;
        Log(LogLevel::Error,"BACKGROUND_POLICE update fault=%08X; feature disabled until restart",GetExceptionCode());
    }
}
