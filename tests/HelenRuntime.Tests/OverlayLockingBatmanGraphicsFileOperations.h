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

public:
    /**
     * @brief Selects the exact real overlay that must reject the post-publication synchronization copy.
     * @param overlay_path Session-owned overlay path to lock after final publication.
     */
    explicit OverlayLockingBatmanGraphicsFileOperations(std::filesystem::path overlay_path)
        : OverlayPath(std::move(overlay_path))
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
        const bool replaced = helen::BatmanGraphicsFileOperations::Replace(target, staged, error);
        if (replaced && ReplacementCount == 2)
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
};
