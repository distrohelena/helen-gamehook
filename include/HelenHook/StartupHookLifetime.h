#pragma once
#include <windows.h>
#include <atomic>

namespace helen {
    /** @brief One startup installation window; owned by the runtime for process lifetime, not a security boundary. */
    class StartupHookLifetime {
    private:
        /** @brief Prevents retries or reentrant installation after an admitted startup attempt. */
        std::atomic<bool> Attempted{false};
        /** @brief Nonzero only on the thread currently executing the initialization callback. */
        std::atomic<DWORD> InstallationThread{0};
    public:
        /** @brief Creates an idle gate without touching loader state. */
        StartupHookLifetime() noexcept = default;
        /** @brief Installation ownership cannot be duplicated. */
        StartupHookLifetime(const StartupHookLifetime&) = delete;
        /** @brief Installation ownership cannot be transferred by assignment. */
        StartupHookLifetime& operator=(const StartupHookLifetime&) = delete;
        /** @brief Validates a live proxy startup context and pins modules before invoking initialize once.
         * The proxy and callback must belong to cooperating loaded DLLs. Callback must not throw.
         * False preserves refusal/failure; a failed pin may leave earlier pins permanently retained.
         */
        bool Run(HMODULE proxy, bool (__cdecl* initialize)() noexcept) noexcept;
        /** @brief True only inside the admitted callback on its originating startup thread. */
        bool CanInstallNow() const noexcept;
    };
}
