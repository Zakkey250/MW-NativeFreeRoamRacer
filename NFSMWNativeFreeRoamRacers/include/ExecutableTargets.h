#pragma once
#include <cstdint>
#include <string_view>

namespace nfrr_target {
struct Target { const char* id; std::uint64_t size; const char* sha256; };
inline constexpr Target targets[]{
    {"nfspatcher-en-1.3-4gb",6029312,"B248271BF8EAC8C9B283B8C95E3ADD672B713BF529B05F1780E58268493B9D06"},
    {"redux-3.04-4gb",5926912,"0C5675A08CD71FD6D31CA87E992A915054BD8B80D268BFF0561D7ECC2067E342"},
};
// Identical file-backed .text/.rdata/.data at identical RVAs were verified.
// Resource/layout differences do not authorize other executables or relaxed hooks.
inline constexpr const Target* Find(std::uint64_t size,std::string_view sha256) noexcept {
    for(const auto& target:targets)if(size==target.size&&sha256==target.sha256)return &target;
    return nullptr;
}
inline constexpr bool KnownSize(std::uint64_t size) noexcept {
    for(const auto& target:targets)if(size==target.size)return true;
    return false;
}
}
