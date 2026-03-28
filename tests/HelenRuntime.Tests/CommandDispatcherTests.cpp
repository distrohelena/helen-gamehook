#include <HelenHook/CommandDispatcher.h>
#include <HelenHook/JsonConfigStore.h>

#include <filesystem>
#include <stdexcept>

namespace
{
    /**
     * @brief Throws when a required condition is false so the test harness stops at the first failed assertion.
     * @param condition Boolean condition under test.
     * @param message Failure text reported to stderr by the shared test main.
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
 * @brief Verifies that registered integer config keys expose defaults, accept updates, reject unknown keys, and persist through a JSON config store.
 */
void RunCommandDispatcherTests()
{
    {
        helen::CommandDispatcher dispatcher;
        dispatcher.RegisterConfigInt("ui.subtitleSize", 1);

        Expect(dispatcher.TryGetInt("ui.subtitleSize") == 1, "Default value was not registered.");
        Expect(dispatcher.TrySetInt("ui.subtitleSize", 2), "SetInt failed.");
        Expect(dispatcher.TryGetInt("ui.subtitleSize") == 2, "GetInt returned the wrong value.");
        Expect(!dispatcher.TrySetInt("missing.key", 2), "Unknown key unexpectedly succeeded.");
        Expect(!dispatcher.TryGetInt("missing.key").has_value(), "Unknown key unexpectedly returned a value.");
    }

    {
        const std::filesystem::path root = std::filesystem::temp_directory_path() / "HelenRuntimeTests" / "CommandDispatcher";
        std::filesystem::remove_all(root);
        std::filesystem::create_directories(root);
        const std::filesystem::path path = root / "runtime.json";

        helen::JsonConfigStore initialStore(path);
        helen::CommandDispatcher initialDispatcher(initialStore);
        initialDispatcher.RegisterConfigInt("ui.subtitleSize", 1);
        Expect(initialDispatcher.TrySetInt("ui.subtitleSize", 3), "Persisted SetInt failed.");

        helen::JsonConfigStore reloadedStore(path);
        helen::CommandDispatcher reloadedDispatcher(reloadedStore);
        reloadedDispatcher.RegisterConfigInt("ui.subtitleSize", 1);
        Expect(reloadedDispatcher.TryGetInt("ui.subtitleSize") == 3, "Persisted dispatcher value did not reload.");

        std::filesystem::remove_all(root);
    }
}
