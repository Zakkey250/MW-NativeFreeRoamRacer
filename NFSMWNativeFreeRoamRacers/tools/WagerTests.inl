template<class Expect> void RunWagerTests(Expect expect) {
    using namespace encounter_wager;
    unsigned amount=0;
    expect(Quote(300,0,&amount)&&amount==300,"wager first battle base reward");
    expect(Quote(300,1,&amount)&&amount==315,"wager five percent no hundred rounding");
    expect(Quote(300,2,&amount)&&amount==330,"wager additive not compound");
    expect(Quote(3000,1000,&amount)&&amount==153000,"wager no gameplay multiplier cap");
    expect(!Quote(3000,UINT64_MAX,&amount),"wager huge streak cannot overflow calculation");
    for(unsigned bin=1;bin<=15;++bin)for(unsigned wins=0;wins<100;++wins) {
        const auto base=encounter_reward::Amount(true,bin);
        expect(Quote(base,wins,&amount)&&amount==base+(base/20)*wins,"wager all BL tiers exact integer five percent");
    }
    expect(CanStart(315,315)&&!CanStart(314,315)&&!CanStart(-1,315)&&!CanStart(INT32_MAX,315),
        "wager start requires full stake and reward headroom");
    std::int32_t after=99;
    expect(Transfer(315,315,false,&after)&&after==0,"wager loss exact balance reaches zero");
    expect(!Transfer(314,315,false,&after)&&after==0,"wager insufficient settlement leaves output untouched");
    expect(!Transfer(INT32_MAX,315,true,&after)&&after==0,"wager credit cannot overflow");
    expect(Transfer(315,315,true,&after)&&after==630,"wager win exact credit");
    Session session;session.Bind(1);session.Complete(true,true);session.Bind(1);
    expect(session.wins==1,"wager same profile retains streak");session.Bind(2);
    expect(session.wins==0,"wager new profile resets streak");session.Complete(true,true);session.Complete(false,true);
    expect(session.wins==0,"wager defeat resets streak");session.Complete(true,true);session.Reset();
    expect(!session.profile&&!session.wins,"wager world exit clears identity and streak");
    expect(encounter_text::Format(encounter_text::Id::InsufficientFunds,false)==L"\u639b\u3051\u91d1\u304c\u8db3\u308a\u306a\u3044\u305e\uff01",
        "wager Japanese insufficient funds exact requested text");
    expect(encounter_text::Format(encounter_text::Id::WagerDefeat,true,315)==L"Defeated...\nCash -315",
        "wager defeat displays exact deducted amount");

    using namespace NFSPluginSDK::MW05;
    const auto savedBattle=g_battle;const auto savedSession=g_wagerSession;const auto savedSettings=g_settings;
    const auto savedMessage=g_pendingEncounterMessage;
    std::vector<unsigned char> dbBytes(sizeof(cFrontEndDatabase)),profileBytes(sizeof(UserProfile));
    auto* db=reinterpret_cast<cFrontEndDatabase*>(dbBytes.data());
    auto* profile=reinterpret_cast<UserProfile*>(profileBytes.data());
    void* oldDb=nullptr;auto* slot=reinterpret_cast<void*>(Address(0x0091CF90));
    std::memcpy(&oldDb,slot,sizeof(oldDb));std::memcpy(slot,&db,sizeof(db));
    db->bProfileLoaded=true;db->CurrentUserProfiles[0]=profile;
    g_settings.encounterWinStreak=true;g_settings.encounterAIMode=encounter_custom::Mode::Custom;
    g_wagerSession.Reset();g_battle={};g_battle.profile=profile;g_battle.reward=300;
    profile->mTheCareerSettings.CurrentCash=1000;
    expect(PrepareEncounterWager()&&g_battle.wager&&g_battle.reward==300,"wager runtime initial quote");
    battle::Sample sample{};sample.player.identity={1,11,111};sample.rival.identity={2,22,222};
    sample.player.forward=sample.rival.forward={0,0,1};sample.player.speed=sample.rival.speed=30;sample.rival.position.z=10;
    auto win=[&] {
        g_battle.model.Reset();g_battle.rewardAttempted=false;
        sample.player.position={};sample.rival.position.z=10;g_battle.model.Start(sample,g_battle.reward);
        sample.player.position.z=13;sample.rival.position.z=11;g_battle.model.Step(sample,.05f);
        sample.player.position.z=15;sample.rival.position.z=12;g_battle.model.Step(sample,.05f);
        for(unsigned i=0;i<30;++i){sample.player.position.z+=10;g_battle.model.Step(sample,.1f);}
    };
    win();expect(SettleEncounterWagerResult(true)&&profile->mTheCareerSettings.CurrentCash==1300&&g_wagerSession.wins==1,
        "wager production win credits and increments once");
    expect(!SettleEncounterWagerResult(true)&&profile->mTheCareerSettings.CurrentCash==1300&&g_wagerSession.wins==1,
        "wager duplicate result does not change cash or streak");
    g_battle.reward=300;
    expect(PrepareEncounterWager()&&g_battle.reward==315,"wager production second battle includes prior win");
    win();expect(SettleEncounterWagerResult(true)&&profile->mTheCareerSettings.CurrentCash==1615&&g_wagerSession.wins==2,
        "wager production non-hundred credit accepted");
    g_battle.reward=300;PrepareEncounterWager();g_battle.model.Reset();g_battle.rewardAttempted=false;
    sample.player.position={};sample.rival.position.z=10;g_battle.model.Start(sample,g_battle.reward);g_battle.model.Forfeit();
    expect(SettleEncounterWagerResult(false)&&profile->mTheCareerSettings.CurrentCash==1285&&g_wagerSession.wins==0,
        "wager production forfeit deducts locked amount and clears streak");
    expect(!SettleEncounterWagerResult(false)&&profile->mTheCareerSettings.CurrentCash==1285,"wager loss cannot charge twice");
    g_battle.reward=300;PrepareEncounterWager();g_battle.rewardAttempted=false;g_battle.model.Reset();
    sample.player.position={};sample.rival.position.z=10;g_battle.model.Start(sample,g_battle.reward);
    for(unsigned i=0;i<35;++i){sample.rival.position.z+=10;g_battle.model.Step(sample,.1f);}
    expect(g_battle.model.phase()==battle::Phase::Lost&&SettleEncounterWagerResult(false)&&
        profile->mTheCareerSettings.CurrentCash==985,"wager ordinary 300m loss uses same exact debit path");
    g_battle.reward=300;profile->mTheCareerSettings.CurrentCash=299;
    expect(!PrepareEncounterWager()&&profile->mTheCareerSettings.CurrentCash==299&&!g_battle.nativeStart.issued,
        "wager insufficient balance rejects before native AI start with no cash write");
    profile->mTheCareerSettings.CurrentCash=1000;g_battle.reward=300;PrepareEncounterWager();win();
    db->CurrentUserProfiles[0]=nullptr;
    expect(!SettleEncounterWagerResult(true)&&profile->mTheCareerSettings.CurrentCash==1000&&g_wagerSession.wins==0,
        "wager changed profile prevents payment and resets streak");
    db->CurrentUserProfiles[0]=profile;g_settings.encounterAIMode=encounter_custom::Mode::Stable;g_battle.reward=1000;
    expect(PrepareEncounterWager()&&!g_battle.wager&&g_battle.reward==1000,"wager Stable ignores enabled switch");
    g_settings.encounterAIMode=encounter_custom::Mode::Custom;g_settings.encounterWinStreak=false;g_battle.reward=300;
    expect(PrepareEncounterWager()&&!g_battle.wager&&g_battle.reward==300,"wager OFF preserves existing Custom reward");
    g_wagerSession.Bind(reinterpret_cast<std::uintptr_t>(profile));g_wagerSession.wins=3;
    const auto beforeCancel=profile->mTheCareerSettings.CurrentCash;
    EndEncounterBattle(false,true);
    expect(g_wagerSession.wins==3&&profile->mTheCareerSettings.CurrentCash==beforeCancel,
        "wager technical cancellation does not charge or erase prior wins");
    g_wagerSession.Bind(1);g_wagerSession.wins=5;ResetEncounterWager();
    expect(!g_wagerSession.wins&&!g_wagerSession.profile,"wager production world reset clears state");
    std::memcpy(slot,&oldDb,sizeof(oldDb));g_battle=savedBattle;g_wagerSession=savedSession;
    g_settings=savedSettings;g_pendingEncounterMessage=savedMessage;
}
