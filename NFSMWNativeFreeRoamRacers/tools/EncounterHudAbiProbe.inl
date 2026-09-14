// Isolated x86 call frame: report ABI damage and restore the harness stack even
// for the deliberately broken alpha.21-shaped negative control.
__declspec(naked) unsigned __cdecl ProbeEncounterHudAbi(void*, void*, void*) {
    __asm {
        push ebp
        mov ebp, esp
        push ebx
        push esi
        push edi
        mov ebx, 11223344h
        mov esi, 55667788h
        mov edi, 1234ABCDh
        push dword ptr [ebp+16]
        mov ecx, dword ptr [ebp+12]
        call dword ptr [ebp+8]
        xor eax, eax
        lea edx, [ebp-12]
        cmp esp, edx
        je StackOk
        or eax, 1
    StackOk:
        cmp ebx, 11223344h
        je EbxOk
        or eax, 2
    EbxOk:
        cmp esi, 55667788h
        je EsiOk
        or eax, 4
    EsiOk:
        cmp edi, 1234ABCDh
        je EdiOk
        or eax, 8
    EdiOk:
        mov ecx, dword ptr [ebp+12]
        mov edx, dword ptr [ebp+16]
        cmp dword ptr [ecx+54h], edx
        je ArgOk
        or eax, 16
    ArgOk:
        lea esp, [ebp-12]
        pop edi
        pop esi
        pop ebx
        pop ebp
        ret
    }
}

// Deliberately omit the stack argument and RET 4, reproducing both alpha.21
// contract errors. Never installed in a game; the probe contains the damage.
void* g_brokenEncounterMenuTarget = nullptr;
__declspec(naked) void BrokenEncounterHudAbi() {
    __asm {
        push ebp
        mov ebp, esp
        push ebx
        push esi
        push edi
        mov ebx, 01010101h
        mov esi, 02020202h
        mov edi, 03030303h
        call dword ptr [g_brokenEncounterMenuTarget]
        pop edi
        pop esi
        pop ebx
        mov esp, ebp
        pop ebp
        ret
    }
}
