    AudioPoolContext poolContext;
    constexpr std::uintptr_t fakePool=0x12340000;
    poolContext.valid=0x3FFF;poolContext.global=fakePool;poolContext.vt=Address(kAudioRacerPoolVtable);
    poolContext.type=3;poolContext.kind=1;poolContext.raceStatus=0x12350000;poolContext.budget=4;
    AudioPoolReason poolReason;
    auto choosePool=[&](const AudioPoolContext& c,int count=0,unsigned requested=1,bool enabled=true) {
        return SelectAudioPoolCount(count,fakePool,c,enabled,requested,&poolReason);
    };
    expect(choosePool(poolContext)==1 && poolReason==AudioPoolReason::Adjusted,
        "audio pool empty no-event native budget can reserve a single native slot");
    expect(choosePool(poolContext,0,4)==4 && choosePool(poolContext,0,99)==4,
        "audio pool fallback never exceeds native four-slot cap");
    expect(choosePool(poolContext,0,0)==0 && choosePool(poolContext,0,1,false)==0,
        "audio pool disabled settings preserve zero native count");
    expect(choosePool(poolContext,3)==3 && choosePool(poolContext,-1)==-1 && choosePool(poolContext,5)==5,
        "audio pool never alters nonzero native event counts including unexpected values");
    for(unsigned bit=0;bit<14;++bit) {
        auto c=poolContext;c.valid&=~(1u<<bit);
        expect(choosePool(c)==0 && poolReason==AudioPoolReason::Unreadable,
            "audio pool missing prerequisite read fails closed");
    }
    auto changedPool=poolContext;changedPool.global=0;
    expect(choosePool(changedPool)==0,"audio pool foreign manager address is refused");
    changedPool=poolContext;changedPool.vt=0;
    expect(choosePool(changedPool)==0,"audio pool unknown manager vtable is refused");
    changedPool=poolContext;changedPool.type=2;
    expect(choosePool(changedPool)==0,"audio pool player class is untouched");
    changedPool=poolContext;changedPool.kind=2;
    expect(choosePool(changedPool)==0,"audio pool cop class is untouched");
    changedPool=poolContext;changedPool.raceParms=1;
    expect(choosePool(changedPool)==0,"audio pool event parameters veto fallback even in Roaming mode");
    changedPool=poolContext;changedPool.playMode=1;
    expect(choosePool(changedPool)==0,"audio pool Racing mode is untouched even without parameters");
    changedPool=poolContext;changedPool.raceStatus=0;
    expect(choosePool(changedPool)==0,"audio pool missing race status fails closed");
    changedPool=poolContext;changedPool.head=1;
    expect(choosePool(changedPool)==0,"audio pool existing list prevents duplicate allocation");
    changedPool=poolContext;changedPool.count=1;
    expect(choosePool(changedPool)==0,"audio pool inconsistent nonzero count prevents duplicate allocation");
    changedPool=poolContext;changedPool.budget=3;
    expect(choosePool(changedPool)==0,"audio pool absent native four-unit budget prevents fallback");
    changedPool=poolContext;changedPool.budget=0xFFFFFFFF;
    expect(choosePool(changedPool)==0,"audio pool nonsensical budget fails closed");
    changedPool=poolContext;changedPool.alternate=1;
    expect(choosePool(changedPool)==0,"audio pool alternate low-budget configuration is untouched");
    changedPool=poolContext;changedPool.countA=1;
    expect(choosePool(changedPool)==0,"audio pool changed first native source count vetoes fallback");
    changedPool=poolContext;changedPool.countB=1;
    expect(choosePool(changedPool)==0,"audio pool changed second native source count vetoes fallback");

    auto frontendContext=poolContext;
    frontendContext.raceStatus=0;frontendContext.frontend=0x12360000;frontendContext.valid=0xFCFF;
    expect(choosePool(frontendContext)==1,"audio pool native alternate event lookup permits pre-race-status empty context");
    for(unsigned bit=14;bit<16;++bit) {
        auto c=frontendContext;c.valid&=~(1u<<bit);
        expect(choosePool(c)==0,"audio pool unreadable alternate owner or parameters fails closed");
    }
    changedPool=frontendContext;changedPool.frontend=0;
    expect(choosePool(changedPool)==0,"audio pool null alternate owner does not imply no event");
    changedPool=frontendContext;changedPool.raceParms=1;
    expect(choosePool(changedPool)==0,"audio pool alternate event parameters veto early fallback");
    changedPool=frontendContext;changedPool.raceStatus=1;
    expect(choosePool(changedPool)==0,"audio pool unreadable present race status cannot use alternate owner");
    changedPool=frontendContext;changedPool.valid&=~128u;
    expect(choosePool(changedPool)==0,"audio pool unreadable global status pointer cannot use alternate owner");

    std::array<std::uint8_t,0x28> poolMemory{};
    std::array<std::uint8_t,0x1970> poolRace{};
    std::array<std::uint8_t,0x30> poolFrontend{};
    const auto poolAddress=reinterpret_cast<std::uintptr_t>(poolMemory.data());
    auto setGlobal=[&](std::uintptr_t va,const auto& value) {std::memcpy(reinterpret_cast<void*>(Address(va)),&value,sizeof(value));};
    setGlobal(kAudioRacerPoolGlobal,poolAddress);
    setGlobal(kRaceStatus,reinterpret_cast<std::uintptr_t>(poolRace.data()));
    setGlobal(0x0091E004,reinterpret_cast<std::uintptr_t>(poolFrontend.data()));
    setGlobal(0x00911ED4,std::uint32_t{4});setGlobal(0x0092CE44,std::uint32_t{0});setGlobal(0x0092CFC4,std::uint32_t{0});
    setGlobal(0x009142E7,std::uint8_t{0});
    put(poolMemory,0,Address(kAudioRacerPoolVtable));put(poolMemory,0x0C,std::uint32_t{3});put(poolMemory,0x1C,std::uint32_t{1});
    auto readPool=ReadAudioPoolContext(poolAddress);
    expect(readPool.valid==0x3FFF && readPool.type==3 && readPool.count==0 && readPool.budget==4,
        "audio pool context mirrors exact native globals and fields without calls");
    expect(std::strcmp(ReadAudioPoolMembership(readPool,objectAddress).status,"empty")==0,
        "audio pool empty list is measured directly rather than inferred from source history");
    std::array<std::uint8_t,0x40> poolNode{};
    put(poolNode,0,Address(0x00896FA8));put(poolNode,0x34,objectAddress);poolNode[0x38]=1;
    put(poolMemory,0x10,reinterpret_cast<std::uintptr_t>(poolNode.data()));put(poolMemory,0x14,std::uint32_t{1});
    readPool=ReadAudioPoolContext(poolAddress);
    expect(ReadAudioPoolMembership(readPool,objectAddress).matched==1 && ReadAudioPoolMembership(readPool,objectAddress).active==1,
        "audio pool measures active ownership using the native parent chain");
    poolNode[0x38]=0;
    expect(ReadAudioPoolMembership(readPool,objectAddress).matched==0,
        "audio pool inactive object is not reported as playing");
    put(poolNode,4,reinterpret_cast<std::uintptr_t>(poolNode.data()));readPool.count=2;
    expect(std::strcmp(ReadAudioPoolMembership(readPool,objectAddress).status,"cycle")==0,
        "audio pool cycle terminates with unknown membership");
    readPool.count=17;
    expect(std::strcmp(ReadAudioPoolMembership(readPool,objectAddress).status,"limit")==0,
        "audio pool read is bounded even on corrupted counts");
    readPool.count=1;put(poolNode,4,std::uintptr_t{0});put(poolNode,0,std::uintptr_t{0});
    expect(ReadAudioPoolMembership(readPool,objectAddress).matched==-1,
        "audio pool foreign parent is not traversed");
    put(poolMemory,0x10,std::uintptr_t{0});put(poolMemory,0x14,std::uint32_t{0});
    // Native-shaped inline site: EAX=count, EBP=pool; TEST/JLE/PUSH ESI are relocated by MinHook.
    // Both the forward zero branch and the positive branch return through caller-owned locals.
    auto* poolCode=static_cast<std::uint8_t*>(VirtualAlloc(nullptr,4096,MEM_COMMIT|MEM_RESERVE,PAGE_EXECUTE_READWRITE));
    const std::uint8_t prefix[]={0x55,0x8B,0x6C,0x24,8,0x8B,0x44,0x24,0x0C,0x83,0xEC,0x10};
    const std::uint8_t countSite[]={0x85,0xC0,0x7E,0x45,0x56,0x89,0x44,0x24,0x0C,
        0x8B,0x44,0x24,0x0C,0x5E,0xEB,0x39};
    const std::uint8_t suffix[]={0x83,0xC4,0x10,0x5D,0xC3};
    bool poolInstalled=false;
    if(poolCode) {
        std::memset(poolCode,0x90,256);
        std::memcpy(poolCode,prefix,sizeof(prefix));std::memcpy(poolCode+sizeof(prefix),countSite,sizeof(countSite));
        std::memcpy(poolCode+sizeof(prefix)+0x49,suffix,sizeof(suffix));
        FlushInstructionCache(GetCurrentProcess(),poolCode,256);
        if(MH_Initialize()==MH_OK) poolInstalled=MH_CreateHook(poolCode+sizeof(prefix),&AudioPoolCountHook,&g_originalAudioPoolCount)==MH_OK &&
            MH_EnableHook(poolCode+sizeof(prefix))==MH_OK;
    }
    expect(poolInstalled,"audio pool inline conditional-branch MinHook trampoline installs");
    if(poolInstalled) {
        using PoolStubFn=int(__cdecl*)(std::uintptr_t,int);
        const auto invoke=reinterpret_cast<PoolStubFn>(poolCode);
        const auto savedPool=poolMemory; const auto savedPoolRace=poolRace;
        g_settings.enabled=true;g_settings.freeRoamAudioSlots=1;g_settings.maximumRacers=6;
        const auto attempts=g_audioPoolAttempts.load(),adjusted=g_audioPoolAdjusted.load();
        expect(invoke(poolAddress,0)==1 && g_audioPoolAttempts.load()==attempts+1 && g_audioPoolAdjusted.load()==adjusted+1,
            "audio pool inline hook preserves stack and manager while replacing only zero count");
        expect(poolMemory==savedPool && poolRace==savedPoolRace && ReadAudioPoolContext(poolAddress).countA==0 &&
            ReadAudioPoolContext(poolAddress).countB==0 && ReadAudioPoolContext(poolAddress).budget==4,
            "audio pool inline policy does not write race globals pool lists or budgets");
        expect(invoke(poolAddress,3)==3 && invoke(poolAddress,-1)==-1,
            "audio pool trampoline preserves native positive and negative branches");
        g_settings.freeRoamAudioSlots=0;
        expect(invoke(poolAddress,0)==0,"audio pool disabled path retains native zero branch and stack cleanup");
        g_settings.freeRoamAudioSlots=4;g_settings.maximumRacers=2;
        expect(invoke(poolAddress,0)==2,"audio pool fallback is also capped by configured racer population");
        put(poolRace,0x1968,std::uintptr_t{1});
        expect(invoke(poolAddress,0)==0,"audio pool real trampoline refuses event context");
        put(poolRace,0x1968,std::uintptr_t{0});g_settings.freeRoamAudioSlots=1;g_settings.maximumRacers=6;
        setGlobal(kRaceStatus,std::uintptr_t{0});
        const auto savedFrontend=poolFrontend;
        expect(ReadAudioPoolContext(poolAddress).valid==0xFCFF,
            "audio pool actual reads preserve distinct pre-status validity mask");
        expect(invoke(poolAddress,0)==1 && g_audioPoolLastValid.load()==0xFCFF &&
            g_audioPoolLastRaceStatus.load()==0 && g_audioPoolLastFrontend.load()==reinterpret_cast<std::uintptr_t>(poolFrontend.data()),
            "audio pool real trampoline handles pre-status native lookup and records initial context");
        expect(poolFrontend==savedFrontend && poolMemory==savedPool,
            "audio pool alternate lookup does not change native owner or pool");
        put(poolFrontend,0x2C,std::uintptr_t{1});
        expect(invoke(poolAddress,0)==0 && g_audioPoolLastParms.load()==1,
            "audio pool real trampoline rejects pending frontend event");
        setGlobal(kRaceStatus,reinterpret_cast<std::uintptr_t>(poolRace.data()));
        expect(invoke(poolAddress,0)==1,
            "audio pool present status takes precedence over stale alternate parameters as native code does");
        setGlobal(kRaceStatus,std::uintptr_t{1});
        expect(invoke(poolAddress,0)==0,"audio pool invalid present status is never treated as absent");
        setGlobal(kRaceStatus,std::uintptr_t{0});setGlobal(0x0091E004,std::uintptr_t{0});
        expect(invoke(poolAddress,0)==0,"audio pool real trampoline refuses missing alternate owner");
        setGlobal(kRaceStatus,reinterpret_cast<std::uintptr_t>(poolRace.data()));
    }
    if(poolCode) {MH_DisableHook(poolCode+sizeof(prefix));MH_RemoveHook(poolCode+sizeof(prefix));MH_Uninitialize();VirtualFree(poolCode,0,MEM_RELEASE);}
