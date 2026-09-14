    {
        const auto updateCandidate=[](const auto& p,const auto& cars,float dt) {
            // These existing fixtures are already canonical XYZ. Raw ABI/axis
            // conversion is covered separately by Alpha30Tests.
            UpdateEncounterCandidateUsing(p,cars,dt,[](const auto& in,auto& out){out=in;return true;});
        };
        VehicleSnapshot player{}, rival{};
        player.pointer = reinterpret_cast<void*>(0x1111);
        rival.pointer = reinterpret_cast<void*>(0x2222);
        player.driverClass = kDriverHuman; rival.driverClass = kDriverRacer;
        player.heading = rival.heading = {0, 0, 1};
        player.speed = rival.speed = 20.0f;
        rival.position = {0, 0, 30};
        EncounterFollow f{};
        expect(IsEncounterFollowing(player, rival, &f) && f.ahead == 30,
            "encounter rear following at same speed qualifies");
        rival.position.z = -30;
        expect(!IsEncounterFollowing(player, rival), "encounter player ahead cannot challenge");
        rival.position = {21,0,30};
        expect(!IsEncounterFollowing(player, rival), "encounter parallel separated road excluded");
        rival.position = {0,13,30};
        expect(!IsEncounterFollowing(player, rival), "encounter overpass excluded");
        rival.position = {0,0,61};
        expect(!IsEncounterFollowing(player, rival), "encounter beyond 60m excluded");
        rival.position.z = 0.9f;
        expect(!IsEncounterFollowing(player, rival), "encounter side overlap excluded");
        rival.position.z = 30; rival.heading.z = -1;
        expect(!IsEncounterFollowing(player, rival), "encounter oncoming traffic excluded");
        rival.heading.z = 1; rival.speed = 0;
        expect(IsEncounterFollowing(player, rival), "encounter stationary rival now eligible");
        rival.speed = 37;
        expect(IsEncounterFollowing(player, rival), "encounter speed delta above 60kmh no longer blocks engagement");
        rival.speed = 20; rival.driverClass = kDriverTraffic;
        expect(!IsEncounterFollowing(player, rival), "encounter ambient traffic excluded");
        rival.driverClass = kDriverRacer; rival.position.x = std::numeric_limits<float>::quiet_NaN();
        expect(!IsEncounterFollowing(player, rival), "encounter NaN position rejected");
        rival.position = {0,0,30}; player.heading = {};
        expect(!IsEncounterFollowing(player, rival), "encounter invalid heading rejected");
        player.heading = rival.heading = {1,0,0}; rival.position = {30,0,0};
        expect(IsEncounterFollowing(player, rival, &f) && f.ahead == 30,
            "encounter eastward follows same coordinate convention");
        player.heading = rival.heading = {0,0,-1}; rival.position = {0,0,-30};
        expect(IsEncounterFollowing(player, rival), "encounter southward follows correctly");
        expect(EncounterActionIsEngage({46,0,0}) && EncounterActionIsEngage({46,3,1}),
            "encounter action uses remapped native ID without physical-key assumptions");
        expect(!EncounterActionIsEngage({45,0,1}) && !EncounterActionIsEngage({47,0,1}),
            "encounter pause and pad-left actions do not signal");
        std::array<std::uint8_t,0x400> hud{};
        expect(IsEncounterPeekContext(Address(kEncounterPeekReturn),hud.data()+0x30,hud.data()),
            "encounter observes only converted HUD queue at verified callsite");
        expect(!IsEncounterPeekContext(Address(kEncounterPeekReturn)+1,hud.data()+0x30,hud.data()) &&
            !IsEncounterPeekContext(Address(kEncounterPeekReturn),hud.data()+0x40,hud.data()) &&
            !IsEncounterPeekContext(Address(kEncounterPeekReturn),hud.data()+0x30,nullptr),
            "encounter other queue/caller/no context excluded");
        std::array<std::uint8_t,0x60> widget{};
        put(widget,0,Address(kEncounterMenuVtable));
        expect(EncounterWidgetFree(widget.data()), "encounter empty native widget can be borrowed");
        put(widget,0x44,1u);
        expect(!EncounterWidgetFree(widget.data()), "encounter real safehouse/menu gate wins");
        put(widget,0x44,0u); put(widget,0x48,1u);
        expect(!EncounterWidgetFree(widget.data()), "encounter real race event wins");
        put(widget,0x48,0u); widget[0x4d]=1;
        expect(!EncounterWidgetFree(widget.data()), "encounter native visible prompt wins");
        widget[0x4d]=0; put(widget,0,static_cast<std::uintptr_t>(1));
        expect(!EncounterWidgetFree(widget.data()), "encounter widget vtable mismatch rejected");
        expect(!EncounterWidgetFree(reinterpret_cast<void*>(1)), "encounter invalid widget is fail closed");
        put(widget,0,Address(kEncounterMenuVtable));widget[0x4c]=1;
        expect(!EncounterWidgetFree(widget.data()),"encounter pending SMS notification has priority");
        widget[0x4c]=0;put(widget,0x50,42);
        expect(!EncounterWidgetFree(widget.data()),"encounter active SMS notification has priority");
        g_encounterCandidate.rival=rival.pointer;g_encounterCandidate.ready=true;
        g_encounterOwnedWidget=reinterpret_cast<void*>(1);g_encounterCooldownUntil=42;
        ResetEncounterSignal();
        expect(!g_encounterCandidate.rival && !g_encounterCandidate.ready &&
            g_encounterOwnedWidget==reinterpret_cast<void*>(1) && !g_encounterCooldownUntil,
            "encounter world reset cancels target/cooldown and defers UI cleanup to live callback");
        g_encounterOwnedWidget=nullptr;
        // Exercise the production candidate state machine against fabricated game globals.
        std::array<std::uint8_t,0x2000> challengeRace{};
        std::array<std::uint8_t,0x40> challengeFrontend{};
        setGlobal(kGameFlowState,6u);
        setGlobal(kRaceStatus,reinterpret_cast<std::uintptr_t>(challengeRace.data()));
        setGlobal(0x0091CB20,reinterpret_cast<std::uintptr_t>(challengeFrontend.data()));
        put(hud,0x18,std::uint64_t{1});
        g_encounterHud=hud.data();g_encounterHudTick=GetTickCount64();
        g_encounterEnabled=true;g_encounterFaulted=false;
        player.heading=rival.heading={0,0,1};rival.position={0,0,30};rival.vehicleKey=77;
        std::vector<VehicleSnapshot> challengeVehicles={player,rival};
        ManagedRacer challengeRacer{};challengeRacer.pointer=rival.pointer;
        challengeRacer.simable=reinterpret_cast<void*>(0x3333);challengeRacer.vehicleKey=77;
        g_racers.push_back(challengeRacer);
        updateCandidate(player,challengeVehicles,0.05f);
        expect(!g_encounterCandidate.ready && g_encounterCandidate.rival==rival.pointer,
            "encounter candidate alone does not instantly arm");
        for(unsigned i=0;i<3;++i) updateCandidate(player,challengeVehicles,0.05f);
        expect(g_encounterCandidate.ready,"encounter sustained 0.15-second following arms");
        challengeVehicles[1].speed = 42.0f;
        player.heading = {0.98f,0,0.15f};
        const auto lastQualified = g_encounterCandidate.qualifiedTick = GetTickCount64() - 125;
        updateCandidate(player,challengeVehicles,0.05f);
        expect(g_encounterCandidate.ready && g_encounterCandidate.holding && g_encounterCandidate.qualifiedTick == lastQualified,
            "encounter transient turn keeps ready without renewing grace deadline");
        g_encounterCandidate.qualifiedTick = GetTickCount64() - 2001;
        updateCandidate(player,challengeVehicles,0.05f);
        expect(!g_encounterCandidate.ready && !g_encounterCandidate.rival,
            "encounter continuous soft violation expires grace and clears candidate");
        challengeVehicles[1].speed = 20.0f;
        player.heading = {0,0,1};
        for(unsigned i=0;i<3;++i) updateCandidate(player,challengeVehicles,0.05f);
        expect(g_encounterCandidate.ready && !g_encounterCandidate.holding,
            "encounter valid following rearms after grace expiration");
        g_racers[0].simable=reinterpret_cast<void*>(0x4444);
        updateCandidate(player,challengeVehicles,0.05f);
        expect(!g_encounterCandidate.ready && g_encounterCandidate.heldSeconds<=0.05f,
            "encounter recycled generation cannot inherit dwell");
        g_encounterCandidate.sampleTick=GetTickCount64()-1000;
        updateCandidate(player,challengeVehicles,0.05f);
        expect(g_encounterCandidate.heldSeconds<=0.05f,"encounter stale sample resets dwell");
        g_encounterCooldownUntil=GetTickCount64()+8000;
        updateCandidate(player,challengeVehicles,0.05f);
        expect(!g_encounterCandidate.rival,"encounter cooldown prevents immediate repeat");
        g_encounterCooldownUntil=0;hud[0x2bc]=1;
        updateCandidate(player,challengeVehicles,0.05f);
        expect(!g_encounterCandidate.rival,"encounter pursuit blocks challenges");
        hud[0x2bc]=0;challengeFrontend[0x1e]=1;
        updateCandidate(player,challengeVehicles,0.05f);
        expect(!g_encounterCandidate.rival,"encounter frontend modal blocks challenges");
        challengeFrontend[0x1e]=0;
        g_racers[0].missingSeconds=0.1f;
        updateCandidate(player,challengeVehicles,0.05f);
        expect(!g_encounterCandidate.rival,"encounter missing managed racer cannot arm");
        g_racers.clear();updateCandidate(player,challengeVehicles,0.05f);
        expect(!g_encounterCandidate.rival,"encounter unmanaged racer cannot arm");
        ResetEncounterSignal();g_encounterEnabled=false;
        // Exact native-shaped peek prologue/RET4 tested with a real MinHook trampoline.
        const std::uint8_t peekStub[]={0x83,0x39,0,0x7E,0x13,0x8B,0x41,8,
            0x8D,4,0x40,0x8D,0x4C,0x81,0x10,0x8B,0x44,0x24,4,0x89,8,0xC2,4,0,
            0x8B,0x44,0x24,4,0xC7,0,0,0,0,0,0xC2,4,0};
        auto* code=VirtualAlloc(nullptr,4096,MEM_RESERVE|MEM_COMMIT,PAGE_EXECUTE_READWRITE);
        bool installed=false;
        if(code) {
            std::memcpy(code,peekStub,sizeof(peekStub));FlushInstructionCache(GetCurrentProcess(),code,sizeof(peekStub));
            if(MH_Initialize()==MH_OK) installed=MH_CreateHook(code,&EncounterPeekHook,
                reinterpret_cast<void**>(&g_encounterOriginalPeek))==MH_OK && MH_EnableHook(code)==MH_OK;
        }
        expect(installed,"encounter native peek prologue real trampoline installs");
        if(installed) {
            std::array<std::uint8_t,64> queue{};
            put(queue,0,1u);put(queue,8,0u);put(queue,16,EncounterAction{46,3,1});
            const auto before=queue;void* action=nullptr;
            void* result=reinterpret_cast<EncounterPeekFn>(code)(queue.data(),&action);
            expect(action==queue.data()+16 && result==&action && queue==before,
                "encounter hook preserves native EAX, output pointer and entire input queue");
            put(queue,0,0u);result=reinterpret_cast<EncounterPeekFn>(code)(queue.data(),&action);
            expect(!action && result==&action,"encounter empty queue ABI preserved");
        }
        if(code) {MH_DisableHook(code);MH_RemoveHook(code);MH_Uninitialize();VirtualFree(code,0,MEM_RELEASE);}
        // Actual Menu Update prologue and RET 4. Record the caller's stack
        // argument at +54 and call count at +58; execute no game UI routines.
        const std::uint8_t menuStub[]={0x51,0x56,0x8B,0xF1,0x8A,0x46,0x4C,
            0x8B,0x44,0x24,0x0C,0x89,0x46,0x54,0xFF,0x46,0x58,
            0x5E,0x59,0xC2,4,0};
        auto* menuCode=VirtualAlloc(nullptr,4096,MEM_RESERVE|MEM_COMMIT,PAGE_EXECUTE_READWRITE);
        bool menuInstalled=false;
        std::array<std::uint8_t,0x60> menuWidget{};
        if(menuCode) {
            std::memcpy(menuCode,menuStub,sizeof(menuStub));
            FlushInstructionCache(GetCurrentProcess(),menuCode,sizeof(menuStub));
            const auto direct=ProbeEncounterHudAbi(menuCode,menuWidget.data(),reinterpret_cast<void*>(0x13572468));
            expect(direct==0,"HUD ABI native fixture preserves ESP EBX ESI EDI and IPlayer argument");
            g_brokenEncounterMenuTarget=menuCode;
            const auto broken=ProbeEncounterHudAbi(reinterpret_cast<void*>(&BrokenEncounterHudAbi),menuWidget.data(),reinterpret_cast<void*>(0x13572468));
            expect((broken&1) && (broken&14) && (broken&16),
                "HUD ABI negative control detects alpha21 stack register and argument corruption");
            if(MH_Initialize()==MH_OK) menuInstalled=MH_CreateHook(menuCode,&EncounterMenuHook,
                reinterpret_cast<void**>(&g_encounterOriginalMenu))==MH_OK && MH_EnableHook(menuCode)==MH_OK;
        }
        expect(menuInstalled,"HUD ABI actual Menu Update prologue real trampoline installs");
        if(menuInstalled) {
            g_encounterOwnedWidget=nullptr;g_encounterEnabled=false;
            put(menuWidget,0x58,0u);
            unsigned damage=0;
            for(unsigned i=0;i<1000;++i) damage|=ProbeEncounterHudAbi(menuCode,menuWidget.data(),reinterpret_cast<void*>(0x13572468));
            std::uint32_t calls=0;SafeRead(menuWidget.data()+0x58,&calls);
            expect(damage==0,"HUD ABI production detour preserves stack registers argument for 1000 frames");
            expect(calls==1000,"HUD ABI production detour forwards original exactly once per frame");
            expect(ProbeEncounterHudAbi(menuCode,menuWidget.data(),nullptr)==0,
                "HUD ABI production detour forwards null IPlayer without stack damage");
            put(menuWidget,0,Address(kEncounterMenuVtable));put(menuWidget,0x48,42u);
            expect(ProbeEncounterHudAbi(menuCode,menuWidget.data(),reinterpret_cast<void*>(0x24681357))==0,
                "HUD ABI native event priority path preserves calling convention");
            put(menuWidget,0x48,0u);g_encounterFaulted=true;
            expect(ProbeEncounterHudAbi(menuCode,menuWidget.data(),nullptr)==0,
                "HUD ABI disabled fault path still preserves native argument and stack");
            g_encounterFaulted=false;
        }
        if(menuCode) {MH_DisableHook(menuCode);MH_RemoveHook(menuCode);MH_Uninitialize();VirtualFree(menuCode,0,MEM_RELEASE);}
    }
    {
        VehicleSnapshot p{}, r{};
        p.pointer=reinterpret_cast<void*>(0x1111); r.pointer=reinterpret_cast<void*>(0x2222);
        p.driverClass=kDriverHuman; r.driverClass=kDriverRacer; r.vehicleKey=77;
        p.heading=r.heading={0,0,1}; p.speed=r.speed=20;
        r.position={0,0,1};
        expect(IsEncounterFollowing(p,r),"encounter inclusive rear boundary at one meter accepted");
        r.position.z=0.99f;
        expect(!IsEncounterFollowing(p,r),"encounter below one meter rejected");
        r.position.z=3;
        expect(IsEncounterFollowing(p,r),"encounter formerly excluded one-to-four meter region accepted");
        r.position.z=60;
        expect(IsEncounterFollowing(p,r),"encounter sixty meter boundary accepted");
        r.position.z=60.01f;
        expect(!IsEncounterFollowing(p,r),"encounter sixty meter radius not extended");
        r.position={14,0,30}; r.speed=29;
        expect(IsEncounterFollowing(p,r),"encounter wider lane offset and former speed-delta rejection now accepted");
        p.speed=r.speed=5.0f/3.6f;
        expect(IsEncounterFollowing(p,r),"encounter low-speed following at five kmh accepted");
        p.speed=r.speed=20; p.heading={0.8f,0,0.6f};
        expect(IsEncounterFollowing(p,r),"encounter moderate turning angle accepted");
        p.heading={0,0,1}; r.position={0,0,30}; r.speed=42;
        expect(IsEncounterFollowing(p,r),"encounter approaching fast racer needs no speed matching");
        p.heading={0.98f,0,0.15f};
        EncounterCandidate c{};c.player=p.pointer;c.rival=r.pointer;c.key=r.vehicleKey;c.ready=true;
        c.sampleTick=10000;c.qualifiedTick=10000;
        expect(!IsEncounterFollowing(p,r) && CanHoldEncounterFollowing(c,p,r,10125),
            "encounter ready candidate accepts brief soft violation during input recheck");
        c.sampleTick=12000;
        expect(CanHoldEncounterFollowing(c,p,r,12000) && !CanHoldEncounterFollowing(c,p,r,12001),
            "encounter hold deadline is inclusive 2000ms and never extends to 2001ms");
        c.sampleTick=10000;
        expect(!CanHoldEncounterFollowing(c,p,r,10300),"encounter grace never accepts stale vehicle samples");
        c.ready=false;
        expect(!CanHoldEncounterFollowing(c,p,r,10100),"encounter grace cannot arm a new candidate");
        c.ready=true;r.position.z=61;
        expect(!CanHoldEncounterFollowing(c,p,r,10100),"encounter grace cannot bypass 60m limit");
        r.position.z=0.5f;
        expect(!CanHoldEncounterFollowing(c,p,r,10100),"encounter grace cannot bypass rear one meter minimum");
        r.position.z=-10;
        expect(!CanHoldEncounterFollowing(c,p,r,10100),"encounter grace cannot challenge after overtaking");
        r.position={0,13,30};
        expect(!CanHoldEncounterFollowing(c,p,r,10100),"encounter grace cannot challenge across overpass");
        r.position={21,0,30};
        expect(!CanHoldEncounterFollowing(c,p,r,10100),"encounter grace keeps lateral safety boundary");
        r.position={0,0,30};p.heading={0,0,-1};
        expect(!CanHoldEncounterFollowing(c,p,r,10100),"encounter grace cannot challenge oncoming traffic");
        p.heading={0,0,1};r.vehicleKey=88;
        expect(!CanHoldEncounterFollowing(c,p,r,10100),"encounter grace rejects replaced vehicle identity");
        r.vehicleKey=77;r.position.x=std::numeric_limits<float>::quiet_NaN();
        expect(!CanHoldEncounterFollowing(c,p,r,10100),"encounter grace rejects non-finite snapshot");
    }
