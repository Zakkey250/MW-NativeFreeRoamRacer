    {
        encounter_route::Pursuit pursuit;
        using battle::Point;
        const Point forward{0,0,1};
        auto hint=pursuit.Update({0,0,100},forward,{0,0,0},forward);
        expect(hint.valid&&hint.position.z==100&&!pursuit.passing(),"alpha37 pursuit targets current player instead of old route");
        hint=pursuit.Update({80,0,110},{1,0,0},{0,0,10},forward);
        expect(hint.valid&&hint.position.x==80&&hint.position.z==110&&hint.heading.x==1,"alpha37 junction change immediately updates road destination and tangent");
        hint=pursuit.Update({0,0,5.01f},forward,{},forward);
        expect(hint.valid&&!pursuit.passing(),"alpha37 outside 5m remains road pursuit");
        hint=pursuit.Update({0,0,5},forward,{},forward);
        expect(hint.valid&&!pursuit.passing(),"alpha44 transient 5m proximity retains pursuit");
        for(int i=0;i<4;++i)hint=pursuit.Update({0,0,5},forward,{},forward);
        expect(!hint.valid&&pursuit.passing(),"alpha44 sustained 5m opportunity permits native road pass");
        for(float gap:{5.01f,4.99f,12.f,17.99f}) {
            hint=pursuit.Update({0,0,gap},forward,{},forward);
            expect(!hint.valid&&pursuit.passing(),"alpha37 pass hysteresis prevents boundary chatter");
        }
        hint=pursuit.Update({0,0,18},forward,{},forward);
        expect(hint.valid&&!pursuit.passing(),"alpha37 lost passing opportunity reacquires current player at 18m");
        hint=pursuit.Update({0,0,3},forward,{},Point{0,0,-1});
        expect(hint.valid&&!pursuit.passing(),"alpha37 opposing direction remains road-routed, not forward pass");
        hint=pursuit.Update({0,4,0},forward,{},forward);
        expect(hint.valid&&!pursuit.passing(),"alpha37 stacked road proximity does not trigger pass");
        pursuit.Update({0,0,3},forward,{},forward);
        hint=pursuit.Update({0,0,4},forward,{},Point{1,0,0});
        expect(hint.valid&&!pursuit.passing(),"alpha37 turn away from player cancels passing phase");
        pursuit.Update({0,0,3},forward,{},forward);pursuit.Reset();
        expect(!pursuit.passing(),"alpha37 role change and new race clear pursuit phase");
        hint=pursuit.Update({NAN,0,0},forward,{},forward);
        expect(!hint.valid&&!pursuit.passing(),"alpha37 invalid position cannot issue native path");
        hint=pursuit.Update({0,0,100},forward,{},Point{NAN,0,0});
        expect(!hint.valid,"alpha37 invalid heading fails closed");
        hint=pursuit.Update({0,0,2},{}, {},{});
        expect(hint.valid&&!pursuit.passing(),"alpha37 unknown headings cannot enter close passing");
    }
