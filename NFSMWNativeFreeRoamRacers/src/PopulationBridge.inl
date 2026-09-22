// Optional capability API, not a release/version dependency. Module references
// are held only across the call, so a cached function pointer cannot go stale.
unsigned QueryPopulation(unsigned missing,nfsmw_population::Status* status) noexcept {
    HMODULE module=nullptr;
    if(!GetModuleHandleExW(0,L"NFSMWLimitBreaker.asi",&module))return nfsmw_population::Unavailable;
    unsigned result=nfsmw_population::Unavailable;
    __try {
        const auto reserve=reinterpret_cast<nfsmw_population::ReserveFn>(GetProcAddress(module,nfsmw_population::exportName));
        if(reserve)result=reserve(0x4E465252u,missing,status);
    } __except(EXCEPTION_EXECUTE_HANDLER) {result=nfsmw_population::Unsafe;}
    FreeLibrary(module);
    return result;
}
PopulationTransaction g_populationTransaction;
PopulationReleaseState g_populationRelease;
struct PopulationApiCounters {
    std::uint64_t idle=0,reserve=0,release=0,releaseSkipped=0;
};
PopulationApiCounters g_populationApiCounters;
ULONGLONG g_reclaimDiagnosticAt=0;
// Injectable callables are used by offline tests only; the production caller
// still resolves the live export per call and never caches provider status.
template<typename Query>
PopulationAccess TrackedPopulationCapacity(nfsmw_population::Status& status,unsigned requested,
                                          ULONGLONG now,Query query) noexcept {
    const auto result=query(requested,&status);
    const auto access=PopulationDecision(result,status,requested);
    g_populationRelease.Observe(requested,result,access,now);
    if(requested)++g_populationApiCounters.reserve;else ++g_populationApiCounters.idle;
    return access;
}
PopulationAccess PopulationCapacity(nfsmw_population::Status& status,unsigned requested=0) noexcept {
    return TrackedPopulationCapacity(status,requested,GetTickCount64(),QueryPopulation);
}
template<typename Query>
void ReleasePopulationReservationWith(ULONGLONG now,Query query) noexcept {
    g_populationTransaction.Reset();
    if(!g_populationRelease.ShouldRelease(now)) {++g_populationApiCounters.releaseSkipped;return;}
    g_populationRelease.AttemptedRelease(now);
    nfsmw_population::Status status;
    const auto result=query(0,&status);
    g_populationRelease.Observe(0,result,PopulationDecision(result,status,0),now);
    ++g_populationApiCounters.release;
}
void ReleasePopulationReservation() noexcept {
    ReleasePopulationReservationWith(GetTickCount64(),QueryPopulation);
}
struct PopulationAttemptScope {
    ~PopulationAttemptScope() {
        if(!g_populationTransaction.Awaiting(GetTickCount64()))ReleasePopulationReservation();
    }
};
ULONGLONG g_nativeHeadroomDiagnosticAt=0;
bool DeferForNativeHeadroom(PopulationAccess access,const nfsmw_population::Status& status) noexcept {
    if(!NativeHeadroomReached(access,status.limit,g_racers.size(),g_settings.lbNonRacerSlots))return false;
    const auto now=GetTickCount64();
    if(now>=g_nativeHeadroomDiagnosticAt){
        g_nativeHeadroomDiagnosticAt=now+5000;
        Log(LogLevel::Info,"LB_HEADROOM_WAIT managed=%u limit=%u nonRacerShare=%u configured=%u live=%u free=%u extraReservation=0 existingRacersUntouched=1 resume=next-due-check",
            static_cast<unsigned>(g_racers.size()),status.limit,EffectiveNativeHeadroom(g_settings.lbNonRacerSlots,status.limit),
            g_settings.lbNonRacerSlots,status.live,status.freeSlots);
    }
    return true;
}
bool ReclaimableTraffic(const VehicleSnapshot& v,const VehicleSnapshot& player,const VehicleSnapshot& anchor) noexcept {
    const float distance=Distance(player.position,v.position);
    return v.pointer&&v.pointer!=anchor.pointer&&v.pointer!=player.pointer&&
        v.driverClass==kDriverTraffic&&std::isfinite(distance)&&
        distance>=std::max(150.0f,g_settings.anchorMinimumMeters)&&
        std::isfinite(v.onScreenTime)&&std::isfinite(v.offScreenTime)&&
        v.onScreenTime<=0.0f&&v.offScreenTime>=3.0f;
}
unsigned CountMovingOrdinaryTraffic() noexcept {
    unsigned moving=0;
    for(const auto& v:g_vehicleScratch){
        VehicleSnapshot current;
        if(v.pointer&&v.driverClass==kDriverTraffic&&ReadVehicle(v.pointer,&current)&&current.vehicleKey==v.vehicleKey&&
            current.driverClass==kDriverTraffic&&std::isfinite(current.speed)&&
            current.speed>=g_settings.anchorMinimumSpeedMps&&!IsAdvancedCivilianVehicle(v.pointer)&&
            std::none_of(g_racers.begin(),g_racers.end(),[&](const ManagedRacer& r){return r.pointer==v.pointer;}))++moving;
    }
    return moving;
}
template<class CountMoving>
bool ReclaimOneTrafficWithCount(const VehicleSnapshot& player,const VehicleSnapshot& anchor,
                              CountMoving countMoving) noexcept {
    unsigned traffic=0,safeGeometry=0,special=0,owned=0,moving=0,preserved=0;
    bool movingMeasured=false;
    for(const auto& v:g_vehicleScratch){
        if(v.driverClass==kDriverTraffic)++traffic;
        if(!ReclaimableTraffic(v,player,anchor))continue;
        ++safeGeometry;
        if(IsAdvancedCivilianVehicle(v.pointer)){++special;continue;}
        if(std::any_of(g_racers.begin(),g_racers.end(),[&](const ManagedRacer& r){return r.pointer==v.pointer;})){++owned;continue;}
        // Only read all moving traffic if this call has a possible victim.
        // No cached count across attempts. Count BEFORE the existing final
        // victim identity/geometry reads; those safety checks stay freshest.
        if(g_settings.lbNonRacerSlots&&!movingMeasured) {
            moving=countMoving();movingMeasured=true;
        }
        void* simable=GetSimablePointer(v.pointer);
        VehicleSnapshot fresh;
        if(!simable||!ReadVehicle(v.pointer,&fresh)||fresh.vehicleKey!=v.vehicleKey||
           GetSimablePointer(v.pointer)!=simable||!ReclaimableTraffic(fresh,player,anchor)||
           IsAdvancedCivilianVehicle(v.pointer))continue;
        if(g_settings.lbNonRacerSlots&&(!std::isfinite(fresh.speed)||
            !MayReclaimWithNativeHeadroom(g_settings.lbNonRacerSlots,moving,fresh.speed>=g_settings.anchorMinimumSpeedMps))){++preserved;continue;}
        const bool killed=KillVehicle(v.pointer,"optional LB off-screen traffic reservation");
        Log(LogLevel::Info,"LB_POPULATION_RECLAIM vehicle=%p success=%u distance=%.1f offScreen=%.1f constructionThisTick=0",
            v.pointer,killed?1u:0u,Distance(player.position,fresh.position),fresh.offScreenTime);
        return killed; // At most one attempt, then wait for real vacancy.
    }
    const auto now=GetTickCount64();
    if(now>=g_reclaimDiagnosticAt){
        g_reclaimDiagnosticAt=now+5000;
        Log(LogLevel::Info,"LB_POPULATION_WAIT reason=no-safe-traffic-victim traffic=%u distantOffscreen=%u special=%u managed=%u visibleRemoval=0 movingTraffic=%u protectedMoving=%u headroomOptIn=%u movingCountMeasured=%u",
            traffic,safeGeometry,special,owned,moving,preserved,g_settings.lbNonRacerSlots,movingMeasured?1u:0u);
    }
    return false;
}
bool ReclaimOneTraffic(const VehicleSnapshot& player,const VehicleSnapshot& anchor) noexcept {
    return ReclaimOneTrafficWithCount(player,anchor,CountMovingOrdinaryTraffic);
}
