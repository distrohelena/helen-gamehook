#pragma once

#include <atomic>
#include <span>
#include <d3d9.h>
#include <HelenHook/D3d9VsyncOverride.h>

namespace helen {
    /**
     * @brief Address-free, session-only interval policy shared by a hook set's intercepted devices.
     * Owns no COM references, engine state or files. Publication is thread-safe; each Apply
     * uses one snapshot for the entire adapter-group array. The caller owns writable parameters.
     */
    class D3d9PresentationPolicy {
    private:
        /** @brief Desired policy, initially transparent; never represents confirmed device state. */
        std::atomic<D3d9VsyncOverride> Mode{D3d9VsyncOverride::GameControlled};

    public:
        /** @brief Publishes a policy for future API calls; throws invalid_argument for unknown enum values. */
        void SetVsyncOverride(D3d9VsyncOverride mode);
        /** @brief Reads the desired policy, not the current swap-chain interval. */
        D3d9VsyncOverride GetVsyncOverride() const noexcept;
        /**
         * @brief Overrides only PresentationInterval in the supplied complete parameter array.
         * Empty arrays are untouched. No capability fallback, COM call, reset or persistence occurs.
         */
        void Apply(std::span<D3DPRESENT_PARAMETERS> parameters) const noexcept;
    };
}
