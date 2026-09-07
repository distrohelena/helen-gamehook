#pragma once

#include <windows.h>

/**
 * @brief Temporarily selects OEM file-API decoding for ANSI routing tests.
 *
 * The guard changes process-wide Win32 file-API mode only when the fixture starts in ANSI mode,
 * then restores the original mode before another test can observe it.
 */
class FileApiCodePageGuard
{
public:
    /** @brief Selects OEM decoding when the process currently uses ANSI file APIs. */
    FileApiCodePageGuard()
        : restore_ansi_(AreFileApisANSI() != FALSE)
    {
        if (restore_ansi_)
        {
            SetFileApisToOEM();
        }
    }

    /** @brief Restores ANSI file APIs when this guard changed the process mode. */
    ~FileApiCodePageGuard()
    {
        if (restore_ansi_)
        {
            SetFileApisToANSI();
        }
    }

    /** @brief Prevents copying process-wide code-page ownership into another guard. */
    FileApiCodePageGuard(const FileApiCodePageGuard&) = delete;
    /** @brief Prevents assigning process-wide code-page restoration to another guard. */
    FileApiCodePageGuard& operator=(const FileApiCodePageGuard&) = delete;

private:
    /** @brief True when destruction must restore the pre-fixture ANSI file-API mode. */
    bool restore_ansi_;
};
