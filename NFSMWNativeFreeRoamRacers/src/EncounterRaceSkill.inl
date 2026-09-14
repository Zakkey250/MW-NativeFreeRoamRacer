// Native normalized race skill, scoped to the active opponent. GetSkill already
// clamps to 0..1 (00409340). This keeps native cornering, braking and recovery;
// it does not force throttle, speed, position or a fake GRacerInfo/event.
using EncounterSkillFn=float(__thiscall*)(void*);
static_assert(offsetof(NFSPluginSDK::MW05::VehicleBehavior,mVehicle)==0x48);
static_assert(offsetof(NFSPluginSDK::MW05::Behavior,mIOwner)==0x34);
EncounterSkillFn g_originalEncounterSkill=nullptr;
std::atomic<std::uintptr_t> g_encounterSkillAI{0},g_encounterSkillVehicle{0},g_encounterSkillSimable{0};
std::atomic<unsigned> g_encounterSkillKey{0},g_encounterSkillCalls{0};
bool g_encounterSkillInstalled=false;

void ClearEncounterRaceSkill() noexcept {g_encounterSkillAI.store(0,std::memory_order_release);}
void BindEncounterRaceSkill(const battle::Identity& rival) noexcept {
    ClearEncounterRaceSkill();
    EncounterNativeAI ai{};
    if(!g_encounterSkillInstalled||!ReadEncounterAI(rival,ai,false)) return;
    g_encounterSkillVehicle=rival.vehicle;g_encounterSkillSimable=rival.simable;g_encounterSkillKey=rival.key;
    g_encounterSkillCalls=0;
    g_encounterSkillAI.store(reinterpret_cast<std::uintptr_t>(ai.primary)+0x4C,std::memory_order_release);
    Log(LogLevel::Info,"ENCOUNTER_AI racePace=1 nativeSkill=1.0 rival=%p directSteering=0 nativeSpeedLimits=1",reinterpret_cast<void*>(rival.vehicle));
}
bool IsEncounterSkillOwner(void* ai) noexcept {
    const auto address=reinterpret_cast<std::uintptr_t>(ai);
    if(!address||g_encounterSkillAI.load(std::memory_order_acquire)!=address) return false;
    // VehicleBehavior::mVehicle is primary+48; Behavior::mIOwner primary+34.
    std::uintptr_t vehicle=0,simable=0,backReference=0;
    if(!BattleVtable(ai,kRacecarIVehicleAIVtable)||
        !AudioRead(reinterpret_cast<void*>(address-4),&vehicle)||vehicle!=g_encounterSkillVehicle||
        !AudioRead(reinterpret_cast<void*>(address-0x18),&simable)||simable!=g_encounterSkillSimable||
        !AudioRead(reinterpret_cast<void*>(vehicle+0x54),&backReference)||backReference!=address||
        !IsExpectedVehicle(reinterpret_cast<void*>(vehicle))) return false;
    __try {
        auto** table=*reinterpret_cast<void***>(vehicle);
        const auto key=reinterpret_cast<unsigned(__thiscall*)(void*)>(table[kSlotVehicleKey])(reinterpret_cast<void*>(vehicle));
        const auto driver=reinterpret_cast<unsigned(__thiscall*)(void*)>(table[kSlotDriverClass])(reinterpret_cast<void*>(vehicle));
        return key==g_encounterSkillKey&&driver==kDriverRacer&&g_encounterSkillAI.load(std::memory_order_acquire)==address;
    } __except(EXCEPTION_EXECUTE_HANDLER) {return false;}
}
float __fastcall EncounterSkillHook(void* ai,void*) {
    const float native=g_originalEncounterSkill(ai); // original ABI/result for every unrelated AI
    if(!IsEncounterSkillOwner(ai)) return native;
    ++g_encounterSkillCalls;
    return 1.0f;
}
void InstallEncounterRaceSkillHook() noexcept {
    if(!g_battleSurface||!g_settings.encounterSignalEnabled) return;
    auto* address=reinterpret_cast<void*>(Address(0x00409340));
    if(MH_CreateHook(address,&EncounterSkillHook,reinterpret_cast<void**>(&g_originalEncounterSkill))==MH_OK) {
        g_encounterSkillInstalled=MH_EnableHook(address)==MH_OK;
        if(!g_encounterSkillInstalled) MH_RemoveHook(address);
    }
    Log(LogLevel::Info,"ENCOUNTER_AI skillHook=%u ownerScoped=1 persistentSkillWrites=0",unsigned(g_encounterSkillInstalled));
}
