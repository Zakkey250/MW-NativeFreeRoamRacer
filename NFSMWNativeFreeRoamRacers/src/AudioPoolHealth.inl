// alpha.75: bounded, read-only allocation evidence for the optional six-slot test.
// No audio calls, new hooks, object reassignment, cached native pointers or LB polls.
struct AudioPoolHealth {
    const char* status="unreadable";
    unsigned slots=0,active=0,bound=0;
    bool complete=false;
};
AudioPoolHealth ReadAudioPoolHealth(std::uintptr_t pool,const AudioPoolContext& c) noexcept {
    AudioPoolHealth h;
    if((c.valid&0xFF)!=0xFF)return h;
    h.status="foreign-pool";
    if(!pool||pool!=c.global||c.vt!=Address(kAudioRacerPoolVtable)||c.type!=3||c.kind!=1)return h;
    h.status="not-ready";
    if(!c.active)return h;
    h.status="count-limit";
    constexpr unsigned limit=16; // Observation bound only, never an allocation capacity.
    if(c.count>limit)return h;
    std::array<std::uintptr_t,limit> seen{},owners{};
    auto node=c.head;
    h.status="inconsistent-list";
    while(node&&h.slots<c.count) {
        for(unsigned i=0;i<h.slots;++i)if(seen[i]==node){h.status="cycle";return h;}
        seen[h.slots]=node;
        std::uintptr_t vt=0,next=0,owner=0;
        std::uint8_t active=0;
        if(!AudioRead(node,0,&vt)||vt!=Address(0x00896FA8)||!AudioRead(node,4,&next)||
            !AudioRead(node,0x34,&owner)||!AudioRead(node,0x38,&active)){
            h.status="unreadable-or-foreign-node";return h;
        }
        if(active) {
            ++h.active;
            if(owner){
                for(unsigned i=0;i<h.bound;++i)if(owners[i]==owner){h.status="duplicate-active-owner";return h;}
                owners[h.bound++]=owner; // Identity only; never follow the audio object.
            }
        }
        ++h.slots;node=next;
    }
    if(node||h.slots!=c.count)return h;
    // Reject a manager/list replacement during this observation, not gameplay.
    const auto after=ReadAudioPoolContext(pool);
    if((after.valid&0xFF)!=0xFF||after.global!=c.global||after.vt!=c.vt||
        after.type!=c.type||after.kind!=c.kind||after.count!=c.count||
        after.head!=c.head||after.active!=c.active){h.status="changed-during-read";return h;}
    h.complete=true;h.status=h.slots?"enumerated":"empty";return h;
}
bool AudioHealthSampleDue(bool enabled,ULONGLONG now,ULONGLONG* next) noexcept {
    if(!enabled||now<*next)return false;
    *next=now+5000;return true;
}
ULONGLONG g_audioHealthNext=0;
unsigned g_audioHealthPeakBound=0;
void ResetAudioPoolHealth() noexcept {g_audioHealthNext=0;g_audioHealthPeakBound=0;}
void LogAudioPoolHealth() noexcept {
    if(!AudioHealthSampleDue(g_settings.enabled&&g_settings.lbEngineAudioExpansion&&
        g_audioPoolHookEnabled,GetTickCount64(),&g_audioHealthNext))return;
    std::uintptr_t pool=0;
    AudioRead(Address(kAudioRacerPoolGlobal),0,&pool);
    const auto c=ReadAudioPoolContext(pool);
    const auto h=ReadAudioPoolHealth(pool,c);
    const auto ceiling=g_audioSlotCeiling.load();
    const unsigned requested=g_settings.freeRoamAudioSlots?
        std::min(ceiling==6?6u:g_settings.freeRoamAudioSlots,static_cast<unsigned>(g_settings.maximumRacers)):0u;
    const bool roaming=(c.valid&0x380)==0x380&&c.raceStatus&&c.playMode==0&&!c.raceParms;
    const bool confirmed=h.complete&&roaming&&requested!=0&&h.slots==requested;
    if(h.complete&&roaming)g_audioHealthPeakBound=std::max(g_audioHealthPeakBound,h.bound);
    Log(LogLevel::Info,"AUDIO_POOL_HEALTH ceiling=%u requested=%u allocated=%u active=%u bound=%u peakBound=%u complete=%u allocationMatch=%u status=%s roaming=%u budget=%u lastChosen=%d lastReason=%u readOnly=1 audibleOutputVerified=0",
        ceiling,requested,h.slots,h.active,h.bound,g_audioHealthPeakBound,h.complete?1u:0u,
        confirmed?1u:0u,h.status,roaming?1u:0u,c.budget,g_audioPoolChosenCount.load(),g_audioPoolReason.load());
}
