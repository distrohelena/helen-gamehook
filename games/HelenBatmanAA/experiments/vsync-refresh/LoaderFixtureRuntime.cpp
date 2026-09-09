#include <HelenHook/StartupHookLifetime.h>
#include <HelenHook/ProcessModulePin.h>

namespace {
    /** @brief Actual production gate used by the DLL loader fixture. */
    helen::StartupHookLifetime Lifetime;
    /** @brief Counts admitted initialization callbacks, not attempted exports. */
    unsigned Calls = 0;
    /** @brief Records whether installation was authorized inside the admitted callback. */
    bool AllowedInside = false;
    /** @brief Records the most recent explicit startup result for failure-path assertions. */
    bool LastResult = false;
    /** @brief Proxy supplied by the real attach entry for reentrant and pin-failure checks. */
    HMODULE ActiveProxy = nullptr;
    /** @brief Records refusal of a second installation attempt inside the original startup window. */
    bool ReentryRejected = false;
    /** @brief Records that invalid callback code cannot open a window when pinning fails. */
    bool InvalidPinRejected = false;
    /** @brief A forbidden second initializer, whose execution changes the observable callback count. */
    bool __cdecl Reentered() noexcept { ++Calls; return true; }
    /** @brief Stands in only for the heavy runtime initializer; gate and pin implementations are real. */
    bool __cdecl Initialize() noexcept {
        ++Calls;
        AllowedInside = Lifetime.CanInstallNow();
        ReentryRejected = !Lifetime.Run(ActiveProxy, Reentered);
        helen::StartupHookLifetime failedPin;
        InvalidPinRejected = !failedPin.Run(ActiveProxy, reinterpret_cast<bool (__cdecl*)() noexcept>(1)) &&
            !failedPin.CanInstallNow();
#if defined(FIXTURE_FAIL_INIT)
        return false;
#else
        return true;
#endif
    }
}
/** @brief Exercises the startup handshake without loading packs, engine code or renderer resources. */
extern "C" __declspec(dllexport) BOOL __stdcall HelenInitializeAtStartup(HMODULE proxy) {
    ActiveProxy = proxy;
    LastResult = Lifetime.Run(proxy, Initialize);
    return LastResult ? TRUE : FALSE;
}
/** @brief Ordinary initialization has no installation permission in the fixture. */
extern "C" __declspec(dllexport) BOOL __stdcall HelenInitialize() { return TRUE; }
/** @brief Keeps the fixture's proxy shutdown interface compatible without touching game state. */
extern "C" __declspec(dllexport) void __stdcall HelenShutdown() {}
/** @brief Exposes callback count and permission evidence, including whether the window leaked. */
extern "C" __declspec(dllexport) unsigned __stdcall FixtureReadState() {
    return Calls | (AllowedInside ? 0x100u : 0u) | (Lifetime.CanInstallNow() ? 0x200u : 0u) |
        (LastResult ? 0x400u : 0u) | (ReentryRejected ? 0x800u : 0u) | (InvalidPinRejected ? 0x1000u : 0u);
}
/** @brief Independently exercises real pinning on a dynamically loaded fixture module. */
extern "C" __declspec(dllexport) BOOL __stdcall FixturePin() {
    return helen::ProcessModulePin::TryPin(reinterpret_cast<const void*>(&FixturePin)) ? TRUE : FALSE;
}
#pragma comment(linker, "/EXPORT:HelenInitializeAtStartup=_HelenInitializeAtStartup@4")
#pragma comment(linker, "/EXPORT:FixtureReadState=_FixtureReadState@0")
#pragma comment(linker, "/EXPORT:FixturePin=_FixturePin@0")
/** @brief Suppresses process error dialogs before any startup fixture checks can fail. */
BOOL WINAPI DllMain(HMODULE, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) { SetErrorMode(0x8003); }
    return TRUE;
}
