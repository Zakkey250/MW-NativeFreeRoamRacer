#pragma once
#include <Windows.h>
#include <filesystem>

namespace nfrr_config {
inline std::filesystem::path EditorPath(std::filesystem::path module) {
    module.replace_extension(L".editor.ini");
    return module;
}
// Editor packages own a separate fallback. A user's NFRR INI always wins.
inline std::filesystem::path SettingsPath(std::filesystem::path module) {
    module.replace_extension(L".ini");
    if (GetFileAttributesW(module.c_str()) != INVALID_FILE_ATTRIBUTES) return module;
    const auto editor = EditorPath(module);
    return GetFileAttributesW(editor.c_str()) != INVALID_FILE_ATTRIBUTES ? editor : module;
}
inline bool SelectionOnly(bool requested, bool populationEnabled) noexcept {
    return requested && !populationEnabled;
}
}
