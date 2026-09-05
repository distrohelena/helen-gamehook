#pragma once

#include <filesystem>
#include <HelenHook/BatmanGraphicsSnapshot.h>

namespace helen
{
    class BatmanDisplayModeService;
    class CommandDispatcher;

    /**
     * @brief Reads, writes, and normalizes Batman Arkham Asylum graphics settings.
     *
     * The graphics-options ActionScript menu works with normalized integer states rather than the raw
     * UE3 configuration values. This service owns the translation between those menu states and the
     * encoded values persisted in the user config files.
     */
    class BatmanGraphicsConfigService
    {
    public:
        /**
         * @brief Binds the service to the Batman user `BmEngine.ini` anchor path.
         * @param ini_path Absolute or relative path to the Batman user engine INI file.
         * @param display_mode_service Required service that owns the current Batman display-mode catalog and revalidation.
         * @throws std::invalid_argument Thrown when `ini_path` is empty.
         */
        BatmanGraphicsConfigService(std::filesystem::path ini_path, BatmanDisplayModeService& display_mode_service);

        /**
         * @brief Reads the current Batman graphics settings from the authoritative launcher-owned sibling `UserEngine.ini` into registered config keys.
         * @param dispatcher Config dispatcher that receives the normalized graphics draft values.
         * @return True when the sibling launcher INI can be decoded, every required value is present, and every config key updates successfully; otherwise false.
         */
        bool LoadIntoDispatcher(CommandDispatcher& dispatcher) const;

        /** @brief Captures independently valid launcher settings once without modifying dispatcher state or either INI. */
        BatmanGraphicsSnapshot CaptureReadSnapshot() const;

        /**
         * @brief Writes the graphics draft into `BmEngine.ini` and its launcher-owned `UserEngine.ini` sibling.
         * @param dispatcher Config dispatcher that supplies the normalized graphics draft values.
         * @return True when both files contain every required setting and are written successfully; otherwise false.
         */
        bool ApplyFromDispatcher(const CommandDispatcher& dispatcher) const;

        /**
         * @brief Writes the current normalized subtitle-size config value back into the active subtitle INI.
         * @param dispatcher Config dispatcher that supplies the `ui.subtitleSize` value.
         * @return True when the dispatcher value is valid and `Engine.HUD.ConsoleFontSize` is persisted to disk.
         */
        bool ApplySubtitleSizeFromDispatcher(const CommandDispatcher& dispatcher) const;

        /**
         * @brief Reads subtitle size from the active subtitle INI and writes it to `ui.subtitleSize`.
         * @param dispatcher Config dispatcher that owns the `ui.subtitleSize` key.
         * @return True when INI read/convert succeeds and `ui.subtitleSize` updates; otherwise false.
         */
        bool LoadSubtitleSizeIntoDispatcher(CommandDispatcher& dispatcher) const;

        /**
         * @brief Recomputes the derived `detailLevel` draft state from the current individual detail toggles.
         * @param dispatcher Config dispatcher that stores the current Batman graphics draft.
         * @return True when the required draft keys exist and the derived `detailLevel` is updated successfully; otherwise false.
         */
        bool SyncDetailLevelFromDispatcher(CommandDispatcher& dispatcher) const;

        /**
         * @brief Applies the currently selected `detailLevel` preset to the individual detail-toggle draft values.
         * @param dispatcher Config dispatcher that stores the current Batman graphics draft.
         * @return True when `detailLevel` resolves to a supported preset and the dependent draft values update successfully; otherwise false.
         */
        bool ApplySelectedDetailLevelToDispatcher(CommandDispatcher& dispatcher) const;

        /**
         * @brief Revalidates the selected display mode and atomically writes its exact dimensions into the graphics draft.
         * @param dispatcher Dispatcher containing resolutionModeIndex, resolutionWidth, and resolutionHeight.
         * @return True when the selected index remains supported and both draft dimensions update together; otherwise false with the prior pair retained.
         */
        bool ApplySelectedResolutionModeToDispatcher(CommandDispatcher& dispatcher);

        /**
         * @brief Returns the base `BmEngine.ini` anchor path used by this service.
         * @return Bound `BmEngine.ini` anchor path.
         */
        const std::filesystem::path& GetIniPath() const noexcept;

    private:
        /** @brief Bound generated `BmEngine.ini` anchor path used to locate both generated and launcher-owned graphics INI files. */
        std::filesystem::path ini_path_;
        /** @brief Required display-mode catalog service used to revalidate selected resolution pairs at apply time. */
        BatmanDisplayModeService& display_mode_service_;
    };
}
