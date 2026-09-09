#pragma once
#include <HelenHook/MemoryPatchResult.h>
#include <HelenHook/MemoryProtectionRegion.h>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace helen {
/** @brief Stops before unresolved patch ownership or live dependencies can be discarded. */
[[noreturn]] void AbortUnsafePatch() noexcept;

/**
 * @brief Owns original bytes and page protections until explicit commit or confirmed restoration.
 * Callers must exclude concurrent execution/writes/unmapping and provide readable source storage.
 * This class does not relocate instructions, suspend threads or recover access violations.
 */
class MemoryPatch {
private:
    /** @brief Borrowed destination; non-null represents a cleanup obligation, not success. */
    void* Address = nullptr;
    /** @brief Snapshot allocated and populated before any target protection changes. */
    std::vector<std::uint8_t> OriginalBytes;
    /** @brief Original region protections retained across failed restoration attempts. */
    std::vector<MemoryProtectionRegion> Regions;
    /** @brief Outcome of the most recent apply or restoration attempt. */
    MemoryPatchResult LastResult;
    /** @brief True only after a fully successful application, permitting permanent commit. */
    bool Applied = false;

public:
    /** @brief Creates an empty owner that has no target-memory side effects. */
    MemoryPatch() = default;
    /** @brief Restores owned memory; terminates if cleanup cannot be confirmed. */
    ~MemoryPatch();
    /** @brief Patch ownership must not be copied or implicitly duplicated. */
    MemoryPatch(const MemoryPatch&) = delete;
    /** @brief Patch ownership must not be overwritten by assignment. */
    MemoryPatch& operator=(const MemoryPatch&) = delete;
    /** @brief Reports whether callers must retain this owner and all patch dependencies. */
    bool HasOwnership() const noexcept;
    /** @brief Returns separate stage errors for the last operation. */
    const MemoryPatchResult& Result() const noexcept;
    /** @brief Captures and applies bytes; failure may retain ownership requiring Restore. */
    MemoryPatchResult Apply(void* address, const void* data, std::size_t size);
    /** @brief Restores the initial bytes and protections; failure retains all recovery state. */
    MemoryPatchResult Restore() noexcept;
    /** @brief Makes a fully successful write permanent, transferring lifetime responsibility to caller. */
    bool Commit() noexcept;

private:
    /** @brief Writes under captured protections without allocation, preserving every cleanup error. */
    MemoryPatchResult Write(const void* data) noexcept;
    /** @brief Restores all changed regions, including after a write-access failure. */
    void RestoreProtections(MemoryPatchResult& result) noexcept;
    /** @brief Forgets snapshots only when their cleanup obligation has been discharged. */
    void Clear() noexcept;
};
}
