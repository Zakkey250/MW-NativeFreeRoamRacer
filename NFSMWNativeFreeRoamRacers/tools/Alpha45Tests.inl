    {
        using encounter_route::CommitRoute;using encounter_route::CommitDecision;
        expect(CommitRoute(false,false,true,true,3,false,4,100,100,1,80,80)==CommitDecision::HoldPath,"alpha45 committed path retained despite moving player");
        expect(CommitRoute(false,false,true,true,2,false,0,100,100,1,80,5)==CommitDecision::HoldDirection,"alpha45 native Direction continues through passed old position");
        expect(CommitRoute(false,false,true,true,3,false,4,100,101,1.99f,100,80)==CommitDecision::HoldPath,"alpha45 immediate junction segment change does not reverse course");
        expect(CommitRoute(false,false,true,true,3,false,4,100,101,2,40,80)==CommitDecision::RoadChanged,"alpha45 road change after dwell and progress allows recalculation");
        expect(CommitRoute(false,false,true,true,3,false,4,100,101,3,39,80)==CommitDecision::HoldPath,"alpha45 small road-projection jitter cannot change route");
        expect(CommitRoute(false,false,true,true,3,false,4,100,100,3.99f,300,80)==CommitDecision::HoldPath,"alpha45 same long segment retained within bound");
        expect(CommitRoute(false,false,true,true,3,false,4,100,100,4,300,80)==CommitDecision::TimeLimit,"alpha45 wrong road cannot retain stale destination indefinitely");
        expect(CommitRoute(false,false,true,true,2,false,0,100,100,4,300,0)==CommitDecision::TimeLimit,"alpha45 Direction coasting also expires");
        expect(CommitRoute(false,false,true,true,3,true,4,100,100,1,50,80)==CommitDecision::GoalConsumed,"alpha45 crossed goal renews without stopping");
        expect(CommitRoute(false,false,true,true,3,false,4,100,100,1,50,12)==CommitDecision::GoalConsumed,"alpha45 path endpoint proximity may renew immediately");
        expect(CommitRoute(false,true,true,true,3,false,4,100,100,1,0,80)==CommitDecision::Stalled,"alpha45 physical stall overrides commitment");
        expect(CommitRoute(true,false,true,true,3,false,4,100,100,1,50,80)==CommitDecision::Forced,"alpha45 lead and pass transition override commitment");
        expect(CommitRoute(false,false,false,true,3,false,4,100,100,1,50,80)==CommitDecision::Forced,"alpha45 unrelated route never adopted as committed");
        expect(CommitRoute(false,false,true,false,3,false,4,100,100,1,50,80)==CommitDecision::Invalid,"alpha45 invalid nav cannot be retained");
        expect(CommitRoute(false,false,true,true,1,false,4,100,100,1,50,80)==CommitDecision::Invalid,"alpha45 traffic navigation not treated as racer route");
        expect(CommitRoute(false,false,true,true,3,false,0,100,100,1,50,80)==CommitDecision::Invalid,"alpha45 empty Path not mislabeled as completed route");
        expect(CommitRoute(false,false,true,true,3,false,511,100,100,1,50,80)==CommitDecision::Invalid,"alpha45 corrupt edge count rejected");
        expect(CommitRoute(false,false,true,true,3,false,4,-1,100,1,50,80)==CommitDecision::Invalid,"alpha45 unavailable origin road rejected");
        expect(CommitRoute(false,false,true,true,3,false,4,100,-1,1,50,80)==CommitDecision::Invalid,"alpha45 unavailable current road rejected");
        expect(CommitRoute(false,false,true,true,3,false,4,100,100,NAN,50,80)==CommitDecision::Invalid,"alpha45 invalid timer rejected");
        expect(CommitRoute(false,false,true,true,3,false,4,100,100,1,NAN,80)==CommitDecision::Invalid,"alpha45 invalid position progress rejected");
        expect(CommitRoute(false,false,true,true,3,false,4,100,100,1,50,NAN)==CommitDecision::Invalid,"alpha45 invalid endpoint distance rejected");
        expect(!encounter_route::Holding(CommitDecision::Invalid)&&encounter_route::Holding(CommitDecision::HoldDirection),"alpha45 hold flag distinguishes outcomes");
    }
