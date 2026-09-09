#pragma once
#include <cstdint>

namespace helen {
    /** @brief Required no-throw callback receiving original EBP and EBX; set before exposing the splice. */
    extern bool (__cdecl* DecisionCallback)(std::uintptr_t frame, std::uintptr_t renderer) noexcept;
    /** @brief Original JE destination; immutable once game execution can enter the bridge. */
    extern void* SkipTarget;
    /** @brief Original fallthrough destination; immutable once game execution can enter the bridge. */
    extern void* RebuildTarget;
    /** @brief JMP-only x86 bridge preserving legacy x87/SSE state and reproducing TEST EAX,EAX flags; not AVX state. */
    void DecisionBridge();
}
