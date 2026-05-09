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
