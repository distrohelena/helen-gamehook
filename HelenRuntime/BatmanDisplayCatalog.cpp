#include <HelenHook/BatmanDisplayCatalog.h>
#include <HelenHook/BatmanDisplayModeService.h>
#include <stdexcept>
#include <utility>

namespace helen {
    BatmanDisplayCatalog::BatmanDisplayCatalog(BatmanDisplayModeCatalogKind kind, std::wstring device_name,
        std::vector<BatmanDisplayMode> modes, BatmanDisplayMode configured_mode, BatmanDisplayMode desktop_mode)
        : Kind(kind), DeviceName(std::move(device_name)), Modes(std::move(modes)),
          ConfiguredMode(configured_mode), DesktopMode(desktop_mode) {
        if ((Kind != BatmanDisplayModeCatalogKind::Windowed && Kind != BatmanDisplayModeCatalogKind::Fullscreen) ||
            DeviceName.empty() || Modes.empty() || Modes.size() > BatmanDisplayModeService::MaximumModeCount ||
            ConfiguredMode.GetWidth() <= 0 || ConfiguredMode.GetHeight() <= 0 ||
            DesktopMode.GetWidth() <= 0 || DesktopMode.GetHeight() <= 0) {
            throw std::invalid_argument("Display catalog requires valid captured monitor facts and bounded modes.");
        }
        for (const BatmanDisplayMode& mode : Modes) {
            if (mode.GetWidth() <= 0 || mode.GetHeight() <= 0) {
                throw std::invalid_argument("Display catalog contains invalid dimensions.");
            }
        }
    }

    BatmanDisplayModeCatalogKind BatmanDisplayCatalog::GetKind() const noexcept { return Kind; }
    const std::wstring& BatmanDisplayCatalog::GetDeviceName() const noexcept { return DeviceName; }
    const std::vector<BatmanDisplayMode>& BatmanDisplayCatalog::GetModes() const noexcept { return Modes; }
    const BatmanDisplayMode& BatmanDisplayCatalog::GetConfiguredMode() const noexcept { return ConfiguredMode; }
    const BatmanDisplayMode& BatmanDisplayCatalog::GetDesktopMode() const noexcept { return DesktopMode; }
}
