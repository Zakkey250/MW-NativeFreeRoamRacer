#include <Windows.h>
#include <filesystem>
#include <iostream>
#undef NDEBUG
#include <cassert>
// An unsupported isolated host: initialization must reject it without game
// hooks. This checks the actual shipping DLL's opt-in logging, not gameplay.
int wmain(int argc,wchar_t** argv){
    assert(argc==3);
    const std::filesystem::path module=argv[1];auto log=module;log.replace_extension(L".log");
    assert(!std::filesystem::exists(log));
    const auto loaded=LoadLibraryW(module.c_str());assert(loaded);
    Sleep(2200);
    if(std::wstring(argv[2])==L"on")assert(std::filesystem::exists(log)&&std::filesystem::file_size(log)>0);
    else assert(!std::filesystem::exists(log));
    std::wcout<<L"PASS actual ASI logging "<<argv[2]<<L" "<<module.filename().c_str()<<L" (unsupported host, no game hooks)\n";
    // The logging worker owns its handle until process exit; do not unload it.
}
