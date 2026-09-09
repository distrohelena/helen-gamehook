#pragma once
#include "RefreshRequest.h"

namespace helen {
    /** @brief Owns one accepted request in a C++ scope; the request must outlive this nonmovable guard. */
    class RefreshRequestScope {
    private:
        /** @brief Borrowed state whose accepted operation this guard disarms. */
        RefreshRequest& Request;
        /** @brief Required monotonic identity, validated when arming. */
        const std::uint64_t Operation;
        /** @brief True only after this guard successfully acquires the request. */
        bool Armed = false;
    public:
        /** @brief Associates an operation without publishing any engine identity. */
        RefreshRequestScope(RefreshRequest& request, std::uint64_t operation) noexcept;
        /** @brief Releases an accepted request on normal exit or C++ exception unwinding. */
        ~RefreshRequestScope() noexcept;
        /** @brief Unique scope ownership cannot be copied. */
        RefreshRequestScope(const RefreshRequestScope&) = delete;
        /** @brief Unique scope ownership cannot be assigned. */
        RefreshRequestScope& operator=(const RefreshRequestScope&) = delete;
        /** @brief Publishes identities once; refusal never gives this guard cleanup ownership. */
        bool Arm(std::uintptr_t renderer, std::uintptr_t device, DWORD thread, std::uintptr_t anchor) noexcept;
    };
}
