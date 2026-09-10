#pragma once
#include "SessionSettingsPayload.h"
#include <windows.h>
#include <d3d9.h>

namespace helen {
    /** @brief Value-only snapshot of one validated idle viewport; never retains COM resources across reset. */
    struct SessionNativeObservation {
        /** @brief Single engine viewport identity retained only for post-call lifetime checks. */
        std::uintptr_t Owner;
        /** @brief Renderer identity from the pinned runtime global. */
        std::uintptr_t Renderer;
        /** @brief Required owning-thread top-level window. */
        HWND Window;
        /** @brief Actual swap-chain parameters, copied after releasing the temporary COM reference. */
        D3DPRESENT_PARAMETERS Presentation;
        /** @brief Complete live settings payload captured after any preceding rendering flush. */
        SessionSettingsPayload::Values Payload;
        /** @brief Actual engine viewport dimensions and mode, independent of persisted launcher values. */
        unsigned Width;
        /** @brief Positive current engine viewport height. */
        unsigned Height;
        /** @brief Current engine viewport mode, zero for windowed and one for fullscreen. */
        int Fullscreen;
        /** @brief Initialized GEngine identity, retained to reject lifetime changes during experimental physics edits. */
        std::uintptr_t Engine;
        /** @brief Engine-owned PhysX scalar read by GetPhysXLevel, not the separately cached INI value. */
        int Physx;
        /** @brief Captures checked engine/device state; throws before returning an invalid observation. */
        static SessionNativeObservation Capture();
        /** @brief Validates target fullscreen mode and multisample support without altering device policy. */
        void RequireTarget(const BatmanGraphicsDraftState& draft, const SessionGraphicsDelta& delta) const;
        /** @brief Publishes an explicit VSync override before the single required stock display refresh. */
        void ApplyDisplay(const BatmanGraphicsDraftState& draft, const SessionGraphicsDelta& delta) const;
    };
}
