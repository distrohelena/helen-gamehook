#pragma once
#include "RefreshRequest.h"
#include <array>

namespace helen {
    /** @brief Owns one thread-local resize activation; stack identity excludes recursive stock callbacks. */
    class RefreshActivation {
    private:
        /** @brief Borrowed only on the invoking thread while its native resize call is live. */
        static thread_local RefreshActivation* Active;
        /** @brief Once-only consumption state; no device or renderer ownership is transferred. */
        RefreshRequest Request;
        /** @brief Exact expected engine return sites followed by the published wrapper return site. */
        std::array<std::uintptr_t, 5> Returns;
        /** @brief Renderer identity checked before reading any engine device field. */
        std::uintptr_t Renderer;
        /** @brief Borrowed device identity captured before the synchronous stock resize. */
        std::uintptr_t Device;
        /** @brief Outer return-slot address, valid only between publication and cleanup. */
        std::uintptr_t Anchor = 0;
    public:
        /** @brief Requires nonzero identities/sites; rejects nested Helen activations before publication. */
        RefreshActivation(std::uintptr_t renderer, std::uintptr_t device, const std::array<std::uintptr_t, 4>& returns);
        /** @brief Clears thread-local publication and request even when the engine unwinds exceptionally. */
        ~RefreshActivation() noexcept;
        /** @brief An activation's address and lifetime cannot be transferred. */
        RefreshActivation(const RefreshActivation&) = delete;
        /** @brief Active native frames cannot be replaced by assignment. */
        RefreshActivation& operator=(const RefreshActivation&) = delete;
        /** @brief Native wrapper callback publishes the precise call slot before entering the engine. */
        static bool __cdecl Publish(void* context, std::uintptr_t anchor, std::uintptr_t returnSite) noexcept;
        /** @brief Native wrapper callback invalidates the request before returning to C++. */
        static void __cdecl Clear(void* context) noexcept;
        /** @brief Checks the active renderer without touching engine memory; false is the ordinary idle case. */
        static bool ExpectsRenderer(std::uintptr_t renderer) noexcept;
        /** @brief Consumes once only for the exact thread, device and bounded stock stack chain. */
        static bool Consume(std::uintptr_t frame, std::uintptr_t renderer, std::uintptr_t device) noexcept;
        /** @brief Reports observed decision-hook consumption, not successful device Reset. */
        bool WasConsumed() noexcept;
    };
}
