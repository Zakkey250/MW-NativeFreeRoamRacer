    {
        using encounter_custom::Mode;
        expect(Settings{}.encounterAIMode==Mode::Stable,"alpha51 absent mode preserves accepted Stable AI");
        for(auto text:{L"Custom AI",L"custom ai",L" CUSTOM AI \t; test",L"Custom AI # test"})
            expect(encounter_custom::ParseMode(text)==Mode::Custom,"alpha51 Custom AI case and comments accepted");
        for(auto text:{L"",L"Stable",L"Custom",L"Custom AI garbage",L"custom-ai",L"\t"})
            expect(encounter_custom::ParseMode(text)==Mode::Stable,"alpha51 unknown or missing mode fails back to Stable");
        const auto oldMode=g_settings.encounterAIMode;
        const auto oldScales=g_settings.encounterPowerScales;
        const auto oldLeader=g_settings.customAILeaderPowerScale;
        wchar_t ini[MAX_PATH]{};GetFullPathNameW(L"tools\\fixtures\\custom-ai.ini",MAX_PATH,ini,nullptr);
        LoadEncounterPowerSettings(ini);
        expect(g_settings.encounterAIMode==Mode::Custom&&g_settings.customAILeaderPowerScale==1.25f,"alpha51 actual INI selects Custom AI and default lead boost");
        for(float gap:{0.f,5.f,100.f,101.f,200.f,201.f,300.f})
            expect(EncounterPowerTier(true,false,gap)==1.25f,"alpha51 Custom AI leader receives uniform 1.25 output at all distances");
        expect(EncounterPowerTier(true,true,50)==1.5f&&EncounterPowerTier(true,true,150)==1.75f&&EncounterPowerTier(true,true,250)==2,
            "alpha51 Custom AI trailing tiers remain configured distance policy");
        expect(EncounterPowerTier(false,false,100)==1&&EncounterPowerTier(true,false,NAN)==1&&
            EncounterPowerTier(true,false,-1)==1&&EncounterPowerTier(true,false,301)==1,"alpha51 inactive and invalid-gap boost rejected");
        for(float native:{0.f,.2f,1.f})
            expect(std::abs((1+.5f*ScaleEncounterPowerTerm(native,1.25f))-(1+.5f*native)*1.25f)<.00001f,
                "alpha51 leader scale multiplies drive output not raw catchup value");
        GetFullPathNameW(L"tools\\fixtures\\power-settings-absent.ini",MAX_PATH,ini,nullptr);LoadEncounterPowerSettings(ini);
        expect(g_settings.encounterAIMode==Mode::Stable&&EncounterPowerTier(true,false,250)==1,"alpha51 INI removal restores stable no-leader-boost semantics");
        g_settings.encounterAIMode=oldMode;g_settings.encounterPowerScales=oldScales;g_settings.customAILeaderPowerScale=oldLeader;

        encounter_custom::Trail trace;
        expect(!trace.Select({0,0,0},40,{0,0,1}).valid,"alpha51 empty trace does not synthesize player destination");
        expect(!trace.Record({NAN,0,0},{0,0,1})&&trace.size()==0,"alpha51 invalid player sample cannot poison trace");
        trace.Record({0,0,0},{0,0,1});trace.Record({0,0,10},{0,0,1});trace.Record({0,0,20},{0,0,1});
        trace.Record({10,0,20},{1,0,0});trace.Record({20,0,20},{1,0,0});
        auto dest=trace.Select({0,0,-10},80,{0,0,1});
        expect(dest.valid&&dest.position.x==0&&dest.position.z==20,"alpha51 high speed lookahead stops at recorded right-angle corner");
        const auto initialCursor=trace.cursor(),initialTarget=trace.target();
        dest=trace.Select({20,0,0},40,{1,0,0});
        expect(trace.cursor()==initialCursor&&dest.position.z==20,"alpha51 wrong branch cannot jump to nearest future waypoint");
        dest=trace.Select({0,8,10},40,{0,0,1});
        expect(trace.cursor()==initialCursor,"alpha51 stacked road cannot consume lower-road samples");
        dest=trace.Select({0,0,-10},0,{0,0,1});
        expect(trace.target()==initialTarget&&dest.position.z==20,"alpha51 braking cannot move issued target backwards");
        dest=trace.Select({0,0,20},40,{0,0,1});
        expect(dest.valid&&dest.position.x==20&&dest.position.z==20&&trace.cursor()>initialCursor,"alpha51 actual corner arrival advances recorded turn in order");
        dest=trace.Select({20,0,20},40,{1,0,0});
        expect(dest.position.x==20&&dest.position.z==20,"alpha51 terminal destination is recorded point with no forward extrapolation");
        const auto size=trace.size();trace.Record({20,0,20},{1,0,0});
        expect(trace.size()==size,"alpha51 stationary player cannot fill buffer");
        expect(!trace.Select({0,0,0},NAN,{0,0,1}).valid,"alpha51 invalid rival speed fails closed");
        expect(!trace.Record({500,0,500},{1,0,0})&&trace.size()==1,"alpha51 teleport clears old trace instead of linking disconnected roads");
        trace.Reset();expect(trace.size()==0&&trace.cursor()==0&&!trace.blocked(),"alpha51 reset releases all route state for next role or battle");

        // Bounded long drive exercises ring wrap and monotonic consumed cursor.
        bool bounded=true,ordered=true;std::uint64_t previous=0;
        for(unsigned i=0;i<10000;++i) {
            const float z=float(i)*2;
            bounded=bounded&&trace.Record({0,0,z},{0,0,1});
            dest=trace.Select({0,0,z-20},40,{0,0,1});
            ordered=ordered&&trace.cursor()>=previous&&dest.valid&&dest.position.z<=z;
            previous=trace.cursor();
        }
        expect(bounded&&trace.size()<32&&!trace.blocked(),"alpha51 long following drive reuses fixed ring without growth or allocation");
        expect(ordered,"alpha51 wrapped cursor stays ordered and never projects beyond recorded player");
        trace.Reset();
        for(std::size_t i=0;i<encounter_custom::Trail::Capacity;++i) trace.Record({0,0,float(i)*2},{0,0,1});
        const auto first=trace.cursor();
        expect(!trace.Record({0,0,float(encounter_custom::Trail::Capacity)*2},{0,0,1})&&trace.blocked()&&trace.cursor()==first,
            "alpha51 overflow never overwrites unvisited samples");
        expect(!trace.Select({0,0,-10},40,{0,0,1}).valid,"alpha51 overflow blocks shortcut fallback until route reset");

        trace.Reset();encounter_route::Pursuit attack;bool attacked=false,returned=false;
        std::uint64_t attackCursor=0;
        for(unsigned i=0;i<50;++i) {
            const float z=float(i)*2, gap=i<20?4.f:20.f;
            trace.Record({0,0,z},{0,0,1});
            (void)attack.Update({0,0,z},{0,0,1},{0,0,z-gap},{0,0,1},.05f);
            dest=trace.Select({0,0,z-gap},40,{0,0,1});
            if(attack.passing()){attacked=true;attackCursor=trace.cursor();}
            if(attacked&&!attack.passing()&&i>=20) returned=dest.valid&&trace.cursor()>=attackCursor;
        }
        expect(attacked,"alpha51 close aligned following switches to native attack");
        expect(returned&&trace.size()>1,"alpha51 attack exit resumes recorded trail without clearing history");
        attack.Reset();
        for(unsigned i=0;i<10;++i) (void)attack.Update({0,0,0},{0,0,1},{0,0,-4},{0,0,-1},.05f);
        expect(!attack.passing(),"alpha51 opposing traffic direction never enters attack");
        attack.Reset();
        for(unsigned i=0;i<10;++i) (void)attack.Update({0,5,0},{0,0,1},{0,0,0},{0,0,1},.05f);
        expect(!attack.passing(),"alpha51 nearby stacked road never enters attack");
        attack.Reset();
        for(unsigned i=0;i<5;++i) (void)attack.Update({0,0,4},{0,0,1},{0,0,0},{0,0,1},.05f);
        expect(attack.passing(),"alpha51 attack requires sustained 250ms opportunity");
        for(unsigned i=0;i<31;++i) (void)attack.Update({0,0,4},{0,0,1},{0,0,0},{0,0,1},.05f);
        expect(!attack.passing(),"alpha51 attack timeout prevents indefinite direction-only pursuit");

        using encounter_custom::KeepPath;
        expect(KeepPath(false,false,false,true,3,false,3,1),"alpha51 completed same breadcrumb path retained");
        expect(!KeepPath(false,true,false,true,3,false,3,1),"alpha51 new breadcrumb overrides stable four-second commitment");
        expect(!KeepPath(false,false,true,true,3,false,3,1),"alpha51 stopped rival may retry same breadcrumb");
        expect(!KeepPath(false,false,false,true,2,false,0,1),"alpha51 native Direction not mistaken for breadcrumb Path");
        expect(!KeepPath(false,false,false,true,3,true,3,1)&&!KeepPath(false,false,false,true,3,false,0,1),"alpha51 consumed and empty paths renewed");
        expect(!KeepPath(false,false,false,true,3,false,3,4)&&!KeepPath(false,false,false,true,3,false,3,NAN),"alpha51 expired or invalid path time not retained");
        g_battle.customTrail.Record({0,0,0},{0,0,1});EndEncounterBattle(false,false);
        expect(g_battle.customTrail.size()==0&&g_encounterPowerScale==1,"alpha51 battle end clears custom trace and boost");
    }
