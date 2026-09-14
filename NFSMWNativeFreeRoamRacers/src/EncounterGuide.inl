// Native GPS lease. No map destination/trigger is fabricated, and a player or
// another mod changing the GPS target revokes our lease for this battle.
using GuideEngageFn=bool(__cdecl*)(const Vec3*,float);
using GuideDisengageFn=void(__cdecl*)();
GuideEngageFn g_guideEngage=nullptr;
GuideDisengageFn g_guideDisengage=nullptr;
bool g_guideSurface=false;
struct GuideSnapshot {void* object=nullptr;Vec3 target{};unsigned state=0;float deviation=0;};
struct GuideLease {
    GuideSnapshot previous{};
    Vec3 issued{};
    bool owned=false,blocked=false,recovering=false;
    ULONGLONG nextRequest=0,nextDiagnostic=0;
};
GuideLease g_guide;
bool ReadEncounterGuide(GuideSnapshot& s) noexcept {
    void* object=nullptr;
    if(!AudioRead(reinterpret_cast<void*>(Address(0x0090D8E4)),&object)||
        !BattleVtable(object,0x00891204)) return false;
    auto* p=static_cast<unsigned char*>(object);
    s.object=object;
    return AudioRead(p+0x48,&s.target)&&AudioRead(p+0x74,&s.state)&&s.state<=2&&
        AudioRead(p+0x370,&s.deviation)&&
        std::isfinite(s.target.x)&&std::isfinite(s.target.y)&&std::isfinite(s.target.z)&&
        std::isfinite(s.deviation)&&s.deviation>=0&&s.deviation<=10000;
}
bool OwnEncounterGuide(const GuideSnapshot& s) noexcept {
    return g_guide.owned&&s.object==g_guide.previous.object&&Distance(s.target,g_guide.issued)<0.01f;
}
void StopEncounterGuide(bool restore) noexcept {
    if(!g_guide.owned) return;
    GuideSnapshot current{};
    const auto previous=g_guide.previous;
    const bool own=ReadEncounterGuide(current)&&OwnEncounterGuide(current);
    g_guide.owned=false;
    if(!own) {g_guide.blocked=true;return;}
    __try {
        if(g_guideDisengage) g_guideDisengage();
        if(restore&&previous.state&&g_guideEngage) {
            const bool ok=g_guideEngage(&previous.target,previous.deviation);
            Log(LogLevel::Info,"ENCOUNTER_GPS restorePrevious=%u",unsigned(ok));
        }
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        g_guide.blocked=true;
        Log(LogLevel::Warning,"ENCOUNTER_GPS stop exception=%08X battleUnaffected=1",GetExceptionCode());
    }
}
bool EncounterGuideWanted(bool active,bool playerLeads,float gap) noexcept {
    return active&&!playerLeads&&std::isfinite(gap)&&gap>=100;
}
void UpdateEncounterGuide(bool active,bool playerLeads,float gap,const Vec3& rival,ULONGLONG now) noexcept {
    if(!g_guideSurface||g_guide.blocked) return;
    if(!EncounterGuideWanted(active,playerLeads,gap)) {StopEncounterGuide(true);return;}
    GuideSnapshot current{};
    if(!ReadEncounterGuide(current)) {
        // A transient read failure is not proof of a new owner. Retain our
        // original restore target, perform no writes, and retry on later ticks.
        if(now>=g_guide.nextDiagnostic) {
            Log(LogLevel::Info,"ENCOUNTER_GPS stateUnavailable=1 retry=1 preserveLease=%u",unsigned(g_guide.owned));
            g_guide.nextDiagnostic=now+5000;
        }
        return;
    }
    if(g_guide.owned&&!OwnEncounterGuide(current)) {
        g_guide.owned=false;g_guide.blocked=true;
        Log(LogLevel::Info,"ENCOUNTER_GPS externalTargetChange=1 preserveExternal=1");return;
    }
    if(g_guide.owned&&!current.state) {
        // Native GPS can finish its fixed target while the rival moves on.
        // Same object + exact owned target still required. During the battle,
        // auto-guide takes precedence over cancellation of this same target;
        // a genuinely different external destination above remains untouched.
        if(!g_guide.recovering) Log(LogLevel::Info,"ENCOUNTER_GPS inactiveOwned=1 retry=1 gap=%.1f",gap);
        g_guide.recovering=true;
    } else g_guide.recovering=false;
    if(now<g_guide.nextRequest||!std::isfinite(rival.x)||!std::isfinite(rival.y)||!std::isfinite(rival.z)) return;
    if(g_guide.owned&&current.state&&Distance(g_guide.issued,rival)<25) return;
    if(!g_guide.owned) g_guide.previous=current;
    g_guide.issued=rival;g_guide.owned=true;g_guide.nextRequest=now+2000;
    __try {
        const auto begin=GetTickCount64();
        const bool ok=g_guideEngage&&g_guideEngage(&rival,0.0f);
        const auto elapsed=GetTickCount64()-begin;
        // Native GPS uses a synchronous path search. Back off if that search
        // proved expensive; never request on every management/render frame.
        if(elapsed>30) g_guide.nextRequest=now+5000;
        Log(LogLevel::Info,"ENCOUNTER_GPS guide=%u gap=%.1f target=%.1f/%.1f/%.1f requestMs=%llu",
            unsigned(ok),gap,rival.x,rival.y,rival.z,elapsed);
        if(!ok) StopEncounterGuide(true);
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        StopEncounterGuide(false);g_guide.blocked=true;
        Log(LogLevel::Warning,"ENCOUNTER_GPS engage exception=%08X battleUnaffected=1",GetExceptionCode());
    }
}
void InitializeEncounterGuide() noexcept {
    struct Guard {std::uintptr_t va;const char* bytes;unsigned size;};
    const Guard guards[]={
        {0x0042C830,"\x8b\x0d\xe4\xd8\x90\x00\x85\xc9",8},
        {0x0042C842,"\x50\x52\xe8\x77\xfe\xff\xff\xc3",8},
        {0x0041ACE0,"\xa1\xe4\xd8\x90\x00\x85\xc0\x74\x07\xc7\x40\x74\x00\x00\x00\x00\xc3",17},
        {0x0042C6C0,"\x53\x55\x56\x8b\xf1\x8b\x4c\x24\x10",9},
        {0x0042C6CE,"\x8b\x11\x8d\x46\x48\x89\x10",7},
        {0x0042C800,"\xc7\x46\x74\x02\x00\x00\x00",7},
        {0x0042C816,"\xc2\x08\x00",3},
        {0x0041A62F,"\xc7\x06\x04\x12\x89\x00",6}
    };
    if(!g_battleSurface||!g_settings.encounterSignalEnabled) return;
    for(const auto& guard:guards) if(std::memcmp(reinterpret_cast<void*>(Address(guard.va)),guard.bytes,guard.size)) {
        Log(LogLevel::Warning,"ENCOUNTER_GPS surfaceRejected=%08X battleUnaffected=1",unsigned(guard.va));return;
    }
    g_guideEngage=reinterpret_cast<GuideEngageFn>(Address(0x0042C830));
    g_guideDisengage=reinterpret_cast<GuideDisengageFn>(Address(0x0041ACE0));
    g_guideSurface=true;
    Log(LogLevel::Info,"ENCOUNTER_GPS native=1 threshold=100 refreshMaxHz=0.5 preserveExisting=1");
}
