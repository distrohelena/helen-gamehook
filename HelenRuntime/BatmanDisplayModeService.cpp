#include <HelenHook/BatmanDisplayModeService.h>

#include <windows.h>

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace
{
    /**
     * @brief Collects visible, ownerless top-level windows belonging to the current process.
     * @param window Enumerated top-level window handle.
     * @param parameter Pointer to the vector receiving qualifying window handles.
     * @return True to continue enumeration for every window.
     */
    BOOL CALLBACK CollectQualifyingProcessWindow(HWND window, LPARAM parameter)
    {
        auto* const windows = reinterpret_cast<std::vector<HWND>*>(parameter);
        DWORD process_id = 0;
        GetWindowThreadProcessId(window, &process_id);
        if (process_id != GetCurrentProcessId() || !IsWindowVisible(window) || GetWindow(window, GW_OWNER) != nullptr)
        {
            return TRUE;
        }

        windows->push_back(window);
        return TRUE;
    }

    /**
     * @brief Enumerates the monitor environment for the one monitor containing the current process window.
     * @return A complete monitor environment when exactly one qualifying process window and its monitor could
     * be identified and enumerated; no value when monitor discovery is ambiguous or incomplete.
     */
    std::optional<helen::BatmanDisplayEnvironment> EnumerateCurrentDisplayEnvironment()
    {
        std::vector<HWND> windows;
        if (EnumWindows(&CollectQualifyingProcessWindow, reinterpret_cast<LPARAM>(&windows)) == FALSE || windows.size() != 1)
        {
            return std::nullopt;
        }

        const HMONITOR monitor = MonitorFromWindow(windows.front(), MONITOR_DEFAULTTONULL);
        if (monitor == nullptr)
        {
            return std::nullopt;
        }

        MONITORINFOEXW monitor_info{};
        monitor_info.cbSize = sizeof(monitor_info);
        if (GetMonitorInfoW(monitor, &monitor_info) == FALSE)
        {
            return std::nullopt;
        }

        std::wstring display_device_name = monitor_info.szDevice;
        const int work_area_width = monitor_info.rcWork.right - monitor_info.rcWork.left;
        const int work_area_height = monitor_info.rcWork.bottom - monitor_info.rcWork.top;
        if (work_area_width <= 0 || work_area_height <= 0)
        {
            return std::nullopt;
        }

        DEVMODEW current_mode{};
        current_mode.dmSize = sizeof(current_mode);
        if (EnumDisplaySettingsExW(display_device_name.c_str(), ENUM_CURRENT_SETTINGS, &current_mode, 0) == FALSE ||
            current_mode.dmPelsWidth == 0 || current_mode.dmPelsHeight == 0)
        {
            return std::nullopt;
        }
        std::vector<helen::BatmanDisplayMode> supported_modes;
        for (DWORD mode_index = 0;; ++mode_index)
        {
            DEVMODEW mode{};
            mode.dmSize = sizeof(mode);
            if (EnumDisplaySettingsExW(display_device_name.c_str(), mode_index, &mode, 0) == FALSE)
            {
                break;
            }

            supported_modes.emplace_back(static_cast<int>(mode.dmPelsWidth), static_cast<int>(mode.dmPelsHeight));
            if (mode_index == MAXDWORD)
            {
                return std::nullopt;
            }
        }

        if (supported_modes.empty())
        {
            return std::nullopt;
        }
        return helen::BatmanDisplayEnvironment(
            std::move(display_device_name),
            std::move(supported_modes),
            helen::BatmanDisplayMode(work_area_width, work_area_height),
            helen::BatmanDisplayMode(
                static_cast<int>(current_mode.dmPelsWidth),
                static_cast<int>(current_mode.dmPelsHeight)));
    }

    /**
     * @brief Validates, sorts, and exact-pair deduplicates one raw display-mode enumeration.
     * @param display_device_name Device name supplied by the enumeration source.
     * @param raw_modes Raw mode list supplied by the enumeration source.
     * @param normalized_modes Receives the validated normalized catalog on success.
     * @return True when the source data is valid, non-empty, and within the supported bound; otherwise false.
     */
    bool TryNormalizeModes(
        const std::wstring& display_device_name,
        const std::vector<helen::BatmanDisplayMode>& raw_modes,
        std::vector<helen::BatmanDisplayMode>& normalized_modes)
    {
        if (display_device_name.empty() || raw_modes.empty())
        {
            return false;
        }

        for (const helen::BatmanDisplayMode& mode : raw_modes)
        {
            if (mode.GetWidth() <= 0 || mode.GetHeight() <= 0)
            {
                return false;
            }
        }

        normalized_modes = raw_modes;
        std::sort(
            normalized_modes.begin(),
            normalized_modes.end(),
            [](const helen::BatmanDisplayMode& left, const helen::BatmanDisplayMode& right)
            {
                const double left_area = static_cast<double>(left.GetWidth()) * static_cast<double>(left.GetHeight());
                const double right_area = static_cast<double>(right.GetWidth()) * static_cast<double>(right.GetHeight());
                if (left_area != right_area)
                {
                    return left_area < right_area;
                }

                if (left.GetWidth() != right.GetWidth())
                {
                    return left.GetWidth() < right.GetWidth();
                }

                return left.GetHeight() < right.GetHeight();
            });

        normalized_modes.erase(
            std::unique(normalized_modes.begin(), normalized_modes.end()),
            normalized_modes.end());
        return !normalized_modes.empty() && normalized_modes.size() <= helen::BatmanDisplayModeService::MaximumModeCount;
    }

    /**
     * @brief Builds the windowed catalog from common fitting sizes plus the exact configured pair.
     * @param environment Valid monitor facts used for filtering and identity.
     * @param configured_width Width read from the launcher draft.
     * @param configured_height Height read from the launcher draft.
     * @param normalized_modes Receives the staged windowed catalog.
     * @return True when every required dimension is valid and the bounded catalog normalizes successfully.
     */
    bool TryBuildWindowedModes(
        const helen::BatmanDisplayEnvironment& environment,
        int configured_width,
        int configured_height,
        std::vector<helen::BatmanDisplayMode>& normalized_modes)
    {
        if (configured_width <= 0 || configured_height <= 0 ||
            environment.WorkArea.GetWidth() <= 0 || environment.WorkArea.GetHeight() <= 0)
        {
            return false;
        }

        std::vector<helen::BatmanDisplayMode> candidate_modes;
        const helen::BatmanDisplayMode common_sizes[] = {
            helen::BatmanDisplayMode(640, 480),
            helen::BatmanDisplayMode(800, 600),
            helen::BatmanDisplayMode(1024, 768),
            helen::BatmanDisplayMode(1280, 720),
            helen::BatmanDisplayMode(1280, 800),
            helen::BatmanDisplayMode(1280, 1024),
            helen::BatmanDisplayMode(1366, 768),
            helen::BatmanDisplayMode(1600, 900),
            helen::BatmanDisplayMode(1680, 1050),
            helen::BatmanDisplayMode(1920, 1080),
            helen::BatmanDisplayMode(1920, 1200),
            helen::BatmanDisplayMode(2560, 1080),
            helen::BatmanDisplayMode(2560, 1440),
            helen::BatmanDisplayMode(2560, 1600),
            helen::BatmanDisplayMode(3440, 1440),
            helen::BatmanDisplayMode(3840, 2160)
        };

        for (const helen::BatmanDisplayMode& common_size : common_sizes)
        {
            if (common_size.GetWidth() <= environment.WorkArea.GetWidth() &&
                common_size.GetHeight() <= environment.WorkArea.GetHeight())
            {
                candidate_modes.push_back(common_size);
            }
        }
        candidate_modes.emplace_back(configured_width, configured_height);
        return TryNormalizeModes(environment.DisplayDeviceName, candidate_modes, normalized_modes);
    }
}

namespace helen
{
    /**
     * @brief Constructs a service using the default Win32 current-game-display enumeration source.
     */
    BatmanDisplayModeService::BatmanDisplayModeService()
        : environment_enumeration_callback_(EnumerateCurrentDisplayEnvironment)
    {
    }

    /**
     * @brief Constructs a service using an injected display enumeration source.
     * @param enumeration_callback Callback that supplies the current display device and raw modes.
     * @throws std::invalid_argument Thrown when the callback is empty.
     */
    BatmanDisplayModeService::BatmanDisplayModeService(EnumerationCallback enumeration_callback)
        : enumeration_callback_(std::move(enumeration_callback))
    {
        if (!enumeration_callback_)
        {
            throw std::invalid_argument("Batman display mode service requires an enumeration callback.");
        }
    }

    BatmanDisplayModeService::BatmanDisplayModeService(EnvironmentEnumerationCallback enumeration_callback)
        : environment_enumeration_callback_(std::move(enumeration_callback))
    {
        if (!environment_enumeration_callback_)
        {
            throw std::invalid_argument("Batman display mode service requires an environment enumeration callback.");
        }
    }

    /** @brief Copies validated captured facts into independently owned session storage. */
    std::optional<BatmanDisplayCatalog> BatmanDisplayModeService::CaptureCatalog(BatmanDisplayModeCatalogKind kind, int width, int height) {
        if ((kind != BatmanDisplayModeCatalogKind::Windowed && kind != BatmanDisplayModeCatalogKind::Fullscreen) ||
            !Refresh(kind, width, height)) {
            return std::nullopt;
        }
        const std::optional<BatmanDisplayMode> desktop = GetDesktopMode(kind);
        if (!desktop.has_value()) {
            return std::nullopt;
        }
        const bool windowed = kind == BatmanDisplayModeCatalogKind::Windowed;
        return BatmanDisplayCatalog(kind,
            windowed ? windowed_display_device_name_ : fullscreen_display_device_name_,
            windowed ? windowed_modes_ : fullscreen_modes_, BatmanDisplayMode(width, height), *desktop);
    }

    /** @brief Revalidates a session's exact original pair even if service scratch catalogs were replaced. */
    std::optional<BatmanDisplayMode> BatmanDisplayModeService::RevalidateMode(const BatmanDisplayCatalog& catalog, std::size_t index) {
        return RevalidateCapturedMode(catalog.GetKind(), catalog.GetModes(), catalog.GetDeviceName(), catalog.GetConfiguredMode(), index);
    }

    /** @brief Refreshes legacy catalog storage using current monitor enumeration without fabricating modes. */
    bool BatmanDisplayModeService::Refresh()
    {
        if (enumeration_callback_ == nullptr)
        {
            if (environment_enumeration_callback_ == nullptr)
            {
                return false;
            }
            const std::optional<BatmanDisplayEnvironment> environment_result = environment_enumeration_callback_();
            if (!environment_result.has_value())
            {
                return false;
            }
            const BatmanDisplayEnvironment& environment = *environment_result;
            std::vector<BatmanDisplayMode> normalized_modes;
            if (!TryNormalizeModes(environment.DisplayDeviceName, environment.SupportedModes, normalized_modes) ||
                environment.CurrentDesktopMode.GetWidth() <= 0 || environment.CurrentDesktopMode.GetHeight() <= 0 ||
                std::find(normalized_modes.begin(), normalized_modes.end(), environment.CurrentDesktopMode) == normalized_modes.end())
            {
                return false;
            }
            fullscreen_modes_ = normalized_modes;
            fullscreen_display_device_name_ = environment.DisplayDeviceName;
            fullscreen_current_mode_.reset();
            fullscreen_desktop_mode_ = environment.CurrentDesktopMode;
            active_catalog_kind_ = BatmanDisplayModeCatalogKind::Fullscreen;
            modes_ = std::move(normalized_modes);
            display_device_name_ = environment.DisplayDeviceName;
            return true;
        }

        std::wstring display_device_name;
        std::vector<BatmanDisplayMode> raw_modes;
        if (!enumeration_callback_(display_device_name, raw_modes))
        {
            return false;
        }
        std::vector<BatmanDisplayMode> normalized_modes;
        if (!TryNormalizeModes(display_device_name, raw_modes, normalized_modes))
        {
            return false;
        }
        display_device_name_ = std::move(display_device_name);
        modes_ = std::move(normalized_modes);
        active_catalog_kind_ = BatmanDisplayModeCatalogKind::Fullscreen;
        return true;
    }

    bool BatmanDisplayModeService::Refresh(
        BatmanDisplayModeCatalogKind kind,
        int configured_width,
        int configured_height)
    {
        if (environment_enumeration_callback_ == nullptr)
        {
            return false;
        }

        const std::optional<BatmanDisplayEnvironment> environment_result = environment_enumeration_callback_();
        if (!environment_result.has_value())
        {
            return false;
        }
        const BatmanDisplayEnvironment& environment = *environment_result;

        std::vector<BatmanDisplayMode> normalized_modes;
        if (kind == BatmanDisplayModeCatalogKind::Windowed)
        {
            if (!TryBuildWindowedModes(environment, configured_width, configured_height, normalized_modes))
            {
                return false;
            }
        }
        else
        {
            if (!TryNormalizeModes(environment.DisplayDeviceName, environment.SupportedModes, normalized_modes) ||
                environment.CurrentDesktopMode.GetWidth() <= 0 || environment.CurrentDesktopMode.GetHeight() <= 0)
            {
                return false;
            }
        }

        if (configured_width <= 0 || configured_height <= 0)
        {
            return false;
        }
        const BatmanDisplayMode configured_mode(configured_width, configured_height);
        if (environment.CurrentDesktopMode.GetWidth() <= 0 || environment.CurrentDesktopMode.GetHeight() <= 0 ||
            (kind == BatmanDisplayModeCatalogKind::Fullscreen &&
             std::find(normalized_modes.begin(), normalized_modes.end(), environment.CurrentDesktopMode) == normalized_modes.end()))
        {
            return false;
        }

        if (kind == BatmanDisplayModeCatalogKind::Windowed)
        {
            windowed_modes_ = std::move(normalized_modes);
            windowed_display_device_name_ = environment.DisplayDeviceName;
            windowed_current_mode_ = configured_mode;
            windowed_desktop_mode_ = environment.CurrentDesktopMode;
            windowed_custom_mode_ = configured_mode;
        }
        else
        {
            fullscreen_modes_ = std::move(normalized_modes);
            fullscreen_display_device_name_ = environment.DisplayDeviceName;
            fullscreen_current_mode_ = configured_mode;
            fullscreen_desktop_mode_ = environment.CurrentDesktopMode;
        }
        active_catalog_kind_ = kind;
        return true;
    }

    /**
     * @brief Returns the number of display modes in the last successful catalog snapshot.
     * @return Number of normalized display modes.
     */
    std::size_t BatmanDisplayModeService::GetModeCount() const noexcept
    {
        return modes_.size();
    }

    std::size_t BatmanDisplayModeService::GetModeCount(BatmanDisplayModeCatalogKind kind) const noexcept
    {
        return kind == BatmanDisplayModeCatalogKind::Windowed ? windowed_modes_.size() : fullscreen_modes_.size();
    }

    /**
     * @brief Returns one catalog mode by its stable index.
     * @param index Zero-based index into the last successful catalog snapshot.
     * @return Catalog mode stored at the requested index.
     * @throws std::out_of_range Thrown when index is outside the captured catalog.
     */
    const BatmanDisplayMode& BatmanDisplayModeService::GetMode(std::size_t index) const
    {
        return modes_.at(index);
    }

    const BatmanDisplayMode& BatmanDisplayModeService::GetMode(
        BatmanDisplayModeCatalogKind kind,
        std::size_t index) const
    {
        const std::vector<BatmanDisplayMode>& modes = kind == BatmanDisplayModeCatalogKind::Windowed
            ? windowed_modes_
            : fullscreen_modes_;
        return modes.at(index);
    }

    /**
     * @brief Finds the catalog index for an exact width and height pair.
     * @param width Horizontal dimension to locate.
     * @param height Vertical dimension to locate.
     * @return Matching catalog index, or no value when the exact pair is absent.
     */
    std::optional<std::size_t> BatmanDisplayModeService::FindModeIndex(int width, int height) const noexcept
    {
        for (std::size_t index = 0; index < modes_.size(); ++index)
        {
            if (modes_[index].GetWidth() == width && modes_[index].GetHeight() == height)
            {
                return index;
            }
        }

        return std::nullopt;
    }

    std::optional<std::size_t> BatmanDisplayModeService::FindModeIndex(
        BatmanDisplayModeCatalogKind kind,
        int width,
        int height) const noexcept
    {
        const std::vector<BatmanDisplayMode>& modes = kind == BatmanDisplayModeCatalogKind::Windowed
            ? windowed_modes_
            : fullscreen_modes_;
        for (std::size_t index = 0; index < modes.size(); ++index)
        {
            if (modes[index].GetWidth() == width && modes[index].GetHeight() == height)
            {
                return index;
            }
        }
        return std::nullopt;
    }

    /**
     * @brief Resolves one raw scalar request against the captured catalog.
     * @param raw_request_value 4700 for the count, or 4701+2*i/4702+2*i for mode dimensions.
     * @return Requested catalog scalar, or no value for an unsupported request.
     */
    std::optional<int> BatmanDisplayModeService::QueryCatalogScalar(int raw_request_value) const
    {
        if (raw_request_value == 4700)
        {
            return static_cast<int>(modes_.size());
        }

        if (raw_request_value < 4701)
        {
            return std::nullopt;
        }

        const int scalar_offset = raw_request_value - 4701;
        const std::size_t mode_index = static_cast<std::size_t>(scalar_offset / 2);
        if (mode_index >= modes_.size())
        {
            return std::nullopt;
        }

        return (scalar_offset % 2 == 0)
            ? modes_[mode_index].GetWidth()
            : modes_[mode_index].GetHeight();
    }

    std::optional<int> BatmanDisplayModeService::QueryCatalogScalar(
        BatmanDisplayModeCatalogKind kind,
        int raw_request_value) const
    {
        const std::vector<BatmanDisplayMode>& modes = kind == BatmanDisplayModeCatalogKind::Windowed
            ? windowed_modes_
            : fullscreen_modes_;
        const int count_request = kind == BatmanDisplayModeCatalogKind::Windowed ? 5200 : 5400;
        const int first_pair_request = count_request + 1;
        if (raw_request_value == count_request)
        {
            return static_cast<int>(modes.size());
        }
        if (raw_request_value < first_pair_request || raw_request_value >= first_pair_request + static_cast<int>(modes.size()) * 2)
        {
            return std::nullopt;
        }
        const int offset = raw_request_value - first_pair_request;
        const std::size_t mode_index = static_cast<std::size_t>(offset / 2);
        return offset % 2 == 0 ? modes[mode_index].GetWidth() : modes[mode_index].GetHeight();
    }

    std::optional<BatmanDisplayMode> BatmanDisplayModeService::GetCurrentMode(BatmanDisplayModeCatalogKind kind) const noexcept
    {
        return kind == BatmanDisplayModeCatalogKind::Windowed ? windowed_current_mode_ : fullscreen_current_mode_;
    }

    std::optional<BatmanDisplayMode> BatmanDisplayModeService::GetDesktopMode(BatmanDisplayModeCatalogKind kind) const noexcept
    {
        return kind == BatmanDisplayModeCatalogKind::Windowed ? windowed_desktop_mode_ : fullscreen_desktop_mode_;
    }

    BatmanDisplayModeCatalogKind BatmanDisplayModeService::GetActiveCatalogKind() const noexcept
    {
        return active_catalog_kind_;
    }

    /**
     * @brief Rechecks whether a previously selected exact pair remains supported after fresh enumeration.
     * @param index Index in the original catalog whose pair should be revalidated.
     * @return The originally selected pair when it remains present, or no value otherwise.
     */
    std::optional<BatmanDisplayMode> BatmanDisplayModeService::RevalidateMode(std::size_t index)
    {
        if (enumeration_callback_ == nullptr)
        {
            return RevalidateMode(active_catalog_kind_, index);
        }
        if (index >= modes_.size())
        {
            return std::nullopt;
        }

        const BatmanDisplayMode selected_mode = modes_[index];
        std::wstring display_device_name;
        std::vector<BatmanDisplayMode> raw_modes;
        if (!enumeration_callback_(display_device_name, raw_modes))
        {
            return std::nullopt;
        }

        if (display_device_name != display_device_name_)
        {
            return std::nullopt;
        }

        std::vector<BatmanDisplayMode> normalized_modes;
        if (!TryNormalizeModes(display_device_name, raw_modes, normalized_modes))
        {
            return std::nullopt;
        }

        const auto selected_mode_iterator = std::find(normalized_modes.begin(), normalized_modes.end(), selected_mode);
        return selected_mode_iterator == normalized_modes.end()
            ? std::nullopt
            : std::optional<BatmanDisplayMode>(selected_mode);
    }

    std::optional<BatmanDisplayMode> BatmanDisplayModeService::RevalidateMode(
        BatmanDisplayModeCatalogKind kind, std::size_t index) {
        const bool windowed = kind == BatmanDisplayModeCatalogKind::Windowed;
        return RevalidateCapturedMode(kind, windowed ? windowed_modes_ : fullscreen_modes_,
            windowed ? windowed_display_device_name_ : fullscreen_display_device_name_, windowed_custom_mode_, index);
    }

    std::optional<BatmanDisplayMode> BatmanDisplayModeService::RevalidateCapturedMode(
        BatmanDisplayModeCatalogKind kind, const std::vector<BatmanDisplayMode>& modes,
        const std::wstring& device_name, const std::optional<BatmanDisplayMode>& configured_mode, std::size_t index) {
        if (index >= modes.size())
        {
            return std::nullopt;
        }

        const BatmanDisplayMode selected_mode = modes[index];
        std::optional<BatmanDisplayEnvironment> environment_result;
        if (environment_enumeration_callback_ != nullptr)
        {
            environment_result = environment_enumeration_callback_();
            if (!environment_result.has_value() || environment_result->DisplayDeviceName != device_name)
            {
                return std::nullopt;
            }
        }
        else
        {
            if (kind == BatmanDisplayModeCatalogKind::Windowed)
            {
                return std::nullopt;
            }
            std::wstring refreshed_device_name;
            std::vector<BatmanDisplayMode> raw_modes;
            if (enumeration_callback_ == nullptr || !enumeration_callback_(refreshed_device_name, raw_modes) || refreshed_device_name != device_name)
            {
                return std::nullopt;
            }
            std::vector<BatmanDisplayMode> normalized_modes;
            if (!TryNormalizeModes(refreshed_device_name, raw_modes, normalized_modes) ||
                std::find(normalized_modes.begin(), normalized_modes.end(), selected_mode) == normalized_modes.end())
            {
                return std::nullopt;
            }
            return selected_mode;
        }

        const BatmanDisplayEnvironment& environment = *environment_result;

        std::vector<BatmanDisplayMode> normalized_modes;
        if (kind == BatmanDisplayModeCatalogKind::Windowed)
        {
            if (!configured_mode.has_value() ||
                !TryBuildWindowedModes(environment, configured_mode->GetWidth(), configured_mode->GetHeight(), normalized_modes))
            {
                return std::nullopt;
            }
        }
        else if (!TryNormalizeModes(environment.DisplayDeviceName, environment.SupportedModes, normalized_modes))
        {
            return std::nullopt;
        }
        return std::find(normalized_modes.begin(), normalized_modes.end(), selected_mode) == normalized_modes.end()
            ? std::nullopt
            : std::optional<BatmanDisplayMode>(selected_mode);
    }
}
