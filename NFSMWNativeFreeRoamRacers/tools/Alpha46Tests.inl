    {
        const auto savedState=g_promptAssetState;const auto savedSlow=g_promptAssetSlow;
        const auto savedTick=g_promptAssetRequestTick;
        g_promptAssetState=1;g_promptAssetSlow=false;g_promptAssetRequestTick=1000;
        expect(ObserveEncounterIconTexture(true,27000)&&g_promptAssetState==2,"alpha46 loaded texture accepted after long gap before next prompt");
        expect(!ObserveEncounterIconTexture(false,28000)&&g_promptAssetState==1&&g_promptAssetRequestTick==28000,"alpha46 missing cached texture resumes polling without resource allocation");
        expect(!ObserveEncounterIconTexture(false,44000)&&g_promptAssetState==1&&g_promptAssetSlow,"alpha46 delayed load never permanently disables icon");
        expect(ObserveEncounterIconTexture(true,60000)&&g_promptAssetState==2&&!g_promptAssetSlow,"alpha46 delayed resource recovers when native texture appears");
        g_promptAssetState=-1;
        expect(!ObserveEncounterIconTexture(true,70000)&&g_promptAssetState==-1,"alpha46 fatal identity guard remains fail-closed");
        expect(!PromptTexturePollDue(1249,1000,false)&&PromptTexturePollDue(1250,1000,false),"alpha46 loading polls bounded to 250ms");
        expect(!PromptTexturePollDue(2999,1000,true)&&PromptTexturePollDue(3000,1000,true),"alpha46 delayed or cached texture polls bounded to two seconds");
        expect(!PromptTexturePollDue(999,1000,false),"alpha46 polling rejects clock reversal");
        g_promptAssetState=savedState;g_promptAssetSlow=savedSlow;g_promptAssetRequestTick=savedTick;
    }
