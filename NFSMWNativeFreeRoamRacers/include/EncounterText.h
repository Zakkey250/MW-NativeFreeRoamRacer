#pragma once
#include <array>
#include <string>
namespace native_freeroam::encounter_text {
enum class Id { Start, Lead, Passed, Victory, Defeat, Cancelled, PaymentFailed };
// Stable IDs, Unicode strings and a separate table allow later language packs.
inline constexpr std::array<const wchar_t*,7> japanese = {
    L"\u30d0\u30c8\u30eb\u958b\u59cb\uff01\n\u307e\u305a\u306f\u8ffd\u3044\u629c\u304b\u306a\u3044\u3068\uff01",
    L"\u3088\u3063\u3057\u3083\uff01\u4ffa\u304c\u30ea\u30fc\u30c9\u3060\uff01",
    L"\u3057\u307e\u3063\u305f\uff01\u629c\u304b\u308c\u3061\u307e\u3063\u305f\uff01",
    L"\u52dd\u5229\uff01\n\u30ad\u30e3\u30c3\u30b7\u30e5 +1000",
    L"\u6557\u5317\u3057\u305f\u2026",
    L"\u30d0\u30c8\u30eb\u4e2d\u6b62",
    L"\u52dd\u5229\uff01\n\u5831\u916c\u306e\u52a0\u7b97\u306b\u5931\u6557\u3057\u307e\u3057\u305f"
};
inline constexpr std::array<const wchar_t*,7> english = {
    L"Battle started!\nFirst, overtake your rival!", L"All right! I'm in the lead!",
    L"Oh no! I've been overtaken!", L"Victory!\nCash +1000", L"Defeated...",
    L"Battle cancelled", L"Victory!\nCash could not be awarded"
};
inline const wchar_t* Get(Id id, bool useEnglish=false) {
    return (useEnglish?english:japanese)[static_cast<unsigned>(id)];
}
}
