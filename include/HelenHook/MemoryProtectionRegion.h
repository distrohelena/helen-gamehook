#pragma once
#include <windows.h>

namespace helen {
/** @brief Records one homogeneous committed region's original protection for recovery. */
struct MemoryProtectionRegion {
    /** @brief Start of the portion covered by this patch, valid until cleanup completes. */
    void* Address;
    /** @brief Nonzero byte count, confined to one VirtualQuery region. */
    SIZE_T Size;
    /** @brief Original protection, never replaced by temporary writable permissions. */
    DWORD Protection;
    /** @brief True while a successful temporary protection change has not been restored. */
    bool Changed;
};
}
