#include <HelenHook/ModuleLoadRoutingHookSet.h>

#include <HelenHook/Log.h>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>

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
 * @brief Runs coverage for loader-hook installation, removal, and Bink request logging.
 */
void RunModuleLoadRoutingHookSetTests()
{
    const std::filesystem::path log_path = std::filesystem::temp_directory_path() / L"helen-module-load-routing-hook-set.log";
    std::error_code remove_error;
    std::filesystem::remove(log_path, remove_error);

    helen::SetLogPath(log_path);

    helen::ModuleLoadRoutingService routing_service({});
    helen::ModuleLoadRoutingHookSet hook_set(routing_service);

    Expect(hook_set.Install(), "Expected the loader hook set to install on the test runner's main module.");
    Expect(hook_set.IsInstalled(), "Expected the loader hook set to report an installed state.");

    {
        const HMODULE module_handle = LoadLibraryW(L"binkw32.dll");
        Expect(module_handle == nullptr, "Expected a missing Bink DLL request to fail after the detour logs it.");
    }

    hook_set.Remove();
    Expect(!hook_set.IsInstalled(), "Expected the loader hook set to report a removed state.");

    std::ifstream stream(log_path, std::ios::binary);
    Expect(static_cast<bool>(stream), "Expected the routing hook test to create a log file.");

    const std::string log_text((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
    Expect(log_text.find("api=LoadLibraryW") != std::string::npos, "Expected the log file to capture the loader API name.");
    Expect(log_text.find("requested=binkw32.dll") != std::string::npos, "Expected the log file to capture the normalized Bink DLL name.");
    Expect(log_text.find("matched=bink alias") != std::string::npos, "Expected the log file to capture the Bink alias classification.");
    Expect(log_text.find("redirect=<none>") != std::string::npos, "Expected the log file to preserve pass-through behavior in the log-only slice.");

    std::error_code cleanup_error;
    std::filesystem::remove(log_path, cleanup_error);
}
