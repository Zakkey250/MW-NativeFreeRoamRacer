#pragma once
#include <Windows.h>
#include <cstring>
#include <cwchar>
// Opt-in bundle marker only. Callers must still verify the full executable
// hash, supported hook bytes and their own asset format before installing.
inline bool World2MWPortableHost(){
    wchar_t path[MAX_PATH]{};
    if(!GetModuleFileNameW(nullptr,path,MAX_PATH))return false;
    auto tail=wcsrchr(path,L'\\');
    if(!tail||_wcsicmp(tail+1,L"World2MW.exe"))return false;
    wchar_t root[MAX_PATH]{};wcsncpy_s(root,path,static_cast<size_t>(tail-path+1));
    wcscpy_s(tail+1,MAX_PATH-static_cast<size_t>(tail+1-path),L".World2MW-portable");
    auto file=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,0,nullptr);
    if(file==INVALID_HANDLE_VALUE)return false;
    constexpr char marker[]="W2MW-PORTABLE-V1\r\n";
    char bytes[sizeof(marker)]{};DWORD count=0;
    bool ok=ReadFile(file,bytes,sizeof(bytes),&count,nullptr)&&count==sizeof(marker)-1&&
        !std::memcmp(bytes,marker,sizeof(marker)-1);
    CloseHandle(file);
    for(auto asset:{L"NativeWorld\\index.txt",L"NativeProps\\catalog.bin"}){
        wcscpy_s(path,root);wcscat_s(path,asset);
        auto attributes=GetFileAttributesW(path);
        ok=ok&&attributes!=INVALID_FILE_ATTRIBUTES&&!(attributes&FILE_ATTRIBUTE_DIRECTORY);
    }
    return ok;
}
