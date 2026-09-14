    {
        using namespace battle;
        auto start=[] {
            Sample s{};s.player.identity={1,11,111};s.rival.identity={2,22,222};
            s.player.forward=s.rival.forward={0,0,1};s.rival.position.z=10;
            s.player.speed=s.rival.speed=20;return s;
        };
        auto s=start();Model m;m.Start(s);
        s.rival.speed=0;s.rival.forward={1,0,0};
        for(unsigned i=0;i<1200;++i) m.Step(s,.1f);
        expect(m.phase()==Phase::Active&&m.leader()==Leader::Rival,"alpha29 crashed stationary rival remains in battle for 120 seconds");
        s.player.position.z=13;s.player.speed=0;s.player.forward={0,1,0};
        m.Step(s,.05f);m.Step(s,.05f);
        expect(m.leader()==Leader::Player&&m.signedProgress()>0,"alpha29 passing spun stopped rival uses common progress not either heading or speed");
        for(unsigned i=0;i<30;++i){s.player.position.z+=10;m.Step(s,.1f);}
        expect(m.phase()==Phase::Won,"alpha29 immobilized rival loses by 300m separation without cancellation");
        s=start();m.Reset();m.Start(s);s.player.speed=0;s.player.forward={0,0,-1};
        for(unsigned i=0;i<29;++i){s.rival.position.z+=10;m.Step(s,.1f);}
        expect(m.phase()==Phase::Lost,"alpha29 immobilized player loses normally at 300m");
        s=start();m.Reset();m.Start(s);
        s.rival.forward={0,0,-1};m.Step(s,.1f);
        expect(m.leader()==Leader::Rival,"alpha29 stationary heading flip alone cannot swap lead");
        s.player.forward={0,0,-1};s.player.position.z=-5;m.Step(s,.1f);
        expect(m.leader()==Leader::Rival&&m.signedProgress()<0,"alpha29 reverse movement away is not a pass");
        s=start();m.Reset();m.Start(s);s.paused=true;s.rival.position.z=350;
        m.Step(s,10);s.paused=false;m.Step(s,.1f);
        expect(m.phase()==Phase::Active,"alpha29 menu hold and first resume sample cannot end battle");
        s=start();m.Reset();m.Start(s);s.rival.position.x=std::numeric_limits<float>::quiet_NaN();
        m.Step(s,.1f);s.rival.position.x=0;m.Step(s,.1f);
        expect(m.phase()==Phase::Active,"alpha29 transient invalid physics recovers without cancellation");
        // Shared polyline distinguishes positions on the incoming and outgoing
        // sides of a bend even when projecting onto one heading would reverse.
        SharedTrail trail;trail.Seed({0,0,0},{0,0,10},{0,0,1});
        trail.Extend({0,0,20});trail.Extend({10,0,20});trail.Extend({20,0,20});
        const auto behind=trail.Project({0,0,18}),ahead=trail.Project({23,0,20});
        expect(behind.valid&&ahead.valid&&ahead.progress>behind.progress&&behind.lateral<.01f,
            "alpha29 shared progress follows a right-angle corner instead of a single live heading");
        const auto upper=trail.Project({20,6,20}),parallel=trail.Project({20,0,40});
        expect(upper.height>4&&parallel.lateral>12,"alpha29 elevation and remote parallel-road samples fail association");
        for(unsigned i=0;i<1000;++i) trail.Extend({30+float(i)*2,0,20});
        const auto tail=trail.Project({2026,0,20}),tip=trail.Project({2030,0,20});
        expect(tip.progress>tail.progress&&tip.valid,"alpha29 bounded trail rollover preserves forward progress");
        // Active policy must be independent of the HUD features/pursuit bits.
        std::array<unsigned char,0x30> frontend{};
        auto* global=reinterpret_cast<void**>(Address(0x0091CB20));void* saved=*global;*global=frontend.data();
        expect(!EncounterBattlePaused(),"alpha29 active battle does not consult HUD visibility or pursuit gate");
        frontend[0x1E]=1;
        expect(EncounterBattlePaused(),"alpha29 actual frontend overlay holds battle rather than ending it");
        *global=saved;
        g_battle={};g_battle.model.Start(start());g_battle.player=start().player.identity;g_battle.rival=start().rival.identity;
        EncounterMissingSample(.1f);
        expect(g_battle.model.phase()==Phase::Active,"alpha29 transient absent enumeration does not cancel battle");
        EncounterMissingSample(10);
        expect(!EncounterBattleBusy(),"alpha29 persistently absent vehicle ends safely without reward or stale dereference");
    }
