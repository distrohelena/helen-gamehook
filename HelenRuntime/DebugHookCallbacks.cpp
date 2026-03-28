#include <HelenHook/DebugHookCallbacks.h>

namespace
{
    /** @brief Active runtime-owned callback that observes installed hook execution hits. */
    helen::RuntimeHookHitCallback g_runtime_hook_hit_callback = nullptr;
}

namespace helen
{
    void __stdcall RecordRuntimeHookHit(const char* hook_id) noexcept
    {
        if (g_runtime_hook_hit_callback == nullptr)
        {
            return;
        }

        g_runtime_hook_hit_callback(hook_id);
    }

    void SetRuntimeHookHitCallback(RuntimeHookHitCallback callback) noexcept
    {
        g_runtime_hook_hit_callback = callback;
    }
}