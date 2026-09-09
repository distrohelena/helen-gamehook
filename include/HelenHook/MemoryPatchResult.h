#pragma once
#include <windows.h>

namespace helen {
/** @brief Reports one patch attempt without equating failure with unchanged memory. */
struct MemoryPatchResult {
    /** @brief True only when the requested bytes, cache flush and protections all completed. */
    bool Completed = false;
    /** @brief True when this attempt copied bytes, even if a later operation failed. */
    bool BytesWritten = false;
    /** @brief Validation or write-access Win32 error; zero means that stage succeeded. */
    DWORD AccessError = ERROR_SUCCESS;
    /** @brief Instruction-cache Win32 error, independent of protection cleanup. */
    DWORD FlushError = ERROR_SUCCESS;
    /** @brief First protection-restoration error; all changed regions are still attempted. */
    DWORD ProtectionError = ERROR_SUCCESS;
};
}
