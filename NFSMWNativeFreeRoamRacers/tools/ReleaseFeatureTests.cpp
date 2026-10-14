#include "../include/EncounterWager.h"
#include "../include/EncounterReward.h"
#include "../include/EncounterWeapons.h"
#undef NDEBUG
#include <cassert>
#include <iostream>
int main(){
    using namespace native_freeroam;
    unsigned checks=0;
    for(unsigned bin=1;bin<=15;++bin){
        const auto base=encounter_reward::Amount(true,bin);
        assert(encounter_reward::Amount(false,bin)==1000);
        for(unsigned wins=0;wins<100;++wins){
            unsigned amount=0;assert(encounter_wager::Quote(base,wins,&amount));
            assert(amount==base+(base/20)*wins);assert(encounter_wager::CanStart(1000000,amount));
            std::int32_t cash=0;assert(encounter_wager::Transfer(1000000,amount,true,&cash)&&cash==1000000+int(amount));
            assert(encounter_wager::Transfer(1000000,amount,false,&cash)&&cash==1000000-int(amount));++checks;
        }
    }
    unsigned amount=0;
    assert(encounter_wager::Quote(300,1,&amount)&&amount==315);
    assert(!encounter_wager::Quote(300,UINT64_MAX,&amount));
    assert(!encounter_wager::CanStart(299,300));
    assert(!encounter_wager::CanStart(INT32_MAX,300));
    encounter_wager::Session s;s.Bind(1);s.Complete(true,true);assert(s.wins==1);
    s.Complete(false,true);assert(s.wins==0);s.Complete(true,true);s.Bind(2);assert(s.wins==0);
    assert(encounter_weapons::RivalEmpTarget(3,0x1000-0xAC,0x1000));
    assert(!encounter_weapons::RivalEmpTarget(1,0x1000-0xAC,0x1000));
    assert(!encounter_weapons::RivalEmpTarget(2,0x1000-0xAC,0x1000));
    assert(!encounter_weapons::RivalEmpTarget(3,0x2000-0xAC,0x1000));
    std::cout<<"PASS "<<checks<<" reward/stake cases; insufficient cash, overflow, streak reset and police/rival forfeit identity\n";
}
