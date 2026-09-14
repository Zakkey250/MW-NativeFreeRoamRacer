    {
        encounter_route::Trail trail;
        for(unsigned i=0;i<=20;++i) trail.Record({0,0,float(i)*12});
        auto hint=trail.Select({0,0,95},60,{0,0,1});
        expect(hint.valid&&hint.position.z>95&&hint.position.z<220,"alpha34 fast straight travel consumes passed samples in order and aims ahead");
        trail.Reset();for(unsigned i=0;i<=20;++i) trail.Record({0,0,float(i)*12});
        hint=trail.Select({0,0,95},60,{0,0,-1});
        expect(trail.size()==21,"alpha34 opposite heading cannot consume distant future trail");
        trail.Reset();for(unsigned i=0;i<=20;++i) trail.Record({0,0,float(i)*12});
        hint=trail.Select({0,20,95},60,{0,0,1});
        expect(trail.size()==21,"alpha34 bridge height rejects parallel road projection");
        trail.Reset();for(unsigned i=0;i<=20;++i) trail.Record({0,0,float(i)*12});
        hint=trail.Select({25,0,95},60,{0,0,1});
        expect(trail.size()==21,"alpha34 adjacent road cannot consume distant trail");
        trail.Reset();trail.Record({0,0,0});trail.Record({0,0,12});trail.Record({5,0,12});
        hint=trail.Select({0,0,0});
        expect(hint.valid&&hint.position.x==5&&hint.heading.x>.9f,"alpha34 terminal target uses latest corner tangent rather than stale committed segment");
        trail.Reset();for(unsigned i=0;i<=5;++i) trail.Record({0,0,float(i)*12});
        for(unsigned i=1;i<=8;++i) trail.Record({float(i)*12,0,60});
        hint=trail.Select({0,0,0},100,{0,0,1});
        expect(hint.position.x==0&&hint.position.z==60&&hint.heading.x>.9f,"alpha34 high speed horizon stops at unvisited sharp turn with outgoing direction");
        for(unsigned cap=0;cap<=6;++cap) for(unsigned unlock=0;unlock<=4;++unlock) {
            PerformanceLevels maximum{};maximum.fill(cap);
            const auto chosen=SelectCareerPerformance(maximum,[&](unsigned,int shop){return shop<=int(unlock);});
            bool valid=true;
            for(unsigned type=0;type<7;++type) {
                const auto level=chosen[type];
                const int shop=int(kShopPackageCounts[type])+int(level)-int(cap);
                valid=valid && level<=cap && (!level || (shop>=1&&shop<=int(unlock)));
                for(unsigned next=level+1;next<=cap;++next) {
                    const int nextShop=int(kShopPackageCounts[type])+int(next)-int(cap);
                    if(nextShop>=1&&nextShop<=int(unlock)&&nextShop<=int(kShopPackageCounts[type])) valid=false;
                }
            }
            expect(valid,"alpha34 all seven upgrades select highest career-unlocked vehicle-local level");
        }
        PerformanceLevels maximum{3,4,3,4,4,3,3};
        auto chosen=SelectCareerPerformance(maximum,[](unsigned,int){return false;});
        expect(chosen==PerformanceLevels{},"alpha34 all locked including NOS remains stock, no mandatory NOS bypass");
        chosen=SelectCareerPerformance(maximum,[](unsigned,int shop){return shop<=1;});
        expect(chosen[6]==1&&chosen[4]==1,"alpha34 NOS always equipped when first shop package unlocked");
        maximum.fill(0xFFFFFFFFu);unsigned calls=0;
        chosen=SelectCareerPerformance(maximum,[&](unsigned,int){++calls;return true;});
        expect(chosen==PerformanceLevels{}&&calls==0,"alpha34 invalid vehicle cap cannot request arbitrary upgrade or junkman");
    }
