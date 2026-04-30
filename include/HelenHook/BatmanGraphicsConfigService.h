#pragma once

#include <filesystem>

namespace helen
{
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
         * @throws std::invalid_argument Thrown when `ini_path` is empty.
         */
        explicit BatmanGraphicsConfigService(std::filesystem::path ini_path);

        /**
         * @brief Reads the current Batman graphics settings from `BmEngine.ini` into registered config keys.
         * @param dispatcher Config dispatcher that receives the normalized graphics draft values.
         * @return True when every required INI value is present and every config key updates successfully; otherwise false.
         */
        bool LoadIntoDispatcher(CommandDispatcher& dispatcher) const;

        /**
         * @brief Writes the current normalized graphics draft values back into `BmEngine.ini`.
         * @param dispatcher Config dispatcher that supplies the normalized graphics draft values.
         * @return True when every required config key is present and every target INI value is updated successfully; otherwise false.
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
         * @brief Returns the base `BmEngine.ini` anchor path used by this service.
         * @return Bound `BmEngine.ini` anchor path.
         */
        const std::filesystem::path& GetIniPath() const noexcept;

    private:
        /** @brief Bound Batman user-engine INI path used as the anchor for graphics reads and subtitle sibling-file discovery. */
        std::filesystem::path ini_path_;
    };
}
