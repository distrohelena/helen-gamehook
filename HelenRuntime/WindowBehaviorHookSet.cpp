#include <HelenHook/WindowBehaviorHookSet.h>

#include <HelenHook/CommandDispatcher.h>
#include <HelenHook/WindowBehaviorConfig.h>

#include <optional>

namespace
{
    /**
     * @brief Enumerates top-level windows for the current process and captures the first visible unowned candidate.
     * @param hwnd Current enumerated top-level window handle.
     * @param lParam Opaque pointer to the target handle slot that should receive the chosen window.
     * @return False once a candidate window has been selected; otherwise true to continue enumeration.
     */
    BOOL CALLBACK CollectProcessWindow(HWND hwnd, LPARAM lParam)
    {
        auto* target = reinterpret_cast<HWND*>(lParam);

        DWORD process_id = 0;
        GetWindowThreadProcessId(hwnd, &process_id);
        if (process_id != GetCurrentProcessId())
        {
            return TRUE;
        }

        if (!IsWindowVisible(hwnd))
        {
            return TRUE;
        }

        if (GetWindow(hwnd, GW_OWNER) != nullptr)
        {
            return TRUE;
        }

        *target = hwnd;
        return FALSE;
    }
}

namespace helen
{
    WindowBehaviorHookSet* WindowBehaviorHookSet::active_instance_ = nullptr;

    /**
     * @brief Binds the hook set to the dispatcher that owns the generic `window.*` config keys.
     * @param dispatcher Dispatcher used to resolve the current focus and background-input settings.
     */
    WindowBehaviorHookSet::WindowBehaviorHookSet(CommandDispatcher& dispatcher)
        : dispatcher_(dispatcher)
    {
    }

    /**
     * @brief Restores any installed hooks and tracked WndProc subclass state.
     */
    WindowBehaviorHookSet::~WindowBehaviorHookSet()
    {
        Remove();
    }

    /**
     * @brief Returns the process-global active hook set used by the static detours.
     * @return Active instance or null when no window behavior hook set is installed.
     */
    WindowBehaviorHookSet* WindowBehaviorHookSet::Current()
    {
        return active_instance_;
    }

    /**
     * @brief Returns the currently tracked top-level process window for tests and diagnostics.
     * @return Tracked window handle or null when discovery has not succeeded.
     */
    HWND WindowBehaviorHookSet::DebugGetTrackedWindow() const noexcept
    {
        return tracked_window_;
    }

    /**
     * @brief Discovers and stores the process-owned top-level window that should receive spoofing and filtering.
     * @return True when one visible top-level process window is found; otherwise false.
     */
    bool WindowBehaviorHookSet::CaptureTrackedWindow()
    {
        tracked_window_ = nullptr;
        EnumWindows(&CollectProcessWindow, reinterpret_cast<LPARAM>(&tracked_window_));
        return tracked_window_ != nullptr;
    }

    /**
     * @brief Installs one optional user32 import hook only when the main executable imports the requested symbol.
     * @param hook Mutable IAT hook instance that should own the installed replacement.
     * @param module Main executable module whose import table should be patched.
     * @param imported_name Exact user32 import name that should be replaced.
     * @param replacement Detour function pointer that should replace the import.
     * @param import_present Receives true when the main executable imports the requested symbol.
     * @return True when the import was absent or the hook installed successfully; otherwise false.
     */
    bool WindowBehaviorHookSet::InstallOptionalHook(
        IatHook& hook,
        const ModuleView& module,
        std::string_view imported_name,
        void* replacement,
        bool& import_present)
    {
        void** const slot = FindImportAddress(module, "user32.dll", imported_name);
        import_present = slot != nullptr;
        if (!import_present)
        {
            return true;
        }

        return hook.Install(module, "user32.dll", imported_name, replacement);
    }

    /**
     * @brief Returns whether the current background-input config requires the supplied message to be filtered.
     * @param message Win32 message identifier received by the tracked window.
     * @return True when the message should be swallowed; otherwise false.
     */
    bool WindowBehaviorHookSet::ShouldFilterMessage(UINT message) const
    {
        const WindowBehaviorSettings settings = ReadWindowBehaviorSettings(dispatcher_);
        if (!settings.BackgroundInputEnabled)
        {
            return false;
        }

        return message == WM_ACTIVATE || message == WM_NCACTIVATE || message == WM_INPUT;
    }

    /**
     * @brief Reads the current tracked game window rect from user32.
     * @return Current tracked window rect or a zero rect when no tracked window is available.
     */
    RECT WindowBehaviorHookSet::ReadTrackedWindowRect() const
    {
        RECT rect{};
        if (tracked_window_ == nullptr)
        {
            return rect;
        }

        GetWindowRect(tracked_window_, &rect);
        return rect;
    }

    /**
     * @brief Calls the original `RegisterRawInputDevices` import when that IAT slot is hooked.
     * @param devices Raw-input device array passed by the caller.
     * @param device_count Number of entries in the raw-input device array.
     * @param struct_size Size of one raw-input device record supplied by the caller.
     * @return Result of the original or direct `RegisterRawInputDevices` call.
     */
    BOOL WindowBehaviorHookSet::CallOriginalRegisterRawInputDevices(
        PCRAWINPUTDEVICE devices,
        UINT device_count,
        UINT struct_size) const
    {
        if (register_raw_input_devices_hook_.IsInstalled())
        {
            using RegisterRawInputDevicesFunction = BOOL(WINAPI*)(PCRAWINPUTDEVICE, UINT, UINT);
            return register_raw_input_devices_hook_.Original<RegisterRawInputDevicesFunction>()(
                devices,
                device_count,
                struct_size);
        }

        return RegisterRawInputDevices(devices, device_count, struct_size);
    }

    /**
     * @brief Calls the original `ClipCursor` import when that IAT slot is hooked.
     * @param clip_rect Clip rectangle that should be passed to user32.
     * @return Result of the original or direct `ClipCursor` call.
     */
    BOOL WindowBehaviorHookSet::CallOriginalClipCursor(const RECT* clip_rect) const
    {
        if (clip_cursor_hook_.IsInstalled())
        {
            using ClipCursorFunction = BOOL(WINAPI*)(const RECT*);
            return clip_cursor_hook_.Original<ClipCursorFunction>()(clip_rect);
        }

        return ClipCursor(clip_rect);
    }

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
    BOOL WindowBehaviorHookSet::CallOriginalSetWindowPos(
        HWND hwnd,
        HWND insert_after,
        int x,
        int y,
        int cx,
        int cy,
        UINT flags) const
    {
        if (set_window_pos_hook_.IsInstalled())
        {
            using SetWindowPosFunction = BOOL(WINAPI*)(HWND, HWND, int, int, int, int, UINT);
            return set_window_pos_hook_.Original<SetWindowPosFunction>()(
                hwnd,
                insert_after,
                x,
                y,
                cx,
                cy,
                flags);
        }

        return SetWindowPos(hwnd, insert_after, x, y, cx, cy, flags);
    }

    /**
     * @brief Calls the original `SetWindowLongW` import when that IAT slot is hooked.
     * @param hwnd Window whose long value is being updated.
     * @param index Long slot requested by the caller.
     * @param new_long Replacement long value supplied by the caller.
     * @return Previous long value reported by the original or direct `SetWindowLongW` call.
     */
    LONG WindowBehaviorHookSet::CallOriginalSetWindowLongW(HWND hwnd, int index, LONG new_long) const
    {
        if (set_window_long_w_hook_.IsInstalled())
        {
            using SetWindowLongWFunction = LONG(WINAPI*)(HWND, int, LONG);
            return set_window_long_w_hook_.Original<SetWindowLongWFunction>()(hwnd, index, new_long);
        }

        return SetWindowLongW(hwnd, index, new_long);
    }

    /**
     * @brief Calls the original `SetWindowLongPtrW` import when that IAT slot is hooked.
     * @param hwnd Window whose long-pointer value is being updated.
     * @param index Long-pointer slot requested by the caller.
     * @param new_long Replacement value supplied by the caller.
     * @return Previous long-pointer value reported by the original or direct `SetWindowLongPtrW` call.
     */
    LONG_PTR WindowBehaviorHookSet::CallOriginalSetWindowLongPtrW(HWND hwnd, int index, LONG_PTR new_long) const
    {
        if (set_window_long_ptr_w_hook_.IsInstalled())
        {
            using SetWindowLongPtrWFunction = LONG_PTR(WINAPI*)(HWND, int, LONG_PTR);
            return set_window_long_ptr_w_hook_.Original<SetWindowLongPtrWFunction>()(hwnd, index, new_long);
        }

        return SetWindowLongPtrW(hwnd, index, new_long);
    }

    /**
     * @brief Reapplies the configured tracked window style, rect, and topmost policy when any related flags are enabled.
     * @return True when the policy is not needed or the tracked window is available for enforcement.
     */
    bool WindowBehaviorHookSet::TryApplyConfiguredWindowState()
    {
        if (tracked_window_ == nullptr || applying_window_state_)
        {
            return tracked_window_ != nullptr;
        }

        const WindowBehaviorSettings settings = ReadWindowBehaviorSettings(dispatcher_);
        if (!BuildWindowBehaviorHookPlan(settings).RequiresWindowRectEnforcement)
        {
            return true;
        }

        applying_window_state_ = true;

        if (settings.RemoveWindowFrameEnabled)
        {
            LONG style = GetWindowLongW(tracked_window_, GWL_STYLE);
            style &= ~WS_CAPTION;
            style &= ~WS_THICKFRAME;
            style &= ~WS_MINIMIZE;
            style &= ~WS_MAXIMIZE;
            style &= ~WS_SYSMENU;
            CallOriginalSetWindowLongW(tracked_window_, GWL_STYLE, style);
        }

        if (settings.ForcePositionEnabled || settings.ForceSizeEnabled || settings.ForceTopmostEnabled)
        {
            const UINT flags =
                (settings.ForcePositionEnabled ? 0U : SWP_NOMOVE) |
                (settings.ForceSizeEnabled ? 0U : SWP_NOSIZE) |
                SWP_FRAMECHANGED;
            CallOriginalSetWindowPos(
                tracked_window_,
                settings.ForceTopmostEnabled ? HWND_TOPMOST : nullptr,
                settings.PositionX,
                settings.PositionY,
                settings.Width,
                settings.Height,
                flags);
        }

        applying_window_state_ = false;
        return true;
    }

    /**
     * @brief Installs the requested focus spoof hooks and optional WndProc subclass for the tracked process window.
     * @return True when at least one requested behavior installed successfully; otherwise false.
     */
    bool WindowBehaviorHookSet::Install()
    {
        if (IsInstalled() || (active_instance_ != nullptr && active_instance_ != this))
        {
            return false;
        }

        if (!CaptureTrackedWindow())
        {
            return false;
        }

        const WindowBehaviorHookPlan plan = BuildWindowBehaviorHookPlan(ReadWindowBehaviorSettings(dispatcher_));
        if (!plan.RequiresFocusSpoofHooks &&
            !plan.RequiresWndProcSubclass &&
            !plan.RequiresRawInputRegistrationHook &&
            !plan.RequiresClipCursorHook &&
            !plan.RequiresWindowPolicyHooks)
        {
            tracked_window_ = nullptr;
            return false;
        }

        const std::optional<ModuleView> main_module = QueryMainModule();
        if (!main_module.has_value())
        {
            tracked_window_ = nullptr;
            return false;
        }

        active_instance_ = this;

        bool any_hook_installed = false;
        if (plan.RequiresFocusSpoofHooks)
        {
            bool import_present = false;
            if (!InstallOptionalHook(
                    get_foreground_window_hook_,
                    *main_module,
                    "GetForegroundWindow",
                    reinterpret_cast<void*>(&GetForegroundWindowDetour),
                    import_present))
            {
                Remove();
                return false;
            }

            any_hook_installed = any_hook_installed || import_present;

            if (!InstallOptionalHook(
                    get_focus_hook_,
                    *main_module,
                    "GetFocus",
                    reinterpret_cast<void*>(&GetFocusDetour),
                    import_present))
            {
                Remove();
                return false;
            }

            any_hook_installed = any_hook_installed || import_present;

            if (!InstallOptionalHook(
                    get_active_window_hook_,
                    *main_module,
                    "GetActiveWindow",
                    reinterpret_cast<void*>(&GetActiveWindowDetour),
                    import_present))
            {
                Remove();
                return false;
            }

            any_hook_installed = any_hook_installed || import_present;
        }

        if (plan.RequiresRawInputRegistrationHook)
        {
            bool import_present = false;
            if (!InstallOptionalHook(
                    register_raw_input_devices_hook_,
                    *main_module,
                    "RegisterRawInputDevices",
                    reinterpret_cast<void*>(&RegisterRawInputDevicesDetour),
                    import_present))
            {
                Remove();
                return false;
            }

            any_hook_installed = any_hook_installed || import_present;
        }

        if (plan.RequiresClipCursorHook)
        {
            bool import_present = false;
            if (!InstallOptionalHook(
                    clip_cursor_hook_,
                    *main_module,
                    "ClipCursor",
                    reinterpret_cast<void*>(&ClipCursorDetour),
                    import_present))
            {
                Remove();
                return false;
            }

            any_hook_installed = any_hook_installed || import_present;
        }

        if (plan.RequiresWindowPolicyHooks)
        {
            bool import_present = false;
            if (!InstallOptionalHook(
                    set_window_pos_hook_,
                    *main_module,
                    "SetWindowPos",
                    reinterpret_cast<void*>(&SetWindowPosDetour),
                    import_present))
            {
                Remove();
                return false;
            }

            any_hook_installed = any_hook_installed || import_present;

            if (!InstallOptionalHook(
                    set_window_long_w_hook_,
                    *main_module,
                    "SetWindowLongW",
                    reinterpret_cast<void*>(&SetWindowLongWDetour),
                    import_present))
            {
                Remove();
                return false;
            }

            any_hook_installed = any_hook_installed || import_present;

            if (!InstallOptionalHook(
                    set_window_long_ptr_w_hook_,
                    *main_module,
                    "SetWindowLongPtrW",
                    reinterpret_cast<void*>(&SetWindowLongPtrWDetour),
                    import_present))
            {
                Remove();
                return false;
            }

            any_hook_installed = any_hook_installed || import_present;
        }

        if (plan.RequiresWndProcSubclass)
        {
            original_window_proc_ = reinterpret_cast<WNDPROC>(
                CallOriginalSetWindowLongPtrW(tracked_window_, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&WindowProcDetour)));
            if (original_window_proc_ == nullptr)
            {
                Remove();
                return false;
            }

            any_hook_installed = true;
        }

        if (!any_hook_installed)
        {
            Remove();
            return false;
        }

        return true;
    }

    /**
     * @brief Removes every installed import hook and restores the original WndProc when present.
     */
    void WindowBehaviorHookSet::Remove()
    {
        if (tracked_window_ != nullptr && original_window_proc_ != nullptr && IsWindow(tracked_window_))
        {
            CallOriginalSetWindowLongPtrW(tracked_window_, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(original_window_proc_));
        }

        original_window_proc_ = nullptr;
        applying_window_state_ = false;
        set_window_long_ptr_w_hook_.Remove();
        set_window_long_w_hook_.Remove();
        set_window_pos_hook_.Remove();
        clip_cursor_hook_.Remove();
        register_raw_input_devices_hook_.Remove();
        get_active_window_hook_.Remove();
        get_focus_hook_.Remove();
        get_foreground_window_hook_.Remove();

        if (active_instance_ == this)
        {
            active_instance_ = nullptr;
        }

        tracked_window_ = nullptr;
    }

    /**
     * @brief Returns whether this instance currently owns any installed window behavior override.
     * @return True when at least one hook or WndProc subclass is active.
     */
    bool WindowBehaviorHookSet::IsInstalled() const noexcept
    {
        return active_instance_ == this &&
            (original_window_proc_ != nullptr ||
             register_raw_input_devices_hook_.IsInstalled() ||
             clip_cursor_hook_.IsInstalled() ||
             set_window_pos_hook_.IsInstalled() ||
             set_window_long_w_hook_.IsInstalled() ||
             set_window_long_ptr_w_hook_.IsInstalled() ||
             get_foreground_window_hook_.IsInstalled() ||
             get_focus_hook_.IsInstalled() ||
             get_active_window_hook_.IsInstalled());
    }

    /**
     * @brief IAT detour for `GetForegroundWindow` that returns the tracked game window when spoofing is enabled.
     * @return The tracked game window or null when no active hook set is installed.
     */
    HWND WINAPI WindowBehaviorHookSet::GetForegroundWindowDetour()
    {
        const WindowBehaviorHookSet* const current = Current();
        return current != nullptr ? current->tracked_window_ : nullptr;
    }

    /**
     * @brief IAT detour for `GetFocus` that returns the tracked game window when spoofing is enabled.
     * @return The tracked game window or null when no active hook set is installed.
     */
    HWND WINAPI WindowBehaviorHookSet::GetFocusDetour()
    {
        const WindowBehaviorHookSet* const current = Current();
        return current != nullptr ? current->tracked_window_ : nullptr;
    }

    /**
     * @brief IAT detour for `GetActiveWindow` that returns the tracked game window when spoofing is enabled.
     * @return The tracked game window or null when no active hook set is installed.
     */
    HWND WINAPI WindowBehaviorHookSet::GetActiveWindowDetour()
    {
        const WindowBehaviorHookSet* const current = Current();
        return current != nullptr ? current->tracked_window_ : nullptr;
    }

    /**
     * @brief IAT detour for `RegisterRawInputDevices` that optionally short-circuits raw-input registration.
     * @param devices Raw-input device array passed by the caller.
     * @param device_count Number of entries in the raw-input device array.
     * @param struct_size Size of one raw-input device record supplied by the caller.
     * @return True when registration is intentionally suppressed or when the direct user32 call succeeds.
     */
    BOOL WINAPI WindowBehaviorHookSet::RegisterRawInputDevicesDetour(PCRAWINPUTDEVICE devices, UINT device_count, UINT struct_size)
    {
        const WindowBehaviorHookSet* const current = Current();
        if (current != nullptr && ReadWindowBehaviorSettings(current->dispatcher_).BlockRawInputRegistrationEnabled)
        {
            return TRUE;
        }

        return current != nullptr
            ? current->CallOriginalRegisterRawInputDevices(devices, device_count, struct_size)
            : RegisterRawInputDevices(devices, device_count, struct_size);
    }

    /**
     * @brief IAT detour for `ClipCursor` that optionally enforces clipping against the tracked game window rect.
     * @param clip_rect Original clip rect requested by the caller.
     * @return True when the enforced or original clip request succeeds.
     */
    BOOL WINAPI WindowBehaviorHookSet::ClipCursorDetour(const RECT* clip_rect)
    {
        WindowBehaviorHookSet* const current = Current();
        if (current == nullptr)
        {
            return ClipCursor(clip_rect);
        }

        const WindowBehaviorSettings settings = ReadWindowBehaviorSettings(current->dispatcher_);
        if (!settings.ClipCursorEnabled || current->tracked_window_ == nullptr)
        {
            return current->CallOriginalClipCursor(clip_rect);
        }

        const RECT tracked_rect = current->ReadTrackedWindowRect();
        return current->CallOriginalClipCursor(&tracked_rect);
    }

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
    BOOL WINAPI WindowBehaviorHookSet::SetWindowPosDetour(HWND hwnd, HWND insert_after, int x, int y, int cx, int cy, UINT flags)
    {
        WindowBehaviorHookSet* const current = Current();
        const BOOL result = current != nullptr
            ? current->CallOriginalSetWindowPos(hwnd, insert_after, x, y, cx, cy, flags)
            : SetWindowPos(hwnd, insert_after, x, y, cx, cy, flags);
        if (current != nullptr && hwnd == current->tracked_window_ && !current->applying_window_state_)
        {
            current->TryApplyConfiguredWindowState();
        }

        return result;
    }

    /**
     * @brief IAT detour for `SetWindowLongW` that preserves the WndProc subclass and reapplies tracked style policy.
     * @param hwnd Window whose long value is being updated.
     * @param index Long slot requested by the caller.
     * @param new_long Replacement long value supplied by the caller.
     * @return Previous long value reported by the direct user32 `SetWindowLongW` call or the preserved WndProc value.
     */
    LONG WINAPI WindowBehaviorHookSet::SetWindowLongWDetour(HWND hwnd, int index, LONG new_long)
    {
        WindowBehaviorHookSet* const current = Current();
        if (current != nullptr && hwnd == current->tracked_window_ && index == GWLP_WNDPROC && current->original_window_proc_ != nullptr)
        {
            if (reinterpret_cast<WNDPROC>(new_long) != &WindowProcDetour)
            {
                current->original_window_proc_ = reinterpret_cast<WNDPROC>(new_long);
            }

            return reinterpret_cast<LONG>(current->original_window_proc_);
        }

        const LONG result = current != nullptr
            ? current->CallOriginalSetWindowLongW(hwnd, index, new_long)
            : SetWindowLongW(hwnd, index, new_long);
        if (current != nullptr && hwnd == current->tracked_window_ && !current->applying_window_state_)
        {
            current->TryApplyConfiguredWindowState();
        }

        return result;
    }

    /**
     * @brief IAT detour for `SetWindowLongPtrW` that preserves the WndProc subclass and reapplies tracked style policy.
     * @param hwnd Window whose long-pointer value is being updated.
     * @param index Long-pointer slot requested by the caller.
     * @param new_long Replacement value supplied by the caller.
     * @return Previous long-pointer value reported by the direct user32 `SetWindowLongPtrW` call or the preserved WndProc value.
     */
    LONG_PTR WINAPI WindowBehaviorHookSet::SetWindowLongPtrWDetour(HWND hwnd, int index, LONG_PTR new_long)
    {
        WindowBehaviorHookSet* const current = Current();
        if (current != nullptr && hwnd == current->tracked_window_ && index == GWLP_WNDPROC && current->original_window_proc_ != nullptr)
        {
            if (reinterpret_cast<WNDPROC>(new_long) != &WindowProcDetour)
            {
                current->original_window_proc_ = reinterpret_cast<WNDPROC>(new_long);
            }

            return reinterpret_cast<LONG_PTR>(current->original_window_proc_);
        }

        const LONG_PTR result = current != nullptr
            ? current->CallOriginalSetWindowLongPtrW(hwnd, index, new_long)
            : SetWindowLongPtrW(hwnd, index, new_long);
        if (current != nullptr && hwnd == current->tracked_window_ && !current->applying_window_state_)
        {
            current->TryApplyConfiguredWindowState();
        }

        return result;
    }

    /**
     * @brief WndProc subclass used to suppress selected deactivate and raw-input messages when background input is enabled.
     * @param hwnd Tracked top-level process window.
     * @param message Win32 message identifier received by the tracked window.
     * @param wParam First message parameter supplied by the OS.
     * @param lParam Second message parameter supplied by the OS.
     * @return Zero for filtered messages; otherwise the original WndProc result.
     */
    LRESULT CALLBACK WindowBehaviorHookSet::WindowProcDetour(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
    {
        WindowBehaviorHookSet* const current = Current();
        if (current == nullptr || current->original_window_proc_ == nullptr)
        {
            return DefWindowProcW(hwnd, message, wParam, lParam);
        }

        current->TryApplyConfiguredWindowState();
        if (current->ShouldFilterMessage(message))
        {
            return 0;
        }

        return CallWindowProcW(current->original_window_proc_, hwnd, message, wParam, lParam);
    }
}
