#pragma once

#include <HelenHook/BatmanDisplayModeService.h>

namespace helen {
    /** @brief Identifies an explicit selection within the transaction's owning session catalog. */
    struct BatmanGraphicsResolutionSelection {
        /** @brief Catalog flavor, required to agree with staged Fullscreen. */
        BatmanDisplayModeCatalogKind Kind;
        /** @brief Index meaningful only within the immutable catalog held by this transaction's session. */
        std::size_t Index;
        /** @brief Requires both identity components rather than inventing a default resolution. */
        BatmanGraphicsResolutionSelection(BatmanDisplayModeCatalogKind kind, std::size_t index) : Kind(kind), Index(index) {
        }
    };
}
