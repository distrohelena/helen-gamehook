#include <HelenHook/CommandDispatcher.h>
#include <HelenHook/CommandIntPair.h>
#include <HelenHook/JsonConfigStore.h>

#include <atomic>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <thread>
#include <windows.h>

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

    /**
     * @brief Reads all bytes from a persistence fixture for exact rollback assertions.
     * @param path File whose bytes should be loaded.
     * @return Complete file contents, or an empty string when the file cannot be opened.
     */
    std::string ReadAllBytes(const std::filesystem::path& path)
    {
        std::ifstream stream(path, std::ios::binary);
        if (!stream)
        {
            return {};
        }

        return std::string(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
    }

    /**
     * @brief Opens a config file without write/delete sharing so an atomic replacement attempt fails.
     * @param path Config file that should be held open.
     * @return Handle that blocks replacement, or nullptr when the file cannot be opened.
     */
    HANDLE OpenConfigFileDenyingReplacement(const std::filesystem::path& path)
    {
        return CreateFileW(
            path.c_str(),
            GENERIC_READ,
            FILE_SHARE_READ,
            nullptr,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            nullptr);
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
        helen::CommandDispatcher dispatcher;
        dispatcher.RegisterConfigInt("resolutionWidth", 1280);
        dispatcher.RegisterConfigInt("resolutionHeight", 720);

        Expect(
            dispatcher.TrySetIntPair("resolutionWidth", 1920, "resolutionHeight", 1080),
            "Integer pair update failed.");
        Expect(dispatcher.TryGetInt("resolutionWidth") == 1920, "Integer pair did not update its first value.");
        Expect(dispatcher.TryGetInt("resolutionHeight") == 1080, "Integer pair did not update its second value.");
    }

    {
        helen::CommandDispatcher dispatcher;
        dispatcher.RegisterConfigInt("resolutionWidth", 1280);
        dispatcher.RegisterConfigInt("resolutionHeight", 720);

        Expect(
            !dispatcher.TrySetIntPair("missingWidth", 1920, "resolutionHeight", 1080),
            "Pair update unexpectedly accepted a missing first key.");
        Expect(
            !dispatcher.TrySetIntPair("resolutionWidth", 1920, "missingHeight", 1080),
            "Pair update unexpectedly accepted a missing second key.");
        Expect(
            !dispatcher.TrySetIntPair("resolutionWidth", 1920, "resolutionWidth", 1080),
            "Pair update unexpectedly accepted identical keys.");
        Expect(dispatcher.TryGetInt("resolutionWidth") == 1280, "Failed pair update changed the first value.");
        Expect(dispatcher.TryGetInt("resolutionHeight") == 720, "Failed pair update changed the second value.");
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

        initialDispatcher.RegisterConfigInt("resolutionWidth", 1280);
        initialDispatcher.RegisterConfigInt("resolutionHeight", 720);
        Expect(
            initialDispatcher.TrySetIntPair("resolutionWidth", 1920, "resolutionHeight", 1080),
            "Persisted integer pair update failed.");

        helen::JsonConfigStore reloadedStore(path);
        helen::CommandDispatcher reloadedDispatcher(reloadedStore);
        reloadedDispatcher.RegisterConfigInt("ui.subtitleSize", 1);
        Expect(reloadedDispatcher.TryGetInt("ui.subtitleSize") == 3, "Persisted dispatcher value did not reload.");
        reloadedDispatcher.RegisterConfigInt("resolutionWidth", 1280);
        reloadedDispatcher.RegisterConfigInt("resolutionHeight", 720);
        Expect(reloadedDispatcher.TryGetInt("resolutionWidth") == 1920, "Persisted pair first value did not reload.");
        Expect(reloadedDispatcher.TryGetInt("resolutionHeight") == 1080, "Persisted pair second value did not reload.");

        const std::size_t save_count_before_pair = initialStore.GetSaveCount();
        Expect(
            initialDispatcher.TrySetIntPair("resolutionWidth", 3440, "resolutionHeight", 1440),
            "Second persisted integer pair update failed.");
        Expect(
            initialStore.GetSaveCount() == save_count_before_pair + 1,
            "Persisted pair update did not perform exactly one save.");

        std::filesystem::remove_all(root);
    }

    {
        helen::CommandDispatcher dispatcher;
        dispatcher.RegisterConfigInt("resolutionWidth", 1280);
        dispatcher.RegisterConfigInt("resolutionHeight", 720);

        std::atomic<bool> start{ false };
        std::atomic<bool> invalid_snapshot{ false };
        std::thread writer(
            [&dispatcher, &start]()
            {
                while (!start.load(std::memory_order_acquire))
                {
                    std::this_thread::yield();
                }

                for (int iteration = 0; iteration < 100000; ++iteration)
                {
                    const bool first_pair = (iteration % 2) == 0;
                    dispatcher.TrySetIntPair(
                        "resolutionWidth",
                        first_pair ? 1920 : 3440,
                        "resolutionHeight",
                        first_pair ? 1080 : 1440);
                }
            });
        start.store(true, std::memory_order_release);
        for (int iteration = 0; iteration < 100000; ++iteration)
        {
            const std::optional<helen::CommandIntPair> snapshot = dispatcher.TryGetIntPair(
                "resolutionWidth",
                "resolutionHeight");
            if (!snapshot.has_value())
            {
                invalid_snapshot.store(true, std::memory_order_release);
                break;
            }

            const bool valid_pair =
                (snapshot->FirstValue == 1280 && snapshot->SecondValue == 720) ||
                (snapshot->FirstValue == 1920 && snapshot->SecondValue == 1080) ||
                (snapshot->FirstValue == 3440 && snapshot->SecondValue == 1440);
            if (!valid_pair)
            {
                invalid_snapshot.store(true, std::memory_order_release);
                break;
            }
        }
        writer.join();
        Expect(!invalid_snapshot.load(std::memory_order_acquire), "Pair snapshot observed a torn resolution update.");
    }

    {
        const std::filesystem::path root = std::filesystem::temp_directory_path() / "HelenRuntimeTests" / "CommandDispatcherFailure";
        std::filesystem::remove_all(root);
        std::filesystem::create_directories(root);
        const std::filesystem::path path = root / "runtime.json";

        helen::JsonConfigStore store(path);
        helen::CommandDispatcher dispatcher(store);
        dispatcher.RegisterConfigInt("resolutionWidth", 1280);
        dispatcher.RegisterConfigInt("resolutionHeight", 720);
        Expect(dispatcher.TrySetIntPair("resolutionWidth", 1920, "resolutionHeight", 1080), "Failed to persist the original pair.");
        const std::string original_bytes = ReadAllBytes(path);
        const std::size_t save_count_before_failure = store.GetSaveCount();
        const HANDLE blocked_file = OpenConfigFileDenyingReplacement(path);
        Expect(blocked_file != INVALID_HANDLE_VALUE, "Failed to hold the config file for the Save failure test.");

        const bool failure_result = dispatcher.TrySetIntPair("resolutionWidth", 3440, "resolutionHeight", 1440);
        Expect(!failure_result, "Pair update propagated a persistence failure instead of returning false.");
        Expect(dispatcher.TryGetInt("resolutionWidth") == 1920, "Persistence failure changed the dispatcher width.");
        Expect(dispatcher.TryGetInt("resolutionHeight") == 1080, "Persistence failure changed the dispatcher height.");
        Expect(store.GetInt("resolutionWidth", -1) == 1920, "Persistence failure changed the store width.");
        Expect(store.GetInt("resolutionHeight", -1) == 1080, "Persistence failure changed the store height.");
        Expect(ReadAllBytes(path) == original_bytes, "Persistence failure changed the config file bytes.");
        Expect(store.GetSaveCount() == save_count_before_failure + 1, "Persistence failure did not issue one attempted save.");
        Expect(CloseHandle(blocked_file) != FALSE, "Failed to release the blocked config file.");

        Expect(dispatcher.TrySetIntPair("resolutionWidth", 2560, "resolutionHeight", 1440), "Pair update did not recover after persistence failure.");
        Expect(store.GetInt("resolutionWidth", -1) == 2560, "Recovered save retained the failed width.");
        Expect(store.GetInt("resolutionHeight", -1) == 1440, "Recovered save retained the failed height.");
        helen::JsonConfigStore reloaded_store(path);
        Expect(reloaded_store.GetInt("resolutionWidth", -1) == 2560, "Recovered save leaked the failed width to disk.");
        Expect(reloaded_store.GetInt("resolutionHeight", -1) == 1440, "Recovered save leaked the failed height to disk.");

        std::filesystem::remove_all(root);
    }
}
