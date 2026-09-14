// Included inside RunCacheTests; all memory belongs to this test process.
    put(producer,0,Address(0x00896BF0));
    AudioLifeSnapshot lifeSnapshot;
    expect(ReadAudioLife(producerAddress,&lifeSnapshot) && lifeSnapshot.valid==0x1FF &&
        lifeSnapshot.id.object==objectAddress,"lifecycle captures bound engine producer and field validity");
    expect(!ReadAudioLife(1,&lifeSnapshot),"lifecycle unreadable producer stays unknown");
    put(producer,0,Address(0x00896D2C));
    expect(!ReadAudioLife(producerAddress,&lifeSnapshot),"lifecycle rejects other producer classes");
    put(producer,0,Address(0x00896BF0));
    put(producer,0x14,std::uintptr_t{0}); put(producer,0x18,std::uintptr_t{0});
    expect(ReadAudioLife(producerAddress,&lifeSnapshot) && !(lifeSnapshot.valid&28),
        "lifecycle retains new unbound producer without inventing a parent");
    g_audioLifeRecords={};g_audioLifeEvictions=0;
    StoreAudioLife(0,lifeSnapshot,123,1000);
    const auto lifeGeneration=g_audioLifeRecords[0].generation;
    put(producer,0x14,reinterpret_cast<std::uintptr_t>(producerParent.data()));
    put(producer,0x18,reinterpret_cast<std::uintptr_t>(producerState.data()));
    ReadAudioLife(producerAddress,&lifeSnapshot);
    StoreAudioLife(1,lifeSnapshot,lifeSnapshot.id.parent,1001);
    StoreAudioLife(2,lifeSnapshot,0,1002);
    auto lifeMatch=FindAudioLife(objectAddress);
    expect(lifeMatch.record.generation==lifeGeneration && lifeMatch.record.counts[0]==1 &&
        lifeMatch.record.counts[1]==1 && lifeMatch.record.counts[2]==1 &&
        std::strcmp(lifeMatch.status,"live-link")==0,"lifecycle joins create bind init with current verified link");
    put(producerParent,0x34,std::uintptr_t{0});
    expect(std::strcmp(FindAudioLife(objectAddress).status,"historical-link")==0,
        "lifecycle detached producer does not claim current linkage");
    ReadAudioLife(producerAddress,&lifeSnapshot);
    StoreAudioLife(1,lifeSnapshot,lifeSnapshot.id.parent,1003);
    put(producerParent,0x34,objectAddress);
    expect(std::strcmp(FindAudioLife(objectAddress).status,"live-link")==0,
        "lifecycle resolves object assigned after parent binding");
    StoreAudioLife(0,lifeSnapshot,456,1004);
    expect(g_audioLifeRecords[0].generation!=lifeGeneration && g_audioLifeRecords[0].counts[1]==0 &&
        g_audioLifeRecords[0].counts[2]==0,"lifecycle recycled factory address resets prior history");
    for(unsigned i=0;i<129;++i) {
        auto s=lifeSnapshot;s.id.source=100+i;StoreAudioLife(0,s,i,2000+i);
    }
    expect(g_audioLifeRecords.size()==128 && g_audioLifeEvictions==2,
        "lifecycle capacity is fixed and history eviction is explicit");
    expect(std::strcmp(FindAudioLife(0).status,"no-object")==0,
        "lifecycle zero object cannot match unbound history");
    AcquireSRWLockExclusive(&g_audioLifeLock);
    expect(std::strcmp(FindAudioLife(objectAddress).status,"busy")==0,
        "lifecycle periodic read never waits on observer lock");
    ReleaseSRWLockExclusive(&g_audioLifeLock);
    // Executable stubs reproduce the three prologue shapes and calling conventions.
    // Factory has a caller-popped argument despite its plain RET (not a no-arg factory).
    const std::uint8_t lifeFactoryStub[]={0x6A,0xFF,0x68,0,0,0,0,
        0x8B,0x44,0x24,0x0C,0x83,0xC4,8,0xC3};
    const std::uint8_t lifeBindStub[]={0x8B,0x44,0x24,4,0x56,0x8B,0xF1,
        0x89,0x46,0x14,0x89,0x46,0x18,0xFF,0x46,0x20,0x33,0xC1,0x5E,0xC2,4,0};
    const std::uint8_t lifeInitStub[]={0x53,0x56,0x8B,0xF1,0x33,0xDB,
        0xFF,0x46,0x20,0x8B,0x46,0x24,0x5E,0x5B,0xC3};
    for(unsigned stage=0;stage<3;++stage) {
        auto* code=VirtualAlloc(nullptr,4096,MEM_COMMIT|MEM_RESERVE,PAGE_EXECUTE_READWRITE);
        const void* stubs[]={lifeFactoryStub,lifeBindStub,lifeInitStub};
        const std::size_t sizes[]={sizeof(lifeFactoryStub),sizeof(lifeBindStub),sizeof(lifeInitStub)};
        void* hooks[]={reinterpret_cast<void*>(&AudioLifeFactoryHook),reinterpret_cast<void*>(&AudioLifeBindHook),reinterpret_cast<void*>(&AudioLifeInitHook)};
        void** originals[]={reinterpret_cast<void**>(&g_originalAudioLifeFactory),reinterpret_cast<void**>(&g_originalAudioLifeBind),reinterpret_cast<void**>(&g_originalAudioLifeInit)};
        bool installed=false;
        if(code) {
            std::memcpy(code,stubs[stage],sizes[stage]);FlushInstructionCache(GetCurrentProcess(),code,sizes[stage]);
            if(MH_Initialize()==MH_OK) installed=MH_CreateHook(code,hooks[stage],originals[stage])==MH_OK && MH_EnableHook(code)==MH_OK;
        }
        expect(installed,stage==0?"lifecycle factory trampoline installs":stage==1?"lifecycle bind trampoline installs":"lifecycle init trampoline installs");
        if(installed) {
            g_audioLifeRecords={};
            put(producer,0x20,std::uint32_t{0}); put(producer,0x24,std::uint32_t{0xDE123456});
            auto expectedProducer=producer;
            if(stage==1) {put(expectedProducer,0x14,reinterpret_cast<std::uintptr_t>(producerParent.data()));put(expectedProducer,0x18,reinterpret_cast<std::uintptr_t>(producerParent.data()));}
            if(stage) put(expectedProducer,0x20,std::uint32_t{1});
            const auto savedParent=producerParent;
            const auto savedState=producerState;
            const auto savedObject=audioObject;
            const auto beforeCalls=g_audioLifeCalls[stage].load(),beforeReturns=g_audioLifeReturns[stage].load();
            auto invoke=[&]() -> std::uintptr_t {
                if(stage==0) return reinterpret_cast<AudioLifeFactoryFn>(code)(producerAddress);
                if(stage==1) return reinterpret_cast<AudioLifeBindFn>(code)(producer.data(),reinterpret_cast<std::uintptr_t>(producerParent.data()));
                return reinterpret_cast<AudioLifeInitFn>(code)(producer.data());
            };
            const auto expected=stage==0?producerAddress:stage==1?
                reinterpret_cast<std::uintptr_t>(producerParent.data())^producerAddress:0xDE123456u;
            expect(invoke()==expected && g_audioLifeCalls[stage].load()==beforeCalls+1 &&
                g_audioLifeReturns[stage].load()==beforeReturns+1 && g_audioLifeRecords[0].counts[stage]==1,
                "lifecycle trampoline preserves raw arguments EAX and exact-once native execution");
            expect(producer==expectedProducer && producerParent==savedParent && producerState==savedState && audioObject==savedObject,
                "lifecycle observer adds no writes to producer parent state or audio object");
            const auto drops=g_audioLifeDrops.load();
            AcquireSRWLockExclusive(&g_audioLifeLock);
            const auto contended=invoke();
            ReleaseSRWLockExclusive(&g_audioLifeLock);
            expect(contended==expected && g_audioLifeDrops.load()==drops+1 && g_audioLifeCalls[stage].load()==beforeCalls+2,
                "lifecycle contention drops observation without blocking native execution");
            if(stage==0) {
                const auto unknown=g_audioLifeUnknown.load();
                expect(reinterpret_cast<AudioLifeFactoryFn>(code)(0)==0 && g_audioLifeUnknown.load()==unknown+1,
                    "lifecycle factory failure preserves null result and reports unknown");
            }
        }
        if(code) {MH_DisableHook(code);MH_RemoveHook(code);MH_Uninitialize();VirtualFree(code,0,MEM_RELEASE);}
    }
