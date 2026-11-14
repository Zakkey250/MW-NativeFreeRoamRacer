#pragma once
namespace car_streaming_capacity {
// Expand in measured steps; keep earlier accepted budgets selectable.
constexpr unsigned DefaultMiB=256;
constexpr unsigned SelectMiB(unsigned requested){return requested==160||requested==192||requested==224||requested==256?requested:160;}
constexpr unsigned Units(unsigned requested){return SelectMiB(requested)*1024;}
}
