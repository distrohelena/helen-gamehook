#include <HelenHook/HelenRuntimeExports.h>
#include <iostream>
#include <stdexcept>

#pragma comment(linker, "/INCLUDE:__imp__DirectInput8Create@20")
// The real runtime requires an enabled loader-import hook target, as Batman provides.
#pragma comment(linker, "/INCLUDE:__imp__LoadLibraryW@4")
#if defined(HELEN_ENABLE_STARTUP_HOOKS)
/** @brief Imports the real opt-in proxy's attach-window observation. */
extern "C" __declspec(dllimport) DWORD __stdcall HelenQueryStartupThread() noexcept;
#endif
/** @brief Loads the freshly built real runtime/proxy pair in a pack-free host, never Batman. */
int main() {
    SetErrorMode(0x8003);
    try {
        if (HelenInitialize() == FALSE) { throw std::runtime_error("ordinary runtime initialization failed"); }
#if defined(HELEN_ENABLE_STARTUP_HOOKS)
        if (HelenQueryStartupThread() != 0) { throw std::runtime_error("real proxy leaked startup context"); }
        if (HelenInitializeAtStartup(GetModuleHandleW(L"dinput8.dll")) != FALSE) {
            throw std::runtime_error("real runtime reopened startup window");
        }
#else
        if (GetProcAddress(GetModuleHandleW(L"HelenGameHook.dll"), "HelenInitializeAtStartup") != nullptr) {
            throw std::runtime_error("normal control exposed opt-in startup interface");
        }
#endif
        HelenShutdown();
        if (HelenInitialize() == FALSE) { throw std::runtime_error("ordinary idempotent initialization regressed"); }
        std::cout << "REAL_RUNTIME_STARTUP_PASS\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "REAL_RUNTIME_STARTUP_FAIL: " << error.what() << '\n';
        return 1;
    }
}
