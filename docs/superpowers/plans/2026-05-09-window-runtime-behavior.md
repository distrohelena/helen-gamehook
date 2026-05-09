# Window Runtime Behavior Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a generic, flag-driven window behavior runtime service to Helen GameHook that supports focus spoofing, background input, raw-input suppression, cursor clipping, and window rect/frame control without enabling anything by default.

**Architecture:** Introduce one `WindowBehaviorHookSet` runtime service plus one small `WindowBehaviorConfig` helper layer. The config helper owns stable `window.*` key names, defaults, and hook-plan resolution, while the hook set owns process-local Win32 hooks, main-window discovery, WndProc subclassing, and policy enforcement. `HelenGameHook.cpp` registers the generic keys after pack config so pack-declared defaults win, then installs the optional hook set without turning window behavior failures into whole-runtime failures.

**Tech Stack:** C++20, Win32, existing `IatHook` and `InlineHook` utilities, JSON config via `CommandDispatcher`, MSBuild, `HelenRuntimeTests.exe`

---

## File Structure

- Create: `include/HelenHook/WindowBehaviorConfig.h`
  - Declares the stable `window.*` config keys, the typed `WindowBehaviorSettings` snapshot, and the derived `WindowBehaviorHookPlan`.
- Create: `HelenRuntime/WindowBehaviorConfig.cpp`
  - Registers missing window keys, reads dispatcher values into a typed snapshot, and derives which hooks are actually required.
- Create: `include/HelenHook/WindowBehaviorHookSet.h`
  - Declares the runtime-owned hook service for focus spoofing, WndProc filtering, raw-input suppression, cursor control, and window policy enforcement.
- Create: `HelenRuntime/WindowBehaviorHookSet.cpp`
  - Implements main-window discovery, optional import hooks, WndProc subclassing, and per-flag policy enforcement.
- Create: `tests/HelenRuntime.Tests/WindowBehaviorConfigTests.cpp`
  - Covers default key registration, pack-default preservation, and derived hook-plan behavior.
- Create: `tests/HelenRuntime.Tests/WindowBehaviorHookSetTests.cpp`
  - Covers focus spoofing, WndProc filtering, raw-input suppression, cursor clipping, and forced window-state behavior against a real Win32 test window.
- Modify: `HelenRuntime/HelenRuntime.vcxproj`
  - Adds the new runtime source files and public headers.
- Modify: `tests/HelenRuntime.Tests/HelenRuntime.Tests.vcxproj`
  - Adds the two new test files.
- Modify: `tests/HelenRuntime.Tests/TestMain.cpp`
  - Runs the new test suites.
- Modify: `HelenGameHook/HelenGameHook.cpp`
  - Owns the new runtime service, registers generic keys after pack config registration, and installs the optional hook set during active-pack initialization.
- Modify: `tests/HelenRuntime.Tests/PackRepositoryTests.cpp`
  - Extends the checked-in Batman graphics pack assertions with the new curated feature and config surface.
- Modify: `games/HelenBatmanAA/helengamehook/packs/batman-aa-graphics-options/pack.json`
  - Adds the first curated Batman toggles plus raw numeric rect keys.

### Task 1: Add the generic `window.*` config surface and hook-plan helper

**Files:**
- Create: `include/HelenHook/WindowBehaviorConfig.h`
- Create: `HelenRuntime/WindowBehaviorConfig.cpp`
- Create: `tests/HelenRuntime.Tests/WindowBehaviorConfigTests.cpp`
- Modify: `HelenRuntime/HelenRuntime.vcxproj`
- Modify: `tests/HelenRuntime.Tests/HelenRuntime.Tests.vcxproj`
- Modify: `tests/HelenRuntime.Tests/TestMain.cpp`

- [ ] **Step 1: Write the failing config and hook-plan tests**

Create `tests/HelenRuntime.Tests/WindowBehaviorConfigTests.cpp`:

```cpp
#include <HelenHook/CommandDispatcher.h>
#include <HelenHook/WindowBehaviorConfig.h>

#include <stdexcept>

namespace
{
    void Expect(bool condition, const char* message)
    {
        if (!condition)
        {
            throw std::runtime_error(message);
        }
    }
}

void RunWindowBehaviorConfigTests()
{
    {
        helen::CommandDispatcher dispatcher;
        dispatcher.RegisterConfigInt("window.focusSpoofEnabled", 1);
        dispatcher.RegisterConfigInt("window.width", 1600);

        helen::RegisterWindowBehaviorConfigKeys(dispatcher);
        const helen::WindowBehaviorSettings settings = helen::ReadWindowBehaviorSettings(dispatcher);

        Expect(settings.FocusSpoofEnabled, "Expected the pack-declared focus spoof default to survive generic key registration.");
        Expect(!settings.BackgroundInputEnabled, "Background input should remain disabled by default.");
        Expect(settings.Width == 1600, "Expected the pack-declared width default to survive generic key registration.");
        Expect(settings.Height == 0, "Expected undeclared numeric keys to default to zero.");
    }

    {
        helen::CommandDispatcher dispatcher;
        helen::RegisterWindowBehaviorConfigKeys(dispatcher);
        dispatcher.TrySetInt("window.backgroundInputEnabled", 1);
        dispatcher.TrySetInt("window.blockRawInputRegistrationEnabled", 1);
        dispatcher.TrySetInt("window.clipCursorEnabled", 1);
        dispatcher.TrySetInt("window.forcePositionEnabled", 1);
        dispatcher.TrySetInt("window.positionX", 48);
        dispatcher.TrySetInt("window.positionY", 64);

        const helen::WindowBehaviorSettings settings = helen::ReadWindowBehaviorSettings(dispatcher);
        const helen::WindowBehaviorHookPlan plan = helen::BuildWindowBehaviorHookPlan(settings);

        Expect(plan.RequiresWndProcSubclass, "Background input should require WndProc subclassing.");
        Expect(plan.RequiresRawInputRegistrationHook, "Raw-input suppression should require RegisterRawInputDevices interception.");
        Expect(plan.RequiresClipCursorHook, "Cursor clipping should require ClipCursor interception.");
        Expect(plan.RequiresWindowPolicyHooks, "Forced position should require window policy hooks.");
        Expect(plan.RequiresWindowRectEnforcement, "Forced position should require runtime rect enforcement.");
        Expect(!plan.RequiresFocusSpoofHooks, "Focus spoof hooks should stay disabled when the flags are off.");
    }
}
```

Add the declaration and runner call to `tests/HelenRuntime.Tests/TestMain.cpp`:

```cpp
void RunWindowBehaviorConfigTests();
```

```cpp
        RunWindowBehaviorConfigTests();
```

Add the test file to `tests/HelenRuntime.Tests/HelenRuntime.Tests.vcxproj`:

```xml
<ClCompile Include="WindowBehaviorConfigTests.cpp" />
```

- [ ] **Step 2: Run the tests to verify they fail**

Run:

```powershell
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' 'tests\HelenRuntime.Tests\HelenRuntime.Tests.vcxproj' /p:Configuration=Debug /p:Platform=Win32
```

Expected: FAIL because `WindowBehaviorConfig.h` and `WindowBehaviorConfig.cpp` do not exist yet.

- [ ] **Step 3: Add the typed config snapshot and derived hook plan**

Create `include/HelenHook/WindowBehaviorConfig.h`:

```cpp
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

    class WindowBehaviorSettings
    {
    public:
        bool FocusSpoofEnabled = false;
        bool ActiveWindowSpoofEnabled = false;
        bool BackgroundInputEnabled = false;
        bool BlockRawInputRegistrationEnabled = false;
        bool ClipCursorEnabled = false;
        bool RemoveWindowFrameEnabled = false;
        bool ForcePositionEnabled = false;
        bool ForceSizeEnabled = false;
        bool ForceTopmostEnabled = false;
        int PositionX = 0;
        int PositionY = 0;
        int Width = 0;
        int Height = 0;
    };

    class WindowBehaviorHookPlan
    {
    public:
        bool RequiresFocusSpoofHooks = false;
        bool RequiresWndProcSubclass = false;
        bool RequiresRawInputRegistrationHook = false;
        bool RequiresClipCursorHook = false;
        bool RequiresWindowPolicyHooks = false;
        bool RequiresWindowRectEnforcement = false;
    };

    void RegisterWindowBehaviorConfigKeys(CommandDispatcher& dispatcher);
    WindowBehaviorSettings ReadWindowBehaviorSettings(const CommandDispatcher& dispatcher);
    WindowBehaviorHookPlan BuildWindowBehaviorHookPlan(const WindowBehaviorSettings& settings);
}
```

Create `HelenRuntime/WindowBehaviorConfig.cpp`:

```cpp
#include <HelenHook/WindowBehaviorConfig.h>

#include <HelenHook/CommandDispatcher.h>

#include <array>
#include <string>

namespace
{
    bool ReadFlag(const helen::CommandDispatcher& dispatcher, std::string_view key)
    {
        const std::optional<int> value = dispatcher.TryGetInt(std::string(key));
        return value.has_value() && *value != 0;
    }

    int ReadIntOrDefault(const helen::CommandDispatcher& dispatcher, std::string_view key, int default_value)
    {
        const std::optional<int> value = dispatcher.TryGetInt(std::string(key));
        return value.has_value() ? *value : default_value;
    }
}

namespace helen
{
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
```

Add the files to `HelenRuntime/HelenRuntime.vcxproj`:

```xml
<ClCompile Include="WindowBehaviorConfig.cpp" />
<ClInclude Include="..\include\HelenHook\WindowBehaviorConfig.h" />
```

- [ ] **Step 4: Re-run the runtime tests**

Run:

```powershell
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' 'tests\HelenRuntime.Tests\HelenRuntime.Tests.vcxproj' /p:Configuration=Debug /p:Platform=Win32
& 'C:\dev\helenhook\bin\Win32\Debug\tests\HelenRuntimeTests.exe'
```

Expected: PASS with the new window-config test coverage included.

- [ ] **Step 5: Commit**

```bash
git add include/HelenHook/WindowBehaviorConfig.h HelenRuntime/WindowBehaviorConfig.cpp tests/HelenRuntime.Tests/WindowBehaviorConfigTests.cpp tests/HelenRuntime.Tests/TestMain.cpp tests/HelenRuntime.Tests/HelenRuntime.Tests.vcxproj HelenRuntime/HelenRuntime.vcxproj
git commit -m "Add window behavior config surface"
```

### Task 2: Implement focus spoofing and background-input WndProc filtering

**Files:**
- Create: `include/HelenHook/WindowBehaviorHookSet.h`
- Create: `HelenRuntime/WindowBehaviorHookSet.cpp`
- Create: `tests/HelenRuntime.Tests/WindowBehaviorHookSetTests.cpp`
- Modify: `HelenRuntime/HelenRuntime.vcxproj`
- Modify: `tests/HelenRuntime.Tests/HelenRuntime.Tests.vcxproj`
- Modify: `tests/HelenRuntime.Tests/TestMain.cpp`

- [ ] **Step 1: Write the failing focus-spoof and WndProc tests**

Create `tests/HelenRuntime.Tests/WindowBehaviorHookSetTests.cpp`:

```cpp
#include <HelenHook/CommandDispatcher.h>
#include <HelenHook/WindowBehaviorConfig.h>
#include <HelenHook/WindowBehaviorHookSet.h>

#include <windows.h>

#include <stdexcept>

namespace
{
    bool g_received_activate = false;
    bool g_received_custom = false;

    void Expect(bool condition, const char* message)
    {
        if (!condition)
        {
            throw std::runtime_error(message);
        }
    }

    LRESULT CALLBACK TestWindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
    {
        if (message == WM_ACTIVATE)
        {
            g_received_activate = true;
        }

        if (message == WM_APP + 42)
        {
            g_received_custom = true;
        }

        return DefWindowProcW(hwnd, message, wParam, lParam);
    }

    HWND CreateTestWindow()
    {
        WNDCLASSW window_class{};
        window_class.lpfnWndProc = &TestWindowProc;
        window_class.hInstance = GetModuleHandleW(nullptr);
        window_class.lpszClassName = L"HelenWindowBehaviorHookSetTests";
        RegisterClassW(&window_class);

        const HWND hwnd = CreateWindowExW(
            0,
            window_class.lpszClassName,
            L"Helen Window Behavior Hook Test",
            WS_OVERLAPPEDWINDOW,
            40,
            60,
            320,
            200,
            nullptr,
            nullptr,
            window_class.hInstance,
            nullptr);
        ShowWindow(hwnd, SW_SHOWNOACTIVATE);
        UpdateWindow(hwnd);
        return hwnd;
    }
}

void RunWindowBehaviorHookSetTests()
{
    const HWND hwnd = CreateTestWindow();
    Expect(hwnd != nullptr, "Expected the window behavior hook test window to be created.");

    helen::CommandDispatcher dispatcher;
    helen::RegisterWindowBehaviorConfigKeys(dispatcher);
    dispatcher.TrySetInt("window.focusSpoofEnabled", 1);
    dispatcher.TrySetInt("window.activeWindowSpoofEnabled", 1);
    dispatcher.TrySetInt("window.backgroundInputEnabled", 1);

    helen::WindowBehaviorHookSet hook_set(dispatcher);
    Expect(hook_set.Install(), "Expected the window behavior hook set to install with focus spoofing and background input enabled.");
    Expect(hook_set.IsInstalled(), "Expected the window behavior hook set to report an installed state.");
    Expect(hook_set.DebugGetTrackedWindow() == hwnd, "Expected the hook set to discover the process-owned visible test window.");
    Expect(GetForegroundWindow() == hwnd, "Expected GetForegroundWindow to be spoofed to the tracked game window.");
    Expect(GetFocus() == hwnd, "Expected GetFocus to be spoofed to the tracked game window.");
    Expect(GetActiveWindow() == hwnd, "Expected GetActiveWindow to be spoofed to the tracked game window.");

    g_received_activate = false;
    const LRESULT activate_result = SendMessageW(hwnd, WM_ACTIVATE, WA_INACTIVE, 0);
    Expect(activate_result == 0, "Expected the background-input WndProc filter to short-circuit WM_ACTIVATE.");
    Expect(!g_received_activate, "Expected WM_ACTIVATE to stay out of the original test window proc while background input is enabled.");

    g_received_custom = false;
    SendMessageW(hwnd, WM_APP + 42, 0, 0);
    Expect(g_received_custom, "Expected unrelated messages to keep flowing through the original window proc.");

    hook_set.Remove();
    Expect(!hook_set.IsInstalled(), "Expected the window behavior hook set to report a removed state.");
    DestroyWindow(hwnd);
}
```

Add the declaration and runner call to `tests/HelenRuntime.Tests/TestMain.cpp`:

```cpp
void RunWindowBehaviorHookSetTests();
```

```cpp
        RunWindowBehaviorHookSetTests();
```

Add the file to `tests/HelenRuntime.Tests/HelenRuntime.Tests.vcxproj`:

```xml
<ClCompile Include="WindowBehaviorHookSetTests.cpp" />
```

- [ ] **Step 2: Run the tests to verify they fail**

Run:

```powershell
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' 'tests\HelenRuntime.Tests\HelenRuntime.Tests.vcxproj' /p:Configuration=Debug /p:Platform=Win32
```

Expected: FAIL because `WindowBehaviorHookSet` does not exist yet.

- [ ] **Step 3: Add the hook-set interface**

Create `include/HelenHook/WindowBehaviorHookSet.h`:

```cpp
#pragma once

#include <windows.h>

#include <HelenHook/Hook.h>
#include <HelenHook/Memory.h>

namespace helen
{
    class CommandDispatcher;

    class WindowBehaviorHookSet
    {
    public:
        explicit WindowBehaviorHookSet(CommandDispatcher& dispatcher);
        ~WindowBehaviorHookSet();

        WindowBehaviorHookSet(const WindowBehaviorHookSet&) = delete;
        WindowBehaviorHookSet& operator=(const WindowBehaviorHookSet&) = delete;

        bool Install();
        void Remove();
        bool IsInstalled() const noexcept;

        HWND DebugGetTrackedWindow() const noexcept;

    private:
        static WindowBehaviorHookSet* Current();
        static HMODULE WINAPI GetForegroundWindowDetour();
        static HWND WINAPI GetFocusDetour();
        static HWND WINAPI GetActiveWindowDetour();
        static LRESULT CALLBACK WindowProcDetour(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

        bool CaptureTrackedWindow();
        bool InstallOptionalHook(IatHook& hook, const ModuleView& module, std::string_view imported_name, void* replacement, bool& import_present);
        bool ShouldFilterMessage(UINT message) const;

        CommandDispatcher& dispatcher_;
        HWND tracked_window_{};
        WNDPROC original_window_proc_{};
        IatHook get_foreground_window_hook_;
        IatHook get_focus_hook_;
        IatHook get_active_window_hook_;

        static WindowBehaviorHookSet* active_instance_;
    };
}
```

Add the files to `HelenRuntime/HelenRuntime.vcxproj`:

```xml
<ClCompile Include="WindowBehaviorHookSet.cpp" />
<ClInclude Include="..\include\HelenHook\WindowBehaviorHookSet.h" />
```

- [ ] **Step 4: Implement focus spoofing and WndProc subclassing**

Create `HelenRuntime/WindowBehaviorHookSet.cpp`:

```cpp
#include <HelenHook/WindowBehaviorHookSet.h>

#include <HelenHook/CommandDispatcher.h>
#include <HelenHook/WindowBehaviorConfig.h>

#include <optional>

namespace
{
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

    WindowBehaviorHookSet::WindowBehaviorHookSet(CommandDispatcher& dispatcher)
        : dispatcher_(dispatcher)
    {
    }

    WindowBehaviorHookSet::~WindowBehaviorHookSet()
    {
        Remove();
    }

    WindowBehaviorHookSet* WindowBehaviorHookSet::Current()
    {
        return active_instance_;
    }

    HWND WindowBehaviorHookSet::DebugGetTrackedWindow() const noexcept
    {
        return tracked_window_;
    }

    bool WindowBehaviorHookSet::CaptureTrackedWindow()
    {
        tracked_window_ = nullptr;
        EnumWindows(&CollectProcessWindow, reinterpret_cast<LPARAM>(&tracked_window_));
        return tracked_window_ != nullptr;
    }

    bool WindowBehaviorHookSet::InstallOptionalHook(IatHook& hook, const ModuleView& module, std::string_view imported_name, void* replacement, bool& import_present)
    {
        void** const slot = FindImportAddress(module, "user32.dll", imported_name);
        import_present = slot != nullptr;
        return !import_present || hook.Install(module, "user32.dll", imported_name, replacement);
    }

    bool WindowBehaviorHookSet::ShouldFilterMessage(UINT message) const
    {
        const WindowBehaviorSettings settings = ReadWindowBehaviorSettings(dispatcher_);
        if (!settings.BackgroundInputEnabled)
        {
            return false;
        }

        return message == WM_ACTIVATE || message == WM_NCACTIVATE || message == WM_INPUT;
    }

    bool WindowBehaviorHookSet::Install()
    {
        if (active_instance_ != nullptr || !CaptureTrackedWindow())
        {
            return false;
        }

        const WindowBehaviorHookPlan plan = BuildWindowBehaviorHookPlan(ReadWindowBehaviorSettings(dispatcher_));
        if (!plan.RequiresFocusSpoofHooks && !plan.RequiresWndProcSubclass)
        {
            return false;
        }

        const std::optional<ModuleView> main_module = QueryMainModule();
        if (!main_module.has_value())
        {
            return false;
        }

        active_instance_ = this;

        if (plan.RequiresFocusSpoofHooks)
        {
            bool import_present = false;
            if (!InstallOptionalHook(get_foreground_window_hook_, *main_module, "GetForegroundWindow", reinterpret_cast<void*>(&GetForegroundWindowDetour), import_present) ||
                !InstallOptionalHook(get_focus_hook_, *main_module, "GetFocus", reinterpret_cast<void*>(&GetFocusDetour), import_present) ||
                !InstallOptionalHook(get_active_window_hook_, *main_module, "GetActiveWindow", reinterpret_cast<void*>(&GetActiveWindowDetour), import_present))
            {
                Remove();
                return false;
            }
        }

        if (plan.RequiresWndProcSubclass)
        {
            original_window_proc_ = reinterpret_cast<WNDPROC>(
                SetWindowLongPtrW(tracked_window_, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&WindowProcDetour)));
            if (original_window_proc_ == nullptr)
            {
                Remove();
                return false;
            }
        }

        return IsInstalled();
    }

    void WindowBehaviorHookSet::Remove()
    {
        if (tracked_window_ != nullptr && original_window_proc_ != nullptr)
        {
            SetWindowLongPtrW(tracked_window_, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(original_window_proc_));
        }

        original_window_proc_ = nullptr;
        tracked_window_ = nullptr;
        get_active_window_hook_.Remove();
        get_focus_hook_.Remove();
        get_foreground_window_hook_.Remove();

        if (active_instance_ == this)
        {
            active_instance_ = nullptr;
        }
    }

    bool WindowBehaviorHookSet::IsInstalled() const noexcept
    {
        return active_instance_ == this &&
            (original_window_proc_ != nullptr ||
             get_foreground_window_hook_.IsInstalled() ||
             get_focus_hook_.IsInstalled() ||
             get_active_window_hook_.IsInstalled());
    }

    HMODULE WINAPI WindowBehaviorHookSet::GetForegroundWindowDetour()
    {
        const WindowBehaviorHookSet* current = Current();
        return reinterpret_cast<HMODULE>(current != nullptr ? current->tracked_window_ : nullptr);
    }

    HWND WINAPI WindowBehaviorHookSet::GetFocusDetour()
    {
        const WindowBehaviorHookSet* current = Current();
        return current != nullptr ? current->tracked_window_ : nullptr;
    }

    HWND WINAPI WindowBehaviorHookSet::GetActiveWindowDetour()
    {
        const WindowBehaviorHookSet* current = Current();
        return current != nullptr ? current->tracked_window_ : nullptr;
    }

    LRESULT CALLBACK WindowBehaviorHookSet::WindowProcDetour(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
    {
        WindowBehaviorHookSet* const current = Current();
        if (current == nullptr || current->original_window_proc_ == nullptr)
        {
            return DefWindowProcW(hwnd, message, wParam, lParam);
        }

        if (current->ShouldFilterMessage(message))
        {
            return 0;
        }

        return CallWindowProcW(current->original_window_proc_, hwnd, message, wParam, lParam);
    }
}
```

- [ ] **Step 5: Re-run the runtime tests**

Run:

```powershell
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' 'tests\HelenRuntime.Tests\HelenRuntime.Tests.vcxproj' /p:Configuration=Debug /p:Platform=Win32
& 'C:\dev\helenhook\bin\Win32\Debug\tests\HelenRuntimeTests.exe'
```

Expected: PASS with focus spoofing and WndProc filtering covered by the new test window harness.

- [ ] **Step 6: Commit**

```bash
git add include/HelenHook/WindowBehaviorHookSet.h HelenRuntime/WindowBehaviorHookSet.cpp tests/HelenRuntime.Tests/WindowBehaviorHookSetTests.cpp tests/HelenRuntime.Tests/TestMain.cpp tests/HelenRuntime.Tests/HelenRuntime.Tests.vcxproj HelenRuntime/HelenRuntime.vcxproj
git commit -m "Add window behavior focus and background hooks"
```

### Task 3: Extend the hook set with raw-input blocking, cursor clipping, and forced window state

**Files:**
- Modify: `include/HelenHook/WindowBehaviorHookSet.h`
- Modify: `HelenRuntime/WindowBehaviorHookSet.cpp`
- Modify: `tests/HelenRuntime.Tests/WindowBehaviorHookSetTests.cpp`

- [ ] **Step 1: Add the failing raw-input, cursor, and window-policy tests**

Append to `tests/HelenRuntime.Tests/WindowBehaviorHookSetTests.cpp`:

```cpp
    {
        const HWND window_policy_hwnd = CreateTestWindow();
        Expect(window_policy_hwnd != nullptr, "Expected the window-policy test window to be created.");

        helen::CommandDispatcher dispatcher;
        helen::RegisterWindowBehaviorConfigKeys(dispatcher);
        dispatcher.TrySetInt("window.backgroundInputEnabled", 1);
        dispatcher.TrySetInt("window.blockRawInputRegistrationEnabled", 1);
        dispatcher.TrySetInt("window.clipCursorEnabled", 1);
        dispatcher.TrySetInt("window.removeWindowFrameEnabled", 1);
        dispatcher.TrySetInt("window.forcePositionEnabled", 1);
        dispatcher.TrySetInt("window.forceSizeEnabled", 1);
        dispatcher.TrySetInt("window.positionX", 48);
        dispatcher.TrySetInt("window.positionY", 64);
        dispatcher.TrySetInt("window.width", 320);
        dispatcher.TrySetInt("window.height", 200);

        helen::WindowBehaviorHookSet hook_set(dispatcher);
        Expect(hook_set.Install(), "Expected the window behavior hook set to install with raw-input blocking and window policy enabled.");

        SendMessageW(window_policy_hwnd, WM_APP + 7, 0, 0);

        RECT window_rect{};
        GetWindowRect(window_policy_hwnd, &window_rect);
        Expect(window_rect.left == 48 && window_rect.top == 64, "Expected the forced window position to be applied.");
        Expect((window_rect.right - window_rect.left) == 320, "Expected the forced window width to be applied.");
        Expect((window_rect.bottom - window_rect.top) == 200, "Expected the forced window height to be applied.");
        Expect((GetWindowLongPtrW(window_policy_hwnd, GWL_STYLE) & WS_CAPTION) == 0, "Expected the forced borderless policy to remove WS_CAPTION.");

        RAWINPUTDEVICE dummy_device{};
        Expect(RegisterRawInputDevices(&dummy_device, 1, 0), "Expected the raw-input registration detour to short-circuit an otherwise invalid registration call.");

        Expect(ClipCursor(nullptr), "Expected ClipCursor to succeed through the window behavior detour.");
        RECT clip_rect{};
        Expect(GetClipCursor(&clip_rect), "Expected GetClipCursor to return the enforced clip rect.");
        Expect(
            clip_rect.left == window_rect.left &&
                clip_rect.top == window_rect.top &&
                clip_rect.right == window_rect.right &&
                clip_rect.bottom == window_rect.bottom,
            "Expected the cursor clip rect to match the tracked game window rect.");

        hook_set.Remove();
        ClipCursor(nullptr);
        DestroyWindow(window_policy_hwnd);
    }
```

- [ ] **Step 2: Run the tests to verify they fail**

Run:

```powershell
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' 'tests\HelenRuntime.Tests\HelenRuntime.Tests.vcxproj' /p:Configuration=Debug /p:Platform=Win32
& 'C:\dev\helenhook\bin\Win32\Debug\tests\HelenRuntimeTests.exe'
```

Expected: FAIL because the current hook set only implements focus spoofing and background input.

- [ ] **Step 3: Extend the hook-set state and detour list**

Update `include/HelenHook/WindowBehaviorHookSet.h`:

```cpp
        static BOOL WINAPI RegisterRawInputDevicesDetour(PCRAWINPUTDEVICE devices, UINT device_count, UINT struct_size);
        static BOOL WINAPI ClipCursorDetour(const RECT* clip_rect);
        static BOOL WINAPI SetWindowPosDetour(HWND hwnd, HWND insert_after, int x, int y, int cx, int cy, UINT flags);
        static LONG WINAPI SetWindowLongWDetour(HWND hwnd, int index, LONG new_long);
        static LONG_PTR WINAPI SetWindowLongPtrWDetour(HWND hwnd, int index, LONG_PTR new_long);

        bool TryApplyConfiguredWindowState();
        RECT ReadTrackedWindowRect() const;
```

Add the new hook members:

```cpp
        IatHook register_raw_input_devices_hook_;
        IatHook clip_cursor_hook_;
        IatHook set_window_pos_hook_;
        IatHook set_window_long_w_hook_;
        IatHook set_window_long_ptr_w_hook_;
        bool applying_window_state_ = false;
```

- [ ] **Step 4: Implement the raw-input, cursor, and window policy detours**

Update `HelenRuntime/WindowBehaviorHookSet.cpp`:

```cpp
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
            SetWindowLongW(tracked_window_, GWL_STYLE, style);
        }

        if (settings.ForcePositionEnabled || settings.ForceSizeEnabled || settings.ForceTopmostEnabled)
        {
            const UINT flags =
                (settings.ForcePositionEnabled ? 0 : SWP_NOMOVE) |
                (settings.ForceSizeEnabled ? 0 : SWP_NOSIZE) |
                SWP_FRAMECHANGED;
            SetWindowPos(
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
```

Add hook installation inside `Install()`:

```cpp
        if (plan.RequiresRawInputRegistrationHook)
        {
            if (!InstallOptionalHook(register_raw_input_devices_hook_, *main_module, "RegisterRawInputDevices", reinterpret_cast<void*>(&RegisterRawInputDevicesDetour), import_present))
            {
                Remove();
                return false;
            }
        }

        if (plan.RequiresClipCursorHook)
        {
            if (!InstallOptionalHook(clip_cursor_hook_, *main_module, "ClipCursor", reinterpret_cast<void*>(&ClipCursorDetour), import_present))
            {
                Remove();
                return false;
            }
        }

        if (plan.RequiresWindowPolicyHooks)
        {
            if (!InstallOptionalHook(set_window_pos_hook_, *main_module, "SetWindowPos", reinterpret_cast<void*>(&SetWindowPosDetour), import_present) ||
                !InstallOptionalHook(set_window_long_w_hook_, *main_module, "SetWindowLongW", reinterpret_cast<void*>(&SetWindowLongWDetour), import_present) ||
                !InstallOptionalHook(set_window_long_ptr_w_hook_, *main_module, "SetWindowLongPtrW", reinterpret_cast<void*>(&SetWindowLongPtrWDetour), import_present))
            {
                Remove();
                return false;
            }
        }
```

Add the detours:

```cpp
    BOOL WINAPI WindowBehaviorHookSet::RegisterRawInputDevicesDetour(PCRAWINPUTDEVICE devices, UINT device_count, UINT struct_size)
    {
        const WindowBehaviorHookSet* current = Current();
        if (current != nullptr && ReadWindowBehaviorSettings(current->dispatcher_).BlockRawInputRegistrationEnabled)
        {
            return TRUE;
        }

        return RegisterRawInputDevices(devices, device_count, struct_size);
    }

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
            return ClipCursor(clip_rect);
        }

        const RECT tracked_rect = current->ReadTrackedWindowRect();
        return ClipCursor(&tracked_rect);
    }
```

At the top of `WindowProcDetour`, enforce the configured state before message filtering:

```cpp
        current->TryApplyConfiguredWindowState();
```

Clear the new hooks in `Remove()`:

```cpp
        set_window_long_ptr_w_hook_.Remove();
        set_window_long_w_hook_.Remove();
        set_window_pos_hook_.Remove();
        clip_cursor_hook_.Remove();
        register_raw_input_devices_hook_.Remove();
```

- [ ] **Step 5: Re-run the runtime tests**

Run:

```powershell
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' 'tests\HelenRuntime.Tests\HelenRuntime.Tests.vcxproj' /p:Configuration=Debug /p:Platform=Win32
& 'C:\dev\helenhook\bin\Win32\Debug\tests\HelenRuntimeTests.exe'
```

Expected: PASS with raw-input blocking, clip rect enforcement, and forced borderless position/size behavior covered by the hook-set harness.

- [ ] **Step 6: Commit**

```bash
git add include/HelenHook/WindowBehaviorHookSet.h HelenRuntime/WindowBehaviorHookSet.cpp tests/HelenRuntime.Tests/WindowBehaviorHookSetTests.cpp
git commit -m "Add window behavior raw input and window policies"
```

### Task 4: Wire the service into `HelenGameHook` and expose the first Batman pack toggles

**Files:**
- Modify: `HelenGameHook/HelenGameHook.cpp`
- Modify: `tests/HelenRuntime.Tests/PackRepositoryTests.cpp`
- Modify: `games/HelenBatmanAA/helengamehook/packs/batman-aa-graphics-options/pack.json`

- [ ] **Step 1: Add the failing checked-in Batman pack assertions**

Extend `tests/HelenRuntime.Tests/PackRepositoryTests.cpp` after the checked-in Batman graphics pack load:

```cpp
        bool found_focus_spoof_config = false;
        bool found_background_input_config = false;
        bool found_clip_cursor_config = false;
        bool found_force_width_config = false;
        bool found_focus_spoof_feature = false;
        bool found_background_input_feature = false;
        bool found_clip_cursor_feature = false;

        for (const helen::ConfigEntryDefinition& config_entry : loaded_batman_pack->Pack.ConfigEntries)
        {
            if (config_entry.Key == "window.focusSpoofEnabled")
            {
                found_focus_spoof_config = true;
            }
            else if (config_entry.Key == "window.backgroundInputEnabled")
            {
                found_background_input_config = true;
            }
            else if (config_entry.Key == "window.clipCursorEnabled")
            {
                found_clip_cursor_config = true;
            }
            else if (config_entry.Key == "window.width")
            {
                found_force_width_config = true;
            }
        }

        for (const helen::FeatureDefinition& feature : loaded_batman_pack->Pack.Features)
        {
            if (feature.Id == "windowFocusSpoof")
            {
                found_focus_spoof_feature = true;
            }
            else if (feature.Id == "windowBackgroundInput")
            {
                found_background_input_feature = true;
            }
            else if (feature.Id == "windowClipCursor")
            {
                found_clip_cursor_feature = true;
            }
        }

        Expect(found_focus_spoof_config, "Checked-in Batman graphics pack is missing the window.focusSpoofEnabled config entry.");
        Expect(found_background_input_config, "Checked-in Batman graphics pack is missing the window.backgroundInputEnabled config entry.");
        Expect(found_clip_cursor_config, "Checked-in Batman graphics pack is missing the window.clipCursorEnabled config entry.");
        Expect(found_force_width_config, "Checked-in Batman graphics pack is missing the raw window.width config entry.");
        Expect(found_focus_spoof_feature, "Checked-in Batman graphics pack is missing the curated focus spoof feature.");
        Expect(found_background_input_feature, "Checked-in Batman graphics pack is missing the curated background input feature.");
        Expect(found_clip_cursor_feature, "Checked-in Batman graphics pack is missing the curated clip cursor feature.");
```

- [ ] **Step 2: Run the tests to verify they fail**

Run:

```powershell
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' 'tests\HelenRuntime.Tests\HelenRuntime.Tests.vcxproj' /p:Configuration=Debug /p:Platform=Win32
& 'C:\dev\helenhook\bin\Win32\Debug\tests\HelenRuntimeTests.exe'
```

Expected: FAIL because the checked-in Batman graphics pack does not declare the new window keys or features yet, and `HelenGameHook.cpp` does not own the new service.

- [ ] **Step 3: Register the generic keys after pack config and install the optional hook set**

Update `HelenGameHook/HelenGameHook.cpp` globals:

```cpp
#include <HelenHook/WindowBehaviorConfig.h>
#include <HelenHook/WindowBehaviorHookSet.h>
```

```cpp
    /** @brief Optional window behavior hook set driven by `window.*` config keys. */
    std::unique_ptr<helen::WindowBehaviorHookSet> g_window_behavior_hooks;
```

After `RegisterDeclaredConfigEntries(active_pack_set.ConfigEntries)` in `InitializeActivePackRuntime(...)`, register the generic keys so undeclared raw keys still exist while pack-declared defaults remain authoritative:

```cpp
        helen::RegisterWindowBehaviorConfigKeys(*g_command_dispatcher);
```

Then install the hook set narrowly:

```cpp
        const helen::WindowBehaviorSettings window_settings =
            helen::ReadWindowBehaviorSettings(*g_command_dispatcher);
        const helen::WindowBehaviorHookPlan window_plan =
            helen::BuildWindowBehaviorHookPlan(window_settings);
        if (window_plan.RequiresFocusSpoofHooks ||
            window_plan.RequiresWndProcSubclass ||
            window_plan.RequiresRawInputRegistrationHook ||
            window_plan.RequiresClipCursorHook ||
            window_plan.RequiresWindowPolicyHooks)
        {
            g_window_behavior_hooks = std::make_unique<helen::WindowBehaviorHookSet>(*g_command_dispatcher);
            if (!g_window_behavior_hooks->Install())
            {
                helen::Log(L"[runtime] window behavior hook set failed to install; continuing without window behavior overrides.");
                g_window_behavior_hooks.reset();
            }
        }
```

Reset and release it during teardown alongside the other optional services:

```cpp
        g_window_behavior_hooks.reset();
```

```cpp
        static_cast<void>(g_window_behavior_hooks.release());
```

- [ ] **Step 4: Add the first curated Batman toggles and raw numeric keys**

Update `games/HelenBatmanAA/helengamehook/packs/batman-aa-graphics-options/pack.json` by extending `config`:

```json
                   {
                       "key":  "window.focusSpoofEnabled",
                       "type":  "int",
                       "defaultValue":  0
                   },
                   {
                       "key":  "window.activeWindowSpoofEnabled",
                       "type":  "int",
                       "defaultValue":  0
                   },
                   {
                       "key":  "window.backgroundInputEnabled",
                       "type":  "int",
                       "defaultValue":  0
                   },
                   {
                       "key":  "window.blockRawInputRegistrationEnabled",
                       "type":  "int",
                       "defaultValue":  0
                   },
                   {
                       "key":  "window.clipCursorEnabled",
                       "type":  "int",
                       "defaultValue":  0
                   },
                   {
                       "key":  "window.removeWindowFrameEnabled",
                       "type":  "int",
                       "defaultValue":  0
                   },
                   {
                       "key":  "window.forcePositionEnabled",
                       "type":  "int",
                       "defaultValue":  0
                   },
                   {
                       "key":  "window.forceSizeEnabled",
                       "type":  "int",
                       "defaultValue":  0
                   },
                   {
                       "key":  "window.forceTopmostEnabled",
                       "type":  "int",
                       "defaultValue":  0
                   },
                   {
                       "key":  "window.positionX",
                       "type":  "int",
                       "defaultValue":  0
                   },
                   {
                       "key":  "window.positionY",
                       "type":  "int",
                       "defaultValue":  0
                   },
                   {
                       "key":  "window.width",
                       "type":  "int",
                       "defaultValue":  0
                   },
                   {
                       "key":  "window.height",
                       "type":  "int",
                       "defaultValue":  0
                   }
```

Add `features` to the same file:

```json
    "features":  [
                     {
                         "id":  "windowFocusSpoof",
                         "name":  "Spoof Foreground Focus",
                         "kind":  "toggle",
                         "configKey":  "window.focusSpoofEnabled",
                         "defaultValue":  0
                     },
                     {
                         "id":  "windowActiveFocusSpoof",
                         "name":  "Spoof Active Window",
                         "kind":  "toggle",
                         "configKey":  "window.activeWindowSpoofEnabled",
                         "defaultValue":  0
                     },
                     {
                         "id":  "windowBackgroundInput",
                         "name":  "Background Input",
                         "kind":  "toggle",
                         "configKey":  "window.backgroundInputEnabled",
                         "defaultValue":  0
                     },
                     {
                         "id":  "windowBlockRawInputRegistration",
                         "name":  "Block Raw Input Registration",
                         "kind":  "toggle",
                         "configKey":  "window.blockRawInputRegistrationEnabled",
                         "defaultValue":  0
                     },
                     {
                         "id":  "windowClipCursor",
                         "name":  "Clip Cursor To Window",
                         "kind":  "toggle",
                         "configKey":  "window.clipCursorEnabled",
                         "defaultValue":  0
                     },
                     {
                         "id":  "windowRemoveFrame",
                         "name":  "Remove Window Frame",
                         "kind":  "toggle",
                         "configKey":  "window.removeWindowFrameEnabled",
                         "defaultValue":  0
                     },
                     {
                         "id":  "windowForceTopmost",
                         "name":  "Keep Window Topmost",
                         "kind":  "toggle",
                         "configKey":  "window.forceTopmostEnabled",
                         "defaultValue":  0
                     }
                 ],
```

- [ ] **Step 5: Re-run full verification**

Run:

```powershell
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' 'tests\HelenRuntime.Tests\HelenRuntime.Tests.vcxproj' /p:Configuration=Debug /p:Platform=Win32
& 'C:\dev\helenhook\bin\Win32\Debug\tests\HelenRuntimeTests.exe'
```

Expected:

- build succeeds
- `HelenRuntimeTests.exe` prints `PASS`
- the checked-in Batman graphics pack assertions now see the new `window.*` config entries and curated toggle features

- [ ] **Step 6: Commit**

```bash
git add HelenGameHook/HelenGameHook.cpp tests/HelenRuntime.Tests/PackRepositoryTests.cpp games/HelenBatmanAA/helengamehook/packs/batman-aa-graphics-options/pack.json
git commit -m "Expose window behavior runtime through Batman pack"
```

## Self-Review

- Spec coverage:
  - generic runtime-owned service: Tasks 2 and 3
  - independent `window.*` config keys: Task 1
  - focus spoofing and background input: Task 2
  - raw-input suppression, cursor clipping, forced rect/frame/topmost behavior: Task 3
  - narrow runtime integration and pack-surfaced features: Task 4
- Placeholder scan:
  - No `TODO`, `TBD`, or “implement later” markers remain.
  - Every task includes exact file paths, commands, and code shape.
  - The plan does not rely on “similar to previous task” shortcuts.
- Type consistency:
  - `WindowBehaviorSettings`, `WindowBehaviorHookPlan`, `RegisterWindowBehaviorConfigKeys`, `ReadWindowBehaviorSettings`, and `WindowBehaviorHookSet` are named consistently across all tasks.
