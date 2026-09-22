    {
        using namespace nfsmw_limitbreaker;
        ResourceLimitsV1 limits{sizeof(ResourceLimitsV1),1,ResourceAudioMixerPoolExpanded,0x800000,64,64,40};
        expect(SupportsSixAudioSlots(1,limits),"alpha71 exact resource ABI audio capability accepted");
        limits.logicalVehicleLimit=48;expect(SupportsSixAudioSlots(1,limits),"alpha71 audio capability independent of vehicle limit");
        limits.abiVersion=2;expect(!SupportsSixAudioSlots(1,limits),"alpha71 unknown ABI rejected");limits.abiVersion=1;
        limits.capabilities=0;expect(!SupportsSixAudioSlots(1,limits),"alpha71 absent audio capability rejected");limits.capabilities=1;
        limits.audioMixerPoolBytes=0x567000;expect(!SupportsSixAudioSlots(1,limits),"alpha71 stock arena cannot enable six");limits.audioMixerPoolBytes=0x800000;
        expect(!SupportsSixAudioSlots(0,limits),"alpha71 missing export or failed query falls back");
        limits.structSize=0;expect(!SupportsSixAudioSlots(1,limits),"alpha71 invalid structure falls back");
        for(bool enabled:{false,true})for(bool opt:{false,true})for(bool cap:{false,true})
            expect(ChooseAudioCeiling(enabled,opt,cap)==(enabled&&opt&&cap?6u:4u),"alpha71 opt-in capability and enabled gates");
        AudioPoolContext c{};c.valid=0xFCFF;c.global=0x10000;c.vt=Address(kAudioRacerPoolVtable);c.type=3;c.kind=1;c.frontend=0x20000;c.budget=6;
        AudioPoolReason reason;
        expect(SelectAudioPoolCount(0,c.global,c,true,6,&reason,6)==6,"alpha71 empty no-event pool chooses prepared six");
        c.budget=4;expect(SelectAudioPoolCount(0,c.global,c,true,6,&reason,6)==0,"alpha71 budget mismatch cannot create six");c.budget=6;
        c.raceParms=1;expect(SelectAudioPoolCount(0,c.global,c,true,6,&reason,6)==0,"alpha71 ordinary event allocation untouched");c.raceParms=0;
        c.count=4;expect(SelectAudioPoolCount(0,c.global,c,true,6,&reason,6)==0,"alpha71 populated four-slot pool never resized");c.count=0;
        expect(SelectAudioPoolCount(4,c.global,c,true,6,&reason,6)==4,"alpha71 nonzero native count unchanged");
        const auto oldFrozen=g_audioPreparationFrozen.load();const auto oldCeiling=g_audioSlotCeiling.load();
        const auto oldCounter=g_audioBudgetCounter,oldTail=g_audioBudgetContinue;
        unsigned budget=10;g_audioBudgetCounter=reinterpret_cast<std::uintptr_t>(&budget);g_audioBudgetContinue=reinterpret_cast<std::uintptr_t>(&AudioBudgetTestTail);
        g_audioPreparationFrozen=true;g_audioSlotCeiling=6;
        unsigned savedA=0,savedC=0,savedD=0;
        __asm {
            mov eax,12345678h
            mov ecx,23456789h
            mov edx,3456789Ah
            call AudioExpansionBudgetHook
            mov savedA,eax
            mov savedC,ecx
            mov savedD,edx
        }
        expect(budget==16&&savedA==0x12345678&&savedC==0x23456789&&savedD==0x3456789A,"alpha71 budget hook adds six once and preserves volatile registers");
        g_audioSlotCeiling=4;AudioExpansionBudgetHook();expect(budget==20,"alpha71 fallback budget adds stock four once");
        g_audioPreparationFrozen=false;LockAudioExpansionForWorld();
        expect(SelectAudioBudgetIncrement()==4,"alpha71 world lock forbids late promotion");
        g_audioPreparationFrozen=oldFrozen;g_audioSlotCeiling=oldCeiling;g_audioBudgetCounter=oldCounter;g_audioBudgetContinue=oldTail;
    }
