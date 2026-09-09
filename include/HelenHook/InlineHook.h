#pragma once
#include <HelenHook/MemoryPatch.h>

namespace helen {
/**
 * @brief Owns an x86 detour and its trampoline until confirmed target restoration.
 * Callers exclude concurrent execution and supply complete relocation-safe instructions.
 * Failed TryInstall/TryRemove may retain ownership: keep detour dependencies alive.
 */
class InlineHook {
private:
    /** @brief Maximum complete-instruction span supported by the fixed x86 trampoline buffers. */
    static constexpr std::size_t MaximumPatchSize = 16;
    /** @brief Target byte/protection recovery owner, including partial installations. */
    MemoryPatch Patch;
    /** @brief Owned executable allocation, released only after target restoration. */
    void* Trampoline = nullptr;

public:
    /** @brief Creates an empty hook without modifying executable memory. */
    InlineHook() = default;
    /** @brief Restores the target or terminates before dependent state can disappear. */
    ~InlineHook();
    /** @brief Hook ownership cannot be duplicated. */
    InlineHook(const InlineHook&) = delete;
    /** @brief Hook ownership cannot be overwritten. */
    InlineHook& operator=(const InlineHook&) = delete;
    /** @brief Conservatively reports owned patch or trampoline state, not installation success. */
    bool IsInstalled() const noexcept;
    /** @brief Reports the latest target patch attempt; inspect after TryInstall/TryRemove. */
    const MemoryPatchResult& Result() const noexcept { return Patch.Result(); }
    /** @brief Returns the trampoline; only call it after successful installation with safe instructions. */
    template <typename T> T Original() const { return reinterpret_cast<T>(Trampoline); }
    /** @brief Installs or returns false after confirmed rollback; unresolved cleanup terminates. */
    bool Install(void* target, void* detour, std::size_t patch_size);
    /** @brief Attempts installation; false can retain ownership requiring explicit TryRemove. */
    bool TryInstall(void* target, void* detour, std::size_t patch_size);
    /** @brief Restores and releases the hook or terminates rather than returning unsafe state. */
    void Remove();
    /** @brief Returns false without relinquishing recovery state when restoration/release fails. */
    bool TryRemove() noexcept;
};
}
