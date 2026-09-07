#pragma once

#include <HelenHook/BatmanGraphicsFileOperations.h>

#include <filesystem>
#include <utility>

#include <windows.h>

/**
 * @brief Holds a real session overlay handle after the second original publication so synchronization fails at the filesystem boundary.
 */
class OverlayLockingBatmanGraphicsFileOperations final : public helen::BatmanGraphicsFileOperations
{
private:
    /** @brief Session overlay intentionally held without sharing after the dual-file writer publishes both originals. */
    std::filesystem::path OverlayPath;
    /** @brief Number of real original publication attempts completed by the operation boundary. */
    mutable unsigned ReplacementCount = 0;
    /** @brief Native handle that makes the overlay copy fail with a sharing violation. */
    mutable HANDLE OverlayLock = INVALID_HANDLE_VALUE;
    /** @brief Whether the second original publication must fail after the first target attempt. */
    bool FailSecondPublication = false;
    /** @brief Whether recovery movement must fail after a deliberately partial publication. */
    bool FailRecovery = false;
    /** @brief Whether the overlay should be locked after the first rather than final publication. */
    bool LockAfterFirstPublication = false;

public:
    /**
     * @brief Selects the exact real overlay that must reject the post-publication synchronization copy.
     * @param overlay_path Session-owned overlay path to lock after final publication.
     */
    explicit OverlayLockingBatmanGraphicsFileOperations(std::filesystem::path overlay_path,
        bool fail_second_publication = false,
        bool fail_recovery = false,
        bool lock_after_first_publication = false)
        : OverlayPath(std::move(overlay_path)),
          FailSecondPublication(fail_second_publication),
          FailRecovery(fail_recovery),
          LockAfterFirstPublication(lock_after_first_publication)
    {
    }

    /** @brief Releases the test-owned exclusive overlay handle after the child scenario completes. */
    ~OverlayLockingBatmanGraphicsFileOperations() override
    {
        if (OverlayLock != INVALID_HANDLE_VALUE)
        {
            CloseHandle(OverlayLock);
        }
    }

    /**
     * @brief Performs the real replacement and then locks the selected overlay after the second target commits.
     * @param target Original target path.
     * @param staged Real transaction stage path.
     * @param error Receives the Win32 replacement error.
     * @return True when the original replacement completed.
     */
    bool Replace(const std::filesystem::path& target, const std::filesystem::path& staged, unsigned long& error) const override
    {
        ++ReplacementCount;
        if (FailSecondPublication && ReplacementCount == 2)
        {
            error = ERROR_ACCESS_DENIED;
            return false;
        }

        const bool replaced = helen::BatmanGraphicsFileOperations::Replace(target, staged, error);
        const unsigned lock_publication = LockAfterFirstPublication ? 1u : 2u;
        if (replaced && ReplacementCount == lock_publication)
        {
            OverlayLock = CreateFileW(
                OverlayPath.c_str(),
                GENERIC_READ,
                0,
                nullptr,
                OPEN_EXISTING,
                FILE_ATTRIBUTE_NORMAL,
                nullptr);
        }
        return replaced;
    }

    /**
     * @brief Optionally prevents recovery movement while preserving the real file-operation boundary.
     * @param target Original target path.
     * @param candidate Recovery candidate path.
     * @param error Receives the Win32 recovery error.
     * @return True when the native recovery move completed.
     */
    bool Restore(const std::filesystem::path& target, const std::filesystem::path& candidate, unsigned long& error) const override
    {
        if (FailRecovery)
        {
            error = ERROR_ACCESS_DENIED;
            return false;
        }

        return helen::BatmanGraphicsFileOperations::Restore(target, candidate, error);
    }
};
