    {
        PopulationTransaction transaction;
        for(unsigned missing=0;missing<=15;++missing){
            (void)missing;
            expect(transaction.Requested(100)==0,"alpha65 no standing reservation regardless of racer deficit");
        }
        transaction.Begin(100);
        expect(transaction.Requested(100)==1&&transaction.Requested(1599)==1,"alpha65 one prepared slot reserved");
        expect(transaction.Requested(1600)==0&&transaction.Expired(1600),"alpha65 vacancy wait ends at fixed 1500ms");
        transaction.Reset();expect(!transaction.Expired(2000)&&!transaction.Awaiting(2000),"alpha65 cancellation resets deadline");
        transaction.Begin(2000);
        for(unsigned now=2000;now<3500;now+=50){
            expect(transaction.Requested(now)==1&&transaction.deadline==3500,"alpha65 polls cannot slide the deadline");
        }
        // Model the exact starvation case in the log: 22 native cache entries,
        // 0/6 racers. Idle lookup now requests zero instead of six, so the
        // provider hook returns its native admission result unchanged.
        transaction.Reset();
        const auto admitted=[](bool native,unsigned live,unsigned reserved){return native&&(!reserved||live<28-reserved);};
        expect(!admitted(true,22,6),"alpha65 reproduce old deficit reservation starvation");
        for(unsigned live:{22u,27u,28u}){
            expect(admitted(true,live,transaction.Requested(4000)),"alpha65 no-anchor path preserves native cache reuse admission");
            expect(!admitted(false,live,transaction.Requested(4000)),"alpha65 never overrides native rejection");
        }
        for(unsigned repeat=0;repeat<200;++repeat){
            const auto now=GetTickCount64();
            g_populationTransaction.Begin(now);
            {PopulationAttemptScope scope;g_populationTransaction.Reset();}
            expect(!g_populationTransaction.Awaiting(now),"alpha65 completed or failed prepared attempt releases reservation");
            {PopulationAttemptScope scope;g_populationTransaction.Begin(now);}
            expect(g_populationTransaction.Requested(now)==1,"alpha65 pending one-car reclaim retains bounded lease");
            ReleasePopulationReservation();
            expect(g_populationTransaction.Requested(now)==0,"alpha65 missing anchor/world exit restores native traffic");
        }
    }
