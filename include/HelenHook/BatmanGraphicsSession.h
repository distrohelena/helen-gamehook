#pragma once

#include <HelenHook/BatmanGraphicsSnapshot.h>
#include <HelenHook/BatmanDisplayCatalog.h>
#include <HelenHook/BatmanGraphicsTransaction.h>

namespace helen {
    /** @brief Owns one graphics editor's baseline, immutable catalogs and optional uncommitted transaction. */
    struct BatmanGraphicsSession {
        /** @brief Non-reused session identity independent of borrowed movie addresses. */
        std::uint32_t Id;
        /** @brief Captured settings for read transfer; successful commit replaces them with the new baseline. */
        BatmanGraphicsSnapshot Snapshot;
        /** @brief Complete baseline if parsing succeeded for every required setting. */
        std::optional<BatmanGraphicsDraftState> Baseline;
        /** @brief Owned windowed ordering, absent when its capture genuinely failed. */
        std::optional<BatmanDisplayCatalog> Windowed;
        /** @brief Owned fullscreen ordering, absent when its capture genuinely failed. */
        std::optional<BatmanDisplayCatalog> Fullscreen;
        /** @brief At most one staged transaction; it is consumed before commit starts. */
        std::optional<BatmanGraphicsTransaction> Transaction;
        /** @brief Whether primitive read getters may still transfer this session's captured state. */
        bool ReadOpen = true;
        /** @brief Takes required capture outcomes; missing data stays explicitly absent. */
        BatmanGraphicsSession(std::uint32_t id, BatmanGraphicsSnapshot snapshot,
            std::optional<BatmanDisplayCatalog> windowed, std::optional<BatmanDisplayCatalog> fullscreen)
            : Id(id), Snapshot(std::move(snapshot)), Baseline(Snapshot.TryCreateDraft()),
              Windowed(std::move(windowed)), Fullscreen(std::move(fullscreen)) {
        }
    };
}
