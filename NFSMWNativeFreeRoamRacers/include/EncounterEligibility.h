#pragma once
#include <algorithm>
#include <cmath>
namespace native_freeroam::eligibility {
inline constexpr float dwellSeconds=0.15f;
inline constexpr float headingDotMinimum=0.25f;
// A fixed 5m altitude test rejected an actual 11.63m following sample on a
// slope. Permit slope-proportional height, but cap it and retain the close
// vertical separation guard. This is geometric screening, not road connectivity.
inline float HeightLimit(float dx,float dz) {
    return std::clamp(std::hypot(dx,dz)*0.65f,5.0f,12.0f);
}
}
