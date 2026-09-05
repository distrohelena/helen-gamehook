#pragma once

#include <HelenHook/BatmanDisplayMode.h>

#include <string>
#include <utility>
#include <vector>

namespace helen
{
    /**
     * @brief Describes the monitor facts required to build Batman's windowed and fullscreen catalogs.
     *
     * Every field is supplied by one monitor enumeration. A consumer must reject the environment when
     * any required dimension is non-positive rather than inventing a fallback monitor or mode.
     */
    struct BatmanDisplayEnvironment
    {
        /**
         * @brief Constructs a complete monitor environment with all required discovery values.
         * @param display_device_name Stable monitor device identifier.
         * @param supported_modes Raw monitor modes.
         * @param work_area Usable monitor work area.
         * @param current_desktop_mode Actual desktop mode selected by Windows.
         */
        BatmanDisplayEnvironment(
            std::wstring display_device_name,
            std::vector<BatmanDisplayMode> supported_modes,
            BatmanDisplayMode work_area,
            BatmanDisplayMode current_desktop_mode)
            : DisplayDeviceName(std::move(display_device_name)),
              SupportedModes(std::move(supported_modes)),
              WorkArea(work_area),
              CurrentDesktopMode(current_desktop_mode)
        {
        }

        /** @brief Stable Win32 monitor device name used to reject cross-monitor revalidation. */
        std::wstring DisplayDeviceName;
        /** @brief Raw display modes reported by the active monitor driver. */
        std::vector<BatmanDisplayMode> SupportedModes;
        /** @brief Usable monitor work-area dimensions for windowed size filtering. */
        BatmanDisplayMode WorkArea;
        /** @brief Actual current desktop mode used for fullscreen fallback. */
        BatmanDisplayMode CurrentDesktopMode;
    };
}
