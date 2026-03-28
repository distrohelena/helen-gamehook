#include <HelenHook/ExecutableFingerprint.h>

#include <filesystem>
#include <fstream>
#include <stdexcept>

namespace
{
    /**
     * @brief Throws when a required test assertion is false so the shared test runner stops immediately.
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
 * @brief Verifies that executable fingerprinting returns the file name, size, and SHA-256 digest for a known test file.
 */
void RunExecutableFingerprintTests()
{
    const std::filesystem::path test_directory = std::filesystem::current_path() / "ExecutableFingerprintTests";
    const std::filesystem::path test_file_path = test_directory / "sample.bin";
    std::filesystem::remove_all(test_directory);
    std::filesystem::create_directories(test_directory);

    {
        std::ofstream stream(test_file_path, std::ios::binary);
        stream.write("abc", 3);
    }

    const helen::ExecutableFingerprint fingerprint = helen::ExecutableFingerprint::FromPath(test_file_path);
    Expect(fingerprint.FileName == "sample.bin", "Executable fingerprint file name mismatch.");
    Expect(fingerprint.FileSize == 3, "Executable fingerprint file size mismatch.");
    Expect(
        fingerprint.Sha256 == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
        "Executable fingerprint SHA-256 mismatch.");

    std::filesystem::remove_all(test_directory);
}
