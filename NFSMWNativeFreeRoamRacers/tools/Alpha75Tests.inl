    {
        std::array<std::uint8_t,0x28> memory{};
        std::array<std::array<std::uint8_t,0x40>,16> nodes{};
        const auto manager=reinterpret_cast<std::uintptr_t>(memory.data());
        const auto global=Address(kAudioRacerPoolGlobal);
        std::uintptr_t oldPool=0;AudioRead(global,0,&oldPool);
        std::memcpy(reinterpret_cast<void*>(global),&manager,sizeof(manager));
        put(memory,0,Address(kAudioRacerPoolVtable));
        put(memory,0x0C,std::uint32_t{3});put(memory,0x1C,std::uint32_t{1});memory[0x18]=1;
        auto prepare=[&](unsigned count) {
            for(unsigned i=0;i<nodes.size();++i){
                nodes[i]={};put(nodes[i],0,Address(0x00896FA8));
                put(nodes[i],4,i+1<count?reinterpret_cast<std::uintptr_t>(nodes[i+1].data()):std::uintptr_t{0});
                put(nodes[i],0x34,std::uintptr_t{0x10000u+i*0x100u});nodes[i][0x38]=1;
            }
            put(memory,0x10,count?reinterpret_cast<std::uintptr_t>(nodes[0].data()):std::uintptr_t{0});
            put(memory,0x14,count);
        };
        for(unsigned count:{0u,1u,4u,6u,16u}){
            prepare(count);const auto copy=nodes;const auto managerCopy=memory;
            const auto h=ReadAudioPoolHealth(manager,ReadAudioPoolContext(manager));
            expect(h.complete&&h.slots==count&&h.active==count&&h.bound==count,
                "alpha75 counts real bounded pool nodes, not requested allocation or history");
            expect(copy==nodes&&memory==managerCopy,"alpha75 pool evidence never writes native objects");
        }
        prepare(6);nodes[0][0x38]=0;put(nodes[1],0x34,std::uintptr_t{0});
        auto c=ReadAudioPoolContext(manager);
        auto h=ReadAudioPoolHealth(manager,c);
        expect(h.complete&&h.slots==6&&h.active==5&&h.bound==4,
            "alpha75 distinguishes allocated active and bound, never claims audible output");
        for(unsigned bit=0;bit<8;++bit){auto bad=c;bad.valid&=~(1u<<bit);
            expect(!ReadAudioPoolHealth(manager,bad).complete,"alpha75 unreadable manager evidence is unknown");}
        auto bad=c;bad.global=0;
        expect(!ReadAudioPoolHealth(manager,bad).complete,"alpha75 manager identity mismatch rejected");
        bad=c;bad.kind=2;
        expect(!ReadAudioPoolHealth(manager,bad).complete,"alpha75 police pool not interpreted as racers");
        bad=c;bad.active=0;
        expect(!ReadAudioPoolHealth(manager,bad).complete,"alpha75 allocation not ready is not confirmed");
        bad=c;bad.count=17;
        expect(!ReadAudioPoolHealth(manager,bad).complete,"alpha75 observation bound rejects larger pool without mutation");
        bad=c;bad.count=5;
        expect(!ReadAudioPoolHealth(manager,bad).complete,"alpha75 long list count mismatch rejected");
        bad=c;bad.count=7;
        expect(!ReadAudioPoolHealth(manager,bad).complete,"alpha75 short list count mismatch rejected");
        prepare(6);c=ReadAudioPoolContext(manager);put(nodes[1],4,reinterpret_cast<std::uintptr_t>(nodes[0].data()));
        expect(std::strcmp(ReadAudioPoolHealth(manager,c).status,"cycle")==0,"alpha75 list cycle bounded");
        prepare(6);put(nodes[1],0x34,std::uintptr_t{0x10000});
        expect(!ReadAudioPoolHealth(manager,c).complete,"alpha75 duplicate active binding not reported as six owners");
        prepare(6);put(nodes[0],0,std::uintptr_t{0});
        expect(!ReadAudioPoolHealth(manager,c).complete,"alpha75 foreign node not traversed");
        prepare(6);put(nodes[0],4,std::uintptr_t{1});
        expect(!ReadAudioPoolHealth(manager,c).complete,"alpha75 invalid pointer rejected before dereference");
        prepare(6);memory[0x18]=0;
        expect(std::strcmp(ReadAudioPoolHealth(manager,c).status,"changed-during-read")==0,
            "alpha75 readiness replacement between context and enumeration invalidates evidence");
        ULONGLONG next=0;
        expect(!AudioHealthSampleDue(false,100,&next)&&next==0,"alpha75 opt-out performs no sample");
        expect(AudioHealthSampleDue(true,100,&next)&&next==5100,"alpha75 sample arms five second floor");
        for(ULONGLONG now=101;now<5100;now+=50)
            expect(!AudioHealthSampleDue(true,now,&next),"alpha75 management ticks never increase observer cadence");
        expect(AudioHealthSampleDue(true,5100,&next),"alpha75 next sample resumes at boundary");
        g_audioHealthNext=1234;g_audioHealthPeakBound=6;ResetAudioPoolHealth();
        expect(!g_audioHealthNext&&!g_audioHealthPeakBound,"alpha75 world exit clears evidence without native references");
        const auto settingsBefore=g_settings;const auto hookBefore=g_audioPoolHookEnabled;
        g_settings.lbEngineAudioExpansion=false;g_audioPoolHookEnabled=true;LogAudioPoolHealth();
        expect(g_audioHealthNext==0,"alpha75 real observer is no-op when optional switch is off");
        g_settings.lbEngineAudioExpansion=true;g_audioPoolHookEnabled=false;LogAudioPoolHealth();
        expect(g_audioHealthNext==0,"alpha75 real observer is no-op without validated pool hook");
        g_settings=settingsBefore;g_audioPoolHookEnabled=hookBefore;
        std::memcpy(reinterpret_cast<void*>(global),&oldPool,sizeof(oldPool));
    }
