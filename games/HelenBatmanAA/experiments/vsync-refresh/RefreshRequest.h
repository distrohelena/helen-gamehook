#pragma once
#include <windows.h>
#include <cstdint>

namespace helen {
    /** @brief Isolated CPU-only request prototype; owns no engine objects and never performs engine calls. */
    class RefreshRequest {
    private:
        /** @brief Serializes short state transitions without holding a lock across a callback. */
        SRWLOCK Lock = SRWLOCK_INIT;
        /** @brief Highest accepted operation number; survives disarming to reject stale identities. */
        std::uint64_t LastOperation = 0;
        /** @brief Required borrowed identities while active; zero means idle, not a valid identity. */
        std::uintptr_t Renderer = 0;
        /** @brief Device identity associated with the armed operation. */
        std::uintptr_t Device = 0;
        /** @brief Exact outer return-slot identity associated with the operation. */
        std::uintptr_t Anchor = 0;
        /** @brief Permitted execution thread for consumption. */
        DWORD Thread = 0;
        /** @brief Retains consumption evidence until the next accepted operation. */
        bool Consumed = false;
    public:
        /** @brief Creates an idle request whose first operation must have a positive identity. */
        RefreshRequest() noexcept = default;
        /** @brief Lock and operation ownership cannot be duplicated. */
        RefreshRequest(const RefreshRequest&) = delete;
        /** @brief Lock and operation ownership cannot be assigned. */
        RefreshRequest& operator=(const RefreshRequest&) = delete;
        /** @brief Arms a new nonzero identity, rejecting nested, stale or invalid requests without replacing them. */
        bool Arm(std::uint64_t operation, std::uintptr_t renderer, std::uintptr_t device,
            DWORD thread, std::uintptr_t anchor) noexcept;
        /** @brief Consumes once only when all observed identities match the active request. */
        bool Consume(std::uintptr_t renderer, std::uintptr_t device, DWORD thread, std::uintptr_t anchor) noexcept;
        /** @brief Clears only the specified owning operation, retaining its consumption evidence. */
        void Disarm(std::uint64_t operation) noexcept;
        /** @brief Returns whether the specified latest operation was consumed, even after disarming. */
        bool WasConsumed(std::uint64_t operation) noexcept;
    };
}
