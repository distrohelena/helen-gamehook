#include <HelenHook/JsonConfigStore.h>

#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string_view>
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
     * @brief Throws when two integer values differ so the test harness stops at the first failed assertion.
     * @param actual Value produced by the code under test.
     * @param expected Value required by the scenario.
     * @param message Failure text reported to stderr by the shared test main.
     */
    void ExpectEqual(int actual, int expected, const char* message)
    {
        if (actual != expected)
        {
            throw std::runtime_error(message);
        }
    }

    /**
     * @brief Writes raw test content to a file path, replacing any previous contents.
     * @param path Target file path used by the test.
     * @param text Exact text content that should be written.
     */
    void WriteAllText(const std::filesystem::path& path, std::string_view text)
    {
        std::ofstream stream(path, std::ios::binary | std::ios::trunc);
        if (!stream)
        {
            throw std::runtime_error("Failed to write test file.");
        }

        stream.write(text.data(), static_cast<std::streamsize>(text.size()));
        if (!stream)
        {
            throw std::runtime_error("Failed to write test file.");
        }
    }
}

/**
 * @brief Verifies that integer config entries persist to disk, reject malformed data, and preserve the live file on failed replacement.
 */
void RunJsonConfigStoreTests()
{
    const std::filesystem::path root = std::filesystem::temp_directory_path() / "HelenRuntimeTests" / "JsonConfigStore";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);

    {
        const std::filesystem::path path = root / "runtime.json";
        helen::JsonConfigStore store(path);
        store.SetInt("ui.subtitleSize", 2);
        store.Save();

        const helen::JsonConfigStore reloaded(path);
        ExpectEqual(reloaded.GetInt("ui.subtitleSize", 0), 2, "Config value did not persist.");
    }

    {
        const std::filesystem::path path = root / "locked-runtime.json";
        WriteAllText(path, "{\n  \"ui.subtitleSize\": 2\n}\n");

        HANDLE lockedHandle = CreateFileW(
            path.c_str(),
            GENERIC_READ,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            nullptr,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            nullptr);
        Expect(lockedHandle != INVALID_HANDLE_VALUE, "Failed to lock the live config file.");

        bool saveThrew = false;
        try
        {
            helen::JsonConfigStore store(path);
            store.SetInt("ui.subtitleSize", 4);
            store.Save();
        }
        catch (const std::exception&)
        {
            saveThrew = true;
        }

        CloseHandle(lockedHandle);

        Expect(saveThrew, "Save unexpectedly replaced a locked live config file.");
        const helen::JsonConfigStore reloaded(path);
        ExpectEqual(reloaded.GetInt("ui.subtitleSize", 0), 2, "Failed save changed the live config file.");
    }

    {
        const std::filesystem::path path = root / "malformed-runtime.json";
        WriteAllText(path, "{");

        bool loadThrew = false;
        try
        {
            const helen::JsonConfigStore store(path);
            (void)store;
        }
        catch (const std::exception&)
        {
            loadThrew = true;
        }

        Expect(loadThrew, "Malformed JSON did not fail during load.");
    }

    std::filesystem::remove_all(root);
}
