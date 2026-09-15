    {
        using encounter_custom::ChaseSpeed;
        using encounter_custom::RaiseChaseSpeed;
        expect(std::abs(ChaseSpeed(280/3.6f,160,1)-(280/3.6f+18))<.001f,
            "alpha54 observed 280kmh player demands faster chase rather than 160-220kmh cruise");
        expect(ChaseSpeed(60,80,1)==70&&ChaseSpeed(60,8,1)==61,
            "alpha54 distance-dependent closing speed tapers near player");
        expect(ChaseSpeed(60,300,1)==78,"alpha54 closing speed capped at 18mps");
        expect(ChaseSpeed(150,200,1)==100,"alpha54 requested speed capped at 360kmh");
        expect(ChaseSpeed(60,160,.5f)<ChaseSpeed(60,160,1),"alpha54 sharp aim angle reduces added speed demand");
        expect(ChaseSpeed(60,160,.85f)==ChaseSpeed(60,160,1),"alpha54 gentle aim changes retain pursuit pace");
        expect(ChaseSpeed(0,0,1)==0&&ChaseSpeed(0,80,1)==10,"alpha54 stopped player does not impose a high minimum speed");
        for(float gap:{-1.f,301.f,NAN})expect(ChaseSpeed(60,gap,1)==-1,"alpha54 invalid gap rejected");
        expect(ChaseSpeed(NAN,50,1)==-1&&ChaseSpeed(60,50,NAN)==-1&&ChaseSpeed(60,50,-1)==-1,
            "alpha54 invalid speed or opposing guidance rejected");
        expect(RaiseChaseSpeed(40,80)==80&&RaiseChaseSpeed(90,80)==90,"alpha54 only raises eligible native positive speed");
        expect(RaiseChaseSpeed(0,80)==0&&RaiseChaseSpeed(-5,80)==-5&&RaiseChaseSpeed(.1f,80)==.1f,
            "alpha54 native stop and reverse requests preserved");
        expect(RaiseChaseSpeed(40,-1)==40&&RaiseChaseSpeed(40,101)==40&&RaiseChaseSpeed(40,NAN)==40,
            "alpha54 missing or malformed demand leaves native speed untouched");
        expect(std::isnan(RaiseChaseSpeed(NAN,80)),"alpha54 nonfinite native value is forwarded unchanged");
        g_battle.customSpeedDemand=90;EndEncounterBattle(false,false);
        expect(g_battle.customSpeedDemand<0&&!g_battle.customDirect,"alpha54 battle end clears demand and eligibility");
    }
