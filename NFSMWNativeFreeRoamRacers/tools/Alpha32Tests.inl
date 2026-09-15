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
        g_settings.encounterAIMode=encounter_custom::Mode::Custom;
        g_customDriveInstalled=true;g_battle.customDirect=true;g_battle.customHintAt=1000;
        auto* directOwner=aiBlob.data()+0x4C;
        Vec3 nativeAim{7,8,9},actualAim{};
        g_originalCustomDrive=reinterpret_cast<CustomDriveFn>(&Alpha53Drive);alpha53DriveCalls=0;
        g_originalCustomSpeed=reinterpret_cast<CustomSpeedFn>(&Alpha54Speed);
        g_customSpeedInstalled=true;g_battle.customSpeedDemand=90;alpha54SpeedCalls=0;
        DispatchCustomSpeed(directOwner,Address(0x00428FFA),40,1100);
        expect(alpha54SpeedCalls==1&&alpha54SpeedOwner==directOwner&&alpha54SpeedValue==90,
            "alpha54 exact opponent race setter receives higher chase demand once");
        for(float speed:{0.f,-1.f,.1f}) {
            DispatchCustomSpeed(directOwner,Address(0x00428FFA),speed,1100);
            expect(alpha54SpeedValue==speed,"alpha54 native stop and reverse commands not raised");
        }
        DispatchCustomSpeed(directOwner,0,40,1100);
        expect(alpha54SpeedValue==40,"alpha54 unrelated speed caller unchanged");
        DispatchCustomSpeed(targetBlob.data(),Address(0x00428FFA),40,1100);
        expect(alpha54SpeedValue==40,"alpha54 other AI speed unchanged");
        DispatchCustomSpeed(directOwner,Address(0x00428FFA),40,1251);
        expect(alpha54SpeedValue==40,"alpha54 stale speed demand unchanged");
        g_encounterAdapterThread=0;DispatchCustomSpeed(directOwner,Address(0x00428FFA),40,1100);
        expect(alpha54SpeedValue==40,"alpha54 different thread speed unchanged");g_encounterAdapterThread=GetCurrentThreadId();
        put(actionBlob,0,0u);DispatchCustomSpeed(directOwner,Address(0x00428FFA),40,1100);
        expect(alpha54SpeedValue==40,"alpha54 recovery Action speed unchanged");put(actionBlob,0,Address(kAIActionRaceVtable));
        put(vehicleBlob,4,124u);DispatchCustomSpeed(directOwner,Address(0x00428FFA),40,1100);
        expect(alpha54SpeedValue==40,"alpha54 recycled vehicle cannot inherit chase demand");put(vehicleBlob,4,123u);
        g_settings.encounterAIMode=encounter_custom::Mode::Stable;DispatchCustomSpeed(directOwner,Address(0x00428FFA),40,1100);
        expect(alpha54SpeedValue==40,"alpha54 Stable AI speed unchanged");g_settings.encounterAIMode=encounter_custom::Mode::Custom;
        for(unsigned n=0;n<5;++n)g_battle.pursuit.Update({0,0,4},{0,0,1},{0,0,0},{0,0,1},.05f);
        DispatchCustomSpeed(directOwner,Address(0x00428FFA),40,1100);
        expect(alpha54SpeedValue==40,"alpha55 unarmed attack transition cannot change speed");
        g_battle.passActive=true;g_battle.customDirect=false;g_battle.routeHint.valid=false;
        DispatchCustomSpeed(directOwner,Address(0x00428FFA),40,1100);
        expect(alpha54SpeedValue==90,"alpha55 armed native attack retains speed advantage without trajectory steering");
        expect(!CustomDriveHint(directOwner,Address(0x00428FED),1100,actualAim),"alpha55 attack speed does not restore trail steering");
        DispatchCustomSpeed(directOwner,Address(0x00428FFA),0,1100);
        expect(alpha54SpeedValue==0,"alpha55 attack preserves native stop");
        DispatchCustomSpeed(directOwner,Address(0x00428FFA),40,1251);
        expect(alpha54SpeedValue==40,"alpha55 stale attack demand rejected");
        DispatchCustomSpeed(targetBlob.data(),Address(0x00428FFA),40,1100);
        expect(alpha54SpeedValue==40,"alpha55 attack boost cannot affect another AI");
        put(actionBlob,0,0u);DispatchCustomSpeed(directOwner,Address(0x00428FFA),40,1100);
        expect(alpha54SpeedValue==40,"alpha55 attack recovery action remains native");put(actionBlob,0,Address(kAIActionRaceVtable));
        g_encounterAdapterThread=0;DispatchCustomSpeed(directOwner,Address(0x00428FFA),40,1100);
        expect(alpha54SpeedValue==40,"alpha55 attack other thread unchanged");g_encounterAdapterThread=GetCurrentThreadId();
        g_settings.encounterAIMode=encounter_custom::Mode::Stable;DispatchCustomSpeed(directOwner,Address(0x00428FFA),40,1100);
        expect(alpha54SpeedValue==40,"alpha55 Stable attack speed unchanged");g_settings.encounterAIMode=encounter_custom::Mode::Custom;
        g_battle.passActive=false;g_battle.customDirect=true;g_battle.routeHint.valid=true;g_battle.pursuit.Reset();
        g_customSpeedInstalled=false;
        expect(CustomDriveHint(directOwner,Address(0x00428FED),1100,actualAim)&&actualAim.x==100,
            "alpha53 exact active opponent race caller receives trajectory drive aim");
        DispatchCustomDrive(directOwner,Address(0x00428FED),&nativeAim,1100);
        expect(alpha53DriveCalls==1&&alpha53DriveOwner==directOwner&&alpha53DriveAim.x==100,
            "alpha53 direct target dispatch forwards replacement exactly once");
        for(auto caller:{std::uintptr_t(0),Address(0x00428FED)+1}) {
            DispatchCustomDrive(directOwner,caller,&nativeAim,1100);
            expect(alpha53DrivePointer==&nativeAim,"alpha53 other native target callers keep original argument");
        }
        expect(!CustomDriveHint(directOwner,Address(0x00428FED),1251,actualAim)&&
            !CustomDriveHint(directOwner,Address(0x00428FED),999,actualAim),"alpha53 stale or backwards-time guidance rejected");
        expect(!CustomDriveHint(targetBlob.data(),Address(0x00428FED),1100,actualAim),"alpha53 other AI cannot inherit custom steering");
        put(vehicleBlob,4,124u);
        expect(!CustomDriveHint(directOwner,Address(0x00428FED),1100,actualAim),"alpha53 recycled rival identity rejected");put(vehicleBlob,4,123u);
        put(actionBlob,0,0u);
        expect(!CustomDriveHint(directOwner,Address(0x00428FED),1100,actualAim),"alpha53 native recovery action keeps its target");put(actionBlob,0,Address(kAIActionRaceVtable));
        g_encounterAdapterThread=0;
        expect(!CustomDriveHint(directOwner,Address(0x00428FED),1100,actualAim),"alpha53 other thread does not inspect battle state");g_encounterAdapterThread=GetCurrentThreadId();
        g_settings.encounterAIMode=encounter_custom::Mode::Stable;
        expect(!CustomDriveHint(directOwner,Address(0x00428FED),1100,actualAim),"alpha53 stable mode target unaffected");g_settings.encounterAIMode=encounter_custom::Mode::Custom;
        for(unsigned n=0;n<5;++n)g_battle.pursuit.Update({0,0,4},{0,0,1},{0,0,0},{0,0,1},.05f);
        expect(g_battle.pursuit.passing()&&!CustomDriveHint(directOwner,Address(0x00428FED),1100,actualAim),
            "alpha53 close attack releases directional steering to native overtaking");g_battle.pursuit.Reset();
        g_battle.routeHint.valid=false;
        expect(!CustomDriveHint(directOwner,Address(0x00428FED),1100,actualAim),"alpha53 missing trajectory does not use stale aim");g_battle.routeHint.valid=true;
        g_originalEncounterPath=reinterpret_cast<EncounterPathFn>(&Alpha32Path);alpha32PathCalls=0;
        for(auto caller:{Address(0x00428BCF),Address(0x00428A76)})
            expect(!DispatchEncounterPath(navBlob.data(),caller,&dest,&dir,true)&&alpha32PathCalls==0,
                "alpha53 both opponent race road queries blocked during direct following");
        expect(DispatchEncounterPath(navBlob.data(),0,&dest,&dir,true)&&alpha32PathCalls==1,
            "alpha53 GPS and non-race road requests pass unchanged");
        expect(DispatchEncounterPath(targetBlob.data(),Address(0x00428A76),&dest,&dir,true)&&alpha32PathCalls==2,
            "alpha53 unrelated vehicle road request passes unchanged");
        g_customDriveInstalled=false;g_battle.customDirect=false;
        expect(EncounterPathHint(navBlob.data(),Address(0x00428BCF),dest,dir)&&dest.x==100&&dest.z==200,
            "alpha51 Custom AI renewal follows recorded hint not stale stable commitment");
        g_originalEncounterPath=reinterpret_cast<EncounterPathFn>(&Alpha32Path);alpha32PathCalls=0;
        expect(DispatchEncounterPath(navBlob.data(),Address(0x00428BCF),&dest,&dir,true)&&alpha32PathCalls==1&&
            alpha51CopiedPosition.x==100&&alpha51HadHeading&&alpha51CopiedHeading.x==1,
            "alpha51 native renewal receives recorded position and arrival tangent");
        g_battle.routeHint.valid=false;
        expect(!DispatchEncounterPath(navBlob.data(),Address(0x00428BCF),&dest,&dir,true)&&alpha32PathCalls==1,
            "alpha51 missing trace blocks current-player native fallback for owned opponent only");
        expect(DispatchEncounterPath(navBlob.data(),0,&dest,&dir,true)&&alpha32PathCalls==2,
            "alpha51 custom missing trace leaves GPS and other callers unchanged");
        expect(DispatchEncounterPath(targetBlob.data(),Address(0x00428BCF),&dest,&dir,true)&&alpha32PathCalls==3,
            "alpha51 custom missing trace leaves other vehicle navigation unchanged");
        g_battle.routeHint.valid=true;g_settings.encounterAIMode=encounter_custom::Mode::Stable;
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
        // Actual 26-byte native setter: this+3C vector copy, RET4. Check x86
        // prologue relocation and all three vector values through the detour.
        const unsigned char driveStub[]={0x8b,0x44,0x24,0x04,0x8b,0x10,0x83,0xc1,0x3c,0x89,0x11,
            0x8b,0x50,0x04,0x89,0x51,0x04,0x8b,0x40,0x08,0x89,0x41,0x08,0xc2,0x04,0x00};
        code=static_cast<unsigned char*>(VirtualAlloc(nullptr,4096,MEM_COMMIT|MEM_RESERVE,PAGE_EXECUTE_READWRITE));hooked=false;
        if(code) {
            std::memcpy(code,driveStub,sizeof(driveStub));FlushInstructionCache(GetCurrentProcess(),code,sizeof(driveStub));
            if(MH_Initialize()==MH_OK)hooked=MH_CreateHook(code,&CustomDriveHook,reinterpret_cast<void**>(&g_originalCustomDrive))==MH_OK&&MH_EnableHook(code)==MH_OK;
        }
        expect(hooked,"alpha53 exact SetDriveTarget prologue supports x86 trampoline");
        if(hooked) {
            std::array<unsigned char,128> owner{};Vec3 copied{},input{13,24,35};
            reinterpret_cast<CustomDriveFn>(code)(owner.data(),&input);
            std::memcpy(&copied,owner.data()+0x3C,sizeof(copied));
            expect(copied.x==13&&copied.y==24&&copied.z==35,"alpha53 target trampoline preserves this and RET4 vector copy");
        }
        if(code){MH_DisableHook(code);MH_RemoveHook(code);MH_Uninitialize();VirtualFree(code,0,MEM_RELEASE);}
        const unsigned char speedStub[]={0x8b,0x44,0x24,0x04,0x89,0x41,0x38,0xc2,0x04,0x00};
        code=static_cast<unsigned char*>(VirtualAlloc(nullptr,4096,MEM_COMMIT|MEM_RESERVE,PAGE_EXECUTE_READWRITE));hooked=false;
        if(code) {
            std::memcpy(code,speedStub,sizeof(speedStub));FlushInstructionCache(GetCurrentProcess(),code,sizeof(speedStub));
            if(MH_Initialize()==MH_OK)hooked=MH_CreateHook(code,&CustomSpeedHook,reinterpret_cast<void**>(&g_originalCustomSpeed))==MH_OK&&MH_EnableHook(code)==MH_OK;
        }
        expect(hooked,"alpha54 exact SetDriveSpeed prologue supports x86 trampoline");
        if(hooked) {
            std::array<unsigned char,128> owner{};float copied=0;
            for(float value:{0.f,-1.f,73.25f}) {
                reinterpret_cast<CustomSpeedFn>(code)(owner.data(),value);
                std::memcpy(&copied,owner.data()+0x38,sizeof(copied));
                expect(copied==value,"alpha54 setter trampoline preserves this float argument and RET4");
            }
        }
        if(code){MH_DisableHook(code);MH_RemoveHook(code);MH_Uninitialize();VirtualFree(code,0,MEM_RELEASE);}
    }
