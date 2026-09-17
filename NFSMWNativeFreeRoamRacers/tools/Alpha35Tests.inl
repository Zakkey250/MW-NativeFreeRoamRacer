    {
        auto savedRacers=g_racers;g_racers.clear();g_battle={};ResetEncounterSignal();
        FakeVehicle tuned{table,0x2A65A04Fu,kDriverRacer,&simable}; // reported CELICA instance
        SpawnIdentity captured{};
        expect(CaptureSpawnIdentity(&tuned,&captured)&&captured.key==tuned.key&&captured.simable==&simable,
            "alpha35 captures tuned runtime key and simable instead of stock CELICA A39413FC");
        VehicleSnapshot player{},rival{};
        player.pointer=&managed;player.driverClass=kDriverHuman;player.heading={0,0,1};player.speed=40;
        rival.pointer=&tuned;rival.driverClass=kDriverRacer;rival.vehicleKey=tuned.key;
        rival.heading={0,0,1};rival.speed=40;rival.position={0,0,30}; // Matched speed required since alpha.58.
        std::vector<VehicleSnapshot> vehicles{player,rival};
        ManagedRacer owned{};owned.pointer=&tuned;owned.simable=captured.simable;owned.vehicleKey=0xA39413FC;
        expect(!FindLive(vehicles,owned),"alpha35 reproduces alpha34 stock-key mismatch despite live racer at 30m");
        owned.vehicleKey=captured.key;g_racers.push_back(owned);
        expect(FindLive(vehicles,g_racers[0])==&vehicles[1],"alpha35 tuned identity remains visible to population management");
        nativeVote=1;g_originalCacheQuery=reinterpret_cast<CacheQueryFn>(&FakeNativeQuery);g_streamRetentionEnabled=true;
        ProtectManagedVehicle(0,&tuned,captured.simable,captured.key);
        expect(CacheQueryHook(nullptr,nullptr,&tuned,&asking)==0,"alpha35 tuned racer is retained by native cache ownership");
        std::array<std::uint8_t,0x400> hud{};std::array<std::uint8_t,0x2000> race{};std::array<std::uint8_t,0x40> frontend{};
        setGlobal(kGameFlowState,6u);setGlobal(kRaceStatus,reinterpret_cast<std::uintptr_t>(race.data()));
        setGlobal(0x0091CB20,reinterpret_cast<std::uintptr_t>(frontend.data()));put(hud,0x18,std::uint64_t{1});
        g_encounterHud=hud.data();g_encounterEnabled=true;g_encounterFaulted=false;
        for(unsigned i=0;i<50;++i) {
            g_encounterHudTick=GetTickCount64();
            UpdateEncounterCandidateUsing(player,vehicles,.05f,[](const auto& in,auto& out){out=in;return true;});
        }
        expect(g_encounterCandidate.ready&&g_encounterCandidate.key==tuned.key,
            "alpha35 production start-candidate logic arms tuned rival across 50 management updates");
        battle::Sample s{};s.player.identity={1,2,3};s.rival.identity={4,5,6};s.player.forward=s.rival.forward={0,0,1};s.rival.position.z=30;
        g_battle.model.Start(s);g_battle.rival={reinterpret_cast<std::uintptr_t>(&tuned),reinterpret_cast<std::uintptr_t>(&simable),captured.key};
        expect(EncounterMarkerAllowed(g_racers[0]),"alpha35 opponent marker recognizes tuned identity");
        ++tuned.key;vehicles[1].vehicleKey=tuned.key;
        expect(!FindLive(vehicles,g_racers[0])&&CacheQueryHook(nullptr,nullptr,&tuned,&asking)==1,
            "alpha35 recycled key still loses management and cache retention; no pointer-only relaxation");
        g_battle={};g_encounterHudTick=GetTickCount64();
        UpdateEncounterCandidateUsing(player,vehicles,.05f,[](const auto& in,auto& out){out=in;return true;});
        expect(!g_encounterCandidate.ready,"alpha35 recycled instance cannot inherit start readiness");
        --tuned.key;tuned.simable=&asking;
        expect(CacheQueryHook(nullptr,nullptr,&tuned,nullptr)==1,"alpha35 changed simable is not retained even with matching tuned key");
        tuned.simable=nullptr;captured.key=123;
        expect(!CaptureSpawnIdentity(&tuned,&captured)&&!captured.key&&!captured.simable,"alpha35 missing physical owner rejects spawn transactionally");
        tuned.simable=&simable;tuned.driver=kDriverTraffic;
        expect(!CaptureSpawnIdentity(&tuned,&captured),"alpha35 traffic cannot become managed opponent through identity capture");
        tuned.driver=kDriverRacer;tuned.key=0;
        expect(!CaptureSpawnIdentity(&tuned,&captured),"alpha35 zero instance key rejects incomplete spawn");
        expect(!CaptureSpawnIdentity(nullptr,&captured)&&!CaptureSpawnIdentity(&tuned,nullptr),"alpha35 null identity inputs fail closed");
        UnprotectManagedVehicle(0,&tuned,&simable);g_racers=std::move(savedRacers);ResetEncounterSignal();g_encounterEnabled=false;
    }
