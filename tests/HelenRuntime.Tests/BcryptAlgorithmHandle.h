#pragma once

#include <bcrypt.h>

/**
 * @brief Owns one BCrypt algorithm provider for verifier operations.
 *
 * The wrapper closes a successfully acquired provider on every return path, including
 * property-query and hash-construction failures.
 */
class BcryptAlgorithmHandle {
    /** @brief Native BCrypt provider released by this owning wrapper; null means no provider is owned. */
    BCRYPT_ALG_HANDLE handle_ = nullptr;

public:
    /** @brief Takes ownership of one BCrypt algorithm provider handle. */
    explicit BcryptAlgorithmHandle(BCRYPT_ALG_HANDLE handle) noexcept : handle_(handle) {}
    /** @brief Closes the provider when acquisition succeeded. */
    ~BcryptAlgorithmHandle() { if (handle_ != nullptr) { BCryptCloseAlgorithmProvider(handle_, 0); } }
    /** @brief Prevents copying a unique provider handle. */
    BcryptAlgorithmHandle(const BcryptAlgorithmHandle&) = delete;
    /** @brief Prevents assigning a unique provider handle. */
    BcryptAlgorithmHandle& operator=(const BcryptAlgorithmHandle&) = delete;
    /** @brief Returns the owned provider handle for BCrypt calls. */
    BCRYPT_ALG_HANDLE Get() const noexcept { return handle_; }
};
