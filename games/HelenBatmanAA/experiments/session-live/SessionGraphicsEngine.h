#pragma once
#include "SessionGraphicsDelta.h"
#include <HelenHook/BatmanGraphicsSnapshot.h>

namespace helen {
    /** @brief Required native boundary; tests replace engine calls, not the session state machine. */
    class SessionGraphicsEngine {
    public:
        /** @brief Releases implementation-owned observations without altering game settings. */
        virtual ~SessionGraphicsEngine() = default;
        /** @brief Captures current normalized engine/device values; unavailable required state must fail explicitly. */
        virtual BatmanGraphicsSnapshot Capture() const = 0;
        /** @brief Describes which transitions have a bound live path; device-specific validation follows in Preflight. */
        virtual SessionGraphicsDelta::Capabilities Capabilities() const noexcept = 0;
        /** @brief Validates the complete delta, identities and target device support without mutation. */
        virtual void Preflight(const BatmanGraphicsDraftState& baseline, const BatmanGraphicsDraftState& draft,
            const SessionGraphicsDelta& delta) = 0;
        /** @brief Publishes a complete private session-INI update; any exception may follow mutation. */
        virtual void Stage(const BatmanGraphicsDraftState& draft, const SessionGraphicsDelta& delta) = 0;
        /** @brief Reloads/applies supported settings on the verified owning thread, never saving originals. */
        virtual void Apply(const BatmanGraphicsDraftState& draft, const SessionGraphicsDelta& delta) = 0;
        /** @brief Verifies cache, live payload, render/device state, and preservation before success is published. */
        virtual void Verify(const BatmanGraphicsDraftState& draft, const SessionGraphicsDelta& delta) = 0;
    };
}
