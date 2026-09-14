    {
        auto closeValue=[](float a,float b){return std::abs(a-b)<0.00001f;};
        expect(ParseEncounterPowerScale(nullptr,1.3f)==1.3f&&ParseEncounterPowerScale(L"",1.3f)==1.3f,"alpha40 missing values use default");
        for(auto text:{L"oops",L"nan",L"inf",L"1e999",L"1.5oops",L"1,50"})
            expect(ParseEncounterPowerScale(text,1.3f)==1.3f,"alpha40 invalid numeric text cannot override default");
        expect(ParseEncounterPowerScale(L"0.5",1.3f)==1&&ParseEncounterPowerScale(L"-50",1.3f)==1,"alpha40 values below one clamped to no correction");
        expect(ParseEncounterPowerScale(L"9",1.3f)==2,"alpha40 extreme multiplier clamped to two");
        expect(closeValue(ParseEncounterPowerScale(L" 1.425 \t; comment",1.3f),1.425f),"alpha40 fractional value and comment parsed");
        expect(ParseEncounterPowerScale(L"2 # maximum",1.3f)==2,"alpha40 hash comment accepted");
        const auto oldScales=g_settings.encounterPowerScales;
        wchar_t path[MAX_PATH]{};GetFullPathNameW(L"tools\\fixtures\\power-settings.ini",MAX_PATH,path,nullptr);
        LoadEncounterPowerSettings(path);
        expect(EncounterPowerTier(true,true,50)==1,"alpha40 actual INI can disable near tier");
        expect(closeValue(EncounterPowerTier(true,true,150),1.425f),"alpha40 actual INI sets custom middle tier");
        expect(EncounterPowerTier(true,true,250)==2,"alpha40 actual INI sets far tier maximum");
        for(float scale:{1.f,1.425f,1.75f,2.f}) for(float native:{0.f,.2f,1.f})
            expect(closeValue(1+.5f*ScaleEncounterPowerTerm(native,scale),(1+.5f*native)*scale),"alpha40 configured output multiplier matches native formula");
        expect(EncounterPowerTier(true,false,250)==1&&EncounterPowerTier(false,true,250)==1,"alpha40 config does not bypass role and active guards");
        // Missing keys reset to defaults, rather than carrying previous values.
        GetFullPathNameW(L"tools\\fixtures\\power-settings-absent.ini",MAX_PATH,path,nullptr);
        LoadEncounterPowerSettings(path);
        expect(g_settings.encounterPowerScales==std::array<float,3>{1.50f,1.75f,2.00f},"alpha47 absent INI uses accepted shipping default tiers");
        g_settings.encounterPowerScales=oldScales;
    }
