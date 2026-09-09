#include "DecisionBridge.h"
static_assert(sizeof(void*) == 4, "x86 bridge required");

namespace helen {
    bool (__cdecl* DecisionCallback)(std::uintptr_t, std::uintptr_t) noexcept = nullptr;
    void* SkipTarget = nullptr;
    void* RebuildTarget = nullptr;

    __declspec(naked) void DecisionBridge() {
        __asm {
            pushfd
            pushad
            mov ebx, esp
            sub esp, 528
            and esp, -16
            fxsave [esp]
            cld
            sub esp, 16
            mov [esp], ebp
            mov eax, [ebx+16]
            mov [esp+4], eax
            call dword ptr [DecisionCallback]
            add esp, 16
            test al, al
            jnz Forced
            fxrstor [esp]
            mov esp, ebx
            popad
            popfd
            test eax, eax
            jz Skip
            jmp dword ptr [RebuildTarget]
        Forced:
            fxrstor [esp]
            mov esp, ebx
            popad
            popfd
            test eax, eax
            jmp dword ptr [RebuildTarget]
        Skip:
            jmp dword ptr [SkipTarget]
        }
    }
}
