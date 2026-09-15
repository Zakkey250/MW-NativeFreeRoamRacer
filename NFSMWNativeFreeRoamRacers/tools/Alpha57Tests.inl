    {
        expect(encounter_text::Format(encounter_text::Id::Forfeit,false)==
            L"\u30a6\u30a7\u30dd\u30f3\u3092\u5f53\u3066\u3061\u307e\u3063\u305f\u2026\uff01\n\u53cd\u5247\u8ca0\u3051\u3060\uff01",
            "alpha57 Japanese forfeit uses font-compatible katakana weapon");
        expect(encounter_text::Format(encounter_text::Id::Forfeit,true)==L"I hit them with a weapon...!\nDisqualified!",
            "alpha57 English forfeit remains unchanged");
        constexpr unsigned expected[]={3000,2800,2600,2400,2200,2000,1800,1600,1400,1200,1000,800,600,400,300};
        for(unsigned bin=1;bin<=15;++bin) {
            expect(encounter_reward::Amount(true,bin)==expected[bin-1],"alpha57 Custom career reward table");
            expect(encounter_reward::Amount(false,bin)==1000,"alpha57 Stable reward remains 1000 at every BL");
            expect(encounter_text::Format(encounter_text::Id::Victory,false,expected[bin-1]).find(std::to_wstring(expected[bin-1]))!=std::wstring::npos,
                "alpha57 Japanese victory displays actual reward");
            expect(encounter_text::Format(encounter_text::Id::Victory,true,expected[bin-1]).find(std::to_wstring(expected[bin-1]))!=std::wstring::npos,
                "alpha57 English victory displays actual reward");
        }
        expect(encounter_reward::Amount(true,0)==300&&encounter_reward::Amount(true,255)==300,
            "alpha57 unknown career tier conservatively pays 300");
        expect(encounter_reward::Amount(false,0)==1000,"alpha57 Stable is independent of profile tier");
        battle::Model model;battle::Sample sample;
        sample.player.identity={1,11,111};sample.rival.identity={2,22,222};
        sample.player.forward=sample.rival.forward={0,0,1};sample.rival.position.z=10;
        expect(model.Forfeit().event==battle::Event::None,"alpha57 idle weapon hits do not make a battle");
        model.Start(sample,3000);const auto loss=model.Forfeit();
        expect(loss.event==battle::Event::Lost&&loss.reason==battle::Reason::WeaponHit&&loss.cashIntent==0,
            "alpha57 real weapon hit produces loss without cash");
        expect(model.Forfeit().event==battle::Event::None&&model.Step(sample,.1f).event==battle::Event::None,
            "alpha57 forfeit is terminal and cannot also pay a win");
        for(unsigned amount:{300u,3000u}) {
            model.Reset();sample.player.position={};sample.rival.position={0,0,10};model.Start(sample,amount);
            sample.player.position.z=13;sample.rival.position.z=11;model.Step(sample,.05f);
            sample.player.position.z=15;sample.rival.position.z=12;model.Step(sample,.05f);
            battle::Change result{};
            for(unsigned i=0;i<30;++i){sample.player.position.z+=10;const auto update=model.Step(sample,.1f);if(update.event==battle::Event::Won)result=update;}
            expect(result.event==battle::Event::Won&&result.cashIntent==amount,"alpha57 model win preserves captured variable cash intent");
        }
        sample.player.position={};sample.rival.position={0,0,10};
        for(unsigned mask=0;mask<16;++mask)expect(encounter_weapons::Forfeit(mask&1,mask&2,mask&4,mask&8)==(mask==15),
            "alpha57 forfeit requires active battle human source actual effect and other car");
        expect(encounter_weapons::PlayerEmpCaller(0xC979)&&!encounter_weapons::PlayerEmpCaller(0x82E4)&&
            !encounter_weapons::PlayerEmpCaller(0),"alpha57 only verified human EMP caller counts; AI/external callers excluded");
        expect(encounter_weapons::PlayerShockCaller(0x8F89)&&!encounter_weapons::PlayerShockCaller(0x8715),
            "alpha57 shock callback distinct from EMP flip physics");
        const auto oldThread=g_encounterAdapterThread.load();g_encounterAdapterThread=GetCurrentThreadId();
        g_battle={};g_battle.model.Start(sample);g_battle.player=sample.player.identity;
        EncounterSpikeHit(999,11);expect(!g_battle.weaponForfeit,"alpha57 other source spike cannot disqualify player");
        EncounterSpikeHit(1,999);expect(!g_battle.weaponForfeit,"alpha57 reused source identity rejected");
        EncounterSpikeHit(1,11);expect(g_battle.weaponForfeit,"alpha57 player spike actual-hit notification queues loss");
        g_battle={};g_encounterAdapterThread=oldThread;
        encounter_weapons::EmpSchedule schedule;
        expect(!schedule.Start(1),"alpha57 racer EMP has initial cooldown");
        schedule.Tick(NAN);schedule.Tick(5);expect(schedule.cooldown==5,"alpha57 malformed/stalled delta ignored");
        for(unsigned shot=0;shot<3;++shot) {
            for(unsigned i=0;i<200;++i)schedule.Tick(.1f);
            expect(schedule.Start(1)&&schedule.stock==2-shot,"alpha57 per-racer finite three-shot stock");
            expect(!schedule.Start(1),"alpha57 no second lock during active EMP");
            schedule.Finish();expect(schedule.cooldown==20,"alpha57 hit/miss release starts cooldown");
        }
        for(unsigned i=0;i<210;++i)schedule.Tick(.1f);
        expect(!schedule.Start(1),"alpha57 exhausted racer cannot keep firing");
        VehicleSnapshot source{},target{};source.heading={0,0,1};target.position={0,0,30};
        expect(EncounterEmpCone(source,target,100),"alpha57 forward cop can be targeted");
        target.position.z=-30;expect(!EncounterEmpCone(source,target,100),"alpha57 rear cop excluded from forward EMP");
        target.position.z=101;expect(!EncounterEmpCone(source,target,100),"alpha57 distant cop excluded");
        ResetEncounterArmedRacers();expect(!g_armedRacers[0].source.vehicle&&g_armedGlobalCooldown==0,"alpha57 world clears weapon ownership");
        std::array<std::uint8_t,0x400> hud{};std::array<std::uint8_t,0x2000> race{};std::array<std::uint8_t,0x40> frontend{},iplayer{};
        auto savedFlow=*reinterpret_cast<unsigned*>(Address(kGameFlowState));
        auto savedRace=*reinterpret_cast<std::uintptr_t*>(Address(kRaceStatus));
        auto savedFrontend=*reinterpret_cast<std::uintptr_t*>(Address(0x0091CB20));
        auto savedCount=*reinterpret_cast<unsigned*>(Address(0x0092D884));
        auto savedList=*reinterpret_cast<std::uintptr_t*>(Address(0x0092D87C));
        setGlobal(kGameFlowState,6u);setGlobal(kRaceStatus,reinterpret_cast<std::uintptr_t>(race.data()));
        setGlobal(0x0091CB20,reinterpret_cast<std::uintptr_t>(frontend.data()));put(hud,0x18,std::uint64_t{1});hud[0x2BC]=1;
        expect(!EncounterHudAvailable(hud.data())&&EncounterResultHudAvailable(hud.data()),
            "alpha57 pursuit blocks new challenge prompt but not ongoing battle messages");
        void* playerList=iplayer.data();put(iplayer,0x28,reinterpret_cast<std::uintptr_t>(hud.data()));
        setGlobal(0x0092D884,1u);setGlobal(0x0092D87C,reinterpret_cast<std::uintptr_t>(&playerList));
        expect(CurrentEncounterResultHud()==hud.data(),"alpha57 message resolves current player HUD instead of cached prompt owner");
        put(frontend,0x1E,std::uint16_t{1});expect(!CurrentEncounterResultHud(),"alpha57 menu still suppresses result HUD safely");
        put(frontend,0x1E,std::uint16_t{0});setGlobal(0x0092D884,0u);
        expect(!CurrentEncounterResultHud(),"alpha57 missing player delays notification without stale HUD use");
        g_pendingEncounterMessage.expires=GetTickCount64()-1;FlushEncounterMessage();
        expect(!g_pendingEncounterMessage.expires,"alpha57 deferred notification expires within bounded window");
        setGlobal(kGameFlowState,savedFlow);setGlobal(kRaceStatus,savedRace);setGlobal(0x0091CB20,savedFrontend);
        setGlobal(0x0092D884,savedCount);setGlobal(0x0092D87C,savedList);
    }
