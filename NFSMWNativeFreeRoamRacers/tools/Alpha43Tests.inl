    {
        // Actual MenuZoneTrigger layout: three ring children, EventIcon is NOT
        // a descendant. alpha42's synthetic nested-only fixture missed this.
        frameFixture={};frameAnimations=0;
        const std::uint32_t hashes[]={0x3345911D,0x35236DBE,0x0A729B1B,0xD907EF51,0xB996A84C,0x3B1B624A,0xA206A0B4};
        for(unsigned i=0;i<7;++i){
            put(frameFixture[i],0x0C,100u+i);put(frameFixture[i],0x10,hashes[i]);
            put(frameFixture[i],0x18,i<2?5u:1u);put(frameFixture[i],0x1C,1u);put(frameFixture[i],0x24,0x12340000u+i);
        }
        put(frameFixture[0],0x60,3u);put(frameFixture[0],0x64,reinterpret_cast<std::uintptr_t>(frameFixture[1].data()));
        put(frameFixture[1],4,reinterpret_cast<std::uintptr_t>(frameFixture[3].data()));
        put(frameFixture[3],4,reinterpret_cast<std::uintptr_t>(frameFixture[4].data()));
        put(frameFixture[1],0x60,1u);put(frameFixture[1],0x64,reinterpret_cast<std::uintptr_t>(frameFixture[2].data()));
        std::array<std::uint8_t,0x60> widget{};
        put(widget,0,Address(kEncounterMenuVtable));put(widget,0x10,1u);put(widget,0x38,reinterpret_cast<std::uintptr_t>(frameFixture[0].data()));
        put(widget,0x3C,reinterpret_cast<std::uintptr_t>(frameFixture[5].data()));
        const auto originalWidget=widget;const auto originalSms=frameFixture[6];
        const EncounterUiApi api{FindFrameFixture,AnimateFrameFixture,SetFrameFixtureTexture,true};
        expect(SetEncounterPromptWithApi(widget.data(),true,api)&&g_encounterFrame.independentIcon&&g_encounterFrame.count==6,"alpha43 separately owned icon is acquired");
        expect(frameAnimations==2&&frameAnimatedNode==frameFixture[5].data()&&frameAnimation==0x5079C8F8,"alpha43 native independent icon APPEAR once");
        for(unsigned i=0;i<1000;++i)SetEncounterFrameBits(g_encounterFrame,true);
        expect(frameAnimations==2&&widget==originalWidget&&frameFixture[6]==originalSms,"alpha43 no animation loop or event/SMS mutation");
        expect(SetEncounterPromptWithApi(widget.data(),false,api),"alpha43 independent icon released");
        std::uint32_t hash=0,flags=0;
        EncounterUiRead(frameFixture[5].data(),0x24,&hash);EncounterUiRead(frameFixture[5].data(),0x1C,&flags);
        expect(hash==0x12340005u&&(flags&1)&&frameAnimations==4,"alpha43 LEAVE and original texture/visibility restored");
        SetEncounterPromptWithApi(widget.data(),true,api);
        SetFrameFixtureTexture(frameFixture[5].data(),0x88776655u);
        const auto animationsBefore=frameAnimations;
        expect(RestoreEncounterIcon(widget.data(),api)&&frameAnimations==animationsBefore,"alpha43 incoming native event is not animated or hidden");
        EncounterUiRead(frameFixture[5].data(),0x24,&hash);
        expect(hash==0x88776655u,"alpha43 incoming native texture preserved");
        put(widget,0x3C,reinterpret_cast<std::uintptr_t>(frameFixture[6].data()));
        const auto snapshot=frameFixture;
        expect(!SetEncounterPromptWithApi(widget.data(),false,api)&&frameFixture==snapshot,"alpha43 replaced owner pointer causes no writes");
        expect(!SetEncounterPromptWithApi(widget.data(),true,api),"alpha43 arbitrary detached icon is never borrowed");
        put(widget,0x3C,reinterpret_cast<std::uintptr_t>(frameFixture[5].data()));put(frameFixture[5],0x1C,0x41u);
        expect(!SetEncounterPromptWithApi(widget.data(),true,api),"alpha43 independent locked icon remains locked");
        g_encounterFrame={};g_encounterOwnedWidget=nullptr;
    }
    {
        using encounter_route::KeepPursuitPath;
        expect(KeepPursuitPath(false,false,3,false,10,true,80,1),"alpha43 keep completed path despite moving target");
        expect(KeepPursuitPath(false,false,3,false,510,true,40.01f,2.99f),"alpha43 native route upper bound retained");
        expect(!KeepPursuitPath(false,false,3,false,0,true,80,1),"alpha43 accepted empty path is not success");
        expect(!KeepPursuitPath(false,false,2,false,10,true,80,1),"alpha43 Direction fallback re-requests current player");
        expect(!KeepPursuitPath(false,false,3,true,10,true,80,1),"alpha43 consumed goal replans");
        expect(!KeepPursuitPath(false,false,3,false,10,true,40,1),"alpha43 near old endpoint replans before stopping");
        expect(!KeepPursuitPath(false,false,3,false,10,true,80,3),"alpha43 stale endpoint expires at three seconds");
        expect(!KeepPursuitPath(true,false,3,false,10,true,80,1),"alpha43 role/pass transition can replan");
        expect(!KeepPursuitPath(false,true,3,false,10,true,80,1),"alpha43 physical stall can replan");
        expect(!KeepPursuitPath(false,false,3,false,10,false,80,1),"alpha43 unrelated route is not retained as ours");
        expect(!KeepPursuitPath(false,false,3,false,-1,true,80,1)&&!KeepPursuitPath(false,false,3,false,511,true,80,1),"alpha43 malformed edge count cannot freeze renewal");
        expect(!KeepPursuitPath(false,false,3,false,10,true,NAN,1)&&!KeepPursuitPath(false,false,3,false,10,true,80,NAN),"alpha43 nonfinite telemetry cannot freeze renewal");
    }
