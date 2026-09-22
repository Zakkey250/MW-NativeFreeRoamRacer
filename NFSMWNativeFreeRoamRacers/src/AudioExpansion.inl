// Optional experimental free-roam allocation; never infer safety from LB's release name.
std::atomic<unsigned> g_audioSlotCeiling{4};
std::atomic<bool> g_audioPreparationFrozen{false};
bool g_audioExpansionHookEnabled=false;
void* g_originalAudioBudget=nullptr;
std::uintptr_t g_audioBudgetContinue=0;
std::uintptr_t g_audioBudgetCounter=0;
bool QueryExpandedAudioCapability() noexcept {
    HMODULE module=nullptr;
    if(!GetModuleHandleExW(0,L"NFSMWLimitBreaker.asi",&module))return false;
    nfsmw_limitbreaker::ResourceLimitsV1 status{};unsigned result=0;
    __try {
        const auto query=reinterpret_cast<nfsmw_limitbreaker::QueryResourceLimitsV1>(GetProcAddress(module,"NFSMWLB_QueryResourceLimitsV1"));
        if(query)result=query(&status,sizeof(status));
    } __except(EXCEPTION_EXECUTE_HANDLER) {result=0;}
    FreeLibrary(module);
    return SupportsSixAudioSlots(result,status);
}
unsigned __cdecl SelectAudioBudgetIncrement() noexcept {
    bool expected=false;
    if(g_audioPreparationFrozen.compare_exchange_strong(expected,true)){
        const bool capable=QueryExpandedAudioCapability();
        const auto effective=ChooseAudioCeiling(g_settings.enabled&&g_settings.freeRoamAudioSlots!=0,
            g_settings.lbEngineAudioExpansion,capable);
        g_audioSlotCeiling.store(effective);
        Log(LogLevel::Info,"AUDIO_EXPANSION_FINAL requested=%u effective=%u capability=%u source=first-native-no-event-budget processFrozen=1 actualPoolVerificationRequired=1",
            g_settings.lbEngineAudioExpansion?6u:4u,effective,capable?1u:0u);
    }
    return g_audioSlotCeiling.load();
}
void LockAudioExpansionForWorld() noexcept {
    if(!g_audioPreparationFrozen.exchange(true)&&g_settings.lbEngineAudioExpansion)
        Log(LogLevel::Info,"AUDIO_EXPANSION_FINAL requested=6 effective=4 source=world-before-budget latePromotion=0 restartRequired=1");
}
// Replaces only native ADD [911ED4],4 in the no-event branch. Preserve registers
// and reproduce ADD flags; tail POP EBX/RET remains native. No allocation here.
__declspec(naked) void AudioExpansionBudgetHook(){
    __asm {
        push eax
        push ecx
        push edx
        call SelectAudioBudgetIncrement
        mov edx,dword ptr [g_audioBudgetCounter]
        add dword ptr [edx],eax
        pop edx
        pop ecx
        pop eax
        jmp dword ptr [g_audioBudgetContinue]
    }
}
void InstallAudioExpansionHook() noexcept {
    if(!g_settings.lbEngineAudioExpansion)return;
    const bool startupCapability=QueryExpandedAudioCapability(); // First bounded query; retry at preparation.
    const std::uint8_t expected[]={0x83,5,0xD4,0x1E,0x91,0,4,0x5B,0xC3};
    auto* site=reinterpret_cast<void*>(Address(0x004E5CB9));
    g_audioBudgetContinue=Address(0x004E5CC0);
    g_audioBudgetCounter=Address(0x00911ED4);
    if(AudioCodeMatches(0x004E5CB9,expected)&&MH_CreateHook(site,&AudioExpansionBudgetHook,&g_originalAudioBudget)==MH_OK){
        g_audioExpansionHookEnabled=MH_EnableHook(site)==MH_OK;
        if(!g_audioExpansionHookEnabled)MH_RemoveHook(site);
    }
    Log(LogLevel::Info,"AUDIO_EXPANSION_CONFIG optIn=1 hook=%u startupCapability=%u requested=6 defaultFallback=4 versionGate=0 retryAtNativePreparation=1 eventSourceClampUnchanged=4",
        g_audioExpansionHookEnabled?1u:0u,startupCapability?1u:0u);
}
