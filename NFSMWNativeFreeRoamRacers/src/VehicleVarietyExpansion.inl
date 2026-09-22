// One extra model type, not an extra vehicle or sound slot. Optional and
// capability-based; native audio must already have prepared a real six-node pool.
constexpr std::size_t kExpandedVehicleModelLimit=6;
bool g_vehicleVarietyLocked=false;
bool g_vehicleVarietyExpanded=false;
void ResetVehicleVarietyExpansion() noexcept {
    g_vehicleVarietyLocked=false;g_vehicleVarietyExpanded=false;
}
bool CanExpandVehicleVariety(bool optIn,bool audioOptIn,unsigned audioSlots,
                            bool frozen,unsigned audioCeiling,bool hook,
                            const AudioPoolContext& c,const AudioPoolHealth& h) noexcept {
    return optIn&&audioOptIn&&audioSlots!=0&&frozen&&audioCeiling==6&&hook&&
        (c.valid&0x780)==0x780&&c.raceStatus&&c.playMode==0&&!c.raceParms&&
        c.budget>=6&&c.budget<=64&&h.complete&&h.slots==6;
}
std::size_t VehicleModelLimit() noexcept {return g_vehicleVarietyExpanded?6u:5u;}
std::size_t EffectiveVehicleVariety() noexcept {
    return g_vehicleVarietyExpanded?6u:std::clamp<std::size_t>(g_settings.vehicleVariety,1,5);
}
template<class ReadPool>
void LockVehicleVarietyWith(ReadPool readPool) noexcept {
    if(g_vehicleVarietyLocked||!g_settings.lbVehicleVarietyExpansion)return;
    // First palette of this world only. Late availability never widens an
    // existing palette and retained/loaded cars are never forcibly destroyed.
    g_vehicleVarietyLocked=true;
    AudioPoolContext c{};AudioPoolHealth h{};
    if(g_settings.lbEngineAudioExpansion&&g_settings.freeRoamAudioSlots&&
        g_audioPreparationFrozen.load()&&g_audioSlotCeiling.load()==6&&g_audioPoolHookEnabled)
        readPool(&c,&h);
    g_vehicleVarietyExpanded=CanExpandVehicleVariety(g_settings.lbVehicleVarietyExpansion,
        g_settings.lbEngineAudioExpansion,g_settings.freeRoamAudioSlots,
        g_audioPreparationFrozen.load(),g_audioSlotCeiling.load(),g_audioPoolHookEnabled,c,h);
    Log(LogLevel::Info,"VEHICLE_VARIETY_POLICY optIn=1 requested=6 effective=%u poolSlots=%u poolComplete=%u audioCeiling=%u worldFrozen=1 latePromotion=0 playerIncluded=1 versionGate=0",
        static_cast<unsigned>(EffectiveVehicleVariety()),h.slots,h.complete?1u:0u,g_audioSlotCeiling.load());
}
void LockVehicleVarietyForWorld() noexcept {
    LockVehicleVarietyWith([](AudioPoolContext* c,AudioPoolHealth* h) noexcept {
        std::uintptr_t pool=0;AudioRead(Address(kAudioRacerPoolGlobal),0,&pool);
        *c=ReadAudioPoolContext(pool);*h=ReadAudioPoolHealth(pool,*c);
    });
}
