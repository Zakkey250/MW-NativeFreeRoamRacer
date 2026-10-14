#pragma once
#include <cstdint>

namespace nfrr_marker {
// Shared by every racer: a full native icon table is a manager-wide failure.
struct Retry {
    std::uint64_t nextAttempt = 0, nextLog = 0;
    unsigned failures = 0;
    bool Ready(std::uint64_t now) const noexcept { return now >= nextAttempt; }
    bool Fail(std::uint64_t now) noexcept {
        if (failures < 32) ++failures;
        const auto delay = std::uint64_t{1000} << (failures < 4 ? failures - 1 : 3);
        nextAttempt = now + delay;
        if (nextLog && now < nextLog) return false;
        nextLog = now + 30000;
        return true;
    }
    void Reset() noexcept { *this = {}; }
};
}
