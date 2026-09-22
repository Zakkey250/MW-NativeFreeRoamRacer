template<class Expect>
__declspec(noinline) void RunAlpha82Tests(Expect expect) {
    const auto settings=g_settings;
    auto scratch=std::move(g_vehicleScratch);auto racers=std::move(g_racers);
    const auto report=g_reclaimDiagnosticAt;
    g_reclaimDiagnosticAt=~ULONGLONG{0};g_settings.lbNonRacerSlots=6;
    g_settings.anchorMinimumMeters=200;g_settings.anchorMinimumSpeedMps=1;
    VehicleSnapshot player{},anchor{};
    std::uintptr_t playerMemory=0,anchorMemory=0;
    player.pointer=&playerMemory;anchor.pointer=&anchorMemory;
    unsigned counts=0;
    auto count=[&]() noexcept {++counts;return 10u;};
    expect(!ReclaimOneTrafficWithCount(player,anchor,count)&&counts==0,
        "alpha82 empty traffic does not run fresh census");
    std::array<std::uintptr_t,127> invalidVehicles{};
    for(unsigned population:{20u,32u,64u,127u}) {
        g_vehicleScratch.assign(population,{});g_racers.clear();counts=0;
        for(unsigned i=0;i<population;++i){auto& v=g_vehicleScratch[i];
            v.pointer=&invalidVehicles[i];v.driverClass=kDriverTraffic;
            v.position={400,0,0};v.onScreenTime=1;v.offScreenTime=0;v.speed=10;}
        for(unsigned attempt=0;attempt<10;++attempt)
            expect(!ReclaimOneTrafficWithCount(player,anchor,count)&&counts==0,
                "alpha82 repeated full-pool waits with visible traffic perform no census");
        for(auto& v:g_vehicleScratch){v.onScreenTime=0;v.offScreenTime=3;v.position.x=10;}
        expect(!ReclaimOneTrafficWithCount(player,anchor,count)&&counts==0,
            "alpha82 too-near candidates do not trigger fresh census");
        g_vehicleScratch.back().position.x=400;
        expect(!ReclaimOneTrafficWithCount(player,anchor,count)&&counts==1,
            "alpha82 first geometrically safe candidate triggers census but invalid native victim still rejected");
        expect(!ReclaimOneTrafficWithCount(player,anchor,count)&&counts==2,
            "alpha82 next attempt recounts fresh rather than caching traffic count");
        for(auto& v:g_vehicleScratch)v.position.x=400;
        counts=0;
        expect(!ReclaimOneTrafficWithCount(player,anchor,count)&&counts==1,
            "alpha82 many failed final native checks still invoke only one census per attempt");
        g_settings.lbNonRacerSlots=0;counts=0;
        expect(!ReclaimOneTrafficWithCount(player,anchor,count)&&counts==0,
            "alpha82 optional headroom OFF retains no-census behavior");
        g_settings.lbNonRacerSlots=6;
        g_racers.resize(population);
        for(unsigned i=0;i<population;++i)g_racers[i].pointer=g_vehicleScratch[i].pointer;
        expect(!ReclaimOneTrafficWithCount(player,anchor,count)&&counts==0,
            "alpha82 NFRR-owned candidates remain protected before census");
    }
    g_racers.clear();g_vehicleScratch.resize(2);
    g_vehicleScratch[0].pointer=player.pointer;g_vehicleScratch[1].pointer=anchor.pointer;counts=0;
    expect(!ReclaimOneTrafficWithCount(player,anchor,count)&&counts==0,
        "alpha82 player and active spawn anchor never initiate reclaim census");
    g_vehicleScratch.clear();
    expect(CountMovingOrdinaryTraffic()==0,"alpha82 production census handles empty snapshot");
    VehicleSnapshot invalid{};invalid.pointer=invalidVehicles.data();invalid.driverClass=kDriverTraffic;
    g_vehicleScratch.push_back(invalid);
    expect(CountMovingOrdinaryTraffic()==0,"alpha82 unreadable native vehicle is never counted as moving");
    g_settings=settings;g_vehicleScratch=std::move(scratch);g_racers=std::move(racers);g_reclaimDiagnosticAt=report;
}
