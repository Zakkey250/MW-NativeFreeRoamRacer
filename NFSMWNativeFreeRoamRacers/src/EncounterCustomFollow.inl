// Relaxed trajectory guidance: no traffic graph path is submitted here.
bool NativeEncounterCustomFollow(const VehicleSnapshot& rival,bool force) noexcept {
    (void)rival;
    __try {
        EncounterNativeAI ai{};if(!ReadEncounterAI(g_battle.rival,ai)) return false;
        if(g_battle.pursuit.passing()) {
            g_battle.customDirect=false;
            if(!g_battle.passActive) {
                reinterpret_cast<void(__thiscall*)(void*)>(Address(0x00409E60))(ai.target);
                reinterpret_cast<void(__thiscall*)(void*)>(Address(0x00770060))(ai.nav);
                g_battle.passActive=true;g_battle.destinationSet=false;
                Log(LogLevel::Info,"ENCOUNTER_PURSUIT phase=pass custom=1 entryDistance=15 exitDistance=70 stableSeconds=0.25 maxSeconds=8 closingFloorMps=8 speedCarryKmh=%.1f carryHoldSeconds=1 carryFadeSeconds=1 nativeDirection=1 directSteering=0",
                    g_battle.customAttackEntrySpeed*3.6f);
            }
            return true;
        }
        if(!g_customDriveInstalled||!g_battle.routeHint.valid) {g_battle.customDirect=false;return false;}
        if(!g_battle.customDirect||force||g_battle.passActive) {
            // Release chase target and old road query once on transition.
            // Race action/recovery stay alive; only its final drive aim changes.
            reinterpret_cast<void(__thiscall*)(void*)>(Address(0x00409E60))(ai.target);
            reinterpret_cast<void(__thiscall*)(void*)>(Address(0x00770060))(ai.nav);
            g_battle.customDirect=true;g_battle.passActive=false;g_battle.destinationSet=true;
            Log(LogLevel::Info,"ENCOUNTER_CUSTOM directionOnly=1 trafficPath=0 nativeSteering=1 nativeBraking=1");
        }
        return true;
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        g_battle.customDirect=false;
        Log(LogLevel::Warning,"ENCOUNTER_CUSTOM directDeferred exception=%08X battleContinues=1",GetExceptionCode());return false;
    }
}
