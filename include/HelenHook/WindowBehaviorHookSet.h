#pragma once

#include <string_view>

#include <windows.h>

#include <HelenHook/Hook.h>
#include <HelenHook/Memory.h>

namespace helen
{
    class CommandDispatcher;

    /**
     * @brief Installs optional process-local hooks that spoof focus state and keep input-active messages alive.
     *
     * The hook set is generic runtime infrastructure driven by the typed `window.*` config keys. It owns
     * only the main executable's import hooks plus one tracked top-level process window WndProc subclass.
     */
    class WindowBehaviorHookSet
    {
    public:
        /**
         * @brief Binds the hook set to the dispatcher that owns the generic `window.*` config keys.
         * @param dispatcher Dispatcher used to read the current focus and background-input settings.
         */
        explicit WindowBehaviorHookSet(CommandDispatcher& dispatcher);

        /**
         * @brief Restores any installed hooks and tracked WndProc subclass state.
         */
        ~WindowBehaviorHookSet();

        WindowBehaviorHookSet(const WindowBehaviorHookSet&) = delete;
        WindowBehaviorHookSet& operator=(const WindowBehaviorHookSet&) = delete;
        WindowBehaviorHookSet(WindowBehaviorHookSet&&) = delete;
        WindowBehaviorHookSet& operator=(WindowBehaviorHookSet&&) = delete;

        /**
         * @brief Installs the requested focus spoof hooks and optional WndProc subclass for the tracked process window.
         * @return True when at least one requested behavior installed successfully; otherwise false.
         */
        bool Install();

        /**
         * @brief Removes every installed import hook and restores the original WndProc when present.
         */
        void Remove();

        /**
         * @brief Returns whether this instance currently owns any installed window behavior override.
         * @return True when at least one hook or WndProc subclass is active.
         */
        bool IsInstalled() const noexcept;

        /**
         * @brief Returns the currently tracked top-level process window for tests and diagnostics.
         * @return Tracked window handle or null when discovery has not succeeded.
         */
        HWND DebugGetTrackedWindow() const noexcept;

    private:
        /**
         * @brief Returns the process-global active hook set used by the static detours.
         * @return Active instance or null when no window behavior hook set is installed.
         */
        static WindowBehaviorHookSet* Current();

        /**
         * @brief IAT detour for `GetForegroundWindow` that returns the tracked game window when spoofing is enabled.
         * @return The tracked game window or null when no active hook set is installed.
         */
        static HWND WINAPI GetForegroundWindowDetour();

        /**
         * @brief IAT detour for `GetFocus` that returns the tracked game window when spoofing is enabled.
         * @return The tracked game window or null when no active hook set is installed.
         */
        static HWND WINAPI GetFocusDetour();

        /**
         * @brief IAT detour for `GetActiveWindow` that returns the tracked game window when spoofing is enabled.
         * @return The tracked game window or null when no active hook set is installed.
         */
        static HWND WINAPI GetActiveWindowDetour();

        /**
         * @brief IAT detour for `RegisterRawInputDevices` that optionally short-circuits raw-input registration.
         * @param devices Raw-input device array passed by the caller.
         * @param device_count Number of entries in the raw-input device array.
         * @param struct_size Size of one raw-input device record supplied by the caller.
         * @return True when registration is intentionally suppressed or when the direct user32 call succeeds.
         */
        static BOOL WINAPI RegisterRawInputDevicesDetour(PCRAWINPUTDEVICE devices, UINT device_count, UINT struct_size);

        /**
         * @brief IAT detour for `ClipCursor` that optionally enforces clipping against the tracked game window rect.
         * @param clip_rect Original clip rect requested by the caller.
         * @return True when the enforced or original clip request succeeds.
         */
        static BOOL WINAPI ClipCursorDetour(const RECT* clip_rect);

        /**
         * @brief IAT detour for `SetWindowPos` that preserves requested behavior and then reapplies the tracked window policy.
         * @param hwnd Window whose position or size is being updated.
         * @param insert_after Optional Z-order insertion target.
         * @param x Requested left position.
         * @param y Requested top position.
         * @param cx Requested width.
         * @param cy Requested height.
         * @param flags SetWindowPos flags supplied by the caller.
         * @return Result of the direct user32 `SetWindowPos` call.
         */
        static BOOL WINAPI SetWindowPosDetour(HWND hwnd, HWND insert_after, int x, int y, int cx, int cy, UINT flags);

        /**
         * @brief IAT detour for `SetWindowLongW` that preserves the WndProc subclass and reapplies tracked style policy.
         * @param hwnd Window whose long value is being updated.
         * @param index Long slot requested by the caller.
         * @param new_long Replacement long value supplied by the caller.
         * @return Previous long value reported by the direct user32 `SetWindowLongW` call or the preserved WndProc value.
         */
        static LONG WINAPI SetWindowLongWDetour(HWND hwnd, int index, LONG new_long);

        /**
         * @brief IAT detour for `SetWindowLongPtrW` that preserves the WndProc subclass and reapplies tracked style policy.
         * @param hwnd Window whose long-pointer value is being updated.
         * @param index Long-pointer slot requested by the caller.
         * @param new_long Replacement value supplied by the caller.
         * @return Previous long-pointer value reported by the direct user32 `SetWindowLongPtrW` call or the preserved WndProc value.
         */
        static LONG_PTR WINAPI SetWindowLongPtrWDetour(HWND hwnd, int index, LONG_PTR new_long);

        /**
         * @brief WndProc subclass used to suppress selected deactivate and raw-input messages when background input is enabled.
         * @param hwnd Tracked top-level process window.
         * @param message Win32 message identifier received by the tracked window.
         * @param wParam First message parameter supplied by the OS.
         * @param lParam Second message parameter supplied by the OS.
         * @return Zero for filtered messages; otherwise the original WndProc result.
         */
        static LRESULT CALLBACK WindowProcDetour(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

        /**
         * @brief Discovers and stores the process-owned top-level window that should receive spoofing and filtering.
         * @return True when one visible top-level process window is found; otherwise false.
         */
        bool CaptureTrackedWindow();

        /**
         * @brief Installs one optional user32 import hook only when the main executable imports the requested symbol.
         * @param hook Mutable IAT hook instance that should own the installed replacement.
         * @param module Main executable module whose import table should be patched.
         * @param imported_name Exact user32 import name that should be replaced.
         * @param replacement Detour function pointer that should replace the import.
         * @param import_present Receives true when the main executable imports the requested symbol.
         * @return True when the import was absent or the hook installed successfully; otherwise false.
         */
        bool InstallOptionalHook(IatHook& hook, const ModuleView& module, std::string_view imported_name, void* replacement, bool& import_present);

        /**
         * @brief Returns whether the current background-input config requires the supplied message to be filtered.
         * @param message Win32 message identifier received by the tracked window.
         * @return True when the message should be swallowed; otherwise false.
         */
        bool ShouldFilterMessage(UINT message) const;

        /**
         * @brief Reapplies the configured tracked window style, rect, and topmost policy when any related flags are enabled.
         * @return True when the policy is not needed or the tracked window is available for enforcement.
         */
        bool TryApplyConfiguredWindowState();

        /**
         * @brief Reads the current tracked game window rect from user32.
         * @return Current tracked window rect or a zero rect when no tracked window is available.
         */
        RECT ReadTrackedWindowRect() const;

        /**
         * @brief Calls the original `RegisterRawInputDevices` import when that IAT slot is hooked.
         * @param devices Raw-input device array passed by the caller.
         * @param device_count Number of entries in the raw-input device array.
         * @param struct_size Size of one raw-input device record supplied by the caller.
         * @return Result of the original or direct `RegisterRawInputDevices` call.
         */
        BOOL CallOriginalRegisterRawInputDevices(PCRAWINPUTDEVICE devices, UINT device_count, UINT struct_size) const;

        /**
         * @brief Calls the original `ClipCursor` import when that IAT slot is hooked.
         * @param clip_rect Clip rectangle that should be passed to user32.
         * @return Result of the original or direct `ClipCursor` call.
         */
        BOOL CallOriginalClipCursor(const RECT* clip_rect) const;

        /**
         * @brief Calls the original `SetWindowPos` import when that IAT slot is hooked.
         * @param hwnd Window whose position or size is being updated.
         * @param insert_after Optional Z-order insertion target.
         * @param x Requested left position.
         * @param y Requested top position.
         * @param cx Requested width.
         * @param cy Requested height.
         * @param flags SetWindowPos flags supplied by the caller.
         * @return Result of the original or direct `SetWindowPos` call.
         */
        BOOL CallOriginalSetWindowPos(HWND hwnd, HWND insert_after, int x, int y, int cx, int cy, UINT flags) const;

        /**
         * @brief Calls the original `SetWindowLongW` import when that IAT slot is hooked.
         * @param hwnd Window whose long value is being updated.
         * @param index Long slot requested by the caller.
         * @param new_long Replacement long value supplied by the caller.
         * @return Previous long value reported by the original or direct `SetWindowLongW` call.
         */
        LONG CallOriginalSetWindowLongW(HWND hwnd, int index, LONG new_long) const;

        /**
         * @brief Calls the original `SetWindowLongPtrW` import when that IAT slot is hooked.
         * @param hwnd Window whose long-pointer value is being updated.
         * @param index Long-pointer slot requested by the caller.
         * @param new_long Replacement value supplied by the caller.
         * @return Previous long-pointer value reported by the original or direct `SetWindowLongPtrW` call.
         */
        LONG_PTR CallOriginalSetWindowLongPtrW(HWND hwnd, int index, LONG_PTR new_long) const;

        /** @brief Dispatcher that owns the current `window.*` config keys. */
        CommandDispatcher& dispatcher_;

        /** @brief Process-owned top-level window discovered for spoofing and WndProc filtering. */
        HWND tracked_window_{};

        /** @brief Original tracked window procedure restored during hook removal. */
        WNDPROC original_window_proc_{};

        /** @brief Optional IAT hook that replaces the main executable's `GetForegroundWindow` import. */
        IatHook get_foreground_window_hook_;

        /** @brief Optional IAT hook that replaces the main executable's `GetFocus` import. */
        IatHook get_focus_hook_;

        /** @brief Optional IAT hook that replaces the main executable's `GetActiveWindow` import. */
        IatHook get_active_window_hook_;

        /** @brief Optional IAT hook that replaces the main executable's `RegisterRawInputDevices` import. */
        IatHook register_raw_input_devices_hook_;

        /** @brief Optional IAT hook that replaces the main executable's `ClipCursor` import. */
        IatHook clip_cursor_hook_;

        /** @brief Optional IAT hook that replaces the main executable's `SetWindowPos` import. */
        IatHook set_window_pos_hook_;

        /** @brief Optional IAT hook that replaces the main executable's `SetWindowLongW` import. */
        IatHook set_window_long_w_hook_;

        /** @brief Optional IAT hook that replaces the main executable's `SetWindowLongPtrW` import. */
        IatHook set_window_long_ptr_w_hook_;

        /** @brief True while the runtime is applying its own tracked window policy and should avoid recursive re-entry. */
        bool applying_window_state_ = false;

        /** @brief Singleton-style active instance used by the static detours. */
        static WindowBehaviorHookSet* active_instance_;
    };
}
