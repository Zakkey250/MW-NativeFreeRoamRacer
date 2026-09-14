    {
        const auto savedPowerScales=g_settings.encounterPowerScales;
        g_settings.encounterPowerScales={1.15f,1.30f,1.50f}; // Historical tier fixture, independent of shipping defaults.
        auto approximately=[](float a,float b){return std::abs(a-b)<0.00001f;};
        expect(EncounterPowerTier(false,true,200)==1,"alpha39 inactive battle no boost");
        expect(EncounterPowerTier(true,false,200)==1,"alpha39 leading rival no boost");
        expect(approximately(EncounterPowerTier(true,true,0),1.15f)&&approximately(EncounterPowerTier(true,true,100),1.15f),"alpha39 0 through 100m tier 1.15");
        expect(approximately(EncounterPowerTier(true,true,100.01f),1.30f)&&approximately(EncounterPowerTier(true,true,200),1.30f),"alpha39 above 100 through 200m tier 1.30");
        expect(approximately(EncounterPowerTier(true,true,200.01f),1.5f)&&approximately(EncounterPowerTier(true,true,300),1.5f),"alpha39 above 200 through 300m tier 1.50");
        expect(EncounterPowerTier(true,true,301)==1&&EncounterPowerTier(true,true,-1)==1&&EncounterPowerTier(true,true,NAN)==1,"alpha39 invalid or finished distance cannot boost");
        for(float native:{0.f,.2f,1.f}) for(float scale:{1.15f,1.3f,1.5f})
            expect(approximately(1+.5f*ScaleEncounterPowerTerm(native,scale),(1+.5f*native)*scale),"alpha39 actual native output formula yields requested multiplier");
        expect(ScaleEncounterPowerTerm(.2f,1)==.2f&&ScaleEncounterPowerTerm(.2f,2.01f)==.2f,"alpha39 disabled or invalid scale preserves original");
        expect(std::isnan(ScaleEncounterPowerTerm(NAN,1.5f))&&ScaleEncounterPowerTerm(-1,1.5f)==-1,"alpha39 invalid native sample not manufactured into a boost");
        std::array<unsigned char,0x7CC> aiBlob{};std::array<unsigned char,0x100> vehicleBlob{};
        const auto aiAddress=reinterpret_cast<std::uintptr_t>(aiBlob.data()+0x4C);
        const auto vehicleAddress=reinterpret_cast<std::uintptr_t>(vehicleBlob.data());
        auto* cheater=aiBlob.data()+0x76C;
        put(aiBlob,0x4C,Address(kRacecarIVehicleAIVtable));put(aiBlob,0x48,vehicleAddress);put(aiBlob,0x34,std::uintptr_t(0x1234));
        put(aiBlob,0x76C,Address(0x008925C4));
        put(vehicleBlob,0,reinterpret_cast<std::uintptr_t>(table));put(vehicleBlob,4,unsigned(0x12345678));
        put(vehicleBlob,8,unsigned(kDriverRacer));put(vehicleBlob,0x54,aiAddress);
        g_encounterSkillVehicle=vehicleAddress;g_encounterSkillSimable=0x1234;g_encounterSkillKey=0x12345678;g_encounterSkillAI=aiAddress;
        g_encounterPowerScale=1.3f;g_encounterPowerRefresh=1000;g_encounterPowerCalls=0;
        const auto caller=Address(0x006B0016);
        expect(approximately(ApplyEncounterPowerBoost(cheater,caller,0,1050),.6f)&&g_encounterPowerCalls==1,"alpha39 only identified rival receives output term with applied counter");
        expect(std::isnan(ApplyEncounterPowerBoost(cheater,caller,NAN,1050))&&g_encounterPowerCalls==1,"alpha39 invalid native output cannot increment applied counter");
        for(auto otherCaller:{Address(0x006AC5EC),Address(0x006B2A44),Address(0x00692AE6),std::uintptr_t(0)})
            expect(ApplyEncounterPowerBoost(cheater,otherCaller,.2f,1050)==.2f,"alpha39 NOS handling probe and other consumers untouched");
        expect(ApplyEncounterPowerBoost(cheater,caller,.2f,1251)==.2f,"alpha39 stale management sample disables boost after 250ms");
        expect(ApplyEncounterPowerBoost(cheater,caller,.2f,999)==.2f,"alpha39 inconsistent time disables boost");
        put(vehicleBlob,4,unsigned(99));expect(ApplyEncounterPowerBoost(cheater,caller,.2f,1050)==.2f,"alpha39 recycled vehicle key rejected");
        put(vehicleBlob,4,unsigned(0x12345678));put(vehicleBlob,8,unsigned(kDriverTraffic));
        expect(ApplyEncounterPowerBoost(cheater,caller,.2f,1050)==.2f,"alpha39 traffic/player class not boosted");
        put(vehicleBlob,8,unsigned(kDriverRacer));put(aiBlob,0x34,std::uintptr_t(0x9999));
        expect(ApplyEncounterPowerBoost(cheater,caller,.2f,1050)==.2f,"alpha39 wrong Simable generation rejected");
        put(aiBlob,0x34,std::uintptr_t(0x1234));put(aiBlob,0x76C,0u);
        expect(ApplyEncounterPowerBoost(cheater,caller,.2f,1050)==.2f,"alpha39 wrong cheater interface rejected");
        put(aiBlob,0x76C,Address(0x008925C4));ClearEncounterPowerBoost();
        expect(ApplyEncounterPowerBoost(cheater,caller,.2f,1050)==.2f,"alpha39 clear restores original without persistent physics writes");
        g_encounterPowerInstalled=true;UpdateEncounterPowerBoost(true,true,250);
        expect(g_encounterPowerScale==1.5f,"alpha39 management publishes far tier");
        UpdateEncounterPowerBoost(true,false,250);expect(g_encounterPowerScale==1,"alpha39 lead reversal immediately removes tier");
        UpdateEncounterPowerBoost(true,true,50);EndEncounterBattle(false,false);
        expect(g_encounterPowerScale==1&&g_encounterPowerRefresh==0,"alpha39 battle end clears published boost");
        // Real x86 trampoline of the exact seven-byte getter prologue.
        const unsigned char stub[]={0x51,0xD9,0x41,0x40,0xD8,0x41,0x3C,0x59,0xC3};
        void* code=VirtualAlloc(nullptr,4096,MEM_COMMIT|MEM_RESERVE,PAGE_EXECUTE_READWRITE);bool hooked=false;
        if(code) {
            std::memcpy(code,stub,sizeof(stub));FlushInstructionCache(GetCurrentProcess(),code,64);
            if(MH_Initialize()==MH_OK) hooked=MH_CreateHook(code,&EncounterPowerHook,reinterpret_cast<void**>(&g_originalEncounterCheat))==MH_OK&&MH_EnableHook(code)==MH_OK;
        }
        expect(hooked,"alpha39 native getter prologue accepts real x86 detour");
        if(hooked) {
            put(aiBlob,0x7A8,.5f);put(aiBlob,0x7AC,.2f);
            for(int i=0;i<16;++i) expect(approximately(reinterpret_cast<EncounterCheatFn>(code)(cheater),.7f),"alpha39 unrelated caller preserves original ST0 return and stack");
        }
        if(code){MH_DisableHook(code);MH_RemoveHook(code);MH_Uninitialize();VirtualFree(code,0,MEM_RELEASE);}
        g_originalEncounterCheat=nullptr;g_encounterPowerInstalled=false;ClearEncounterPowerBoost();ClearEncounterRaceSkill();
        g_settings.encounterPowerScales=savedPowerScales;
    }
