#pragma once
#include <cstdint>

namespace helen {
    /** @brief Batman's synchronous viewport resize ABI; callee removes five stack arguments. */
    using ViewportResize = void(__thiscall*)(void*, unsigned, unsigned, int, int, int);
    /** @brief Publishes the exact outer return slot and code site; false prevents the engine call. */
    using PublishResize = bool(__cdecl*)(void*, std::uintptr_t, std::uintptr_t) noexcept;
    /** @brief Invalidates a published request before the native return slot ceases to exist. */
    using ClearResize = void(__cdecl*)(void*) noexcept;
    /** @brief Calls the verified resize ABI with x/y=-1 after publication; caller owns exceptional cleanup. */
    bool AnchoredResize(ViewportResize resize, void* owner, unsigned width, unsigned height,
        int fullscreen, void* context, PublishResize publish, ClearResize clear);
}
