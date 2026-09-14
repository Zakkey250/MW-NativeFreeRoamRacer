// Read-only native catchup sampling. Standard controller depends on event,
// GRacerInfo and PID data. Do NOT forge those or replace the result by a guessed
// multiplier; user requested the stock behavior before custom tuning.
bool g_catchupProbe=false;
void InitializeEncounterCatchupProbe() noexcept {
    g_catchupProbe=g_battleSurface&&
        !std::memcmp(reinterpret_cast<void*>(Address(0x00409390)),"\x51\xd9\x41\x40\xd8\x41\x3c",7)&&
        !std::memcmp(reinterpret_cast<void*>(Address(0x008925C8)),"\x90\x93\x40\x00",4);
}
void LogEncounterCatchup() noexcept {
    if(!g_catchupProbe) return;
    __try {
        EncounterNativeAI ai{};if(!ReadEncounterAI(g_battle.rival,ai,false)) return;
        auto* primary=static_cast<unsigned char*>(ai.primary);
        void* cheater=primary+0x76C;
        float base=0,glue=0;void* info=nullptr;
        if(!BattleVtable(cheater,0x008925C4)||
            !AudioRead(primary+0x7A4,&info)||!AudioRead(primary+0x7A8,&base)||!AudioRead(primary+0x7AC,&glue)||
            !std::isfinite(base)||!std::isfinite(glue)) return;
        const float value=reinterpret_cast<float(__thiscall*)(void*)>(Address(0x00409390))(cheater);
        Log(LogLevel::Info,"ENCOUNTER_CATCHUP nativeSample=%.4f baseSkill=%.3f glueSkill=%.3f racerInfo=%p playerLeads=%u gap=%.1f observeOnly=1 customMultiplier=0",
            value,base,glue,info,unsigned(g_battle.model.leader()==battle::Leader::Player),g_battle.model.gap());
    } __except(EXCEPTION_EXECUTE_HANDLER) {g_catchupProbe=false;}
}
