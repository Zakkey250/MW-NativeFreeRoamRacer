template<class Expect>
__declspec(noinline) void RunAlpha79Tests(Expect expect) {
    PerformanceRetry p;
    expect(!p.Cooling(7,100),"alpha79 no evidence never filters a model");
    p.Failed(7,100);
    expect(p.Cooling(7,60099)&&!p.Cooling(8,60099),"alpha79 only failed key cooled for bounded minute");
    expect(!p.Cooling(7,60100),"alpha79 exact expiry restores eligibility");
    p.Failed(7,200);expect(!p.Cooling(7,199),"alpha79 backward clock clears stale cooldown");
    p={};for(unsigned key=0;key<16;++key)p.Failed(key,100+key);
    p.Failed(99,200);
    expect(!p.Cooling(0,201)&&p.Cooling(1,201)&&p.Cooling(99,201),"alpha79 bounded table replaces oldest without blocking all models");
    const auto savedRetry=g_performanceRetry;const auto savedCatalog=g_catalog;
    const auto savedPalette=g_palette,savedBag=g_vehicleBag;
    const auto savedSettings=g_settings;const auto savedExpanded=g_vehicleVarietyExpanded;
    g_performanceRetry={};
    const PerformanceLevels expected{1,1,1,1,1,1,3};auto actual=expected;
    expect(RecordPerformanceVerification(7,expected,actual,100)&&!g_performanceRetry.Cooling(7,100),
        "alpha79 exact performance match never creates cooldown");
    for(unsigned type=0;type<7;++type) {
        actual=expected;actual[type]=0;
        expect(!RecordPerformanceVerification(10+type,expected,actual,100),"alpha79 missing upgrade remains strict rejection");
        actual=expected;actual[type]++;
        expect(!RecordPerformanceVerification(30+type,expected,actual,100),"alpha79 above career plan also rejected");
    }
    alignas(NFSPluginSDK::MW05::FECustomizationRecord) unsigned char record[sizeof(NFSPluginSDK::MW05::FECustomizationRecord)]{};
    g_performanceRetry={};
    expect(!VerifySpawnPerformance(nullptr,*reinterpret_cast<NFSPluginSDK::MW05::FECustomizationRecord*>(record),9)&&
        !g_performanceRetry.Cooling(9,100),"alpha79 missing native object not misclassified as performance mismatch");
    const InstalledVehicle cars[]={{"A",1,100},{"B",2,200},{"C",3,300}};
    g_catalog.clear();for(unsigned i=0;i<3;++i)g_catalog.push_back({&cars[i],static_cast<int>(i),{1,1,1}});
    g_palette={0,1,2};g_vehicleBag={2,1,0};
    CareerPerformancePlan plan{};unsigned reads=0;
    auto ready=[&](unsigned key,CareerPerformancePlan* out){++reads;out->chosen[0]=key;return PerformanceEligibility::Ready;};
    for(bool expanded:{false,true}) {
        g_vehicleVarietyExpanded=expanded;g_performanceRetry={};g_performanceRetry.Failed(1,100);
        g_vehicleBag={2,1,0};reads=0;
        for(unsigned i=0;i<8;++i){const auto* car=SelectPreparedSpawnVehicle(&plan,ready,101);
            expect(car&&car->installed->key!=1,"alpha79 unrelated candidates continue in both model-budget paths");}
        expect(reads==16,"alpha79 no native plan reads for cooled candidate; others checked fresh");
        g_vehicleBag={0};const auto* car=SelectPreparedSpawnVehicle(&plan,ready,60100);
        expect(car&&car->installed->key==1&&reads==19,"alpha79 expired candidate rechecked against current career before reentry");
    }
    g_performanceRetry={};for(unsigned key=1;key<=3;++key)g_performanceRetry.Failed(key,100);
    reads=0;expect(!SelectPreparedSpawnVehicle(&plan,ready,101)&&reads==0&&g_vehicleBag.empty(),
        "alpha79 all cooled waits without allocating or widening audio palette");
    auto locked=[&](unsigned key,CareerPerformancePlan* out){return key==1?PerformanceEligibility::NoNitrous:ready(key,out);};
    g_vehicleBag={0};const auto* car=SelectPreparedSpawnVehicle(&plan,locked,60100);
    expect(car&&car->installed->key!=1,"alpha79 expiry never bypasses fresh nitrous career gate");
    g_performanceRetry=savedRetry;g_catalog=savedCatalog;g_palette=savedPalette;g_vehicleBag=savedBag;
    g_settings=savedSettings;g_vehicleVarietyExpanded=savedExpanded;ResetSpawnEligibility();
}
