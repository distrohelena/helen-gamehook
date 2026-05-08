#include <HelenHook/HiddenPathMatcher.h>

#include <filesystem>
#include <stdexcept>

namespace
{
    /**
     * @brief Throws when one required boolean condition is false so the shared test harness stops at the first failure.
     * @param condition Boolean condition under test.
     * @param message Failure text reported by the shared test runner.
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
 * @brief Verifies that hidden-path matching covers both relative and game-root absolute file API paths.
 */
void RunFileApiHookSetTests()
{
    helen::HiddenPathMatcher matcher(
        std::filesystem::path(L"D:/steam/steamapps/common/Batman Arkham Asylum GOTY"),
        { "bmgame/movies/legal.bik" });

    Expect(
        matcher.ShouldHidePath(std::filesystem::path(L"BmGame/Movies/Legal.bik")),
        "Expected CreateFile-style hidden path match.");
    Expect(
        matcher.ShouldHidePath(std::filesystem::path(L"D:/steam/steamapps/common/Batman Arkham Asylum GOTY/BmGame/Movies/Legal.bik")),
        "Expected GetFileAttributes-style hidden path match.");
}
