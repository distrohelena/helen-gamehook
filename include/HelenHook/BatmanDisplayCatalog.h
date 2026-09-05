#pragma once

#include <HelenHook/BatmanDisplayMode.h>
#include <string>
#include <vector>

namespace helen {
    enum class BatmanDisplayModeCatalogKind;
    /** @brief Owns the exact resolution ordering and monitor facts captured for a graphics session. */
    class BatmanDisplayCatalog {
    private:
        /** @brief Windowed or fullscreen catalog flavor. */
        BatmanDisplayModeCatalogKind Kind;
        /** @brief Monitor identity used to reject selections after moving to another display. */
        std::wstring DeviceName;
        /** @brief Owned stable index ordering, never borrowed from display-service scratch vectors. */
        std::vector<BatmanDisplayMode> Modes;
        /** @brief Original configured pair used for windowed custom-size preservation. */
        BatmanDisplayMode ConfiguredMode;
        /** @brief Actual desktop pair belonging to this capture. */
        BatmanDisplayMode DesktopMode;
    public:
        /** @brief Constructs an owned catalog with required monitor, mode list and positive captured pairs. */
        BatmanDisplayCatalog(BatmanDisplayModeCatalogKind kind, std::wstring device_name,
            std::vector<BatmanDisplayMode> modes, BatmanDisplayMode configured_mode, BatmanDisplayMode desktop_mode);
        /** @brief Returns the captured windowed/fullscreen flavor. */
        BatmanDisplayModeCatalogKind GetKind() const noexcept;
        /** @brief Returns the captured monitor identity without display discovery. */
        const std::wstring& GetDeviceName() const noexcept;
        /** @brief Returns stable owned modes; callers cannot modify their ordering. */
        const std::vector<BatmanDisplayMode>& GetModes() const noexcept;
        /** @brief Returns the original configured pair for exact windowed exemptions. */
        const BatmanDisplayMode& GetConfiguredMode() const noexcept;
        /** @brief Returns the captured desktop pair, not a fabricated fallback. */
        const BatmanDisplayMode& GetDesktopMode() const noexcept;
    };
}
