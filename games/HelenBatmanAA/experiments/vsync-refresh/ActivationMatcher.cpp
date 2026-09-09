#include "ActivationMatcher.h"

namespace helen {
    bool ActivationMatcher::Matches(std::uintptr_t frame, std::uintptr_t anchor,
        std::uintptr_t stackLimit, std::uintptr_t stackBase,
        const std::array<std::uintptr_t, 5>& returns) noexcept {
        /** @brief Return slots derived from the pinned caller frames, relative to renderer EBP. */
        constexpr std::array<std::uintptr_t, 5> offsets{4, 0xC, 0x4C, 0x84, 0xCC};
        if (frame < stackLimit || frame >= stackBase || stackBase - frame < 0xD0 || frame % 4 != 0) {
            return false;
        }
        if (anchor == 0 || frame + 0xCC != anchor) {
            return false;
        }
        for (std::size_t index = 0; index < offsets.size(); ++index) {
            if (*reinterpret_cast<const std::uintptr_t*>(frame + offsets[index]) != returns[index]) {
                return false;
            }
        }
        return true;
    }
}
