#include <HelenHook/ExecutableFingerprint.h>
#include <HelenHook/FileFingerprint.h>

namespace helen
{
    /**
     * @brief Converts one filesystem path leaf name into UTF-8 for fingerprint storage.
     * @param file_path Path whose filename should be converted.
     * @return UTF-8 file name text.
     */
    std::string ToUtf8FileName(const std::filesystem::path& file_path)
    {
        return file_path.filename().string();
    }

    /**
     * @brief Builds one executable fingerprint from the file at the supplied path.
     * @param file_path Absolute or relative file path that should be fingerprinted.
     * @return Fully populated fingerprint containing the file name, byte size, and SHA-256 digest.
     * @throws std::invalid_argument Thrown when the path is empty or not a regular file.
     * @throws std::runtime_error Thrown when the file cannot be hashed completely.
     */
    ExecutableFingerprint ExecutableFingerprint::FromPath(const std::filesystem::path& file_path)
    {
        const FileFingerprint file_fingerprint = FileFingerprint::FromPath(file_path);
        ExecutableFingerprint fingerprint;
        fingerprint.FileName = ToUtf8FileName(file_path);
        fingerprint.FileSize = file_fingerprint.FileSize;
        fingerprint.Sha256 = file_fingerprint.Sha256;
        return fingerprint;
    }
}
