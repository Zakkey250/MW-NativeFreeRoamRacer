    {
        using namespace battle;
        Sample s{};
        s.player.identity={1,11,111};s.rival.identity={2,22,222};
        s.player.forward=s.rival.forward={0,0,1};s.player.speed=s.rival.speed=30;
        s.rival.position={0,0,10};
        Model model;
        expect(model.Start(s).event==Event::Started && model.leader()==Leader::Rival,
            "battle core begins with challenged rival leading");
        expect(model.Start(s).event==Event::None,"battle core rejects duplicate active start");
        s.player.position.z=13;s.rival.position.z=11;
        expect(model.Step(s,0.05f).event==Event::None,"battle core close crossing requires brief confirmation");
        s.player.position.z=15;s.rival.position.z=12;
        expect(model.Step(s,0.05f).event==Event::LeadChanged && model.leader()==Leader::Player,
            "battle core rear-to-front close pass switches leader");
        s.player.position.z=16;s.rival.position.z=19;
        model.Step(s,0.05f);s.player.position.z=17;s.rival.position.z=21;
        expect(model.Step(s,0.05f).event==Event::LeadChanged && model.leader()==Leader::Rival,
            "battle core rival can pass back without stale previous-race role");
        s.player.position.z=24;s.rival.position.z=22;model.Step(s,0.05f);
        s.player.position.z=26;s.rival.position.z=23;model.Step(s,0.05f);
        Change result{};
        for(unsigned i=0;i<29;++i){s.player.position.z+=10;result=model.Step(s,0.1f);}
        expect(model.phase()==Phase::Active && result.cashIntent==0 && model.gap()==293,
            "battle core cannot decide before 300m separation");
        s.player.position.z+=7;result=model.Step(s,0.1f);
        expect(result.event==Event::Won && result.cashIntent==1000 && model.gap()==300,
            "battle core confirmed player leader wins at exactly 300m with fixed reward intent");
        expect(model.Step(s,0.1f).cashIntent==0,"battle core reward intent emitted only once");
        Sample next{};next.player.identity={3,33,333};next.rival.identity={4,44,444};
        next.player.forward=next.rival.forward={0,0,1};next.player.speed=next.rival.speed=30;
        next.rival.position.z=10;
        expect(model.Start(next).event==Event::Started && model.leader()==Leader::Rival,
            "battle core second race resets winner leader and pass history");
        for(unsigned i=0;i<29;++i){next.rival.position.z+=10;result=model.Step(next,0.1f);}
        expect(result.event==Event::Lost && result.cashIntent==0,"battle core rival lead 300m yields loss without reward");
        model.Reset();next.rival.position.z=10;model.Start(next);
        next.rival.position.z=400;
        expect(model.Step(next,0.1f).event==Event::None && model.phase()==Phase::Active,
            "battle core discontinuity resamples without cancelling or deciding a result");
        model.Reset();next.rival.position.z=10;model.Start(next);next.rival.present=false;
        expect(model.Step(next,0.1f).event==Event::None&&model.phase()==Phase::Active,"battle core missing sample holds instead of winning or cancelling");
        next.rival.present=true;model.Reset();model.Start(next);next.rival.identity.simable=999;
        expect(model.Step(next,0.1f).reason==Reason::IdentityChanged,"battle core recycled generation cancels safely");
        model.Start(next);next.freeRoam=false;
        expect(model.Step(next,0.1f).reason==Reason::WorldEnded,"battle core native event or safehouse entry cancels");
        next.freeRoam=true;model.Start(next);next.paused=true;
        expect(model.Step(next,5).event==Event::None && model.phase()==Phase::Active,
            "battle core pause cannot advance finish or pass timers");
        next.paused=false;
        expect(model.Step(next,1).event==Event::None&&model.phase()==Phase::Active,"battle core large sampling gap resamples without cancellation");
        model.Reset();model.Start(next);next.player.position.x=std::numeric_limits<float>::quiet_NaN();
        expect(model.Step(next,0.1f).event==Event::None&&model.phase()==Phase::Active,"battle core NaN sample is held and cannot decide winner");
        next.player.position={};next.rival.position={0,0,10};model.Reset();model.Start(next);
        next.player.position={15,0,13};next.rival.position.z=11;
        model.Step(next,0.1f);
        expect(model.leader()==Leader::Rival,"battle core separated parallel road is not an overtake");
        model.Reset();next.player.position={};next.rival.position={0,0,10};model.Start(next);
        next.player.position={0,6,13};next.rival.position.z=11;model.Step(next,0.1f);
        expect(model.leader()==Leader::Rival,"battle core grade-separated crossing is not an overtake");
        model.Reset();next.player.position={};next.rival.position={0,0,10};model.Start(next);
        next.player.position.z=13;next.rival.position.z=11;next.player.forward={0,0,-1};
        model.Step(next,0.1f);
        expect(model.leader()==Leader::Player,"battle core actual forward progress can pass while car body points backward");
        model.Reset();next.player.forward={0,0,1};next.player.position={};next.rival.position={0,0,10};model.Start(next);
        for(unsigned i=0;i<5;++i){next.rival.position.z+=10;model.Step(next,0.1f);}
        next.rival.forward={0,0,-1};next.rival.position.z-=2;model.Step(next,0.1f);
        expect(model.leader()==Leader::Rival,"battle core distant hairpin heading reversal alone cannot swap leader");
    }
