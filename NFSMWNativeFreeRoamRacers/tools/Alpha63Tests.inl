    {
        // Exercise the exact production transition with controlled native calls.
        struct Ops {
            EncounterNativeAI ai{reinterpret_cast<void*>(1),reinterpret_cast<void*>(2),reinterpret_cast<void*>(3),
                reinterpret_cast<void*>(4),reinterpret_cast<void*>(5),reinterpret_cast<void*>(6),1,true};
            battle::Identity expected{10,20,30};
            unsigned starts=0,chooses=0,reads=0;
            bool defer=false,nullAction=false,readOK=true,startOK=true,chooseOK=true,chooseReady=true;
            bool Read(const battle::Identity& id,EncounterNativeAI& out,const char*& failure) {
                ++reads;out=ai;failure="fixture-identity";return readOK&&id==expected;
            }
            bool Start(const EncounterNativeAI&) {
                ++starts;if(defer)ai.navValid=0;
                if(nullAction){ai.action=nullptr;ai.raceAction=false;}return startOK;
            }
            bool Recover(const EncounterNativeAI&) {return false;}
            bool Choose(const EncounterNativeAI&) {
                ++chooses;if(chooseReady){ai.action=reinterpret_cast<void*>(4);ai.raceAction=true;}return chooseOK;
            }
        };
        using R=NativeStartResult;
        for(unsigned population:{5u,21u,24u}) {
            Ops ops;EncounterNativeStart state;
            expect(AdvanceNativeStart(state,ops.expected,1000,population,ops)==R::Ready&&ops.starts==1&&ops.chooses==0,
                "alpha63 immediate native readiness works with no LB import at every population");
            for(unsigned i=0;i<4;++i)AdvanceNativeStart(state,ops.expected,1050+i*50,population,ops);
            expect(ops.starts==1&&ops.reads==2,"alpha63 terminal ready cannot reissue start on repeated input/polls");
            ops={};ops.defer=true;state={};
            expect(AdvanceNativeStart(state,ops.expected,1000,population,ops)==R::Pending,"alpha63 post-start invalid nav is pending not rejection");
            for(unsigned i=1;i<8;++i)
                expect(AdvanceNativeStart(state,ops.expected,1000+i*50,population,ops)==R::Pending,
                    "alpha63 later simulation polls wait without native reset or sleep");
            expect(ops.starts==1&&ops.chooses==0&&state.polls==8,"alpha63 delay issues StartRace exactly once and no premature ChooseAction");
            ops.ai.navValid=1;
            expect(AdvanceNativeStart(state,ops.expected,1450,population,ops)==R::Ready&&ops.starts==1,
                "alpha63 delayed nav recovery completes within bound");
        }
        Ops ops;EncounterNativeStart state;ops.defer=true;ops.nullAction=true;
        AdvanceNativeStart(state,ops.expected,1000,24,ops);ops.ai.navValid=1;
        expect(AdvanceNativeStart(state,ops.expected,1100,24,ops)==R::Pending&&ops.chooses==1,
            "alpha63 ChooseAction waits until nav recovery and observes result on next tick");
        expect(AdvanceNativeStart(state,ops.expected,1150,24,ops)==R::Ready&&ops.chooses==1&&ops.starts==1,
            "alpha63 null action becomes ready after exactly one bounded ChooseAction");
        ops={};state={};ops.nullAction=true;ops.chooseReady=false;
        AdvanceNativeStart(state,ops.expected,1000,24,ops);
        for(unsigned i=1;i<20;++i)AdvanceNativeStart(state,ops.expected,1000+i*50,24,ops);
        expect(ops.chooses==1&&AdvanceNativeStart(state,ops.expected,2000,24,ops)==R::Failed,
            "alpha63 permanently null action times out without repeated ChooseAction");
        for(unsigned i=0;i<10;++i)AdvanceNativeStart(state,ops.expected,2100+i*50,24,ops);
        expect(ops.starts==1&&ops.chooses==1,"alpha63 failed terminal cannot retry native writes");
        ops={};state={};ops.defer=true;AdvanceNativeStart(state,ops.expected,1000,24,ops);
        expect(!NativeStartExpired(state,1999)&&NativeStartExpired(state,2000)&&NativeStartExpired(state,999),
            "alpha63 exact one-second deadline and backwards clock fail closed");
        ops.ai.navValid=1;
        expect(AdvanceNativeStart(state,ops.expected,2000,24,ops)==R::Failed,"alpha63 readiness arriving after deadline does not start late");
        for(unsigned change=0;change<8;++change) {
            ops={};state={};ops.defer=true;AdvanceNativeStart(state,ops.expected,1000,24,ops);
            switch(change) {
                case 0:ops.expected.key++;break;
                case 1:ops.readOK=false;break;
                case 2:ops.ai.primary=nullptr;break;
                case 3:ops.ai.goal=nullptr;break;
                case 4:ops.ai.nav=nullptr;break;
                case 5:ops.ai.target=nullptr;break;
                case 6:ops.ai.racer=nullptr;break;
                case 7:ops.ai.raceAction=false;break;
            }
            expect(AdvanceNativeStart(state,{10,20,30},1100,24,ops)==R::Failed&&ops.starts==1&&ops.chooses==0,
                "alpha63 identity structural owner or non-null action vtable change aborts closed");
        }
        for(unsigned bad=0;bad<3;++bad) {
            ops={};state={};if(bad==0)ops.ai.navValid=0;if(bad==1)ops.ai.raceAction=false;if(bad==2)ops.readOK=false;
            expect(AdvanceNativeStart(state,ops.expected,1000,24,ops)==R::Failed&&ops.starts==0,
                "alpha63 strict pre-start guard never issues native start for invalid AI");
        }
        ops={};state={};ops.startOK=false;
        expect(AdvanceNativeStart(state,ops.expected,1000,24,ops)==R::Failed&&state.issued&&ops.starts==1,
            "alpha63 partial StartRace exception records issued flag for single cleanup");
        ops={};state={};ops.nullAction=true;ops.chooseOK=false;
        expect(AdvanceNativeStart(state,ops.expected,1000,24,ops)==R::Failed&&ops.chooses==1,
            "alpha63 ChooseAction exception fails without retry");
        const auto savedBattle=g_battle;const bool savedSurface=g_battleSurface;
        g_battle={};g_battleSurface=true;g_battle.pending=true;
        g_battle.nativeStart.issued=true;g_battle.nativeStart.issuedAt=1000;
        g_battle.rival={10,20,30};
        VehicleSnapshot dummy{};
        expect(!QueueEncounterBattle(dummy,dummy)&&g_battle.nativeStart.issued&&g_battle.rival.key==30,
            "alpha63 repeated join input does not clear an in-progress start");
        expect(g_battle.model.phase()==battle::Phase::Idle&&!AwardEncounterCash(),
            "alpha63 pending state is busy but not active and cannot pay cash");
        EncounterMissingSample(.05f);
        expect(!EncounterBattleBusy()&&!g_battle.nativeStart.issued&&!g_gaugeVisible,
            "alpha63 missing pending owner cancels without dereferencing cached identity");
        g_battle=savedBattle;g_battleSurface=savedSurface;
    }
    {
        // Production structural reader vs strict reader: fabricated native-shaped
        // objects, no real game memory and no write to the game navigation byte.
        std::array<unsigned char,0x100> car{};
        std::array<unsigned char,0x800> ai{};
        std::array<unsigned char,0x300> nav{};
        std::array<unsigned char,0x40> goal{},action{},target{};
        const std::size_t slots[]={kSlotPosition,kSlotComputeHeading,kSlotSpeed,kSlotOffScreenTime,kSlotOnScreenTime};
        std::array<void*,5> savedSlots{};
        for(unsigned i=0;i<5;++i){savedSlots[i]=table[slots[i]];table[slots[i]]=i==0?reinterpret_cast<void*>(&Alpha32Vector):(i==1?reinterpret_cast<void*>(&Alpha32Heading):reinterpret_cast<void*>(&Alpha32Float));}
        put(car,0,reinterpret_cast<std::uintptr_t>(table));put(car,4,123u);put(car,8,unsigned(kDriverRacer));
        put(car,12,reinterpret_cast<std::uintptr_t>(&simable));put(car,0x54,reinterpret_cast<std::uintptr_t>(ai.data()+0x4C));
        put(ai,0,Address(kRacecarPrimaryVtable));put(ai,0x4C,Address(kRacecarIVehicleAIVtable));put(ai,0x7C4,Address(0x008925B4));
        put(ai,0xB8,reinterpret_cast<std::uintptr_t>(goal.data()));put(ai,0x70,reinterpret_cast<std::uintptr_t>(nav.data()));
        put(ai,0xA0,reinterpret_cast<std::uintptr_t>(target.data()));put(goal,0,Address(kAIGoalRacerVtable));
        put(goal,4,reinterpret_cast<std::uintptr_t>(action.data()));put(action,0,Address(kAIActionRaceVtable));nav[0x50]=1;
        const battle::Identity id{reinterpret_cast<std::uintptr_t>(car.data()),reinterpret_cast<std::uintptr_t>(&simable),123};
        EncounterNativeAI snapshot{};const char* failure=nullptr;
        expect(ReadEncounterAI(id,snapshot),"alpha63 unchanged strict reader accepts ready native chain");
        nav[0x50]=0;
        expect(ReadEncounterAIStructure(id,snapshot,failure)&&!snapshot.navValid&&snapshot.raceAction,
            "alpha63 actual structural reader accepts temporary invalid nav without changing it");
        expect(!ReadEncounterAI(id,snapshot)&&nav[0x50]==0,"alpha63 driving callers retain strict nav requirement");
        put(goal,4,std::uintptr_t(0));nav[0x50]=1;
        expect(ReadEncounterAIStructure(id,snapshot,failure)&&!snapshot.action&&!snapshot.raceAction,
            "alpha63 structural reader safely accepts null action during rebuild");
        put(ai,0xB8,std::uintptr_t(0));
        expect(!ReadEncounterAIStructure(id,snapshot,failure)&&std::strcmp(failure,"goal-vtable")==0,
            "alpha63 structural reader reports exact failed goal field");
        put(car,4,124u);
        expect(!ReadEncounterAIStructure(id,snapshot,failure)&&std::strcmp(failure,"vehicle-identity")==0,
            "alpha63 actual reader rejects recycled vehicle key");
        for(unsigned i=0;i<5;++i)table[slots[i]]=savedSlots[i];
    }
