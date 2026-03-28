#pragma once

namespace helen
{
    /**
     * @brief Runtime callback signature used to observe installed hook execution hits.
     */
    using RuntimeHookHitCallback = void(__stdcall*)(const char* hook_id) noexcept;

    /**
     * @brief Records one installed hook execution in the active runtime debug state.
     * @param hook_id Stable hook identifier supplied by the active build metadata.
     */
    void __stdcall RecordRuntimeHookHit(const char* hook_id) noexcept;

    /**
     * @brief Registers the active runtime callback that should observe installed hook execution hits.
     * @param callback Runtime-owned callback that should receive future hook-hit notifications, or null to disable notifications.
     */
    void SetRuntimeHookHitCallback(RuntimeHookHitCallback callback) noexcept;
}