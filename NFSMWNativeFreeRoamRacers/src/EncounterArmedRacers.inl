// Optional Arms Assist extension. EMP is targeted to a validated pursuing cop,
// not an area attack: the player is never submitted to the damage function.
struct EncounterArmedRacer {
    battle::Identity source{},target{};std::uintptr_t pursuit=0;
    encounter_weapons::EmpSchedule schedule{};
};
std::array<EncounterArmedRacer,15> g_armedRacers{};
float g_armedGlobalCooldown=0;bool g_armedFaulted=false;
void ResetEncounterArmedRacers() noexcept {g_armedRacers={};g_armedGlobalCooldown=0;}
bool EncounterEmpCone(const VehicleSnapshot& source,const VehicleSnapshot& target,float range) noexcept {
    const float distance=Distance(source.position,target.position);
    const auto& h=source.heading;
    const float length=std::sqrt(h.x*h.x+h.y*h.y+h.z*h.z);
    if(!std::isfinite(distance)||!std::isfinite(length)||distance<1||distance>range||length<.5f)return false;
    const float dot=((target.position.x-source.position.x)*h.x+(target.position.y-source.position.y)*h.y+
                     (target.position.z-source.position.z)*h.z)/(distance*length);
    return dot>=.5f;
}
bool EncounterArmedCop(const VehicleSnapshot& cop,const VehicleSnapshot& source,const VehicleSnapshot& player,
                       PatrolState& patrol) noexcept {
    int status=-1;
    return ReadPatrol(cop,patrol)&&patrol.inPursuit&&patrol.targetValid&&patrol.pursuit&&
        PoliceRead(patrol.pursuit,0x218,status)&&status==0&&
        (patrol.targetSimable==reinterpret_cast<std::uintptr_t>(GetSimablePointer(source.pointer))||
         patrol.targetSimable==reinterpret_cast<std::uintptr_t>(GetSimablePointer(player.pointer)));
}
void UpdateEncounterArmedRacersUnsafe(const VehicleSnapshot& player,const std::vector<VehicleSnapshot>& cars,float dt) {
    TryEncounterWeaponBridge();
    float range=0,delay=0;
    if(!g_armsReady||!g_armsEmp||g_armsReady(&range,&delay)!=3||!g_policeSurface||
       !std::isfinite(range)||range<=0||range>10000||!std::isfinite(delay)||delay<0||delay>30) {
        ResetEncounterArmedRacers();return;
    }
    if(!std::isfinite(dt)||dt<=0||dt>.5f||EncounterBattlePaused())return;
    // Stock and cooldown belong to a live managed vehicle, not a reusable slot.
    for(auto& state:g_armedRacers)if(state.source.vehicle) {
        const bool exists=std::any_of(g_racers.begin(),g_racers.end(),[&](const auto& r) {
            return state.source.vehicle==reinterpret_cast<std::uintptr_t>(r.pointer)&&
                state.source.simable==reinterpret_cast<std::uintptr_t>(r.simable)&&state.source.key==r.vehicleKey;
        });
        if(!exists)state={};
    }
    g_armedGlobalCooldown=std::max(0.f,g_armedGlobalCooldown-dt);
    for(const auto& owner:g_racers) {
        if(owner.missingSeconds>0||!owner.simable)continue;
        auto source=std::find_if(cars.begin(),cars.end(),[&](const auto& c){return c.pointer==owner.pointer&&c.vehicleKey==owner.vehicleKey&&c.driverClass==kDriverRacer;});
        if(source==cars.end())continue;
        const auto id=EncounterCar(*source).identity;
        auto state=std::find_if(g_armedRacers.begin(),g_armedRacers.end(),[&](const auto& s){return s.source==id;});
        if(state==g_armedRacers.end()) {
            state=std::find_if(g_armedRacers.begin(),g_armedRacers.end(),[](const auto& s){return !s.source.vehicle;});
            if(state==g_armedRacers.end())continue;
            *state={};state->source=id;
        }
        const VehicleSnapshot* target=nullptr;PatrolState patrol{};
        if(state->schedule.locked) {
            auto found=std::find_if(cars.begin(),cars.end(),[&](const auto& c){return reinterpret_cast<std::uintptr_t>(c.pointer)==state->target.vehicle&&c.vehicleKey==state->target.key;});
            if(found!=cars.end()&&EncounterCar(*found).identity==state->target&&EncounterArmedCop(*found,*source,player,patrol)&&patrol.pursuit==state->pursuit)target=&*found;
            state->schedule.Tick(dt);
            if(!target){state->schedule.Finish();state->target={};continue;}
            if(state->schedule.remaining>0)continue;
            bool hit=false;unsigned sh=0,th=0;
            if(EncounterEmpCone(*source,*target,range)&&
               PoliceRead(state->source.simable,8,sh)&&PoliceRead(state->target.simable,8,th))
                hit=g_armsEmp(state->source.vehicle,sh,state->target.vehicle,th);
            Log(LogLevel::Info,"ENCOUNTER_ARMED_EMP resolved source=%p cop=%p hit=%u stock=%u playerDamage=0",source->pointer,target->pointer,unsigned(hit),state->schedule.stock);
            state->schedule.Finish();state->target={};continue;
        }
        // No pursuit, no stock reset/cooldown advance into immediate firing.
        float nearest=range;std::uintptr_t activePursuit=0;
        for(const auto& cop:cars) {
            const float gap=Distance(source->position,cop.position);PatrolState candidate{};
            if(cop.driverClass!=2||gap>300||!EncounterArmedCop(cop,*source,player,candidate))continue;
            if(!activePursuit||candidate.pursuit==state->pursuit)activePursuit=candidate.pursuit;
            if(gap>=nearest||!EncounterEmpCone(*source,cop,range))continue;
            const auto copAddress=reinterpret_cast<std::uintptr_t>(cop.pointer);
            if(std::any_of(g_armedRacers.begin(),g_armedRacers.end(),[&](const auto& s){return s.schedule.locked&&s.target.vehicle==copAddress;}))continue;
            nearest=gap;target=&cop;patrol=candidate;
        }
        if(!activePursuit)continue;
        if(state->pursuit!=activePursuit){state->pursuit=activePursuit;state->schedule={};}
        state->schedule.Tick(dt);
        if(!target||patrol.pursuit!=state->pursuit)continue;
        if(g_armedGlobalCooldown>0||!state->schedule.Start(delay))continue;
        state->target=EncounterCar(*target).identity;g_armedGlobalCooldown=2;
        Log(LogLevel::Info,"ENCOUNTER_ARMED_EMP lock source=%p cop=%p delay=%.2f stock=%u pursuit=%p playerDamage=0",source->pointer,target->pointer,delay,state->schedule.stock,reinterpret_cast<void*>(patrol.pursuit));
    }
}
void UpdateEncounterArmedRacers(const VehicleSnapshot& player,const std::vector<VehicleSnapshot>& cars,float dt) noexcept {
    if(g_armedFaulted)return;
    __try {UpdateEncounterArmedRacersUnsafe(player,cars,dt);}
    __except(EXCEPTION_EXECUTE_HANDLER) {
        g_armedFaulted=true;ResetEncounterArmedRacers();
        Log(LogLevel::Warning,"ENCOUNTER_WEAPONS optional update fault=%08X; population/battle unaffected",GetExceptionCode());
    }
}
