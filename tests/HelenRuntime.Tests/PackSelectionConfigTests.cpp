#include <HelenHook/PackSelectionConfig.h>

#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string_view>

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

    /**
     * @brief Writes one exact UTF-8 payload to disk for pack-selection config scenarios.
     * @param path Destination file path that should be created or replaced.
     * @param text Exact JSON payload written into the file.
     */
    void WriteAllText(const std::filesystem::path& path, std::string_view text)
    {
        std::ofstream stream(path, std::ios::binary | std::ios::trunc);
        if (!stream)
        {
            throw std::runtime_error("Failed to create a pack-selection config test file.");
        }

        stream.write(text.data(), static_cast<std::streamsize>(text.size()));
        if (!stream)
        {
            throw std::runtime_error("Failed to write a pack-selection config test file.");
        }
    }
}

/**
 * @brief Verifies that explicit pack-selection config preserves order and rejects duplicate enabled pack ids.
 */
void RunPackSelectionConfigTests()
{
    const std::filesystem::path root = std::filesystem::temp_directory_path() / "HelenRuntimeTests" / "PackSelectionConfig";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);

    try
    {
        const std::filesystem::path valid_path = root / "packs.json";
        WriteAllText(
            valid_path,
            R"({
  "enabledPacksByExecutable": {
    "ShippingPC-BmGame.exe": [
      "batman-aa-subtitles",
      "batman-aa-skip-videos"
    ]
  }
})");

        const helen::PackSelectionConfig valid_config(valid_path);
        const std::optional<std::vector<std::string>> enabled_packs = valid_config.TryGetEnabledPacks("ShippingPC-BmGame.exe");
        Expect(enabled_packs.has_value(), "Expected Batman enabled-pack list to exist.");
        Expect(enabled_packs->size() == 2, "Batman enabled-pack list size mismatch.");
        Expect((*enabled_packs)[0] == "batman-aa-subtitles", "Batman enabled-pack ordering mismatch for subtitles.");
        Expect((*enabled_packs)[1] == "batman-aa-skip-videos", "Batman enabled-pack ordering mismatch for skip videos.");
        Expect(!valid_config.TryGetEnabledPacks("MissingGame.exe").has_value(), "Unexpected enabled-pack list for an unknown executable.");

        const std::filesystem::path duplicate_path = root / "duplicate-packs.json";
        WriteAllText(
            duplicate_path,
            R"({
  "enabledPacksByExecutable": {
    "ShippingPC-BmGame.exe": [
      "batman-aa-subtitles",
      "batman-aa-subtitles"
    ]
  }
})");

        bool duplicate_threw = false;
        try
        {
            const helen::PackSelectionConfig duplicate_config(duplicate_path);
            (void)duplicate_config;
        }
        catch (const std::exception&)
        {
            duplicate_threw = true;
        }

        Expect(duplicate_threw, "Duplicate enabled-pack ids unexpectedly loaded.");
    }
    catch (...)
    {
        std::filesystem::remove_all(root);
        throw;
    }

    std::filesystem::remove_all(root);
}
