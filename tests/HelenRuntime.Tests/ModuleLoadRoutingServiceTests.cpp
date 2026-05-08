#include <HelenHook/ModuleLoadRoutingService.h>

#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
    /**
     * @brief Fails the test run immediately when the condition is false.
     * @param condition Boolean condition under test.
     * @param message Failure text reported to the console harness.
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
 * @brief Runs coverage for module-name normalization, alias matching, and redirect metadata validation.
 */
void RunModuleLoadRoutingServiceTests()
{
    {
        const std::vector<helen::ModuleRedirectDefinition> redirects = {
            { L"binkw32.dll", std::filesystem::path(L"helengamehook/deps/bink/binkw32.dll") }
        };

        helen::ModuleLoadRoutingService service(redirects);
        const helen::ModuleLoadRoutingDecision decision = service.DescribeRequest(L"LoadLibraryW", L"C:\\Game\\BINKW32.DLL");

        Expect(decision.IsBinkAlias, "Expected Bink DLL requests to be classified as aliases.");
        Expect(decision.RequestedModuleName == L"binkw32.dll", "Expected module-name normalization to strip the path and lower-case the leaf name.");
        Expect(!decision.RedirectTargetPath.has_value(), "Expected the log-only slice to preserve pass-through behavior.");

        const std::wstring log_line = service.BuildLogMessage(decision);
        Expect(log_line.find(L"api=LoadLibraryW") != std::wstring::npos, "Expected the loader API in the log line.");
        Expect(log_line.find(L"requested=binkw32.dll") != std::wstring::npos, "Expected the normalized module name in the log line.");
        Expect(log_line.find(L"matched=bink alias") != std::wstring::npos, "Expected the alias classification in the log line.");
    }

    {
        bool duplicate_threw = false;
        try
        {
            const std::vector<helen::ModuleRedirectDefinition> duplicate_redirects = {
                { L"binkw32.dll", std::filesystem::path(L"helengamehook/deps/bink/binkw32.dll") },
                { L"BINKW32.DLL", std::filesystem::path(L"helengamehook/deps/bink/alt.dll") }
            };

            helen::ModuleLoadRoutingService service(duplicate_redirects);
            (void)service;
        }
        catch (const std::exception&)
        {
            duplicate_threw = true;
        }

        Expect(duplicate_threw, "Expected duplicate redirect names to fail fast.");
    }
}
