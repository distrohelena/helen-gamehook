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
     * @brief Enumerates the display modes for the one monitor containing the current process window.
     * @param display_device_name Receives the monitor device name selected from the process window.
     * @param modes Receives every raw mode returned by EnumDisplaySettingsExW.
     * @return True when exactly one qualifying process window and its monitor could be identified and enumerated.
     */
    bool EnumerateCurrentDisplayModes(std::wstring& display_device_name, std::vector<helen::BatmanDisplayMode>& modes)
    {
        std::vector<HWND> windows;
        if (EnumWindows(&CollectQualifyingProcessWindow, reinterpret_cast<LPARAM>(&windows)) == FALSE || windows.size() != 1)
        {
            return false;
        }

        const HMONITOR monitor = MonitorFromWindow(windows.front(), MONITOR_DEFAULTTONULL);
        if (monitor == nullptr)
        {
            return false;
        }

        MONITORINFOEXW monitor_info{};
        monitor_info.cbSize = sizeof(monitor_info);
        if (GetMonitorInfoW(monitor, &monitor_info) == FALSE)
        {
            return false;
        }

        display_device_name = monitor_info.szDevice;
        for (DWORD mode_index = 0;; ++mode_index)
        {
            DEVMODEW mode{};
            mode.dmSize = sizeof(mode);
            if (EnumDisplaySettingsExW(display_device_name.c_str(), mode_index, &mode, 0) == FALSE)
            {
                break;
            }

            modes.emplace_back(static_cast<int>(mode.dmPelsWidth), static_cast<int>(mode.dmPelsHeight));
            if (mode_index == MAXDWORD)
            {
                return false;
            }
        }

        return !modes.empty();
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
}

namespace helen
{
    /**
     * @brief Constructs a service using the default Win32 current-game-display enumeration source.
     */
    BatmanDisplayModeService::BatmanDisplayModeService()
        : enumeration_callback_(EnumerateCurrentDisplayModes)
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

    /**
     * @brief Replaces the catalog with a validated normalized snapshot from the enumeration source.
     * @return True when refresh succeeds; otherwise false while preserving the previous catalog.
     */
    bool BatmanDisplayModeService::Refresh()
    {
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

    /**
     * @brief Rechecks whether a previously selected exact pair remains supported after fresh enumeration.
     * @param index Index in the original catalog whose pair should be revalidated.
     * @return The originally selected pair when it remains present, or no value otherwise.
     */
    std::optional<BatmanDisplayMode> BatmanDisplayModeService::RevalidateMode(std::size_t index)
    {
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
}
