    {
        using roaming_pace::Parse;
        using roaming_pace::CruiseSpeed;
        using roaming_pace::SpeedMatched;
        expect(Parse(L"60",60,1,100)==60&&Parse(L"35.5 ; test",60,1,100)==35.5f,"alpha58 percentage parses decimal and comments");
        for(auto text:{L"",L"NaN",L"inf",L"60junk",L"60,5"})
            expect(Parse(text,60,1,100)==60,"alpha58 invalid percentage defaults to 60");
        expect(Parse(L"0",60,1,100)==1&&Parse(L"120",60,1,100)==100,"alpha58 percentage bounds 1..100");
        expect(Parse(L"",10,0,100)==10&&Parse(L"-2",10,0,100)==0,"alpha58 tolerance fallback and lower bound");
        expect(std::abs(CruiseSpeed(50,60)-30)<.001f&&CruiseSpeed(50,100)==50,"alpha58 normal target reduced to 60 percent; 100 disables");
        expect(CruiseSpeed(0,60)==0&&CruiseSpeed(-5,60)==-5&&CruiseSpeed(.1f,60)==.1f,"alpha58 stop reverse recovery sentinels unchanged");
        expect(std::isnan(CruiseSpeed(NAN,60))&&CruiseSpeed(50,NAN)==50,"alpha58 invalid speed/config forwarded unchanged");
        expect(SpeedMatched(60/3.6f,70/3.6f,10)&&SpeedMatched(70/3.6f,60/3.6f,10),"alpha58 inclusive plus/minus 10kmh boundary");
        expect(!SpeedMatched(60/3.6f,70.01f/3.6f,10)&&!SpeedMatched(70.01f/3.6f,60/3.6f,10),"alpha58 either side outside tolerance rejected");
        expect(SpeedMatched(0,0,10)&&SpeedMatched(20,20,0)&&!SpeedMatched(20,21,0),"alpha58 zero speed allowed and zero tolerance exact");
        expect(!SpeedMatched(NAN,20,10)&&!SpeedMatched(20,-1,10),"alpha58 invalid snapshots rejected");
        const auto oldTolerance=g_settings.startSpeedToleranceKmh;
        g_settings.startSpeedToleranceKmh=10;
        VehicleSnapshot p{},r{};p.pointer=&p;r.pointer=&r;p.driverClass=kDriverHuman;r.driverClass=kDriverRacer;
        p.heading=r.heading={0,0,1};r.position={0,0,30};p.speed=20;r.speed=22;
        expect(IsEncounterFollowing(p,r),"alpha58 rear envelope plus matched speed qualifies");
        r.speed=30;
        expect(!IsEncounterFollowing(p,r)&&EncounterFollowReason(p,r,nullptr,true)!=nullptr,"alpha58 speed gate cannot be bypassed by holding grace");
        r.speed=20;p.position.z=31;
        expect(!IsEncounterFollowing(p,r),"alpha58 matching speed never bypasses behind requirement");
        g_settings.startSpeedToleranceKmh=oldTolerance;
    }
    {
        const auto savedSettings=g_settings;const auto savedBattle=g_battle;
        const auto savedThread=g_encounterAdapterThread.load();const auto savedRetention=g_streamRetentionEnabled.load();
        const auto savedHook=g_customSpeedInstalled;const auto savedOriginal=g_originalCustomSpeed;
        constexpr std::size_t slot=kMaximumRacersLimit-1;
        const auto savedVehicle=g_protectedVehicles[slot].load(),savedSim=g_protectedSimables[slot].load();
        const auto savedKey=g_protectedKeys[slot].load();
        std::array<unsigned char,0x100> car{};std::array<unsigned char,0x800> ai{};
        put(car,0,reinterpret_cast<std::uintptr_t>(table));put(car,4,123u);put(car,8,unsigned(kDriverRacer));
        put(car,12,reinterpret_cast<std::uintptr_t>(&simable));put(car,0x54,reinterpret_cast<std::uintptr_t>(ai.data()+0x4C));
        put(ai,0,Address(kRacecarPrimaryVtable));put(ai,0x4C,Address(kRacecarIVehicleAIVtable));
        put(ai,0x48,reinterpret_cast<std::uintptr_t>(car.data()));
        ProtectManagedVehicle(slot,car.data(),&simable,123);
        g_battle={};g_settings.enabled=true;g_settings.cruisingSpeedPercent=60;
        g_streamRetentionEnabled=true;g_encounterAdapterThread=GetCurrentThreadId();
        g_customSpeedInstalled=true;g_originalCustomSpeed=reinterpret_cast<CustomSpeedFn>(&Alpha54Speed);
        auto drive=[&](std::uintptr_t caller=0x00428FFA,float speed=50.f){
            const auto before=alpha54SpeedCalls;DispatchCustomSpeed(ai.data()+0x4C,Address(caller),speed,1000);
            expect(alpha54SpeedCalls==before+1&&alpha54SpeedOwner==ai.data()+0x4C,"alpha58 native callback forwarded once with same owner");
            return alpha54SpeedValue;
        };
        for(auto mode:{encounter_custom::Mode::Stable,encounter_custom::Mode::Custom}) {
            g_settings.encounterAIMode=mode;
            expect(std::abs(drive()-30)<.001f,"alpha58 production hook reduces managed cruise in both modes");
        }
        g_settings.encounterSignalEnabled=false;
        expect(std::abs(drive()-30)<.001f,"alpha58 cruising works with battle signal disabled");
        expect(drive(0)==50&&drive(0x00428FFA,0)==0&&drive(0x00428FFA,-5)==-5,"alpha58 unrelated caller stop and reverse preserved");
        put(ai,0xBC,1u);expect(drive()==50,"alpha58 native police pursuit exempt");put(ai,0xBC,0u);
        g_battle.rival={reinterpret_cast<std::uintptr_t>(car.data()),reinterpret_cast<std::uintptr_t>(&simable),123};
        g_battle.pending=true;expect(drive()==50,"alpha58 queued battle opponent exempt immediately");
        g_battle.pending=false;battle::Sample s{};s.player.identity={1,2,3};s.rival.identity=g_battle.rival;
        s.player.forward=s.rival.forward={0,0,1};s.rival.position.z=30;g_battle.model.Start(s);
        expect(drive()==50,"alpha58 active leading opponent not cruise-limited");
        g_battle={};expect(std::abs(drive()-30)<.001f,"alpha58 cruising restored after battle teardown");
        put(car,4,124u);expect(drive()==50,"alpha58 recycled key cannot inherit pace reduction");put(car,4,123u);
        put(car,12,reinterpret_cast<std::uintptr_t>(&asking));expect(drive()==50,"alpha58 recycled simable excluded");put(car,12,reinterpret_cast<std::uintptr_t>(&simable));
        for(auto driver:{kDriverHuman,kDriverTraffic,2u}) {put(car,8,driver);expect(drive()==50,"alpha58 player traffic police excluded");}put(car,8,unsigned(kDriverRacer));
        g_protectedVehicles[slot]=0;expect(drive()==50,"alpha58 unmanaged event racer excluded");g_protectedVehicles[slot]=reinterpret_cast<std::uintptr_t>(car.data());
        g_encounterAdapterThread=0;expect(drive()==50,"alpha58 wrong thread forwards native speed");g_encounterAdapterThread=GetCurrentThreadId();
        g_streamRetentionEnabled=false;expect(drive()==50,"alpha58 ended world forwards native speed");g_streamRetentionEnabled=true;
        g_settings.cruisingSpeedPercent=100;expect(drive()==50,"alpha58 percentage 100 disables reduction at production hook");
        g_protectedVehicles[slot]=savedVehicle;g_protectedSimables[slot]=savedSim;g_protectedKeys[slot]=savedKey;
        g_settings=savedSettings;g_battle=savedBattle;g_encounterAdapterThread=savedThread;g_streamRetentionEnabled=savedRetention;
        g_customSpeedInstalled=savedHook;g_originalCustomSpeed=savedOriginal;
    }
