    {
        encounter_route::Pursuit pursuit;
        auto tick=[&](float gap,bool custom=true){return pursuit.Update({0,0,gap},{0,0,1},{0,0,0},{0,0,1},.05f,custom);};
        for(unsigned i=0;i<4;++i)tick(15);
        expect(!pursuit.passing(),"alpha55 Custom 15m entry still requires sustained opportunity");
        tick(15);expect(pursuit.passing(),"alpha55 Custom enters attack at 15m after 215ms");
        for(unsigned i=0;i<40;++i)tick(10);
        expect(pursuit.passing(),"alpha55 Custom attack continues beyond former 1.5s timeout");
        tick(69);expect(pursuit.passing(),"alpha55 15-70m hysteresis retains attack");
        tick(70);expect(!pursuit.passing(),"alpha55 70m separation returns to trajectory pursuit");
        for(unsigned i=0;i<10;++i)tick(10);
        expect(!pursuit.passing(),"alpha55 cooldown prevents immediate phase chatter");
        for(unsigned i=0;i<20;++i)tick(10);
        expect(pursuit.passing(),"alpha55 retry possible after cooldown");
        pursuit.Reset();for(unsigned i=0;i<5;++i)tick(10);
        for(unsigned i=0;i<161;++i)tick(10);
        expect(!pursuit.passing(),"alpha55 attack eventually expires after bounded eight seconds");
        pursuit.Reset();for(unsigned i=0;i<20;++i)tick(15.1f);
        expect(!pursuit.passing(),"alpha55 outside 15m cannot start attack");
        pursuit.Reset();for(unsigned i=0;i<20;++i)tick(40,false);
        expect(!pursuit.passing(),"alpha55 Stable retains original 5m entry");
        for(unsigned i=0;i<5;++i)tick(4,false);
        expect(pursuit.passing(),"alpha55 Stable close attack unchanged");
        for(unsigned i=0;i<31;++i)tick(4,false);
        expect(!pursuit.passing(),"alpha55 Stable 1.5s bound unchanged");
        for(auto rival:{battle::Point{13,0,0},battle::Point{0,6,0}}) {
            pursuit.Reset();for(unsigned i=0;i<10;++i)pursuit.Update({0,0,5},{0,0,1},rival,{0,0,1},.05f,true);
            expect(!pursuit.passing(),"alpha55 separate lateral or stacked road not a passing opportunity");
        }
        pursuit.Reset();for(unsigned i=0;i<10;++i)pursuit.Update({0,0,5},{0,0,1},{0,0,0},{0,0,-1},.05f,true);
        expect(!pursuit.passing(),"alpha55 opposing direction cannot start attack");
        pursuit.Reset();for(unsigned i=0;i<5;++i)pursuit.Update({0,0,5},{0,0,1},{10,0,0},{0,0,1},.05f,true);
        expect(pursuit.passing(),"alpha55 broad same-direction lane offset can enter attack");
        pursuit.Update({0,0,5},{0,0,1},{19,0,0},{0,0,1},.05f,true);
        expect(!pursuit.passing(),"alpha55 excessive lateral divergence returns to trajectory");
        for(float gap:{0.f,5.f,25.f,50.f})expect(encounter_custom::ChaseSpeed(60,gap,1,true)==68,
            "alpha55 straight attack retains 8mps closing advantage near player");
        expect(encounter_custom::ChaseSpeed(60,200,1)==78,
            "alpha55 distant trailing demand unchanged");
        expect(encounter_custom::ChaseSpeed(99,20,1,true)==100,
            "alpha55 attack retains absolute desired-speed cap");
        expect(encounter_custom::ChaseSpeed(60,20,.65f,true)<68,
            "alpha55 turning still reduces attack speed demand");
        using encounter_custom::AttackEntrySpeed;
        expect(AttackEntrySpeed(50,80,0)==80&&AttackEntrySpeed(50,80,1)==80,
            "alpha55 attack entry keeps prior request through first second");
        expect(AttackEntrySpeed(50,80,1.5f)==65&&AttackEntrySpeed(50,80,2)==50,
            "alpha55 entry floor releases smoothly over second second");
        expect(AttackEntrySpeed(90,80,0)==90,"alpha55 new higher attack demand not suppressed");
        expect(AttackEntrySpeed(50,200,0)==100,"alpha55 entry carry respects desired speed ceiling");
        expect(AttackEntrySpeed(-1,80,0)==-1&&AttackEntrySpeed(50,NAN,0)==50&&AttackEntrySpeed(50,80,NAN)==50,
            "alpha55 invalid entry data cannot force acceleration");
        expect(encounter_custom::RaiseChaseSpeed(0,AttackEntrySpeed(50,80,0))==0,
            "alpha55 carry never overrides native stop");
    }
