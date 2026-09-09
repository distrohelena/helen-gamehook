#pragma once
#include <array>
#include <cstdint>

namespace helen {
    /** @brief Experimental matcher for the pinned positive-size viewport call chain; never installed by this fixture. */
    class ActivationMatcher {
    public:
        /** @brief Checks a captured renderer frame against one live outer call slot and five expected returns.
         * Stack bounds describe readable storage on the current thread, with an exclusive upper bound.
         * This does not validate engine ownership or authorize a settings write.
         */
        static bool Matches(std::uintptr_t frame, std::uintptr_t anchor,
            std::uintptr_t stackLimit, std::uintptr_t stackBase,
            const std::array<std::uintptr_t, 5>& returns) noexcept;
    };
}
