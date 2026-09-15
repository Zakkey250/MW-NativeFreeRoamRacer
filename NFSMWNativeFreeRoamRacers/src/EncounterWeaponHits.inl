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

void RecordEncounterWeaponHit(unsigned weapon) noexcept {
    if(g_encounterAdapterThread.load(std::memory_order_acquire)!=GetCurrentThreadId()||
       !encounter_weapons::Forfeit(g_battle.model.phase()==battle::Phase::Active,true,true,true)||g_battle.weaponForfeit)return;
    g_battle.weaponForfeit=true;
    Log(LogLevel::Info,"ENCOUNTER_WEAPON_HIT weapon=%u source=player affected=1 forfeitQueued=1",weapon);
}
bool __stdcall EncounterEmpHitHook(std::uintptr_t target,unsigned type) {
    const auto caller=reinterpret_cast<std::uintptr_t>(_ReturnAddress())-g_encounterArsenal;
    const bool affected=g_originalEncounterEmp(target,type);
    if(affected&&target&&type>=1&&type<=3&&encounter_weapons::PlayerEmpCaller(unsigned(caller)))RecordEncounterWeaponHit(1);
    return affected;
}
bool __stdcall EncounterShockHitHook(std::uintptr_t victim,std::uintptr_t source,const void* origin,float range) {
    const auto caller=reinterpret_cast<std::uintptr_t>(_ReturnAddress())-g_encounterArsenal;
    const bool affected=g_originalEncounterShock(victim,source,origin,range);
    std::uintptr_t victimTable=0;
    // Arsenal receives SimObject primary; ISimable is its +2C interface.
    if(affected&&victim!=source&&source+0x2C==g_battle.player.simable&&
       encounter_weapons::PlayerShockCaller(unsigned(caller))&&
       AudioRead(reinterpret_cast<void*>(victim+0x2C),&victimTable)&&victimTable==Address(kISimableVtable))RecordEncounterWeaponHit(2);
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
    if(vehicle==g_battle.player.vehicle&&simable==g_battle.player.simable)RecordEncounterWeaponHit(3);
}
