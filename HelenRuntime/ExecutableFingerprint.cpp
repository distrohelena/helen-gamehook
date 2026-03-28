#include <HelenHook/ExecutableFingerprint.h>

#include <windows.h>
#include <wincrypt.h>

#include <array>
#include <fstream>
#include <stdexcept>
#include <string_view>
#include <system_error>
#include <vector>

#pragma comment(lib, "advapi32.lib")

namespace
{
    /**
     * @brief Hexadecimal digits used to materialize lowercase digest text.
     */
    constexpr std::string_view LowerHexDigits = "0123456789abcdef";

    /**
     * @brief Returns whether the supplied path points to an existing regular file.
     * @param file_path Path whose on-disk type should be validated.
     * @return True when the path exists and names a regular file; otherwise false.
     */
    bool IsRegularFile(const std::filesystem::path& file_path)
    {
        std::error_code error_code;
        return std::filesystem::is_regular_file(file_path, error_code) && !error_code;
    }

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
     * @brief Opens one transient Win32 cryptographic provider for SHA-256 hashing.
     * @param provider Receives the opened provider handle on success.
     * @return True when the provider opens successfully; otherwise false.
     */
    bool TryOpenHashProvider(HCRYPTPROV& provider)
    {
        provider = 0;
        return CryptAcquireContextW(&provider, nullptr, nullptr, PROV_RSA_AES, CRYPT_VERIFYCONTEXT) == TRUE;
    }

    /**
     * @brief Opens one SHA-256 hash object from the supplied provider.
     * @param provider Open cryptographic provider handle.
     * @param hash Receives the opened hash handle on success.
     * @return True when the hash object opens successfully; otherwise false.
     */
    bool TryOpenSha256Hash(HCRYPTPROV provider, HCRYPTHASH& hash)
    {
        hash = 0;
        return CryptCreateHash(provider, CALG_SHA_256, 0, 0, &hash) == TRUE;
    }

    /**
     * @brief Finalizes one Win32 SHA-256 hash object and returns its lowercase hexadecimal digest.
     * @param hash Open hash handle whose current digest should be materialized.
     * @param digest Receives the lowercase hexadecimal digest when hashing succeeds.
     * @return True when the digest is produced successfully; otherwise false.
     */
    bool TryFinalizeHash(HCRYPTHASH hash, std::string& digest)
    {
        DWORD hash_length = 0;
        DWORD hash_length_size = sizeof(hash_length);
        if (CryptGetHashParam(hash, HP_HASHSIZE, reinterpret_cast<BYTE*>(&hash_length), &hash_length_size, 0) != TRUE)
        {
            return false;
        }

        std::vector<std::uint8_t> hash_bytes(hash_length, 0);
        DWORD requested_length = hash_length;
        if (CryptGetHashParam(hash, HP_HASHVAL, reinterpret_cast<BYTE*>(hash_bytes.data()), &requested_length, 0) != TRUE)
        {
            return false;
        }

        if (requested_length != hash_length)
        {
            return false;
        }

        digest.clear();
        digest.reserve(hash_bytes.size() * 2);
        for (const std::uint8_t byte : hash_bytes)
        {
            digest.push_back(LowerHexDigits[(byte >> 4) & 0x0F]);
            digest.push_back(LowerHexDigits[byte & 0x0F]);
        }

        return true;
    }

    /**
     * @brief Streams one file through a Win32 SHA-256 hash object.
     * @param file_path Path of the file that should be hashed.
     * @param digest Receives the lowercase SHA-256 digest on success.
     * @return True when the full file is hashed successfully; otherwise false.
     */
    bool TryComputeSha256(const std::filesystem::path& file_path, std::string& digest)
    {
        std::ifstream stream(file_path, std::ios::binary);
        if (!stream)
        {
            return false;
        }

        HCRYPTPROV provider = 0;
        if (!TryOpenHashProvider(provider))
        {
            return false;
        }

        HCRYPTHASH hash = 0;
        if (!TryOpenSha256Hash(provider, hash))
        {
            CryptReleaseContext(provider, 0);
            return false;
        }

        std::array<char, 16 * 1024> buffer{};
        bool succeeded = true;
        while (stream)
        {
            stream.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
            const std::streamsize read_count = stream.gcount();
            if (read_count <= 0)
            {
                break;
            }

            if (CryptHashData(hash, reinterpret_cast<const BYTE*>(buffer.data()), static_cast<DWORD>(read_count), 0) != TRUE)
            {
                succeeded = false;
                break;
            }
        }

        if (stream.bad())
        {
            succeeded = false;
        }

        if (succeeded)
        {
            succeeded = TryFinalizeHash(hash, digest);
        }

        CryptDestroyHash(hash);
        CryptReleaseContext(provider, 0);
        return succeeded;
    }
}

namespace helen
{
    /**
     * @brief Builds one executable fingerprint from the file at the supplied path.
     * @param file_path Absolute or relative file path that should be fingerprinted.
     * @return Fully populated fingerprint containing the file name, byte size, and SHA-256 digest.
     * @throws std::invalid_argument Thrown when the path is empty or not a regular file.
     * @throws std::runtime_error Thrown when the file cannot be hashed completely.
     */
    ExecutableFingerprint ExecutableFingerprint::FromPath(const std::filesystem::path& file_path)
    {
        if (file_path.empty())
        {
            throw std::invalid_argument("Executable fingerprinting requires a non-empty file path.");
        }

        if (!IsRegularFile(file_path))
        {
            throw std::invalid_argument("Executable fingerprinting requires an existing regular file.");
        }

        std::error_code error_code;
        const std::uintmax_t file_size = std::filesystem::file_size(file_path, error_code);
        if (error_code)
        {
            throw std::runtime_error("Executable fingerprinting failed to read the file size.");
        }

        std::string digest;
        if (!TryComputeSha256(file_path, digest))
        {
            throw std::runtime_error("Executable fingerprinting failed to compute the SHA-256 digest.");
        }

        ExecutableFingerprint fingerprint;
        fingerprint.FileName = ToUtf8FileName(file_path);
        fingerprint.FileSize = file_size;
        fingerprint.Sha256 = std::move(digest);
        return fingerprint;
    }
}
