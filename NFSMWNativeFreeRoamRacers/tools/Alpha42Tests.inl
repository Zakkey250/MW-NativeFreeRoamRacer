    {
        frameFixture={};frameAnimations=0;
        const std::uint32_t hashes[]={0x3345911D,0x35236DBE,0x0A729B1B,0xD907EF51,0xB996A84C,0x3B1B624A,0xA206A0B4};
        for(unsigned i=0;i<7;++i){
            put(frameFixture[i],0x0C,100u+i);put(frameFixture[i],0x10,hashes[i]);
            put(frameFixture[i],0x18,i<2?5u:1u);put(frameFixture[i],0x1C,1u);put(frameFixture[i],0x24,0x12340000u+i);
        }
        put(frameFixture[0],0x60,5u);put(frameFixture[0],0x64,reinterpret_cast<std::uintptr_t>(frameFixture[1].data()));
        put(frameFixture[1],4,reinterpret_cast<std::uintptr_t>(frameFixture[3].data()));
        for(unsigned i=3;i<6;++i)put(frameFixture[i],4,reinterpret_cast<std::uintptr_t>(frameFixture[i+1].data()));
        put(frameFixture[1],0x60,1u);put(frameFixture[1],0x64,reinterpret_cast<std::uintptr_t>(frameFixture[2].data()));
        std::array<std::uint8_t,0x60> widget{};
        put(widget,0,Address(kEncounterMenuVtable));put(widget,0x10,1u);put(widget,0x38,reinterpret_cast<std::uintptr_t>(frameFixture[0].data()));
        const auto originalWidget=widget;const auto originalSms=frameFixture[6];
        const EncounterUiApi api{FindFrameFixture,AnimateFrameFixture,SetFrameFixtureTexture,true};
        expect(SetEncounterPromptWithApi(widget.data(),true,api)&&g_encounterFrame.count==6&&g_encounterFrame.icon==frameFixture[5].data(),"alpha42 acquire icon with native ancestor path");
        std::uint32_t hash=0,flags=0;
        EncounterUiRead(frameFixture[5].data(),0x24,&hash);EncounterUiRead(frameFixture[5].data(),0x1C,&flags);
        expect(hash==kEncounterReadyTexture&&!(flags&1),"alpha42 exclusive icon texture visible");
        expect(widget==originalWidget&&frameFixture[6]==originalSms,"alpha42 event metadata and SMS untouched");
        for(unsigned i=0;i<1000;++i)SetEncounterFrameBits(g_encounterFrame,true);
        expect(frameAnimations==2,"alpha43 icon and prompt animate once; upkeep never restarts either");
        expect(SetEncounterPromptWithApi(widget.data(),false,api),"alpha42 prompt release succeeds");
        EncounterUiRead(frameFixture[5].data(),0x24,&hash);EncounterUiRead(frameFixture[5].data(),0x1C,&flags);
        expect(hash==0x12340005u&&(flags&1),"alpha42 original texture and hidden bit restored");
        SetEncounterPromptWithApi(widget.data(),true,api);
        SetFrameFixtureTexture(frameFixture[5].data(),0x99887766u);
        expect(RestoreEncounterIcon(widget.data(),api),"alpha42 native takeover accepts live identity");
        EncounterUiRead(frameFixture[5].data(),0x24,&hash);
        expect(hash==0x99887766u,"alpha42 incoming race texture never overwritten");
        SetEncounterPromptWithApi(widget.data(),false,api);
        SetEncounterPromptWithApi(widget.data(),true,api);
        put(frameFixture[5],0x0C,9090u);
        const auto recycled=frameFixture[5];
        expect(!SetEncounterPromptWithApi(widget.data(),false,api)&&frameFixture[5]==recycled,"alpha42 recycled icon rejected without writes");
        put(frameFixture[5],0x0C,105u);put(widget,0x48,42u);
        expect(!SetEncounterPromptWithApi(widget.data(),true,api),"alpha42 native event blocks custom icon acquisition");
        put(widget,0x48,0u);put(frameFixture[5],0x1C,0x41u);
        expect(!SetEncounterPromptWithApi(widget.data(),true,api),"alpha42 native icon hidden-lock honored");
        // The earlier recycled-node rejection deliberately left a shown ring.
        // Start the independent delayed-load case from a genuinely hidden HUD.
        for(unsigned i=0;i<5;++i)put(frameFixture[i],0x1C,1u);
        const EncounterUiApi fallback{FindFrameFixture,AnimateFrameFixture};
        expect(SetEncounterPromptWithApi(widget.data(),true,fallback)&&!g_encounterFrame.icon,"alpha42 missing texture retains verified frame-only fallback");
        put(frameFixture[5],0x1C,1u);const auto upgradeAnimations=frameAnimations;
        expect(UpgradeEncounterIconWithApi(widget.data(),api)&&g_encounterFrame.icon==frameFixture[5].data(),"alpha46 late texture upgrades existing ring without reacquiring prompt");
        expect(frameAnimations==upgradeAnimations+1,"alpha46 late attach animates icon only once");
        expect(!UpgradeEncounterIconWithApi(widget.data(),api)&&frameAnimations==upgradeAnimations+1,"alpha46 already attached icon cannot restart animations");
        SetEncounterPromptWithApi(widget.data(),false,api);
        EncounterUiRead(frameFixture[0].data(),0x1C,&flags);
        expect((flags&1)!=0,"alpha46 late upgrade restores original hidden ring state");
        expect(PromptSoundDue(0,0,false),"alpha42 first prompt can play immediately");
        expect(!PromptSoundDue(3499,1000,true)&&PromptSoundDue(3500,1000,true),"alpha42 reappear sound bounded by 2500ms cooldown");
        expect(!PromptSoundDue(500,1000,true),"alpha42 sound clock reversal rejected");
        g_encounterFrame={};g_encounterOwnedWidget=nullptr;
    }
