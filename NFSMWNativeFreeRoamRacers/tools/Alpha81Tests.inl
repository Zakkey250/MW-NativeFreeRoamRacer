template<class Expect>
__declspec(noinline) void RunAlpha81Tests(Expect expect) {
    AnchorSearch search;std::mt19937 random(81);
    auto empty=[&]{for(const auto& tier:search.tiers)if(!tier.empty())return false;return true;};
    auto r=search.Select(random,[](std::size_t){return true;});
    expect(r.index==AnchorSearch::none&&r.probes==0&&r.rank==-1,"alpha81 no candidates never probes road state");
    search.Add(1,-1);search.Add(1,4);search.Add(AnchorSearch::none,0);
    expect(empty(),"alpha81 invalid rank/sentinel excluded before native reads");
    for(std::size_t count:{1u,20u,32u,64u,127u,1024u}) {
        search.Clear();for(std::size_t i=0;i<count;++i)search.Add(i,0);
        unsigned reads=0;r=search.Select(random,[&](std::size_t){++reads;return true;});
        expect(r.index<count&&r.candidates==count&&r.probes==1&&reads==1&&empty(),
            "alpha81 valid dense tier needs one road probe with no LB capacity assumption");
        for(std::size_t i=0;i<count;++i)search.Add(i,static_cast<int>(i%4));
        std::vector<bool> seen(count);bool once=true;
        r=search.Select(random,[&](std::size_t i){once&=!seen[i];seen[i]=true;return false;});
        expect(once&&r.probes==count&&r.rejected==count&&r.index==AnchorSearch::none&&empty(),
            "alpha81 all invalid candidates are checked at most once with no unsafe fallback");
    }
    for(unsigned repetition=0;repetition<32;++repetition) {
        search.Add(0,3);search.Add(1,2);search.Add(2,1);search.Add(3,0);
        r=search.Select(random,[](std::size_t){return true;});
        expect(r.index==3&&r.rank==0&&r.probes==1,"alpha81 original best tier takes priority over snapshot order");
    }
    search.Add(10,0);search.Add(11,0);search.Add(12,1);search.Add(13,2);
    std::vector<std::size_t> calls;
    r=search.Select(random,[&](std::size_t i){calls.push_back(i);return i>=12;});
    expect(r.index==12&&r.rank==1&&r.probes==3&&r.rejected==2&&calls.back()==12,
        "alpha81 fall back only after all higher priority roads fail");
    expect(std::find(calls.begin(),calls.end(),13)==calls.end(),"alpha81 lower tier roads remain untouched after success");
    std::array<unsigned,3> frequency{};bool validOnly=true;
    for(unsigned run=0;run<6000;++run) {
        for(std::size_t i=0;i<8;++i)search.Add(i,0);
        r=search.Select(random,[](std::size_t i){return i<3;});
        validOnly&=r.index<3;
        if(r.index<3)++frequency[r.index];
    }
    expect(validOnly&&std::all_of(frequency.begin(),frequency.end(),[](unsigned n){return n>1700&&n<2300;}),
        "alpha81 deterministic distribution smoke test reaches each valid peer without invalid candidates");
    search.Reserve(64);const auto capacity=search.tiers[0].capacity();
    search.Add(99,0);search.Clear();
    expect(search.tiers[0].capacity()==capacity&&empty(),"alpha81 clears indices but reuses scratch allocation");
    search.Add(55,0);r=search.Select(random,[](std::size_t){return true;});
    r=search.Select(random,[](std::size_t){return true;});
    expect(r.index==AnchorSearch::none&&r.probes==0,"alpha81 no validated anchor cached into next search");
    // Same rank function and route eligibility as production; check selection
    // equivalence against exhaustive minimum-valid-tier selection.
    for(unsigned run=0;run<128;++run) {
        std::array<int,24> ranks{};std::array<bool,24> roads{};int expectedRank=99;
        for(std::size_t i=0;i<ranks.size();++i) {
            ranks[i]=static_cast<int>(random()%6)-1;roads[i]=random()%3!=0;
            search.Add(i,ranks[i]);
            if(roads[i]&&ranks[i]>=0&&ranks[i]<4)expectedRank=std::min(expectedRank,ranks[i]);
        }
        r=search.Select(random,[&](std::size_t i){return roads[i];});
        expect(expectedRank==99?r.index==AnchorSearch::none:
            r.index<roads.size()&&roads[r.index]&&r.rank==expectedRank,
            "alpha81 selected set agrees with exhaustive legacy eligibility and ranking");
    }
}
