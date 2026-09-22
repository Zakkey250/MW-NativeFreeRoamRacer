// No Limit Breaker import, filename/version gate, game-thread wait, or validity-byte patch.
// Readiness itself selects the immediate path or the bounded deferred path.
const char* NativeStartOwnerChanged(const EncounterNativeAI& before,const EncounterNativeAI& after) noexcept {
    if(before.primary!=after.primary||before.racer!=after.racer) return "ai-owner-changed";
    if(before.goal!=after.goal) return "goal-changed";
    if(before.nav!=after.nav) return "nav-changed";
    if(before.target!=after.target) return "target-changed";
    return nullptr;
}
bool NativeStartExpired(const EncounterNativeStart& state,ULONGLONG now) noexcept {
    return state.issued&&(now<state.issuedAt||now-state.issuedAt>=kNativeStartTimeoutMs);
}
bool NativeReseedFresh(bool valid,ULONGLONG captured,ULONGLONG now) noexcept {
    return valid&&now>=captured&&now-captured<=100;
}
void LogNativeStart(const EncounterNativeStart& state,const battle::Identity& id,const char* phase,
                    unsigned vehicles,ULONGLONG now,const char* field) noexcept {
    Log(LogLevel::Info,"ENCOUNTER_NATIVE_START phase=%s vehicles=%u vehicle=%p simable=%p key=%08X ai=%p goal=%p action=%p nav=%p target=%p navValid=%u navValidBefore=%u elapsedMs=%llu polls=%u startRaceCalls=%u chooseActionCalls=%u reseedAttempts=%u field=%s",
        phase,vehicles,reinterpret_cast<void*>(id.vehicle),reinterpret_cast<void*>(id.simable),id.key,
        state.last.primary,state.last.goal,state.last.action,state.last.nav,state.last.target,
        unsigned(state.last.navValid),unsigned(state.owner.navValid),state.issued&&now>=state.issuedAt?now-state.issuedAt:0,
        state.polls,unsigned(state.issued),unsigned(state.chooseIssued),unsigned(state.reseedIssued),field);
}
struct EncounterStartOps {
    AnchorRoadSeed roadSeed{};
    bool roadSeedValid=false;
    ULONGLONG seedAt=0;
    bool Read(const battle::Identity& id,EncounterNativeAI& ai,const char*& failure) noexcept {
        return ReadEncounterAIStructure(id,ai,failure);
    }
    bool Start(const EncounterNativeAI& ai) noexcept {
        __try {
            // Owned scalar copy of THIS racer's currently valid road, not a
            // borrowed nav pointer or another vehicle's route. Optional: bad
            // capture preserves the existing bounded native-start path.
            roadSeedValid=CaptureAnchorRoadSeed(ai.nav,roadSeed);
            seedAt=GetTickCount64();
            // Verified IRacer subobject and RET4; Racing=0, not Drag=1.
            reinterpret_cast<void(__thiscall*)(void*,unsigned)>(Address(0x00423010))(ai.racer,0);return true;
        } __except(EXCEPTION_EXECUTE_HANDLER) {
            Log(LogLevel::Error,"ENCOUNTER_AI start exception=%08X",GetExceptionCode());return false;
        }
    }
    bool Recover(const EncounterNativeAI& ai) noexcept {
        const auto now=GetTickCount64();
        if(!NativeReseedFresh(roadSeedValid,seedAt,now)||!ValidateAnchorCopySurface()||
           !ValidateAnchorRoadNetwork(roadSeed))return false;
        __try {
            // Native WRoadNav::InitFromOther rebuilds lane/road splines and
            // cookie state from six validated road scalars. Call it directly:
            // AIVehicle::ResetVehicleToRoadNav would also reposition the car.
            // Keep all StartRace flags/goal/action; never invoke QuitRace here.
            reinterpret_cast<void(__thiscall*)(void*,const void*,bool)>(Address(0x00777660))(
                ai.nav,roadSeed.bytes.data(),false);
            Log(LogLevel::Info,"ENCOUNTER_NATIVE_RESEED segment=%d node=%u seedAgeMs=%llu nativeInitFromOther=1 vehicleReposition=0",
                int(roadSeed.At<std::int16_t>(0x8E)),unsigned(roadSeed.At<std::uint8_t>(0x8C)),now-seedAt);
            return true;
        } __except(EXCEPTION_EXECUTE_HANDLER) {
            Log(LogLevel::Warning,"ENCOUNTER_NATIVE_RESEED exception=%08X",GetExceptionCode());return false;
        }
    }
    bool Choose(const EncounterNativeAI& ai) noexcept {
        __try {
            reinterpret_cast<void(__thiscall*)(void*,float)>(Address(kGoalChooseAction))(ai.goal,0.0f);return true;
        } __except(EXCEPTION_EXECUTE_HANDLER) {
            Log(LogLevel::Error,"ENCOUNTER_AI choose exception=%08X",GetExceptionCode());return false;
        }
    }
};
template<class Ops>
NativeStartResult AdvanceNativeStart(EncounterNativeStart& state,const battle::Identity& id,
                                     ULONGLONG now,unsigned vehicles,Ops& ops) noexcept {
    if(state.result!=NativeStartResult::Pending) return state.result;
    const auto fail=[&](const char* field) {
        state.failure=field;state.result=NativeStartResult::Failed;
        LogNativeStart(state,id,"failed",vehicles,now,field);return state.result;
    };
    if(!state.issued) {
        if(!ops.Read(id,state.last,state.failure)) return fail(state.failure);
        if(!state.last.navValid) return fail("pre-nav-invalid");
        if(!state.last.raceAction) return fail("pre-race-action");
        state.owner=state.last;
        LogNativeStart(state,id,"pre",vehicles,now,"none");
        state.issued=true;state.issuedAt=now; // Also covers a partial native call throwing.
        if(!ops.Start(state.last)) return fail("start-exception");
        // Structural read immediately after the one native call; no retry/reset.
        if(!ops.Read(id,state.last,state.failure)) return fail(state.failure);
        LogNativeStart(state,id,"issued",vehicles,now,"none");
    } else if(!ops.Read(id,state.last,state.failure)) return fail(state.failure);
    ++state.polls;
    if(const char* changed=NativeStartOwnerChanged(state.owner,state.last)) return fail(changed);
    if(state.last.action&&!state.last.raceAction) return fail("action-vtable");
    if(NativeStartExpired(state,now)) return fail("timeout");
    // Only in the SAME call/tick as StartRace. Never reuse a road snapshot on a
    // later frame after the car may have crossed a junction. This is not a
    // repeated StartRace, a copied path pointer, or a forced readiness flag.
    if(!state.last.navValid&&state.polls==1&&!state.reseedIssued) {
        state.reseedIssued=true;
        if(ops.Recover(state.last)) {
            if(!ops.Read(id,state.last,state.failure))return fail(state.failure);
            if(const char* changed=NativeStartOwnerChanged(state.owner,state.last))return fail(changed);
            if(state.last.action&&!state.last.raceAction)return fail("reseed-action-vtable");
            LogNativeStart(state,id,"reseeded",vehicles,now,state.last.navValid?"none":"nav-invalid");
        } else LogNativeStart(state,id,"reseed-unavailable",vehicles,now,"bounded-wait");
    }
    if(state.last.navValid&&state.last.raceAction) {
        state.result=NativeStartResult::Ready;
        LogNativeStart(state,id,"ready",vehicles,now,"none");return state.result;
    }
    if(!state.waitLogged) {
        state.waitLogged=true;
        LogNativeStart(state,id,"pending",vehicles,now,state.last.navValid?"action-null":"nav-invalid");
    }
    if(state.last.navValid&&!state.last.action&&!state.chooseIssued) {
        state.chooseIssued=true;
        if(!ops.Choose(state.last)) return fail("choose-exception");
        LogNativeStart(state,id,"choose-issued",vehicles,now,"action-null");
        // Observe the native result on a later simulation tick, never spin here.
    }
    return NativeStartResult::Pending;
}
NativeStartResult NativeEncounterStart(const battle::Identity& rival,ULONGLONG now,unsigned vehicles) noexcept {
    EncounterStartOps ops;
    return AdvanceNativeStart(g_battle.nativeStart,rival,now,vehicles,ops);
}
