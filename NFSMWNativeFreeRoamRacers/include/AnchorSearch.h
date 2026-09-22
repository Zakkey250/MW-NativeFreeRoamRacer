#pragma once
#include <array>
#include <cstddef>
#include <random>
#include <vector>

namespace native_freeroam {
// Scratch indices into ONE caller-owned vehicle snapshot. Never retain native
// road/vehicle pointers, a validated result or admission across management ticks.
struct AnchorSearch {
    static constexpr std::size_t none=~std::size_t{0};
    struct Result {
        std::size_t index=none,candidates=0,probes=0,rejected=0;
        int rank=-1;
    };
    std::array<std::vector<std::size_t>,4> tiers;
    void Clear() noexcept {for(auto& tier:tiers)tier.clear();}
    void Reserve(std::size_t initial) {for(auto& tier:tiers)tier.reserve(initial);}
    void Add(std::size_t index,int rank) {
        if(rank>=0&&rank<static_cast<int>(tiers.size())&&index!=none)
            tiers[static_cast<std::size_t>(rank)].push_back(index);
    }
    template<class Random,class Validate>
    Result Select(Random& random,Validate validate) {
        Result result;
        for(std::size_t rank=0;rank<tiers.size();++rank) {
            auto& tier=tiers[rank];const auto candidates=tier.size();
            // Random sampling without replacement: the first valid member is
            // uniform among valid members, regardless of invalid candidates.
            while(!tier.empty()) {
                const auto slot=std::uniform_int_distribution<std::size_t>(0,tier.size()-1)(random);
                const auto index=tier[slot];tier[slot]=tier.back();tier.pop_back();
                ++result.probes;
                if(validate(index)) {
                    result.index=index;result.rank=static_cast<int>(rank);result.candidates=candidates;
                    Clear();return result;
                }
                ++result.rejected;
            }
        }
        Clear();return result;
    }
};
}
