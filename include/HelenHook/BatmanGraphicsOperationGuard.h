#pragma once

#include <atomic>

namespace helen {
    /** @brief Rejects concurrent and reentrant graphics operations without waiting or recursively locking a mutex. */
    class BatmanGraphicsOperationGuard {
    private:
        /** @brief Required process-owned service entry flag released only by the operation that acquired it. */
        std::atomic_flag& Busy;
        /** @brief Whether this entry acquired ownership; a rejected entry must not release someone else's operation. */
        bool Acquired;
    public:
        /** @brief Attempts ownership once; no retry, polling, sleep, or timeout participates. */
        explicit BatmanGraphicsOperationGuard(std::atomic_flag& busy) noexcept
            : Busy(busy), Acquired(!busy.test_and_set(std::memory_order_acquire)) {
        }
        /** @brief Releases ownership on every normal or exceptional exit from an acquired operation. */
        ~BatmanGraphicsOperationGuard() {
            if (Acquired) {
                Busy.clear(std::memory_order_release);
            }
        }
        /** @brief Prevents duplicate guard ownership and double release. */
        BatmanGraphicsOperationGuard(const BatmanGraphicsOperationGuard&) = delete;
        /** @brief Prevents transferring ownership by assignment. */
        BatmanGraphicsOperationGuard& operator=(const BatmanGraphicsOperationGuard&) = delete;
        /** @brief Reports whether this entry may access protected session state. */
        bool IsAcquired() const noexcept { return Acquired; }
    };
}
