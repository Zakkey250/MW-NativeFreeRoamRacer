#include "../include/RuntimeConfig.h"
#undef NDEBUG
#include <cassert>
#include <fstream>
#include <iostream>

int main(int argc, char** argv) {
    assert(argc == 2);
    const std::filesystem::path folder(argv[1]);
    std::filesystem::create_directories(folder);
    const auto module=folder/"NFSMWNativeFreeRoamRacers.asi";
    const auto normal=folder/"NFSMWNativeFreeRoamRacers.ini";
    const auto editor=folder/"NFSMWNativeFreeRoamRacers.editor.ini";
    assert(!std::filesystem::exists(normal)&&!std::filesystem::exists(editor));
    assert(nfrr_config::SettingsPath(module)==normal);
    std::ofstream(editor)<<"[Editor]\nEventSelectionOnly=1\n[Population]\nEnabled=0\n[Updates]\nEnabled=0\n";
    assert(nfrr_config::SettingsPath(module)==editor);
    assert(GetPrivateProfileIntW(L"Updates",L"Enabled",1,nfrr_config::SettingsPath(module).c_str())==0);
    std::ofstream(normal)<<"[Population]\nEnabled=1\n[Encounter]\nAIMode=Custom AI\n[Updates]\nEnabled=1\n";
    assert(nfrr_config::SettingsPath(module)==normal);
    assert(GetPrivateProfileIntW(L"Updates",L"Enabled",0,nfrr_config::SettingsPath(module).c_str())==1);
    wchar_t mode[32]{};
    GetPrivateProfileStringW(L"Encounter",L"AIMode",L"",mode,32,nfrr_config::SettingsPath(module).c_str());
    assert(std::wstring(mode)==L"Custom AI");
    assert(nfrr_config::SelectionOnly(true,false));
    assert(!nfrr_config::SelectionOnly(true,true));
    assert(!nfrr_config::SelectionOnly(false,true));
    assert(!nfrr_config::SelectionOnly(false,false));
    std::cout<<"PASS settings precedence, editor fallback, update policy, Custom AI, mode matrix\n";
}
