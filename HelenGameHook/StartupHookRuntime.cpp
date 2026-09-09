#include "StartupHookRuntime.h"
#include <HelenHook/StartupHookLifetime.h>
#include <HelenHook/Log.h>
#if defined(HELEN_ENABLE_SAME_SIZE_REFRESH)
#include "SameSizeRefresh.h"
#endif

/** @brief Existing runtime initializer; the startup adapter does not change ordinary late-call behavior. */
extern "C" BOOL __stdcall HelenInitialize();

namespace {
    /** @brief Process-lifetime owner retained with its pinned runtime module. */
    helen::StartupHookLifetime StartupLifetime;
    /** @brief Contains initializer exceptions at the no-throw DLL startup handshake boundary. */
    bool __cdecl InitializeRuntime() noexcept {
        try {
            if (HelenInitialize() == FALSE) { return false; }
#if defined(HELEN_ENABLE_SAME_SIZE_REFRESH)
            try { helen::SameSizeRefresh::InstallAtStartup(); }
            catch (const std::exception& error) {
                helen::Logf(L"[same-size-refresh] UNAVAILABLE: %hs", error.what());
            }
#endif
            return true;
        }
        catch (...) {
            OutputDebugStringW(L"Helen startup initialization threw; startup-only hooks are unavailable.\n");
            return false;
        }
    }
}

namespace helen {
    bool CanInstallStartupHooks() noexcept { return StartupLifetime.CanInstallNow(); }
}

/** @brief Opens a verified proxy-startup window once; failed or late attempts never authorize new hooks. */
extern "C" __declspec(dllexport) BOOL __stdcall HelenInitializeAtStartup(HMODULE proxy) {
    return StartupLifetime.Run(proxy, InitializeRuntime) ? TRUE : FALSE;
}
#pragma comment(linker, "/EXPORT:HelenInitializeAtStartup=_HelenInitializeAtStartup@4")
