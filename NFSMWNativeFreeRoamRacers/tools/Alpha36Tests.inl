    {
        encounter_route::Trail trail;
        for(unsigned i=0;i<=20;++i) trail.Record({0,0,float(i)*12});
        const auto issued=trail.Select({0,0,0},60,{0,0,1});
        expect(issued.valid&&issued.position.z>60,"alpha36 fast approach selects recorded forward destination");
        for(float speed:{0.f,3.f,10.f,60.f,100.f}) {
            const auto same=trail.Select({0,0,18},speed,{0,0,-1});
            expect(same.position.z==issued.position.z&&same.heading.z==issued.heading.z,
                "alpha36 crash slowdown and reversal cannot retract or move committed destination");
        }
        const auto next=trail.Select(issued.position,50,{0,0,1});
        expect(next.position.z>issued.position.z,"alpha36 reaching committed endpoint advances along ordered trail");
        trail.Reset();trail.Record({500,0,0},{1,0,0});
        expect(trail.Select({480,0,0},30,{1,0,0}).position.x==500,"alpha36 role reset discards committed old route");
        trail.Record({1500,0,0});
        expect(!trail.Select({480,0,0}).valid,"alpha36 discontinuity clears committed route too");
        using encounter_route::ReuseActivePath;
        expect(ReuseActivePath(false,false,3,false,true,0),"alpha36 same active path reused without replan churn");
        expect(!ReuseActivePath(false,false,2,false,true,0),"alpha36 native Direction fallback must request same destination again");
        expect(!ReuseActivePath(false,false,3,true,true,0),"alpha36 consumed path cannot mask missing route");
        expect(!ReuseActivePath(false,true,3,false,true,0),"alpha36 stationary recovery still permits bounded retry");
        expect(!ReuseActivePath(true,false,3,false,true,0)&&!ReuseActivePath(false,false,3,false,false,0),"alpha36 new lead or missing destination cannot reuse stale path");
        expect(!ReuseActivePath(false,false,3,false,true,20)&&!ReuseActivePath(false,false,3,false,true,NAN),"alpha36 changed or invalid endpoint is not treated as identical");
    }
