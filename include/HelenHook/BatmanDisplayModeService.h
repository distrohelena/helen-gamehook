#pragma once

#include <HelenHook/BatmanDisplayMode.h>

#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace helen
{
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
         * @brief Supplies the current game display device name and its raw display modes.
         * @param display_device_name Receives the monitor device name associated with the game window.
         * @param modes Receives raw display dimensions reported by the display enumeration source.
         * @return True when the source identified one current game display and populated its data; otherwise false.
         */
        using EnumerationCallback = std::function<bool(std::wstring&, std::vector<BatmanDisplayMode>&)>;

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
         * @brief Replaces the catalog with a validated normalized snapshot from the enumeration source.
         * @return True when the source succeeds and provides valid, non-empty, bounded display modes; otherwise false.
         */
        bool Refresh();

        /**
         * @brief Returns the number of display modes in the last successful catalog snapshot.
         * @return Number of normalized display modes.
         */
        std::size_t GetModeCount() const noexcept;

        /**
         * @brief Returns one catalog mode by its stable index.
         * @param index Zero-based index into the last successful catalog snapshot.
         * @return Catalog mode stored at the requested index.
         * @throws std::out_of_range Thrown when index is outside the captured catalog.
         */
        const BatmanDisplayMode& GetMode(std::size_t index) const;

        /**
         * @brief Finds the catalog index for an exact width and height pair.
         * @param width Horizontal dimension to locate.
         * @param height Vertical dimension to locate.
         * @return Matching catalog index, or no value when the exact pair is absent.
         */
        std::optional<std::size_t> FindModeIndex(int width, int height) const noexcept;

        /**
         * @brief Resolves one raw scalar request against the captured catalog.
         * @param raw_request_value 4700 for the count, or 4701+2*i/4702+2*i for mode dimensions.
         * @return Requested catalog scalar, or no value for an unsupported request.
         */
        std::optional<int> QueryCatalogScalar(int raw_request_value) const;

        /**
         * @brief Rechecks whether a previously selected exact pair remains supported after fresh enumeration.
         * @param index Index in the original catalog whose pair should be revalidated.
         * @return The originally selected pair when it remains present, or no value otherwise.
         */
        std::optional<BatmanDisplayMode> RevalidateMode(std::size_t index);

    private:
        /** @brief Source callback used to enumerate the current game display and its raw modes. */
        EnumerationCallback enumeration_callback_;
        /** @brief Monitor device name associated with the last successful catalog snapshot. */
        std::wstring display_device_name_;
        /** @brief Sorted and exact-pair deduplicated modes from the last successful refresh. */
        std::vector<BatmanDisplayMode> modes_;
    };
}
