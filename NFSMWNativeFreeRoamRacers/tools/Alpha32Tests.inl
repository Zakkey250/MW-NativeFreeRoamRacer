    {
        g_encounterAdapterThread=GetCurrentThreadId();
        encounter_route::Trail trail;
        expect(!trail.Select({}).valid,"alpha32 empty trail has no invented destination");
        for(unsigned i=0;i<=10;++i) trail.Record({0,0,float(i)*12});
        auto hint=trail.Select({0,0,0});
        expect(hint.valid&&hint.position.z<=60&&hint.heading.z>.9f,"alpha32 bounded lookahead follows early recorded road not live player");
        hint=trail.Select({0,0,95});
        expect(hint.position.z<80,"alpha32 cannot jump to nearest future hairpin point");
        trail.Reset();
        for(unsigned i=0;i<10;++i) trail.Record({0,0,float(i)*12});
        for(unsigned i=1;i<6;++i) trail.Record({float(i)*12,0,108});
        hint=trail.Select({0,0,0});
        expect(hint.position.x==0,"alpha32 L turn keeps entry leg before later branch");
        for(unsigned i=0;i<=8;++i) hint=trail.Select({0,0,float(i)*12});
        expect(hint.position.x>0&&hint.position.z==108&&hint.heading.x>.9f,"alpha32 sequential progress selects actual corner exit heading");
        trail.Reset();for(unsigned i=0;i<10;++i) trail.Record({0,20,float(i)*12});
        const auto bridgeCount=trail.size();hint=trail.Select({0,0,12});
        expect(trail.size()==bridgeCount,"alpha32 lower road does not consume elevated trail");
        hint=trail.Select({0,12,0});expect(trail.size()==bridgeCount,"alpha32 eight metre overpass also prevents proximity shortcut");
        trail.Reset();trail.Record({0,0,0});trail.Record({0,0,12});
        for(unsigned i=0;i<200;++i) trail.Record({0,0,12});
        expect(trail.size()==2,"alpha32 stopped player does not grow history");
        hint=trail.Select({0,0,15});
        expect(hint.valid&&hint.heading.z>.9f,"alpha32 passing latest hint does not reverse destination heading");
        expect(!trail.Record({0,0,1000})&&!trail.Select({0,0,0}).valid,"alpha32 teleport invalidates connecting path");
        trail.Record({0,0,1012});expect(trail.Select({0,0,1000}).valid,"alpha32 new coherent samples recover after discontinuity");
        trail.Reset();unsigned resetCount=0;
        for(unsigned i=0;i<400;++i) if(!trail.Record({0,0,float(i)*12}))++resetCount;
        expect(resetCount>0&&trail.size()<=128,"alpha32 overflow is bounded and explicitly resets without allocation");
        expect(!trail.Record({NAN,0,0})&&!trail.Select({NAN,0,0}).valid,"alpha32 nonfinite route samples rejected");
        trail.Reset();expect(trail.size()==0,"alpha32 each role switch or battle reset discards old path");

        auto savedRacers=g_racers;g_racers.clear();g_battle={};
        FakeVehicle markerCar{table,0xABCDEF12,kDriverRacer,&simable};
        ManagedRacer markerOwner{};markerOwner.pointer=&markerCar;markerOwner.simable=&simable;markerOwner.vehicleKey=markerCar.key;
        g_racers.push_back(markerOwner);
        battle::Sample s{};s.player.identity={1,11,111};s.rival.identity={2,22,222};
        s.player.forward=s.rival.forward={0,0,1};s.rival.position.z=10;
        g_battle.model.Start(s);g_battle.rival=s.rival.identity;
        expect(HideEncounterNativeArrow(&markerCar,Address(0x0057A16A)),"alpha32 own bystander excluded at actual native racer-arrow query");
        g_encounterAdapterThread=0;expect(!HideEncounterNativeArrow(&markerCar,Address(0x0057A16A)),"alpha32 nonowner thread does not access battle or population state");g_encounterAdapterThread=GetCurrentThreadId();
        expect(!HideEncounterNativeArrow(&markerCar,Address(0x0057A16A)+1),"alpha32 simulation and other drawing callers preserve native IsActive");
        g_battle.rival={reinterpret_cast<std::uintptr_t>(&markerCar),reinterpret_cast<std::uintptr_t>(&simable),markerCar.key};
        expect(!HideEncounterNativeArrow(&markerCar,Address(0x0057A16A)),"alpha32 opponent native arrow remains visible");
        g_battle.rival=s.rival.identity;markerCar.key++;
        expect(!HideEncounterNativeArrow(&markerCar,Address(0x0057A16A)),"alpha32 recycled pointer cannot suppress unrelated vehicle");
        markerCar.key--;markerCar.driver=kDriverTraffic;
        expect(!HideEncounterNativeArrow(&markerCar,Address(0x0057A16A)),"alpha32 traffic identity cannot inherit marker policy");
        markerCar.driver=kDriverRacer;
        expect(!HideEncounterNativeArrow(&other,Address(0x0057A16A)),"alpha32 unmanaged car preserved");
        g_battle={};
        expect(!HideEncounterNativeArrow(&markerCar,Address(0x0057A16A)),"alpha32 postbattle native marker eligibility restores immediately");
        g_racers=savedRacers;

        // The obsolete global IsActive detour test was replaced by alpha.33's
        // actual NOP/CALL map fixture and Arms Assist 12-byte guard regression.
        unsigned char* code=nullptr;bool hooked=false;
        std::array<unsigned char,0x100> vehicleBlob{};

        // Full native-shaped owner/nav chain for both positive and fail-closed path routing.
        std::array<unsigned char,0x7CC> aiBlob{};std::array<unsigned char,0x300> navBlob{};
        std::array<unsigned char,0x40> goalBlob{},actionBlob{},targetBlob{};
        const std::size_t slots[]={kSlotPosition,kSlotHeading,kSlotSpeed,kSlotOffScreenTime,kSlotOnScreenTime};
        std::array<void*,5> savedSlots{};
        for(unsigned i=0;i<5;++i){savedSlots[i]=table[slots[i]];table[slots[i]]=i<2?reinterpret_cast<void*>(&Alpha32Vector):reinterpret_cast<void*>(&Alpha32Float);}
        put(vehicleBlob,0,reinterpret_cast<std::uintptr_t>(table));put(vehicleBlob,4,123u);put(vehicleBlob,8,unsigned(kDriverRacer));
        put(vehicleBlob,12,reinterpret_cast<std::uintptr_t>(&simable));put(vehicleBlob,0x54,reinterpret_cast<std::uintptr_t>(aiBlob.data()+0x4C));
        put(aiBlob,0,Address(kRacecarPrimaryVtable));put(aiBlob,0x4C,Address(kRacecarIVehicleAIVtable));put(aiBlob,0x7C4,Address(0x008925B4));
        put(aiBlob,0xB8,reinterpret_cast<std::uintptr_t>(goalBlob.data()));put(aiBlob,0x70,reinterpret_cast<std::uintptr_t>(navBlob.data()));put(aiBlob,0xA0,reinterpret_cast<std::uintptr_t>(targetBlob.data()));
        put(goalBlob,0,Address(kAIGoalRacerVtable));put(goalBlob,4,reinterpret_cast<std::uintptr_t>(actionBlob.data()));put(actionBlob,0,Address(kAIActionRaceVtable));navBlob[0x50]=1;
        g_battle={};g_battle.model.Start(s);s.player.position.z=13;g_battle.model.Step(s,.05f);g_battle.model.Step(s,.05f);
        g_battle.rival={reinterpret_cast<std::uintptr_t>(vehicleBlob.data()),reinterpret_cast<std::uintptr_t>(&simable),123};
        g_battle.routeHint={{100,20,200},{1,0,0},true};Vec3 dest{},dir{};
        expect(EncounterPathHint(navBlob.data(),Address(0x00428BCF),dest,dir)&&dest.x==100&&dest.y==20&&dir.x==1,"alpha32 native opponent renewal receives same canonical trail waypoint");
        g_battle.destinationSet=true;g_battle.lastDestination={10,20,30};g_battle.lastDestinationHeading={0,0,1};
        g_battle.routeOriginSegment=100;g_battle.pathSeconds=1;g_battle.routeProgress=80;g_battle.routeEndpointDistance=80;
        put(navBlob,0x78,3u);put(navBlob,0x2E0,4);put(navBlob,0x8E,std::int16_t(101));
        expect(EncounterPathHint(navBlob.data(),Address(0x00428BCF),dest,dir)&&dest.x==10&&dest.z==30,"alpha45 scoped native renewal preserves committed endpoint");
        g_battle.pathSeconds=2;
        expect(EncounterPathHint(navBlob.data(),Address(0x00428BCF),dest,dir)&&dest.x==100,"alpha45 segment exit returns latest player target");
        g_battle.pathSeconds=1;navBlob[0x2D8]=1;
        expect(EncounterPathHint(navBlob.data(),Address(0x00428BCF),dest,dir)&&dest.x==100,"alpha45 native crossed goal bypasses endpoint latch");
        navBlob[0x2D8]=0;g_battle.routeAge=4;
        expect(EncounterPathHint(navBlob.data(),Address(0x00428BCF),dest,dir)&&dest.x==100,"alpha45 stalled native renewal may replan immediately");
        g_battle.routeAge=0;g_battle.destinationSet=false;
        g_encounterAdapterThread=0;expect(!EncounterPathHint(navBlob.data(),Address(0x00428BCF),dest,dir),"alpha32 nonowner simulation thread forwards native path unchanged");g_encounterAdapterThread=GetCurrentThreadId();
        expect(!EncounterPathHint(navBlob.data(),0,dest,dir),"alpha32 GPS and other native path call sites untouched");
        expect(!EncounterPathHint(targetBlob.data(),Address(0x00428BCF),dest,dir),"alpha32 unrelated RoadNav owner untouched");
        put(vehicleBlob,4,124u);expect(!EncounterPathHint(navBlob.data(),Address(0x00428BCF),dest,dir),"alpha32 recycled opponent cannot inherit trail path");put(vehicleBlob,4,123u);
        put(actionBlob,0,0u);expect(!EncounterPathHint(navBlob.data(),Address(0x00428BCF),dest,dir),"alpha32 native collision recovery action owns its own path");put(actionBlob,0,Address(kAIActionRaceVtable));
        g_battle.routeHint.valid=false;expect(!EncounterPathHint(navBlob.data(),Address(0x00428BCF),dest,dir),"alpha32 invalidated trail does not override native route");
        g_battle={};g_originalEncounterPath=reinterpret_cast<EncounterPathFn>(&Alpha32Path);
        Vec3 nativeDest{1,2,3},nativeDir{0,0,1};alpha32PathCalls=0;
        expect(EncounterPathHook(navBlob.data(),nullptr,&nativeDest,&nativeDir,true)&&alpha32PathCalls==1&&alpha32PathNav==navBlob.data()&&alpha32PathPosition==&nativeDest&&alpha32PathHeading==&nativeDir&&alpha32PathFlag,"alpha32 unrelated path forwards every argument and result exactly once");
        for(unsigned i=0;i<5;++i)table[slots[i]]=savedSlots[i];

        code=static_cast<unsigned char*>(VirtualAlloc(nullptr,4096,MEM_COMMIT|MEM_RESERVE,PAGE_EXECUTE_READWRITE));hooked=false;
        unsigned globalValue=0;
        unsigned char pathStub[]={0x56,0x57,0x8B,0x3D,0,0,0,0,0x8B,0x44,0x24,0x14,0x5F,0x5E,0xC2,0x0C,0};
        const auto globalAddress=reinterpret_cast<std::uintptr_t>(&globalValue);std::memcpy(pathStub+4,&globalAddress,4);
        if(code) {
            std::memcpy(code,pathStub,sizeof(pathStub));FlushInstructionCache(GetCurrentProcess(),code,64);
            if(MH_Initialize()==MH_OK)hooked=MH_CreateHook(code,&EncounterPathHook,reinterpret_cast<void**>(&g_originalEncounterPath))==MH_OK&&MH_EnableHook(code)==MH_OK;
        }
        expect(hooked,"alpha32 native-shaped RoadNav prologue supports x86 trampoline");
        if(hooked)for(bool flag:{false,true})expect(reinterpret_cast<EncounterPathFn>(code)(navBlob.data(),&nativeDest,&nativeDir,flag)==flag,"alpha32 road trampoline preserves RET12 three stack arguments and result");
        if(code){MH_DisableHook(code);MH_RemoveHook(code);MH_Uninitialize();VirtualFree(code,0,MEM_RELEASE);}
    }
