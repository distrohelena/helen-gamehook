#pragma once

#include <HelenHook/BatmanDisplayModeService.h>
#include <HelenHook/CommandIntPair.h>

#include <optional>
#include <string>

namespace helen
{
    class CommandDispatcher;

    /**
     * @brief Resolves Batman display catalog protocol requests against isolated staged catalog sequences.
     *
     * A count request establishes the only sequence from which entry and terminal-pair requests may be
     * answered. This prevents stale responses from one mode bank being mistaken for another bank's values.
     */
    class BatmanDisplayModeResponseProvider
    {
    public:
        /** @brief Dynamic provider identifier declared by Batman graphics packs. */
        static constexpr const char* ProviderId = "batmanDisplayModes";
        /** @brief Legacy fullscreen catalog count request. */
        static constexpr int LegacyCatalogRequest = 4700;
        /** @brief Legacy fullscreen current width request. */
        static constexpr int LegacyCurrentWidthRequest = 4897;
        /** @brief Legacy fullscreen current height request. */
        static constexpr int LegacyCurrentHeightRequest = 4898;
        /** @brief Windowed catalog count request. */
        static constexpr int WindowedCatalogRequest = 5200;
        /** @brief Windowed catalog current width request. */
        static constexpr int WindowedCurrentWidthRequest = 5397;
        /** @brief Windowed catalog current height request. */
        static constexpr int WindowedCurrentHeightRequest = 5398;
        /** @brief Fullscreen catalog count request. */
        static constexpr int FullscreenCatalogRequest = 5400;
        /** @brief Fullscreen catalog current width request. */
        static constexpr int FullscreenCurrentWidthRequest = 5597;
        /** @brief Fullscreen catalog current height request. */
        static constexpr int FullscreenCurrentHeightRequest = 5598;
        /** @brief Actual desktop width request used for explicit fullscreen fallback. */
        static constexpr int DesktopWidthRequest = 5600;
        /** @brief Actual desktop height request used for explicit fullscreen fallback. */
        static constexpr int DesktopHeightRequest = 5601;

        /**
         * @brief Binds the provider to the service and dispatcher used by one active runtime.
         * @param display_mode_service Catalog service that owns per-kind snapshots.
         * @param command_dispatcher Dispatcher containing the exact persisted resolution pair.
         */
        BatmanDisplayModeResponseProvider(
            BatmanDisplayModeService& display_mode_service,
            CommandDispatcher& command_dispatcher);

        /**
         * @brief Resolves one provider request while enforcing count-sequence ownership.
         * @param provider_id Dynamic provider identifier supplied by the observer.
         * @param raw_request Request code supplied by Batman's frontend carrier.
         * @return Resolved scalar, or no value for an unknown provider, stale sequence, or invalid request.
         */
        std::optional<int> Resolve(const std::string& provider_id, int raw_request);

    private:
        /** @brief Catalog service used to refresh and query mode-specific snapshots. */
        BatmanDisplayModeService& display_mode_service_;
        /** @brief Dispatcher used only to read the exact persisted resolution pair. */
        CommandDispatcher& command_dispatcher_;
        /** @brief Exact persisted pair staged for the active count-to-terminal response sequence. */
        std::optional<CommandIntPair> current_pair_;
        /** @brief Count request that owns the currently staged catalog response sequence. */
        std::optional<int> origin_request_;
        /** @brief Catalog kind associated with the currently staged mode-specific sequence. */
        BatmanDisplayModeCatalogKind catalog_kind_{ BatmanDisplayModeCatalogKind::Fullscreen };
        /** @brief Actual desktop pair captured only by a successful fullscreen count sequence. */
        std::optional<BatmanDisplayMode> desktop_mode_;
        /** @brief Count request that owns the captured desktop pair. */
        std::optional<int> desktop_origin_request_;
    };
}
