#include <HelenHook/FileFingerprint.h>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace
{
    /**
     * @brief Throws when one required boolean condition is false so the shared test harness stops immediately.
     * @param condition Boolean condition under test.
     * @param message Failure message reported by the shared test runner.
     */
    void Expect(bool condition, const char* message)
    {
        if (!condition)
        {
            throw std::runtime_error(message);
        }
    }

    /**
     * @brief Writes one exact binary payload to disk for temporary test assets.
     * @param path Destination file path that should be created or replaced.
     * @param bytes Binary payload written into the file.
     */
    void WriteAllBytes(const std::filesystem::path& path, std::string_view bytes)
    {
        std::ofstream stream(path, std::ios::binary | std::ios::trunc);
        if (!stream)
        {
            throw std::runtime_error("Failed to create a file fingerprint test asset.");
        }

        stream.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
        if (!stream)
        {
            throw std::runtime_error("Failed to write a file fingerprint test asset.");
        }
    }
}

/**
 * @brief Verifies that generic file fingerprinting returns the file size and SHA-256 digest for a known sample file.
 */
void RunFileFingerprintTests()
{
    const std::filesystem::path root = std::filesystem::temp_directory_path() / "HelenRuntimeTests" / "FileFingerprint";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);

    try
    {
        const std::filesystem::path file_path = root / "sample.bin";
        WriteAllBytes(file_path, "ABCDE");

        const helen::FileFingerprint fingerprint = helen::FileFingerprint::FromPath(file_path);
        Expect(fingerprint.FileSize == 5, "File fingerprint size mismatch.");
        Expect(
            fingerprint.Sha256 == "f0393febe8baaa55e32f7be2a7cc180bf34e52137d99e056c817a9c07b8f239a",
            "File fingerprint SHA-256 mismatch.");
    }
    catch (...)
    {
        std::filesystem::remove_all(root);
        throw;
    }

    std::filesystem::remove_all(root);
}
