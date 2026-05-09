#include <HelenHook/WindowBehaviorConfig.h>

#include <HelenHook/CommandDispatcher.h>

#include <array>
#include <optional>
#include <string>
#include <utility>

namespace
{
    /**
     * @brief Reads one registered integer flag from the dispatcher and treats any non-zero value as enabled.
     * @param dispatcher Dispatcher that owns the requested runtime key.
     * @param key Generic `window.*` key that should be resolved.
     * @return True when the key exists and its value is non-zero; otherwise false.
     */
    bool ReadFlag(const helen::CommandDispatcher& dispatcher, std::string_view key)
    {
        const std::optional<int> value = dispatcher.TryGetInt(std::string(key));
        return value.has_value() && *value != 0;
    }

    /**
     * @brief Reads one registered integer value from the dispatcher and falls back to the supplied default when missing.
     * @param dispatcher Dispatcher that owns the requested runtime key.
     * @param key Generic `window.*` key that should be resolved.
     * @param default_value Value returned when the key is not registered.
     * @return Stored config value when present; otherwise the supplied default.
     */
    int ReadIntOrDefault(const helen::CommandDispatcher& dispatcher, std::string_view key, int default_value)
    {
        const std::optional<int> value = dispatcher.TryGetInt(std::string(key));
        return value.has_value() ? *value : default_value;
    }
}

namespace helen
{
    /**
     * @brief Registers the generic `window.*` config keys with neutral defaults when they are not already pack-defined.
     * @param dispatcher Dispatcher that should own the stable window behavior config surface.
     */
    void RegisterWindowBehaviorConfigKeys(CommandDispatcher& dispatcher)
    {
        const std::array<std::pair<std::string_view, int>, 13> keys = {{
            { WindowBehaviorConfigKeys::FocusSpoofEnabled, 0 },
            { WindowBehaviorConfigKeys::ActiveWindowSpoofEnabled, 0 },
            { WindowBehaviorConfigKeys::BackgroundInputEnabled, 0 },
            { WindowBehaviorConfigKeys::BlockRawInputRegistrationEnabled, 0 },
            { WindowBehaviorConfigKeys::ClipCursorEnabled, 0 },
            { WindowBehaviorConfigKeys::RemoveWindowFrameEnabled, 0 },
            { WindowBehaviorConfigKeys::ForcePositionEnabled, 0 },
            { WindowBehaviorConfigKeys::ForceSizeEnabled, 0 },
            { WindowBehaviorConfigKeys::ForceTopmostEnabled, 0 },
            { WindowBehaviorConfigKeys::PositionX, 0 },
            { WindowBehaviorConfigKeys::PositionY, 0 },
            { WindowBehaviorConfigKeys::Width, 0 },
            { WindowBehaviorConfigKeys::Height, 0 }
        }};

        for (const auto& entry : keys)
        {
            dispatcher.RegisterConfigInt(std::string(entry.first), entry.second);
        }
    }

    /**
     * @brief Reads the current generic `window.*` config state through the command dispatcher.
     * @param dispatcher Dispatcher that owns the runtime config keys.
     * @return Typed snapshot of the current window behavior settings.
     */
    WindowBehaviorSettings ReadWindowBehaviorSettings(const CommandDispatcher& dispatcher)
    {
        WindowBehaviorSettings settings;
        settings.FocusSpoofEnabled = ReadFlag(dispatcher, WindowBehaviorConfigKeys::FocusSpoofEnabled);
        settings.ActiveWindowSpoofEnabled = ReadFlag(dispatcher, WindowBehaviorConfigKeys::ActiveWindowSpoofEnabled);
        settings.BackgroundInputEnabled = ReadFlag(dispatcher, WindowBehaviorConfigKeys::BackgroundInputEnabled);
        settings.BlockRawInputRegistrationEnabled = ReadFlag(dispatcher, WindowBehaviorConfigKeys::BlockRawInputRegistrationEnabled);
        settings.ClipCursorEnabled = ReadFlag(dispatcher, WindowBehaviorConfigKeys::ClipCursorEnabled);
        settings.RemoveWindowFrameEnabled = ReadFlag(dispatcher, WindowBehaviorConfigKeys::RemoveWindowFrameEnabled);
        settings.ForcePositionEnabled = ReadFlag(dispatcher, WindowBehaviorConfigKeys::ForcePositionEnabled);
        settings.ForceSizeEnabled = ReadFlag(dispatcher, WindowBehaviorConfigKeys::ForceSizeEnabled);
        settings.ForceTopmostEnabled = ReadFlag(dispatcher, WindowBehaviorConfigKeys::ForceTopmostEnabled);
        settings.PositionX = ReadIntOrDefault(dispatcher, WindowBehaviorConfigKeys::PositionX, 0);
        settings.PositionY = ReadIntOrDefault(dispatcher, WindowBehaviorConfigKeys::PositionY, 0);
        settings.Width = ReadIntOrDefault(dispatcher, WindowBehaviorConfigKeys::Width, 0);
        settings.Height = ReadIntOrDefault(dispatcher, WindowBehaviorConfigKeys::Height, 0);
        return settings;
    }

    /**
     * @brief Derives the required low-level runtime hook set from one typed config snapshot.
     * @param settings Current generic `window.*` settings.
     * @return Hook plan describing the exact runtime behaviors that must be installed.
     */
    WindowBehaviorHookPlan BuildWindowBehaviorHookPlan(const WindowBehaviorSettings& settings)
    {
        WindowBehaviorHookPlan plan;
        plan.RequiresFocusSpoofHooks = settings.FocusSpoofEnabled || settings.ActiveWindowSpoofEnabled;
        plan.RequiresWndProcSubclass = settings.BackgroundInputEnabled;
        plan.RequiresRawInputRegistrationHook = settings.BlockRawInputRegistrationEnabled;
        plan.RequiresClipCursorHook = settings.ClipCursorEnabled;
        plan.RequiresWindowPolicyHooks =
            settings.RemoveWindowFrameEnabled ||
            settings.ForcePositionEnabled ||
            settings.ForceSizeEnabled ||
            settings.ForceTopmostEnabled;
        plan.RequiresWindowRectEnforcement =
            settings.RemoveWindowFrameEnabled ||
            settings.ForcePositionEnabled ||
            settings.ForceSizeEnabled ||
            settings.ForceTopmostEnabled ||
            settings.ClipCursorEnabled;
        return plan;
    }
}
