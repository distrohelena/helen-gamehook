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
 * @brief Verifies that hidden-path matching normalizes absolute and relative paths consistently.
 */
void RunHiddenPathMatcherTests()
{
    helen::HiddenPathMatcher matcher(
        std::filesystem::path(L"D:/steam/steamapps/common/Batman Arkham Asylum GOTY"),
        {
            "bmgame/movies/legal.bik",
            "bmgame/movies/nvidia.bik"
        });

    Expect(
        matcher.ShouldHidePath(std::filesystem::path(L"D:\\steam\\steamapps\\common\\Batman Arkham Asylum GOTY\\BmGame\\Movies\\Legal.bik")),
        "Expected absolute Legal.bik path to match.");
    Expect(
        matcher.ShouldHidePath(std::filesystem::path(L"BmGame/Movies/nvidia.bik")),
        "Expected relative nvidia.bik path to match.");
    Expect(
        !matcher.ShouldHidePath(std::filesystem::path(L"BmGame/Movies/Intro.bik")),
        "Unexpected non-hidden movie match.");
}
