#pragma once

#include <string_view>

namespace helen
{
    class CommandDispatcher;

    namespace WindowBehaviorConfigKeys
    {
        inline constexpr std::string_view FocusSpoofEnabled = "window.focusSpoofEnabled";
        inline constexpr std::string_view ActiveWindowSpoofEnabled = "window.activeWindowSpoofEnabled";
        inline constexpr std::string_view BackgroundInputEnabled = "window.backgroundInputEnabled";
        inline constexpr std::string_view BlockRawInputRegistrationEnabled = "window.blockRawInputRegistrationEnabled";
        inline constexpr std::string_view ClipCursorEnabled = "window.clipCursorEnabled";
        inline constexpr std::string_view RemoveWindowFrameEnabled = "window.removeWindowFrameEnabled";
        inline constexpr std::string_view ForcePositionEnabled = "window.forcePositionEnabled";
        inline constexpr std::string_view ForceSizeEnabled = "window.forceSizeEnabled";
        inline constexpr std::string_view ForceTopmostEnabled = "window.forceTopmostEnabled";
        inline constexpr std::string_view PositionX = "window.positionX";
        inline constexpr std::string_view PositionY = "window.positionY";
        inline constexpr std::string_view Width = "window.width";
        inline constexpr std::string_view Height = "window.height";
    }

    /**
     * @brief Captures the current generic `window.*` runtime settings as typed values.
     */
    class WindowBehaviorSettings
    {
    public:
        /** @brief True when `GetForegroundWindow` should be spoofed to the tracked game window. */
        bool FocusSpoofEnabled = false;
        /** @brief True when `GetFocus` and `GetActiveWindow` should be spoofed to the tracked game window. */
        bool ActiveWindowSpoofEnabled = false;
        /** @brief True when selected deactivate and raw-input messages should be filtered through a subclassed WndProc. */
        bool BackgroundInputEnabled = false;
        /** @brief True when `RegisterRawInputDevices` should be short-circuited. */
        bool BlockRawInputRegistrationEnabled = false;
        /** @brief True when `ClipCursor` should be enforced against the tracked game window rect. */
        bool ClipCursorEnabled = false;
        /** @brief True when standard caption and frame styles should be removed from the tracked game window. */
        bool RemoveWindowFrameEnabled = false;
        /** @brief True when the tracked game window should be forced to the configured X/Y position. */
        bool ForcePositionEnabled = false;
        /** @brief True when the tracked game window should be forced to the configured width and height. */
        bool ForceSizeEnabled = false;
        /** @brief True when the tracked game window should be kept in the topmost Z band. */
        bool ForceTopmostEnabled = false;
        /** @brief Configured left coordinate used when forced position is enabled. */
        int PositionX = 0;
        /** @brief Configured top coordinate used when forced position is enabled. */
        int PositionY = 0;
        /** @brief Configured width used when forced size is enabled. */
        int Width = 0;
        /** @brief Configured height used when forced size is enabled. */
        int Height = 0;
    };

    /**
     * @brief Describes which low-level hooks the runtime must install for one window behavior config snapshot.
     */
    class WindowBehaviorHookPlan
    {
    public:
        /** @brief True when focus-related Win32 APIs must be intercepted. */
        bool RequiresFocusSpoofHooks = false;
        /** @brief True when the tracked game window must be subclassed. */
        bool RequiresWndProcSubclass = false;
        /** @brief True when `RegisterRawInputDevices` must be intercepted. */
        bool RequiresRawInputRegistrationHook = false;
        /** @brief True when `ClipCursor` must be intercepted. */
        bool RequiresClipCursorHook = false;
        /** @brief True when window policy hooks such as `SetWindowPos` or `SetWindowLong*` must be installed. */
        bool RequiresWindowPolicyHooks = false;
        /** @brief True when the runtime should actively enforce a tracked window rect or style. */
        bool RequiresWindowRectEnforcement = false;
    };

    void RegisterWindowBehaviorConfigKeys(CommandDispatcher& dispatcher);
    WindowBehaviorSettings ReadWindowBehaviorSettings(const CommandDispatcher& dispatcher);
    WindowBehaviorHookPlan BuildWindowBehaviorHookPlan(const WindowBehaviorSettings& settings);
}
