#pragma once

#include <bcrypt.h>

/**
 * @brief Owns one BCrypt hash object for verifier operations.
 *
 * Destruction always releases the native hash object, including when hashing or digest
 * finalization reports an error.
 */
class BcryptHashHandle {
    /** @brief Native BCrypt hash released by this owning wrapper; null means no hash is owned. */
    BCRYPT_HASH_HANDLE handle_ = nullptr;

public:
    /** @brief Takes ownership of one BCrypt hash handle. */
    explicit BcryptHashHandle(BCRYPT_HASH_HANDLE handle) noexcept : handle_(handle) {}
    /** @brief Destroys the hash when acquisition succeeded. */
    ~BcryptHashHandle() { if (handle_ != nullptr) { BCryptDestroyHash(handle_); } }
    /** @brief Prevents copying a unique hash handle. */
    BcryptHashHandle(const BcryptHashHandle&) = delete;
    /** @brief Prevents assigning a unique hash handle. */
    BcryptHashHandle& operator=(const BcryptHashHandle&) = delete;
    /** @brief Returns the owned hash handle for BCrypt calls. */
    BCRYPT_HASH_HANDLE Get() const noexcept { return handle_; }
};
