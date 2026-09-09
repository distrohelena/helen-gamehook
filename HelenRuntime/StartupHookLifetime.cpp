#include <HelenHook/StartupHookLifetime.h>
#include <HelenHook/ProcessModulePin.h>

namespace helen {
    bool StartupHookLifetime::Run(HMODULE proxy, bool (__cdecl* initialize)() noexcept) noexcept {
        if (proxy == nullptr || initialize == nullptr) { return false; }
        using QueryStartupThread = DWORD (__stdcall*)() noexcept;
        const QueryStartupThread query = reinterpret_cast<QueryStartupThread>(GetProcAddress(proxy, "HelenQueryStartupThread"));
        if (query == nullptr || query() != GetCurrentThreadId()) { return false; }
        bool expected = false;
        if (!Attempted.compare_exchange_strong(expected, true)) { return false; }
        if (!ProcessModulePin::TryPin(proxy) ||
            !ProcessModulePin::TryPin(reinterpret_cast<const void*>(&ProcessModulePin::TryPin)) ||
            !ProcessModulePin::TryPin(reinterpret_cast<const void*>(initialize))) {
            return false;
        }
        InstallationThread.store(GetCurrentThreadId());
        const bool initialized = initialize();
        InstallationThread.store(0);
        return initialized;
    }
    bool StartupHookLifetime::CanInstallNow() const noexcept {
        return InstallationThread.load() == GetCurrentThreadId();
    }
}
