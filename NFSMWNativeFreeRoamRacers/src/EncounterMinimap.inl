// Native racer arrows (0057A110) are separate from GIcon gameplay markers.
// Replace only the map's indirect CALL, never the shared IsActive function.
// MW Arms Assist compares the first 12 bytes of 00688200 before initialization.
using EncounterMapActiveFn=int(__thiscall*)(void*);
std::atomic<unsigned> g_encounterMapSuppressed{0};
bool g_encounterMapInstalled=false;
bool HideEncounterNativeArrow(void* vehicle,std::uintptr_t caller) noexcept {
    if(caller!=Address(0x0057A16A)||g_encounterAdapterThread.load(std::memory_order_acquire)!=GetCurrentThreadId()||
        g_battle.model.phase()!=battle::Phase::Active) return false;
    const auto managed=std::find_if(g_racers.begin(),g_racers.end(),[vehicle](const auto& racer){return racer.pointer==vehicle;});
    if(managed==g_racers.end()||EncounterMarkerAllowed(*managed)||!IsExpectedVehicle(vehicle)) return false;
    // Reject recycled pointers. Unrelated racers / cops / player are untouched.
    __try {
        auto** table=*reinterpret_cast<void***>(vehicle);
        return reinterpret_cast<unsigned(__thiscall*)(void*)>(table[kSlotVehicleKey])(vehicle)==managed->vehicleKey &&
            reinterpret_cast<unsigned(__thiscall*)(void*)>(table[kSlotDriverClass])(vehicle)==kDriverRacer &&
            GetSimablePointer(vehicle)==managed->simable;
    } __except(EXCEPTION_EXECUTE_HANDLER) {return false;}
}
int __fastcall EncounterMapActiveHook(void* vehicle,void** nativeTable) {
    const auto caller=reinterpret_cast<std::uintptr_t>(_ReturnAddress());
    // ECX=vehicle, EDX=vtable, exactly as at the original FF 92 88 00 00 00.
    // Preserve dynamic dispatch, including another vehicle implementation.
    const int native=reinterpret_cast<EncounterMapActiveFn>(nativeTable[34])(vehicle);
    if(native && HideEncounterNativeArrow(vehicle,caller)) {++g_encounterMapSuppressed;return 0;}
    return native;
}
bool PatchEncounterMapCall(void* site,void* target) noexcept {
    constexpr unsigned char original[]={0xFF,0x92,0x88,0,0,0};
    std::array<unsigned char,6> observed{};
    if(!SafeRead(site,&observed)||std::memcmp(observed.data(),original,6)) return false;
    // NOP + CALL rel32 keeps the native return address at 0057A16A. No register
    // setup is needed: the two preceding MOVs still supply ECX and EDX.
    unsigned char replacement[]={0x90,0xE8,0,0,0,0};
    const auto relative=static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(target))-
        static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(site)+6);
    std::memcpy(replacement+2,&relative,4);
    DWORD previous=0,unused=0;
    if(!VirtualProtect(site,6,PAGE_EXECUTE_READWRITE,&previous)) return false;
    std::memcpy(site,replacement,6);
    const bool flushed=FlushInstructionCache(GetCurrentProcess(),site,6)!=FALSE;
    if(!flushed) {std::memcpy(site,original,6);FlushInstructionCache(GetCurrentProcess(),site,6);}
    if(!VirtualProtect(site,6,previous,&unused)) {
        std::memcpy(site,original,6);FlushInstructionCache(GetCurrentProcess(),site,6);
        VirtualProtect(site,6,previous,&unused);return false;
    }
    return flushed;
}
void InstallEncounterMinimapHook() noexcept {
    if(!g_battleSurface||!g_settings.encounterSignalEnabled) return;
    g_encounterMapInstalled=PatchEncounterMapCall(reinterpret_cast<void*>(Address(0x0057A164)),
        reinterpret_cast<void*>(&EncounterMapActiveHook));
    Log(LogLevel::Info,"ENCOUNTER_MAP nativeArrowHook=%u callsite=0057A164 sharedIsActiveUntouched=1 vehicleStateWrites=0",unsigned(g_encounterMapInstalled));
}
