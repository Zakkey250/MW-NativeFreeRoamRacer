// Observe supported Arsenal damage paths, never input/stock alone. Installation
// waits for Arms Assist's verification to finish so its byte guards still pass.
std::uintptr_t g_encounterArsenal=0;
bool g_weaponHooks=false,g_weaponHooksRefused=false;
using ArsenalEmpFn=bool(__stdcall*)(std::uintptr_t,unsigned);
using ArsenalShockFn=bool(__stdcall*)(std::uintptr_t,std::uintptr_t,const void*,float);
ArsenalEmpFn g_originalEncounterEmp=nullptr;
ArsenalShockFn g_originalEncounterShock=nullptr;
using ArmsReadyFn=unsigned(__cdecl*)(float*,float*);
using ArmsEmpFn=bool(__cdecl*)(std::uintptr_t,unsigned,std::uintptr_t,unsigned);
ArmsReadyFn g_armsReady=nullptr;ArmsEmpFn g_armsEmp=nullptr;

bool ReadEncounterWeaponIdentity(const battle::Identity& expected) noexcept {
    VehicleSnapshot car{};
    return expected.vehicle&&expected.simable&&expected.key&&
        ReadVehicle(reinterpret_cast<void*>(expected.vehicle),&car)&&car.vehicleKey==expected.key&&
        reinterpret_cast<std::uintptr_t>(GetSimablePointer(car.pointer))==expected.simable;
}
void RecordEncounterWeaponHit(unsigned weapon,const battle::Identity& victim) noexcept {
    if(g_encounterAdapterThread.load(std::memory_order_acquire)!=GetCurrentThreadId()||
       !encounter_weapons::Forfeit(g_battle.model.phase()==battle::Phase::Active,true,true,
            victim.vehicle&&victim.simable&&victim.key&&victim==g_battle.rival)||g_battle.weaponForfeit)return;
    g_battle.weaponForfeit=true;
    Log(LogLevel::Info,"ENCOUNTER_WEAPON_HIT weapon=%u source=player victim=%p key=%08X target=battle-rival affected=1 forfeitQueued=1",
        weapon,reinterpret_cast<void*>(victim.vehicle),victim.key);
}
bool __stdcall EncounterEmpHitHook(std::uintptr_t target,unsigned type) {
    const auto caller=reinterpret_cast<std::uintptr_t>(_ReturnAddress())-g_encounterArsenal;
    const auto victim=g_battle.rival;
    // Racer type 3 uses the vehicle primary (IVehicle - AC); cop type 1
    // and helicopter type 2 use different objects and never qualify.
    const bool eligible=g_encounterAdapterThread.load(std::memory_order_acquire)==GetCurrentThreadId()&&
        g_battle.model.phase()==battle::Phase::Active&&encounter_weapons::RivalEmpTarget(type,target,victim.vehicle)&&encounter_weapons::PlayerEmpCaller(unsigned(caller))&&
        ReadEncounterWeaponIdentity(victim)&&ReadEncounterWeaponIdentity(g_battle.player);
    const bool affected=g_originalEncounterEmp(target,type);
    if(affected&&eligible)RecordEncounterWeaponHit(1,victim);
    else if(affected&&g_battle.model.phase()==battle::Phase::Active&&encounter_weapons::PlayerEmpCaller(unsigned(caller)))
        Log(LogLevel::Info,"ENCOUNTER_WEAPON_IGNORED weapon=1 type=%u target=%p reason=not-confirmed-battle-rival",type,reinterpret_cast<void*>(target));
    return affected;
}
bool __stdcall EncounterShockHitHook(std::uintptr_t victim,std::uintptr_t source,const void* origin,float range) {
    const auto caller=reinterpret_cast<std::uintptr_t>(_ReturnAddress())-g_encounterArsenal;
    const auto rival=g_battle.rival;
    const bool eligible=g_encounterAdapterThread.load(std::memory_order_acquire)==GetCurrentThreadId()&&
        g_battle.model.phase()==battle::Phase::Active&&encounter_weapons::RivalShockTarget(victim,source,rival.simable,g_battle.player.simable)&&
        encounter_weapons::PlayerShockCaller(unsigned(caller))&&ReadEncounterWeaponIdentity(rival)&&ReadEncounterWeaponIdentity(g_battle.player);
    const bool affected=g_originalEncounterShock(victim,source,origin,range);
    if(affected&&eligible)RecordEncounterWeaponHit(2,rival);
    else if(affected&&g_battle.model.phase()==battle::Phase::Active&&encounter_weapons::PlayerShockCaller(unsigned(caller)))
        Log(LogLevel::Info,"ENCOUNTER_WEAPON_IGNORED weapon=2 target=%p reason=not-confirmed-battle-rival",reinterpret_cast<void*>(victim));
    return affected;
}
void TryEncounterWeaponBridge() {
    static ULONGLONG next=0;const auto now=GetTickCount64();
    if(now<next)return;next=now+1000;
    if(auto assist=GetModuleHandleW(L"MWArmsAssist.asi")) {
        g_armsReady=reinterpret_cast<ArmsReadyFn>(GetProcAddress(assist,"MWAA_EncounterReadyV1"));
        g_armsEmp=reinterpret_cast<ArmsEmpFn>(GetProcAddress(assist,"MWAA_EncounterPoliceEmpV1"));
        // Old assist has no handshake: avoid altering code while it verifies.
        if(!g_armsReady||!(g_armsReady(nullptr,nullptr)&1))return;
    }
    if(g_weaponHooks||g_weaponHooksRefused)return;
    auto arsenal=GetModuleHandleW(L"MWArsenal.asi");if(!arsenal)return;
    wchar_t path[MAX_PATH]{};std::string hash;
    if(!GetModuleFileNameW(arsenal,path,MAX_PATH)||!ComputeSha256(path,&hash)||
       (hash!="133E8F6FC1AF11CA038414AAE1B5057FA11E0C4B928856F3E357146A4FDF9BEF"&&
        hash!="392DDA59241F0478EC01BEB3FDE53D7F31DFA8AA153A4F7F9BCE9B50A87FFE48")) {
        g_weaponHooksRefused=true;Log(LogLevel::Warning,"ENCOUNTER_WEAPONS unsupported Arsenal; optional bridge disabled");return;
    }
    const auto base=reinterpret_cast<std::uintptr_t>(arsenal);
    const unsigned char entry[]={0x55,0x8B,0xEC,0x6A,0xFE};
    const unsigned char empCall[]={0xE8,0x07,0xB9,0xFF,0xFF};
    const unsigned char shockCall[]={0xE8,0x17,0xFD,0xFF,0xFF};
    if(std::memcmp(reinterpret_cast<void*>(base+0x8280),entry,5)||
       std::memcmp(reinterpret_cast<void*>(base+0x8CA0),entry,5)||
       std::memcmp(reinterpret_cast<void*>(base+0xC974),empCall,5)||
       std::memcmp(reinterpret_cast<void*>(base+0x8F84),shockCall,5)) {
        g_weaponHooksRefused=true;Log(LogLevel::Warning,"ENCOUNTER_WEAPONS code mismatch; optional bridge disabled");return;
    }
    auto* emp=reinterpret_cast<void*>(base+0x8280);auto* shock=reinterpret_cast<void*>(base+0x8CA0);
    if(MH_CreateHook(emp,EncounterEmpHitHook,reinterpret_cast<void**>(&g_originalEncounterEmp))!=MH_OK){g_weaponHooksRefused=true;return;}
    if(MH_CreateHook(shock,EncounterShockHitHook,reinterpret_cast<void**>(&g_originalEncounterShock))!=MH_OK) {
        MH_RemoveHook(emp);g_weaponHooksRefused=true;return;
    }
    g_encounterArsenal=base;
    if(MH_EnableHook(emp)!=MH_OK||MH_EnableHook(shock)!=MH_OK) {
        MH_DisableHook(emp);MH_DisableHook(shock);MH_RemoveHook(emp);MH_RemoveHook(shock);g_weaponHooksRefused=true;return;
    }
    g_weaponHooks=true;
    Log(LogLevel::Info,"ENCOUNTER_WEAPONS ready EMP=1 shock=1 spikeViaAssist=%u nativeEffectsUnchanged=1",unsigned(g_armsReady!=nullptr));
}

void EncounterSpikeHit(std::uintptr_t vehicle,std::uintptr_t simable) noexcept {
    // Legacy V1 contains no victim identity. Never infer a rival hit from it.
    (void)vehicle;(void)simable;
}
void EncounterSpikeHitV2(std::uintptr_t vehicle,std::uintptr_t simable,
                         std::uintptr_t victim,std::uintptr_t victimSimable) noexcept {
    if(g_encounterAdapterThread.load(std::memory_order_acquire)!=GetCurrentThreadId()||
       g_battle.model.phase()!=battle::Phase::Active)return;
    if(vehicle==g_battle.player.vehicle&&simable==g_battle.player.simable&&
       victim==g_battle.rival.vehicle&&victimSimable==g_battle.rival.simable&&
       ReadEncounterWeaponIdentity(g_battle.player)&&ReadEncounterWeaponIdentity(g_battle.rival))
        RecordEncounterWeaponHit(3,g_battle.rival);
}
