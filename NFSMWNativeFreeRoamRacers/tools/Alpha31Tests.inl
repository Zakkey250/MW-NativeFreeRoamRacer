    {
        using namespace battle;
        auto start=[] {
            Sample s{};s.player.identity={1,11,111};s.rival.identity={2,22,222};
            s.player.forward=s.rival.forward={0,0,1};s.player.speed=s.rival.speed=30;
            s.rival.position.z=10;return s;
        };
        auto s=start();Model m;m.Start(s);
        s.player.position.z=13;m.Step(s,.05f);m.Step(s,.05f);
        expect(m.leader()==Leader::Player&&m.roleTrusted(),"alpha31 pass confirmed before off-trail collision");
        s.rival.position.x=16;s.rival.speed=0;m.Step(s,.1f);
        expect(!m.progressValid()&&m.roleTrusted(),"alpha31 invalid close trail sample cannot erase confirmed lead");
        Change finish{};
        for(unsigned i=0;i<32&&m.phase()==Phase::Active;++i){s.player.position.z+=10;finish=m.Step(s,.1f);}
        expect(finish.event==Event::Won&&finish.cashIntent==1000,"alpha31 300m finishes despite crashed rival leaving trail");
        expect(m.Step(s,.1f).cashIntent==0,"alpha31 collision finish cannot pay twice");
        s=start();m.Reset();m.Start(s);m.Step(s,.1f);s.player.position.x=16;m.Step(s,.1f);
        for(unsigned i=0;i<31&&m.phase()==Phase::Active;++i){s.rival.position.z+=10;finish=m.Step(s,.1f);}
        expect(finish.event==Event::Lost,"alpha31 confirmed rival lead also finishes when player leaves trail");
        m.Reset();s=start();m.Start(s);s.player.position.x=16;m.Step(s,.1f);
        for(unsigned i=0;i<32;++i){s.rival.position.z+=10;m.Step(s,.1f);}
        expect(m.phase()==Phase::Active&&!m.roleTrusted(),"alpha31 never-observed role still cannot award stale initial loss");
        s=start();m.Reset();m.Start(s);s.player.position.z=13;m.Step(s,.05f);m.Step(s,.05f);
        s.rival.position.x=16;m.Step(s,.1f);s.rival.position.x=0;s.rival.position.z=16;
        m.Step(s,.05f);m.Step(s,.05f);
        expect(m.leader()==Leader::Rival,"alpha31 later genuine repass still replaces retained player lead");
        m.Reset();expect(!m.roleTrusted(),"alpha31 reset clears retained trust between races");

        g_battle={};g_battle.model.Start(start());g_battle.rival=start().rival.identity;
        ManagedRacer opponent{},bystander{};
        opponent.pointer=reinterpret_cast<void*>(2);opponent.simable=reinterpret_cast<void*>(22);opponent.vehicleKey=222;
        bystander=opponent;bystander.pointer=reinterpret_cast<void*>(3);
        expect(EncounterMarkerAllowed(opponent)&&!EncounterMarkerAllowed(bystander),"alpha31 active battle shows only opponent among managed racers");
        auto recycled=opponent;recycled.simable=reinterpret_cast<void*>(23);
        expect(!EncounterMarkerAllowed(recycled),"alpha31 marker filter checks generation identity not pointer alone");
        EndEncounterBattle(false,false);
        expect(EncounterMarkerAllowed(opponent)&&EncounterMarkerAllowed(bystander),"alpha31 end restores normal marker eligibility");
        g_battle.pending=true;
        expect(EncounterMarkerAllowed(bystander),"alpha31 rejected or merely queued start does not hide markers");
        g_battle={};

        std::array<unsigned char,0x7CC> aiBlob{};std::array<unsigned char,0x100> vehicleBlob{};
        const auto aiAddress=reinterpret_cast<std::uintptr_t>(aiBlob.data()+0x4C);
        const auto vehicleAddress=reinterpret_cast<std::uintptr_t>(vehicleBlob.data());
        put(aiBlob,0x4C,Address(kRacecarIVehicleAIVtable));put(aiBlob,0x48,vehicleAddress);put(aiBlob,0x34,std::uintptr_t(0x1234));
        put(vehicleBlob,0,reinterpret_cast<std::uintptr_t>(table));put(vehicleBlob,4,unsigned(0x12345678));
        put(vehicleBlob,8,unsigned(kDriverRacer));put(vehicleBlob,0x54,aiAddress);
        g_encounterSkillVehicle=vehicleAddress;g_encounterSkillSimable=0x1234;g_encounterSkillKey=0x12345678;g_encounterSkillAI=aiAddress;
        auto* skillAI=reinterpret_cast<void*>(aiAddress);
        g_originalEncounterSkill=reinterpret_cast<EncounterSkillFn>(&FakeEncounterSkill);encounterSkillNativeCalls=0;
        expect(EncounterSkillHook(skillAI,nullptr)==1&&encounterSkillNativeCalls==1,"alpha31 native race skill elevated only for validated opponent");
        expect(EncounterSkillHook(nullptr,nullptr)==.35f&&encounterSkillNativeCalls==2,"alpha31 unrelated skill query forwards native result exactly once");
        put(vehicleBlob,4,unsigned(9));
        expect(EncounterSkillHook(skillAI,nullptr)==.35f,"alpha31 recycled vehicle key refuses race skill override");
        put(vehicleBlob,4,unsigned(0x12345678));put(vehicleBlob,8,unsigned(kDriverTraffic));
        expect(EncounterSkillHook(skillAI,nullptr)==.35f,"alpha31 traffic owner cannot inherit race skill");
        put(vehicleBlob,8,unsigned(kDriverRacer));ClearEncounterRaceSkill();
        expect(EncounterSkillHook(skillAI,nullptr)==.35f,"alpha31 skill restores immediately without persistent AI writes");
        // Real x86 native-shaped trampoline, ECX and ST(0), no stack arguments.
        const unsigned char skillStub[]={0x51,0xD9,0x81,0x60,0x07,0,0,0x59,0xC3};
        void* code=VirtualAlloc(nullptr,4096,MEM_COMMIT|MEM_RESERVE,PAGE_EXECUTE_READWRITE);
        bool hooked=false;
        if(code) {
            std::memcpy(code,skillStub,sizeof(skillStub));FlushInstructionCache(GetCurrentProcess(),code,64);
            if(MH_Initialize()==MH_OK) hooked=MH_CreateHook(code,&EncounterSkillHook,reinterpret_cast<void**>(&g_originalEncounterSkill))==MH_OK&&MH_EnableHook(code)==MH_OK;
        }
        expect(hooked,"alpha31 native GetSkill prologue supports real x86 detour");
        if(hooked) {
            put(aiBlob,0x7AC,0.35f);g_encounterSkillAI=aiAddress;
            expect(reinterpret_cast<EncounterSkillFn>(code)(skillAI)==1,"alpha31 skill trampoline returns scoped float through ST0");
            ClearEncounterRaceSkill();
            expect(reinterpret_cast<EncounterSkillFn>(code)(skillAI)==.35f,"alpha31 inactive trampoline preserves native float and stack");
        }
        if(code){MH_DisableHook(code);MH_RemoveHook(code);MH_Uninitialize();VirtualFree(code,0,MEM_RELEASE);}

        std::vector<unsigned char> pcmBytes(16,0xA5);encounter_wave::Pcm pcm{};pcm.offset=4;pcm.bytes=8;
        const std::int16_t original[]={32767,-32768,10000,-10000};std::memcpy(pcmBytes.data()+4,original,8);
        ScaleEncounterPcm(pcmBytes,pcm,.75f);std::int16_t result[4]{};std::memcpy(result,pcmBytes.data()+4,8);
        expect(result[0]==24575&&result[1]==-24576&&result[2]==7500&&result[3]==-7500,"alpha31 PCM gain scales both signs without overflow");
        expect(pcmBytes.front()==0xA5&&pcmBytes.back()==0xA5,"alpha31 gain changes PCM payload only");
        ScaleEncounterPcm(pcmBytes,pcm,0);std::memcpy(result,pcmBytes.data()+4,8);
        expect(result[0]==0&&result[1]==0&&result[2]==0&&result[3]==0,"alpha31 zero volume gives silent PCM");
        std::array<unsigned char,0x30> manager{};float audioSettings[2]={.8f,.5f};
        auto* audioGlobal=reinterpret_cast<void**>(Address(0x00911FA8));const auto saved=*audioGlobal;
        *audioGlobal=manager.data();put(manager,0x24,reinterpret_cast<std::uintptr_t>(audioSettings));
        g_encounterVolumeSurface=true;float gain=1;
        expect(ReadEncounterGameVoiceGain(gain)&&std::abs(gain-.4f)<.0001f,"alpha31 native active audio settings master times speech gain");
        audioSettings[1]=0;expect(ReadEncounterGameVoiceGain(gain)&&gain==0,"alpha31 game voice mute propagates without default fallback");
        audioSettings[1]=std::numeric_limits<float>::quiet_NaN();
        expect(!ReadEncounterGameVoiceGain(gain),"alpha31 invalid native audio values rejected");
        *audioGlobal=nullptr;expect(!ReadEncounterGameVoiceGain(gain),"alpha31 absent audio manager rejected without any INI fallback");
        *audioGlobal=saved;g_encounterVolumeSurface=false;
    }
