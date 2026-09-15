struct EncounterNativeAI { void *primary=nullptr,*racer=nullptr,*goal=nullptr,*action=nullptr,*nav=nullptr,*target=nullptr; };
struct EncounterRuntime {
    battle::Model model;
    battle::Identity player{},rival{};
    bool pending=false, aiChanged=false, rewardAttempted=false, leadRoutePending=false;
    int actor=-1;
    unsigned reward=1000,blacklist=0;
    bool weaponForfeit=false;
    float routeSeconds=0,logSeconds=0,missingSeconds=0,retryLogSeconds=0;
    Vec3 lastDestination{};
    battle::Point lastDestinationHeading{};
    Vec3 routeOrigin{};
    int routeOriginSegment=-1;
    float routeProgress=0,routeEndpointDistance=0;
    bool destinationSet=false;
    encounter_route::Trail routeTrail;
    encounter_custom::DirectionalTrail customTrail;
    bool customWaiting=false;
    bool customDirect=false;
    ULONGLONG customHintAt=0;
    float customSpeedDemand=-1;
    float customLastSpeedRequest=-1,customAttackEntrySpeed=-1,customAttackSeconds=0;
    bool customAttackPace=false;
    encounter_route::Pursuit pursuit;
    bool passActive=false;
    encounter_route::Trail::Destination routeHint{};
    Vec3 lastRouteRival{};
    float routeAge=0,pathSeconds=0;
    void* profile=nullptr;
};
EncounterRuntime g_battle;
static_assert(offsetof(NFSPluginSDK::MW05::WRoadNav,fSegmentInd)==0x8E);
static_assert(offsetof(NFSPluginSDK::MW05::WRoadNav,fValid)==0x50);
std::atomic<DWORD> g_encounterAdapterThread{0};
bool g_battleSurface=false;
static_assert(offsetof(NFSPluginSDK::MW05::cFrontEndDatabase,CurrentUserProfiles)==0x10);
static_assert(offsetof(NFSPluginSDK::MW05::UserProfile,mTheCareerSettings)+
    offsetof(NFSPluginSDK::MW05::CareerSettings,CurrentCash)==0xB4);
static_assert(offsetof(NFSPluginSDK::MW05::UserProfile,mTheCareerSettings)+
    offsetof(NFSPluginSDK::MW05::CareerSettings,CurrentBin)==0xB0);
bool ValidateEncounterBattleSurface() noexcept {
    struct Guard {std::uintptr_t va; const char* bytes; unsigned size;};
    const Guard guards[]={
        {0x00423010,"\x53\x55\x56\x8b\xf1\x8b\x8e\xf4",8},
        {0x00423182,"\xc2\x04\x00",3},
        {0x00416900,"\x56\x57\x8b\xf9\x8b\x87\x88\xf8",8},
        {0x004169AD,"\x5f\x5e\xc3",3},
        {0x00423860,"\x56\x8b\xf1\x8b\x46\x1c\x57\x8b",8},
        {0x00770090,"\xa1\xfc\x38\x9b\x00\x85\xc0\x74",8},
        {0x00788F30,"\x56\x57\x8b\x3d\xfc\x38\x9b\x00",8},
        {0x00516AF0,"\x56\x8b\x74\x24\x08\x85\xf6\x57",8},
        {0x005B8550,"\x6a\xff\x68\x48\x3f\x87\x00\x64",8},
        {0x005BCD20,"\x56\x8b\x74\x24\x08\x85\xf6\x57",8},
        {0x008925BC,"\x10\x30\x42\x00\x00\x69\x41\x00",8},
        {0x008926A4,"\x20\x1d\x43\x00",4},
        {0x00611BAF,"\x8b\x8e\xb4\x00\x00\x00\x03\xc8",8},
        {0x00525328,"\x8b\x46\x18\x83\xf8\x05",6},
        {0x0052538B,"\x83\xf8\x02",3},
        {0x005B1516,"\x8b\x70\x64",3},
        {0x005B1543,"\x8b\x76\x04",3},
        {0x00515D4C,"\x8d\x4e\x64",3},
        {0x005BCD5B,"\xc2\x04\x00",3},
        {0x008AA8E0,"\x00\x07\x67\x00",4},
        {0x00670700,"\x83\xec\x1c\x8b\x41\x80\x83\xc1\x80",9},
        {0x006707F4,"\xc2\x04\x00",3},
        {0x005CA7F5,"\xd8\x2d\x6c\x09\x89\x00\xd9\x58\x04",9},
        {0x00409340,"\x51\xd9\x81\x60\x07\x00\x00",7},
        {0x0089270C,"\x40\x93\x40\x00",4},
        {0x00409E60,"\x56\x33\xc0\x88\x41\x2c\x89\x41\x1c",9},
        {0x00409EC1,"\x5e\xc3",2},
        {0x00770060,"\x56\x8b\xf1\x8b\x0d\xfc\x38\x9b\x00",9},
        {0x00770080,"\x5e\xc3",2},
        {0x00688200,"\x8b\x91\xac\x00\x00\x00\x33\xc0\x85\xd2\x0f\x95\xc0\xc3",14},
        {0x008AA8B0,"\x00\x82\x68\x00",4},
        {0x0057A164,"\xff\x92\x88\x00\x00\x00\x84\xc0\x74\x37",10},
        {0x0057A1D3,"\xe8\x98\xaa\xf9\xff",5},
        {0x00428BCA,"\xe8\x61\x03\x36\x00",5}
    };
    for(const auto& guard:guards) if(std::memcmp(reinterpret_cast<void*>(Address(guard.va)),guard.bytes,guard.size)) {
        Log(LogLevel::Warning,"ENCOUNTER_SURFACE rejected address=%08X freeRoamUnaffected=1",unsigned(guard.va));return false;
    }
    Log(LogLevel::Info,"ENCOUNTER_SURFACE exact guards=%u accepted=1",unsigned(std::size(guards)));
    return true;
}
bool EncounterBattleBusy() noexcept {return g_battle.pending||g_battle.model.phase()==battle::Phase::Active;}

bool EncounterMarkerAllowed(const ManagedRacer& racer) noexcept {
    if(g_battle.model.phase()!=battle::Phase::Active) return true;
    return reinterpret_cast<std::uintptr_t>(racer.pointer)==g_battle.rival.vehicle &&
        reinterpret_cast<std::uintptr_t>(racer.simable)==g_battle.rival.simable && racer.vehicleKey==g_battle.rival.key;
}

bool BattleVtable(void* object,std::uintptr_t expected) noexcept {
    std::uintptr_t value=0;return AudioRead(object,&value)&&value==Address(expected);
}
bool ReadEncounterAI(const battle::Identity& identity,EncounterNativeAI& out,bool actionRequired=true) noexcept {
    auto* vehicle=reinterpret_cast<void*>(identity.vehicle);
    VehicleSnapshot snapshot{};
    if(!ReadVehicle(vehicle,&snapshot)||snapshot.vehicleKey!=identity.key||snapshot.driverClass!=kDriverRacer ||
        reinterpret_cast<std::uintptr_t>(GetSimablePointer(vehicle))!=identity.simable) return false;
    void* ai=nullptr;
    if(!AudioRead(static_cast<unsigned char*>(vehicle)+0x54,&ai)||!BattleVtable(ai,kRacecarIVehicleAIVtable)) return false;
    auto* primary=static_cast<unsigned char*>(ai)-0x4C;
    out.primary=primary;out.racer=primary+0x7C4;
    if(!BattleVtable(primary,kRacecarPrimaryVtable)||!BattleVtable(out.racer,0x008925B4)||
        !AudioRead(primary+0xB8,&out.goal)||!BattleVtable(out.goal,kAIGoalRacerVtable)||
        !AudioRead(static_cast<unsigned char*>(out.goal)+4,&out.action)||
        !AudioRead(static_cast<unsigned char*>(ai)+0x24,&out.nav)||!out.nav||
        !AudioRead(static_cast<unsigned char*>(ai)+0x54,&out.target)||!out.target) return false;
    std::uint8_t valid=0;
    return AudioRead(static_cast<unsigned char*>(out.nav)+0x50,&valid)&&valid &&
        (!actionRequired||BattleVtable(out.action,kAIActionRaceVtable));
}

#include "EncounterRaceSkill.inl"
#include "EncounterPowerBoost.inl"
#include "EncounterMinimap.inl"
#include "EncounterCustomDrive.inl"
#include "EncounterCustomSpeed.inl"
#include "EncounterRoute.inl"
#include "EncounterGuide.inl"
#include "EncounterCatchupProbe.inl"

bool NativeEncounterStart(const battle::Identity& rival) noexcept {
    __try {
        EncounterNativeAI ai{};if(!ReadEncounterAI(rival,ai)) return false;
        // Verified IRacer subobject and RET4; Racing=0, not Drag=1.
        reinterpret_cast<void(__thiscall*)(void*,unsigned)>(Address(0x00423010))(ai.racer,0);
        if(!ReadEncounterAI(rival,ai,false)) return false;
        if(!ai.action) reinterpret_cast<void(__thiscall*)(void*,float)>(Address(kGoalChooseAction))(ai.goal,0.0f);
        return ReadEncounterAI(rival,ai);
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        Log(LogLevel::Error,"ENCOUNTER_AI start exception=%08X",GetExceptionCode());return false;
    }
}
bool NativeEncounterRestore(const battle::Identity& rival) noexcept {
    __try {
        EncounterNativeAI ai{};if(!ReadEncounterAI(rival,ai,false)) return false;
        // QuitRace clears AITarget, resets the native nav to Direction/Racer, and
        // leaves the racecar/goal/action owning their own steering and recovery.
        reinterpret_cast<void(__thiscall*)(void*)>(Address(0x00416900))(ai.racer);
        return true;
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        Log(LogLevel::Error,"ENCOUNTER_AI restore exception=%08X",GetExceptionCode());return false;
    }
}

bool NativeEncounterLead(const battle::Identity& rival) noexcept {
    __try {
        EncounterNativeAI ai{};if(!ReadEncounterAI(rival,ai,false)) return false;
        // A role change is NOT QuitRace. Release only the old chase target/path;
        // keep the existing race Goal, Action, pace policy and native recovery.
        reinterpret_cast<void(__thiscall*)(void*)>(Address(0x00409E60))(ai.target);
        reinterpret_cast<void(__thiscall*)(void*)>(Address(0x00770060))(ai.nav);
        g_battle.destinationSet=false;
        Log(LogLevel::Info,"ENCOUNTER_AI role=leader targetCleared=1 nativeDirection=1 quitRace=0");
        return true;
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        Log(LogLevel::Warning,"ENCOUNTER_AI lead deferred exception=%08X",GetExceptionCode());return false;
    }
}

bool NativeEncounterFollow(const VehicleSnapshot& rival,bool force) noexcept {
    __try {
        EncounterNativeAI ai{};if(!ReadEncounterAI(g_battle.rival,ai)) return false;
        if(g_battle.pursuit.passing()) {
            if(!g_battle.passActive) {
                // One phase transition, not a per-frame recovery/reset. Verified
                // CancelPath removes pending query and switches Path(3) to
                // Direction(2). Preserve Racer path type, Goal and AIActionRace.
                reinterpret_cast<void(__thiscall*)(void*)>(Address(0x00409E60))(ai.target);
                reinterpret_cast<void(__thiscall*)(void*)>(Address(0x00770060))(ai.nav);
                g_battle.passActive=true;g_battle.destinationSet=false;
                Log(LogLevel::Info,"ENCOUNTER_PURSUIT phase=pass entryDistance=5 stableSeconds=0.25 maxSeconds=1.5 nativeDirection=1 directSteering=0 quitRace=0");
            }
            return true;
        }
        if(g_battle.passActive) {
            g_battle.passActive=false;force=true;
            Log(LogLevel::Info,"ENCOUNTER_PURSUIT phase=catchup target=current-player nativeRoadNav=1");
        }
        // Use the existing native target object, never manufacture a fake racer
        // event/GRacerInfo. TrackInternal is normally driven by the simulation.
        auto* simable=reinterpret_cast<void*>(g_battle.player.simable);
        reinterpret_cast<void(__thiscall*)(void*,void*)>(Address(0x00423860))(ai.target,simable);
        void* actual=nullptr;std::uint8_t valid=0;
        if(!AudioRead(static_cast<unsigned char*>(ai.target)+0x1C,&actual)||actual!=simable||
            !AudioRead(static_cast<unsigned char*>(ai.target)+0x2C,&valid)||!valid) return false;
        if(!g_battle.routeHint.valid) return false;
        const auto& hint=g_battle.routeHint;
        const Vec3 position{hint.position.x,hint.position.y,hint.position.z};
        // Physical displacement, not wheel speed (a car against a wall can show
        // full wheel speed). Replan the SAME route hint at most once every 4 s.
        const bool stalled=g_battle.routeAge>=4 && Distance(g_battle.lastRouteRival,rival.position)<3;
        // 00787E85 clears +2D8 when constructing a path request; the adjacent
        // native goal parameter at +2DA is consumed at 00428B8A.
        unsigned navType=0;std::uint8_t crossed=0,navValid=0;int pathEdges=0;
        std::int16_t segment=-1;
        if(!AudioRead(static_cast<unsigned char*>(ai.nav)+0x78,&navType)||
            !AudioRead(static_cast<unsigned char*>(ai.nav)+0x50,&navValid)||
            !AudioRead(static_cast<unsigned char*>(ai.nav)+0x8E,&segment)||
            !AudioRead(static_cast<unsigned char*>(ai.nav)+0x2D8,&crossed)||
            !AudioRead(static_cast<unsigned char*>(ai.nav)+0x2E0,&pathEdges)) return false;
        // Valid native Direction may traverse the chosen road for a bounded
        // interval; it is not mislabeled as a completed Path. Replan after road
        // progress, goal consumption, timeout or stall without a race reset.
        const bool busy=reinterpret_cast<bool(__thiscall*)(void*)>(Address(0x00770090))(ai.nav);
        const float endpointDistance=Distance(rival.position,g_battle.lastDestination);
        const float moved=Distance(g_battle.lastDestination,position);
        const float turnDot=battle::DotXZ(g_battle.lastDestinationHeading,hint.heading);
        const float playerGap=Distance(rival.position,position);
        const float progress=Distance(g_battle.routeOrigin,rival.position);
        const auto decision=encounter_route::CommitRoute(force,stalled,g_battle.destinationSet,navValid!=0,
            navType,crossed!=0,pathEdges,g_battle.routeOriginSegment,segment,g_battle.pathSeconds,progress,endpointDistance);
        const bool keep=encounter_route::Holding(decision);
        Log(LogLevel::Info,"ENCOUNTER_NAV mode=%u crossed=%u busy=%u actionRace=1 targetValid=1 distanceToGoal=%.1f stalled=%u edges=%d endpointDistance=%.1f pathAge=%.2f keep=%u",
            navType,unsigned(crossed),unsigned(busy),Distance(rival.position,position),unsigned(stalled),
            pathEdges,g_battle.destinationSet?endpointDistance:-1.f,g_battle.pathSeconds,unsigned(keep));
        Log(LogLevel::Info,"ENCOUNTER_PATH_POLICY endpointDrift=%.1f turnDot=%.3f rivalSpeed=%.1f gap=%.1f keep=%u passLimit=1.5",
            moved,turnDot,rival.speed,playerGap,unsigned(keep));
        Log(LogLevel::Info,"ENCOUNTER_COMMIT decision=%u originSegment=%d segment=%d progress=%.1f age=%.2f hold=%u minSeconds=2 maxSeconds=4",
            unsigned(decision),g_battle.routeOriginSegment,int(segment),progress,g_battle.pathSeconds,unsigned(keep));
        if(busy) return true;
        if(keep) return true;
        // Same road-network path request as AIActionRace 00428B9D..00428BCA.
        // Current player position is requested at most once per second. NOT
        // SetDriveTarget/DoDriving: native RoadNav still computes the route and
        // native AIActionRace alone controls steering, throttle and avoidance.
        // Native target-tracking branch 00428A61..00428A71 uses a NULL arrival
        // direction: let RoadNav choose the reachable end of the target road.
        // The old player's-heading constraint could require an unnecessary loop.
        const bool accepted=reinterpret_cast<bool(__thiscall*)(void*,const Vec3*,const Vec3*,bool)>(Address(0x00788F30))(
            ai.nav,&position,nullptr,false);
        if(!accepted) return false;
        g_battle.destinationSet=true;g_battle.lastDestination=position;
        g_battle.lastDestinationHeading=hint.heading;
        g_battle.routeOrigin=rival.position;g_battle.routeOriginSegment=segment;
        g_battle.routeProgress=0;g_battle.routeEndpointDistance=playerGap;
        g_battle.lastRouteRival=rival.position;g_battle.routeAge=0;g_battle.pathSeconds=0;
        Log(LogLevel::Info,"ENCOUNTER_ROUTE target=current-player nativeRoadNav=%p request=1 x=%.1f y=%.1f z=%.1f points=%u stalledRetry=%u arrivalDirection=unconstrained acceptedOnly=1 rival=%.1f,%.1f,%.1f speed=%.1f",
            ai.nav,position.x,position.y,position.z,unsigned(g_battle.routeTrail.size()),unsigned(stalled),
            rival.position.x,rival.position.y,rival.position.z,rival.speed);
        return true;
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        Log(LogLevel::Error,"ENCOUNTER_AI route exception=%08X",GetExceptionCode());return false;
    }
}

#include "EncounterCustomFollow.inl"

void* EncounterProfile() noexcept {
    NFSPluginSDK::MW05::cFrontEndDatabase* database=nullptr;
    if(!AudioRead(reinterpret_cast<void*>(Address(0x0091CF90)),&database)||!database) return nullptr;
    bool loaded=false;void* profile=nullptr;
    if(!AudioRead(&database->bProfileLoaded,&loaded)||!loaded||
        !AudioRead(&database->CurrentUserProfiles[0],&profile)) return nullptr;
    return profile;
}
bool AwardEncounterCash() noexcept {
    if(g_battle.model.phase()!=battle::Phase::Won||g_battle.rewardAttempted) return false;
    g_battle.rewardAttempted=true;
    if(!g_battle.profile||EncounterProfile()!=g_battle.profile) return false;
    auto* profile=static_cast<NFSPluginSDK::MW05::UserProfile*>(g_battle.profile);
    auto* cash=&profile->mTheCareerSettings.CurrentCash;
    std::int32_t before=0;
    const auto amount=g_battle.reward;
    if(amount<300||amount>3000||amount%100||!AudioRead(cash,&before)||before<0||before>INT32_MAX-int(amount)) return false;
    __try { *cash=before+int(amount); }
    __except(EXCEPTION_EXECUTE_HANDLER) { return false; }
    Log(LogLevel::Info,"ENCOUNTER_REWARD amount=%u BL=%u before=%d after=%d profile=%p diskSaveForced=0",amount,g_battle.blacklist,before,*cash,profile);
    return true;
}

battle::Car EncounterCar(const VehicleSnapshot& car) noexcept {
    return {{reinterpret_cast<std::uintptr_t>(car.pointer),reinterpret_cast<std::uintptr_t>(GetSimablePointer(car.pointer)),car.vehicleKey},
        {car.position.x,car.position.y,car.position.z},{car.heading.x,car.heading.y,car.heading.z},std::abs(car.speed),true};
}
bool QueueEncounterBattle(const VehicleSnapshot& player,const VehicleSnapshot& rival) noexcept {
    if(!g_battleSurface||EncounterBattleBusy()) return false;
    g_battle={};g_battle.player=EncounterCar(player).identity;g_battle.rival=EncounterCar(rival).identity;
    g_battle.pending=true;
    return true;
}
void EndEncounterBattle(bool restore,bool cancelVoice) noexcept {
    ClearEncounterPowerBoost();
    StopEncounterGuide(restore);g_guide={};
    ClearEncounterRaceSkill();
    if(g_battle.aiChanged&&restore) {
        const bool restored=NativeEncounterRestore(g_battle.rival);
        Log(LogLevel::Info,"ENCOUNTER_AI restored=%u rival=%p",unsigned(restored),reinterpret_cast<void*>(g_battle.rival.vehicle));
    }
    if(cancelVoice) QueueEncounterVoice(-1,0);
    g_gaugeVisible=false;g_battle={};g_encounterCandidate={};
    g_customDriveCalls=0;g_customRoadQueries=0;g_customSpeedCalls=0;g_customSpeedStops=0;
    g_customNativeSpeed=g_customAppliedSpeed=-1;
    g_encounterCooldownUntil=GetTickCount64()+8000;
}

bool EncounterBattlePaused() noexcept {
    // A menu suspends evaluation, but a hidden HUD / pursuit / crash camera
    // never cancels an already started battle. Start eligibility stays separate.
    void* frontend=nullptr;std::uint16_t flags=0;
    return !AudioRead(reinterpret_cast<void*>(Address(0x0091CB20)),&frontend)||!frontend||
        !AudioRead(static_cast<unsigned char*>(frontend)+0x1E,&flags)||flags!=0;
}
void EncounterMissingSample(float dt) noexcept {
    ClearEncounterPowerBoost();
    StopEncounterGuide(false);
    if(!EncounterBattleBusy()) return;
    if(g_battle.missingSeconds==0) {
        Log(LogLevel::Info,"ENCOUNTER_HOLD reason=missing-sample noResult=1");
        if(g_settings.encounterAIMode==encounter_custom::Mode::Custom) {
            g_battle.customTrail.Reset();g_battle.routeHint={};g_battle.destinationSet=false;
        }
    }
    g_battle.missingSeconds+=dt;g_gaugeVisible=false;
    // True vehicle destruction is distinct from a stopped or crashed live car.
    // Never read the cached vehicle in this branch.
    if(g_battle.missingSeconds>=10) {
        Log(LogLevel::Info,"ENCOUNTER_CANCEL reason=identity-missing-10s noResult=1");
        EndEncounterBattle(false,true);
    }
}

void UpdateEncounterBattleUnsafe(const VehicleSnapshot& legacyPlayer,const std::vector<VehicleSnapshot>& vehicles,float dt) noexcept {
    if(g_encounterEnabled&&g_battleSurface) EnsureEncounterGauge();
    if(!EncounterBattleBusy()) return;
    if(g_battle.weaponForfeit&&g_battle.model.phase()==battle::Phase::Active) {
        g_battle.model.Forfeit();
        ShowEncounterMessage(g_encounterHud,encounter_text::Id::Forfeit);
        QueueEncounterVoice(g_battle.actor,2);
        Log(LogLevel::Info,"ENCOUNTER_FINISH won=0 reward=0 reason=weapon-forfeit");
        EndEncounterBattle(true,false);return;
    }
    const auto found=std::find_if(vehicles.begin(),vehicles.end(),[](const auto& v) {
        return reinterpret_cast<std::uintptr_t>(v.pointer)==g_battle.rival.vehicle && v.vehicleKey==g_battle.rival.key;
    });
    if(found==vehicles.end()) {EncounterMissingSample(dt);return;}
    VehicleSnapshot player{},rivalGeometry{};
    if(!ReadEncounterGeometry(legacyPlayer,player,g_battle.pending)||!ReadEncounterGeometry(*found,rivalGeometry,g_battle.pending)) {
        EncounterMissingSample(dt);return;
    }
    const auto* rival=&rivalGeometry;
    if(EncounterCar(player).identity!=g_battle.player || EncounterCar(*rival).identity!=g_battle.rival) {
        Log(LogLevel::Info,"ENCOUNTER_CANCEL reason=identity-changed noResult=1");
        EndEncounterBattle(true,true);return;
    }
    const bool resumed=g_battle.missingSeconds>0;
    g_battle.missingSeconds=0;
    const bool paused=EncounterBattlePaused();
    const battle::Sample sample{EncounterCar(player),EncounterCar(*rival),true,paused||resumed};
    if(paused||resumed) {
        ClearEncounterPowerBoost();
        g_battle.model.Step(sample,dt);g_gaugeVisible=false;return;
    }
    if(g_battle.pending) {
        g_battle.pending=false;
        g_battle.profile=EncounterProfile();g_battle.aiChanged=true;
        unsigned char bin=0;
        if(g_battle.profile)AudioRead(&static_cast<NFSPluginSDK::MW05::UserProfile*>(g_battle.profile)->mTheCareerSettings.CurrentBin,&bin);
        g_battle.blacklist=bin;
        g_battle.reward=encounter_reward::Amount(g_settings.encounterAIMode==encounter_custom::Mode::Custom,bin);
        if(g_battle.model.Start(sample,g_battle.reward).event!=battle::Event::Started) {EndEncounterBattle(false,true);return;}
        if(!NativeEncounterStart(g_battle.rival)) {
            Log(LogLevel::Warning,"ENCOUNTER_CANCEL reason=native-start-rejected");
            EndEncounterBattle(true,true);return;
        }
        BindEncounterRaceSkill(g_battle.rival);
        g_battle.leadRoutePending=true;g_battle.routeSeconds=1;
        auto owner=std::find_if(g_racers.begin(),g_racers.end(),[](const auto& r) {
            return reinterpret_cast<std::uintptr_t>(r.pointer)==g_battle.rival.vehicle&&
                reinterpret_cast<std::uintptr_t>(r.simable)==g_battle.rival.simable&&r.vehicleKey==g_battle.rival.key;
        });
        if(owner!=g_racers.end() && g_voice && !g_voice->actors.empty()) {
            if(owner->encounterActor<0) owner->encounterActor=g_voice->actors[
                std::uniform_int_distribution<std::size_t>(0,g_voice->actors.size()-1)(g_random)];
            g_battle.actor=owner->encounterActor;
        }
        QueueEncounterVoice(g_battle.actor,0);
        ShowEncounterMessage(g_encounterHud,encounter_text::Id::Start);
        Log(LogLevel::Info,"ENCOUNTER_START rival=%p speaker=%02d leader=rival separation=300 reward=%u BL=%u nativeRaceStatusWrites=0",
            rival->pointer,g_battle.actor,g_battle.reward,g_battle.blacklist);
    } else {
        const auto change=g_battle.model.Step(sample,dt);
        if(change.event==battle::Event::Cancelled) {
            Log(LogLevel::Info,"ENCOUNTER_CANCEL reason=model-%u",unsigned(change.reason));
            EndEncounterBattle(true,true);return;
        }
        if(change.event==battle::Event::LeadChanged) {
            const bool playerLeads=g_battle.model.leader()==battle::Leader::Player;
            g_battle.destinationSet=false;g_battle.routeSeconds=1;
            g_battle.routeTrail.Reset();g_battle.routeHint={};g_battle.routeAge=0;
            g_battle.customTrail.Reset();g_battle.customWaiting=false;g_battle.customDirect=false;g_battle.customHintAt=0;
            g_battle.customSpeedDemand=g_battle.customLastSpeedRequest=g_battle.customAttackEntrySpeed=-1;
            g_battle.customAttackPace=false;g_battle.customAttackSeconds=0;
            g_battle.pursuit.Reset();g_battle.passActive=false;
            g_battle.leadRoutePending=!playerLeads;
            ShowEncounterMessage(g_encounterHud,playerLeads?encounter_text::Id::Lead:encounter_text::Id::Passed);
            Log(LogLevel::Info,"ENCOUNTER_LEAD leader=%s gap=%.1f",playerLeads?"player":"rival",g_battle.model.gap());
        }
        if(change.event==battle::Event::Won||change.event==battle::Event::Lost) {
            const bool won=change.event==battle::Event::Won;
            const bool paid=won&&change.cashIntent==g_battle.reward&&AwardEncounterCash();
            ShowEncounterMessage(g_encounterHud,won?(paid?encounter_text::Id::Victory:encounter_text::Id::PaymentFailed):encounter_text::Id::Defeat,g_battle.reward);
            QueueEncounterVoice(g_battle.actor,won?1:2);
            Log(LogLevel::Info,"ENCOUNTER_FINISH won=%u reward=%u gap=%.1f speaker=%02d",unsigned(won),paid?g_battle.reward:0u,g_battle.model.gap(),g_battle.actor);
            EndEncounterBattle(true,false);return;
        }
    }
    UpdateEncounterGuide(true,g_battle.model.leader()==battle::Leader::Player,g_battle.model.gap(),rival->position,GetTickCount64());
    UpdateEncounterPowerBoost(true,g_battle.model.leader()==battle::Leader::Player,g_battle.model.gap());
    if(g_battle.model.leader()==battle::Leader::Player) {
        g_battle.routeHint=g_battle.pursuit.Update({player.position.x,player.position.y,player.position.z},
            {player.heading.x,player.heading.y,player.heading.z},
            {rival->position.x,rival->position.y,rival->position.z},
            {rival->heading.x,rival->heading.y,rival->heading.z},dt,g_settings.encounterAIMode==encounter_custom::Mode::Custom);
        if(g_settings.encounterAIMode==encounter_custom::Mode::Custom) {
            const bool recorded=g_battle.customTrail.Record(
                {player.position.x,player.position.y,player.position.z},
                {player.heading.x,player.heading.y,player.heading.z});
            if(!recorded) g_battle.destinationSet=false;
            // Continue recording during attack; never replace the trajectory
            // with the current-player chase endpoint when attack expires.
            const auto trailHint=g_battle.customTrail.Select({rival->position.x,rival->position.y,rival->position.z},
                rival->speed,{rival->heading.x,rival->heading.y,rival->heading.z});
            g_battle.routeHint=g_battle.pursuit.passing()?encounter_custom::Destination{}:trailHint;
            g_battle.customHintAt=GetTickCount64();
            const bool attack=g_battle.pursuit.passing();
            if(attack&&!g_battle.customAttackPace) {
                g_battle.customAttackSeconds=0;
                g_battle.customAttackEntrySpeed=std::max(std::abs(rival->speed),
                    std::max(g_battle.customSpeedDemand,g_battle.customLastSpeedRequest));
            }
            const auto paceDirection=attack?battle::UnitXZ({player.heading.x,player.heading.y,player.heading.z}):trailHint.heading;
            g_battle.customSpeedDemand=(attack||trailHint.valid)?encounter_custom::ChaseSpeed(player.speed,g_battle.model.gap(),
                battle::DotXZ(battle::UnitXZ({rival->heading.x,rival->heading.y,rival->heading.z}),paceDirection),attack):-1;
            if(attack) {
                g_battle.customSpeedDemand=encounter_custom::AttackEntrySpeed(g_battle.customSpeedDemand,
                    g_battle.customAttackEntrySpeed,g_battle.customAttackSeconds);
                g_battle.customAttackSeconds+=std::isfinite(dt)?std::clamp(dt,0.f,.25f):0.f;
            }
            g_battle.customAttackPace=attack;
        }
        // Sample the 5m window at management frequency, not at the 1Hz replan
        // rate. Only the phase edge bypasses the normal request timer.
        if(g_battle.pursuit.passing()!=g_battle.passActive) g_battle.routeSeconds=1;
    }
    if(Distance(g_battle.lastRouteRival,rival->position)>=3) {g_battle.lastRouteRival=rival->position;g_battle.routeAge=0;}
    g_battle.routeAge+=dt;
    g_battle.pathSeconds+=dt;
    g_battle.routeProgress=Distance(g_battle.routeOrigin,rival->position);
    g_battle.routeEndpointDistance=Distance(g_battle.lastDestination,rival->position);
    g_battle.routeSeconds+=dt;
    const bool customFollow=g_settings.encounterAIMode==encounter_custom::Mode::Custom&&
        g_battle.model.leader()==battle::Leader::Player;
    if(g_battle.routeSeconds>=(customFollow?.25f:1.f)) {
        g_battle.routeSeconds=0;
        bool ready=true;
        if(customFollow) ready=NativeEncounterCustomFollow(*rival,!g_battle.destinationSet);
        else if(g_battle.model.leader()==battle::Leader::Player) ready=NativeEncounterFollow(*rival,!g_battle.destinationSet);
        else if(g_battle.leadRoutePending) {ready=NativeEncounterLead(g_battle.rival);if(ready) g_battle.leadRoutePending=false;}
        // A native Goal may temporarily have no Action while recovering. Route
        // availability is NOT a race-end condition. Retry without replacing AI.
        if(!ready) {
            if(g_battle.retryLogSeconds<=0) Log(LogLevel::Info,"ENCOUNTER_AI deferred=1 retry=1 battleContinues=1");
            g_battle.retryLogSeconds=5;
        }
    }
    g_battle.retryLogSeconds=std::max(0.0f,g_battle.retryLogSeconds-dt);
    g_gaugeGap=g_battle.model.gap();g_gaugeLead=g_battle.model.leader()==battle::Leader::Player;
    g_gaugeVisible.store(true,std::memory_order_release);
    g_battle.logSeconds+=dt;
    if(g_battle.logSeconds>=1) {
        LogEncounterPowerBoost();
        if(g_settings.encounterAIMode==encounter_custom::Mode::Custom)
            Log(LogLevel::Info,"ENCOUNTER_CUSTOM phase=%s points=%u cursor=%llu target=%llu valid=%u blocked=%u direct=%u driveCalls=%u roadQueriesBlocked=%u",
                g_battle.model.leader()!=battle::Leader::Player?"leader":(g_battle.pursuit.passing()?"attack":"trail"),
                unsigned(g_battle.customTrail.size()),g_battle.customTrail.cursor(),g_battle.customTrail.target(),
                unsigned(g_battle.routeHint.valid),unsigned(g_battle.customTrail.blocked()),unsigned(g_battle.customDirect),
                g_customDriveCalls.exchange(0),g_customRoadQueries.exchange(0));
        LogEncounterCatchup();
        if(g_settings.encounterAIMode==encounter_custom::Mode::Custom) {
            const unsigned calls=g_customSpeedCalls.exchange(0),stops=g_customSpeedStops.exchange(0);
            Log(LogLevel::Info,"ENCOUNTER_CUSTOM_SPEED raisedCalls=%u nativeStopCalls=%u nativeKmh=%.1f appliedKmh=%.1f demandKmh=%.1f",
                calls,stops,g_customNativeSpeed*3.6f,g_customAppliedSpeed*3.6f,g_battle.customSpeedDemand*3.6f);
            g_customNativeSpeed=g_customAppliedSpeed=-1;
        }
        g_battle.logSeconds=0;
        Log(LogLevel::Info,"ENCOUNTER_TICK leader=%s gap=%.1f playerSpeed=%.1f rivalSpeed=%.1f signedProgress=%.2f progressValid=%u roleTrusted=%u playerXYZ=%.2f/%.2f/%.2f rivalXYZ=%.2f/%.2f/%.2f playerHeading=%.3f/%.3f/%.3f rivalHeading=%.3f/%.3f/%.3f raceSkillCalls=%u",
            g_gaugeLead?"player":"rival",g_battle.model.gap(),player.speed*3.6f,rival->speed*3.6f,
            g_battle.model.signedProgress(),unsigned(g_battle.model.progressValid()),unsigned(g_battle.model.roleTrusted()),
            player.position.x,player.position.y,player.position.z,rival->position.x,rival->position.y,rival->position.z,
            player.heading.x,player.heading.y,player.heading.z,rival->heading.x,rival->heading.y,rival->heading.z,
            g_encounterSkillCalls.load(std::memory_order_relaxed));
        Log(LogLevel::Info,"ENCOUNTER_AUX mapSuppressed=%u nativeTrailRequests=%u trailPoints=%u",
            g_encounterMapSuppressed.load(),g_encounterNativeTrailRequests.load(),unsigned(g_battle.routeTrail.size()));
    }
}

void UpdateEncounterBattle(const VehicleSnapshot& player,const std::vector<VehicleSnapshot>& vehicles,float dt) noexcept {
    g_encounterAdapterThread.store(GetCurrentThreadId(),std::memory_order_release);
    __try {UpdateEncounterBattleUnsafe(player,vehicles,dt);}
    __except(EXCEPTION_EXECUTE_HANDLER) {
        g_battleSurface=false;
        Log(LogLevel::Error,"ENCOUNTER exception=%08X battleDisabled=1 populationUnaffected=1",GetExceptionCode());
        EndEncounterBattle(true,true);
    }
}
