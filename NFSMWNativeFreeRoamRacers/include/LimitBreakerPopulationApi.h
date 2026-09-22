#pragma once
#include <cstdint>
// Capability ABI, not a MOD release/version requirement. No import library.
namespace nfsmw_population {
constexpr char exportName[]="NFSMWLB_ReservePopulationV1";
enum Result : std::uint32_t { Unavailable=0, Available=1, Unsafe=2, Busy=3 };
constexpr std::uint32_t requiredCapabilities=7; // relocated storage, leases, native traffic gate
struct Status {
    std::uint32_t size=sizeof(Status),capabilities=0,limit=0,live=0,freeSlots=0,reserved=0;
};
static_assert(sizeof(Status)==24);
using ReserveFn=std::uint32_t(__cdecl*)(std::uint32_t owner,std::uint32_t missing,Status* status);
}
