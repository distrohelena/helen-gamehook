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
}
