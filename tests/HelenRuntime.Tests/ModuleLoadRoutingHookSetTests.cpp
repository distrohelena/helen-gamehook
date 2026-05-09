#include <HelenHook/ModuleLoadRoutingHookSet.h>

#include <HelenHook/Log.h>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <initializer_list>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>

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

    /**
     * @brief Returns true when one log line contains every requested token.
     * @param log_text Full log file text to search.
     * @param tokens Required substrings that must appear on the same line.
     * @return True when one line contains every token.
     */
    bool ContainsLogLineWithTokens(std::string_view log_text, std::initializer_list<std::string_view> tokens)
    {
        std::istringstream stream{std::string(log_text)};
        std::string line;
        while (std::getline(stream, line))
        {
            bool matches = true;
            for (const std::string_view token : tokens)
            {
                if (line.find(token) == std::string::npos)
                {
                    matches = false;
                    break;
                }
            }

            if (matches)
            {
                return true;
            }
        }

        return false;
    }
}

/**
 * @brief Runs coverage for loader-hook installation, removal, and runtime flag selection for the Bink request logs.
 */
void RunModuleLoadRoutingHookSetTests()
{
    const std::filesystem::path log_path = std::filesystem::temp_directory_path() / L"helen-module-load-routing-hook-set.log";
    std::error_code remove_error;
    std::filesystem::remove(log_path, remove_error);

    helen::SetLogPath(log_path);

    helen::ModuleLoadRoutingService routing_service({});
    {
        helen::ModuleLoadRoutingHookSet hook_set(routing_service, true, false);

        Expect(hook_set.Install(), "Expected the loader hook set to install with only the LoadLibrary routing flag enabled.");
        Expect(hook_set.IsInstalled(), "Expected the loader hook set to report an installed state with only the LoadLibrary routing flag enabled.");

        {
            const HMODULE module_handle = LoadLibraryW(L"binkw32.dll");
            Expect(module_handle == nullptr, "Expected a missing Bink DLL request to fail after the LoadLibrary detour logs it.");
        }

        hook_set.Remove();
        Expect(!hook_set.IsInstalled(), "Expected the loader hook set to report a removed state after the LoadLibrary-only run.");

        std::ifstream stream(log_path, std::ios::binary);
        Expect(static_cast<bool>(stream), "Expected the LoadLibrary-only routing hook test to create a log file.");

        const std::string log_text((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
        Expect(ContainsLogLineWithTokens(log_text, { "api=LoadLibraryW", "requested=binkw32.dll", "matched=bink alias" }), "Expected the LoadLibrary-only log to capture the normalized Bink DLL name.");
        Expect(!ContainsLogLineWithTokens(log_text, { "api=LdrLoadDll", "requested=binkw32.dll" }), "Expected the deeper loader hook to stay disabled when only the LoadLibrary routing flag is enabled.");
    }

    {
        std::error_code clear_error;
        std::filesystem::remove(log_path, clear_error);

        helen::ModuleLoadRoutingHookSet hook_set(routing_service, false, true);

        Expect(hook_set.Install(), "Expected the loader hook set to install with only the LdrLoadDll routing flag enabled.");
        Expect(hook_set.IsInstalled(), "Expected the loader hook set to report an installed state with only the LdrLoadDll routing flag enabled.");

        {
            const HMODULE module_handle = LoadLibraryW(L"binkw32.dll");
            Expect(module_handle == nullptr, "Expected a missing Bink DLL request to fail after the LdrLoadDll detour logs it.");
        }

        hook_set.Remove();
        Expect(!hook_set.IsInstalled(), "Expected the loader hook set to report a removed state after the LdrLoadDll-only run.");

        std::ifstream stream(log_path, std::ios::binary);
        Expect(static_cast<bool>(stream), "Expected the LdrLoadDll-only routing hook test to create a log file.");

        const std::string log_text((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
        Expect(ContainsLogLineWithTokens(log_text, { "api=LdrLoadDll", "requested=binkw32.dll", "matched=bink alias" }), "Expected the LdrLoadDll-only log to capture the normalized Bink DLL name.");
    }

    {
        helen::ModuleLoadRoutingHookSet hook_set(routing_service, false, false);
        Expect(!hook_set.Install(), "Expected the loader hook set to reject a configuration that disables every loader path.");
        Expect(!hook_set.IsInstalled(), "Expected the loader hook set to remain uninstalled when every loader path is disabled.");
    }

    std::error_code cleanup_error;
    std::filesystem::remove(log_path, cleanup_error);
}
