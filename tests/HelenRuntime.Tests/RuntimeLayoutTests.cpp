#include <HelenHook/RuntimeLayout.h>

#include <stdexcept>

namespace
{
    /**
     * @brief Throws a runtime error when a condition is false so the test harness can stop on the first failure.
     * @param condition The condition that must evaluate to true for the test to continue.
     * @param message The failure message reported to the console.
     */
    void Expect(bool condition, const char* message)
    {
        if (!condition)
        {
            throw std::runtime_error(message);
        }
    }
}

/**
 * @brief Verifies that the runtime layout derives the expected directories from the runtime module path.
 */
void RunRuntimeLayoutTests()
{
    const helen::RuntimeLayout layout = helen::RuntimeLayout::FromRuntimeModulePath(
        LR"(C:\Games\Batman Arkham Asylum GOTY\Binaries\HelenGameHook.dll)");

    Expect(layout.GameRoot == LR"(C:\Games\Batman Arkham Asylum GOTY\Binaries)", "Game root mismatch.");
    Expect(layout.HelenRoot == LR"(C:\Games\Batman Arkham Asylum GOTY\Binaries\helengamehook)", "Helen root mismatch.");
    Expect(layout.PacksDirectory == LR"(C:\Games\Batman Arkham Asylum GOTY\Binaries\helengamehook\packs)", "Pack directory mismatch.");
    Expect(layout.ConfigDirectory == LR"(C:\Games\Batman Arkham Asylum GOTY\Binaries\helengamehook\config)", "Config directory mismatch.");
    Expect(layout.LogsDirectory == LR"(C:\Games\Batman Arkham Asylum GOTY\Binaries\helengamehook\logs)", "Logs directory mismatch.");
    Expect(layout.AssetsDirectory == LR"(C:\Games\Batman Arkham Asylum GOTY\Binaries\helengamehook\assets)", "Assets directory mismatch.");
    Expect(layout.CacheDirectory == LR"(C:\Games\Batman Arkham Asylum GOTY\Binaries\helengamehook\cache)", "Cache directory mismatch.");
}
