#pragma once

#include <HelenHook/BatmanDisplayMode.h>
#include <HelenHook/BatmanDisplayCatalog.h>
#include <HelenHook/BatmanDisplayEnvironment.h>

#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace helen
{
    /**
     * @brief Identifies the resolution catalog currently being prepared for the graphics menu.
     */
    enum class BatmanDisplayModeCatalogKind
    {
        /** @brief Windowed sizes, including a configured custom pair and bounded common sizes. */
        Windowed,
        /** @brief Fullscreen sizes reported by the active monitor driver. */
        Fullscreen
    };

    /**
     * @brief Enumerates and exposes the supported display-mode catalog used by Batman graphics options.
     *
     * The service isolates Win32 display discovery behind an injectable callback so catalog behavior can be
     * tested without creating windows or changing desktop state. A successful refresh owns a sorted, exact-pair
     * deduplicated snapshot until the next successful refresh.
     */
    class BatmanDisplayModeService
    {
    public:
        /**
         * @brief Captures one monitor environment needed to build both resolution catalogs.
         */
        /**
         * @brief Supplies the current game display device name and its raw display modes.
         * @param display_device_name Receives the monitor device name associated with the game window.
         * @param modes Receives raw display dimensions reported by the display enumeration source.
         * @return True when the source identified one current game display and populated its data; otherwise false.
         */
        using EnumerationCallback = std::function<bool(std::wstring&, std::vector<BatmanDisplayMode>&)>;

        /** @brief Supplies the complete monitor environment without touching desktop state. */
        using EnvironmentEnumerationCallback = std::function<std::optional<BatmanDisplayEnvironment>()>;

        /** @brief Maximum number of unique display-mode pairs accepted in one normalized catalog. */
        static constexpr std::size_t MaximumModeCount = 98;

        /**
         * @brief Constructs a service that discovers the current game display through Win32.
         */
        BatmanDisplayModeService();

        /**
         * @brief Constructs a service using an injected display enumeration source.
         * @param enumeration_callback Callback that supplies the current display device and raw modes.
         * @throws std::invalid_argument Thrown when the callback is empty.
         */
        explicit BatmanDisplayModeService(EnumerationCallback enumeration_callback);

        /**
         * @brief Constructs a service using an injected complete monitor-environment source.
         * @param enumeration_callback Callback that supplies supported modes, work area, and desktop mode.
         * @throws std::invalid_argument Thrown when the callback is empty.
         */
        explicit BatmanDisplayModeService(EnvironmentEnumerationCallback enumeration_callback);

        /**
         * @brief Replaces the catalog with a validated normalized snapshot from the enumeration source.
         * @return True when the source succeeds and provides valid, non-empty, bounded display modes; otherwise false.
         */
        bool Refresh();

        /**
         * @brief Builds and stages one mode-specific catalog without mutating the other catalog.
         * @param kind Catalog flavor requested by the graphics screen.
         * @param configured_width Persisted or draft width used for exact windowed preservation.
         * @param configured_height Persisted or draft height used for exact windowed preservation.
         * @return True when monitor data and the requested catalog are valid; false leaves all prior snapshots intact.
         */
        bool Refresh(BatmanDisplayModeCatalogKind kind, int configured_width, int configured_height);

        /** @brief Captures an independently owned catalog; future service refreshes cannot reinterpret its indices. */
        std::optional<BatmanDisplayCatalog> CaptureCatalog(BatmanDisplayModeCatalogKind kind, int width, int height);

        /** @brief Revalidates the exact pair from the supplied owned catalog, not the service's newest ordering. */
        std::optional<BatmanDisplayMode> RevalidateMode(const BatmanDisplayCatalog& catalog, std::size_t index);

        /**
         * @brief Returns the number of display modes in the last successful catalog snapshot.
         * @return Number of normalized display modes.
         */
        std::size_t GetModeCount() const noexcept;

        /** @brief Returns the number of modes in one mode-specific catalog. */
        std::size_t GetModeCount(BatmanDisplayModeCatalogKind kind) const noexcept;

        /**
         * @brief Returns one catalog mode by its stable index.
         * @param index Zero-based index into the last successful catalog snapshot.
         * @return Catalog mode stored at the requested index.
         * @throws std::out_of_range Thrown when index is outside the captured catalog.
         */
        const BatmanDisplayMode& GetMode(std::size_t index) const;

        /** @brief Returns one mode from a selected mode-specific catalog. */
        const BatmanDisplayMode& GetMode(BatmanDisplayModeCatalogKind kind, std::size_t index) const;

        /**
         * @brief Finds the catalog index for an exact width and height pair.
         * @param width Horizontal dimension to locate.
         * @param height Vertical dimension to locate.
         * @return Matching catalog index, or no value when the exact pair is absent.
         */
        std::optional<std::size_t> FindModeIndex(int width, int height) const noexcept;

        /** @brief Finds an exact pair in one mode-specific catalog. */
        std::optional<std::size_t> FindModeIndex(BatmanDisplayModeCatalogKind kind, int width, int height) const noexcept;

        /**
         * @brief Resolves one raw scalar request against the captured catalog.
         * @param raw_request_value 4700 for the count, or 4701+2*i/4702+2*i for mode dimensions.
         * @return Requested catalog scalar, or no value for an unsupported request.
         */
        std::optional<int> QueryCatalogScalar(int raw_request_value) const;

        /** @brief Resolves one scalar request against a selected mode-specific catalog. */
        std::optional<int> QueryCatalogScalar(BatmanDisplayModeCatalogKind kind, int raw_request_value) const;

        /**
         * @brief Returns the current pair staged with a successful catalog refresh.
         * @param kind Catalog whose current pair should be returned.
         * @return The exact configured pair supplied for the catalog, even when fullscreen does not support it.
         */
        std::optional<BatmanDisplayMode> GetCurrentMode(BatmanDisplayModeCatalogKind kind) const noexcept;

        /**
         * @brief Returns the actual desktop mode captured from Windows for a monitor catalog.
         * @param kind Catalog whose monitor desktop mode should be returned.
         * @return The real current desktop pair, or no value when discovery did not provide one.
         */
        std::optional<BatmanDisplayMode> GetDesktopMode(BatmanDisplayModeCatalogKind kind) const noexcept;

        /** @brief Returns the catalog kind most recently staged successfully. */
        BatmanDisplayModeCatalogKind GetActiveCatalogKind() const noexcept;

        /**
         * @brief Rechecks whether a previously selected exact pair remains supported after fresh enumeration.
         * @param index Index in the original catalog whose pair should be revalidated.
         * @return The originally selected pair when it remains present, or no value otherwise.
         */
        std::optional<BatmanDisplayMode> RevalidateMode(std::size_t index);

        /** @brief Revalidates an index against a fresh enumeration for the selected catalog kind. */
        std::optional<BatmanDisplayMode> RevalidateMode(BatmanDisplayModeCatalogKind kind, std::size_t index);

    private:
        /** @brief Source callback used to enumerate the current game display and its raw modes. */
        EnumerationCallback enumeration_callback_;
        /** @brief Optional complete environment source used by mode-specific refreshes. */
        EnvironmentEnumerationCallback environment_enumeration_callback_;
        /** @brief Monitor device name associated with the last successful catalog snapshot. */
        std::wstring display_device_name_;
        /** @brief Sorted and exact-pair deduplicated modes from the last successful refresh. */
        std::vector<BatmanDisplayMode> modes_;
        /** @brief Last successful windowed catalog snapshot. */
        std::vector<BatmanDisplayMode> windowed_modes_;
        /** @brief Last successful fullscreen catalog snapshot. */
        std::vector<BatmanDisplayMode> fullscreen_modes_;
        /** @brief Monitor identity for the windowed snapshot. */
        std::wstring windowed_display_device_name_;
        /** @brief Monitor identity for the fullscreen snapshot. */
        std::wstring fullscreen_display_device_name_;
        /** @brief Current pair staged with the windowed snapshot. */
        std::optional<BatmanDisplayMode> windowed_current_mode_;
        /** @brief Current pair staged with the fullscreen snapshot. */
        std::optional<BatmanDisplayMode> fullscreen_current_mode_;
        /** @brief Actual desktop pair captured with the windowed monitor snapshot. */
        std::optional<BatmanDisplayMode> windowed_desktop_mode_;
        /** @brief Actual desktop pair captured with the fullscreen monitor snapshot. */
        std::optional<BatmanDisplayMode> fullscreen_desktop_mode_;
        /** @brief Original configured windowed pair exempted from later work-area filtering. */
        std::optional<BatmanDisplayMode> windowed_custom_mode_;
        /** @brief Last catalog kind successfully refreshed through the legacy API. */
        BatmanDisplayModeCatalogKind active_catalog_kind_{ BatmanDisplayModeCatalogKind::Fullscreen };

        /** @brief Shares exact-pair revalidation between legacy catalogs and immutable session-owned catalogs. */
        std::optional<BatmanDisplayMode> RevalidateCapturedMode(BatmanDisplayModeCatalogKind kind,
            const std::vector<BatmanDisplayMode>& modes, const std::wstring& device_name,
            const std::optional<BatmanDisplayMode>& configured_mode, std::size_t index);
    };
}
