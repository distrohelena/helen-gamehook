#include "AnchoredResize.h"
namespace helen {
    __declspec(naked) bool AnchoredResize(ViewportResize, void*, unsigned, unsigned, int, void*, PublishResize, ClearResize) {
        __asm {
            push ebp
            mov ebp, esp
            push -1
            push -1
            push dword ptr [ebp+24]
            push dword ptr [ebp+20]
            push dword ptr [ebp+16]
            lea eax, [esp-4]
            push offset Returned
            push eax
            push dword ptr [ebp+28]
            call dword ptr [ebp+32]
            add esp, 12
            test al, al
            jz Refused
            mov ecx, [ebp+12]
            call dword ptr [ebp+8]
        Returned:
            push dword ptr [ebp+28]
            call dword ptr [ebp+36]
            add esp, 4
            mov eax, 1
            pop ebp
            ret
        Refused:
            add esp, 20
            xor eax, eax
            pop ebp
            ret
        }
    }
}
