#pragma once
#include <windows.h>
#include <atomic>

namespace helen {
    /** @brief Publishes the real static-attach thread only for the lexical duration of proxy DllMain initialization. */
    class ProxyStartupScope {
    private:
        /** @brief Required proxy-owned storage, initially zero and queried by the runtime handshake. */
        std::atomic<DWORD>& Thread;
    public:
        /** @brief Publishes only for Windows static startup; dynamic-load reserved=nullptr stays unavailable. */
        ProxyStartupScope(std::atomic<DWORD>& thread, const void* reserved) noexcept : Thread(thread) {
            Thread.store(reserved != nullptr ? GetCurrentThreadId() : 0);
        }
        /** @brief Closes the installation window before the proxy's attach callback returns. */
        ~ProxyStartupScope() noexcept { Thread.store(0); }
        /** @brief Lexical attach ownership cannot be copied. */
        ProxyStartupScope(const ProxyStartupScope&) = delete;
        /** @brief Lexical attach ownership cannot be assigned. */
        ProxyStartupScope& operator=(const ProxyStartupScope&) = delete;
    };
}
