#pragma once
#include <HelenHook/Memory.h>
#include <HelenHook/MemoryPatch.h>

namespace helen {
/** @brief Owns a PE import slot patch and retains its original value across partial failures. */
class IatHook {
private:
    /** @brief Slot bytes and original page protections, retained until cleanup succeeds. */
    MemoryPatch Patch;
    /** @brief Borrowed original function, valid while the importing module remains mapped. */
    void* OriginalValue = nullptr;

public:
    /** @brief Creates an empty import hook with no module side effects. */
    IatHook() = default;
    /** @brief Restores the import or stops before its replacement dependencies can disappear. */
    ~IatHook();
    /** @brief Import patch ownership cannot be duplicated. */
    IatHook(const IatHook&) = delete;
    /** @brief Import patch ownership cannot be overwritten. */
    IatHook& operator=(const IatHook&) = delete;
    /** @brief Reports owned slot recovery state, including incomplete installations. */
    bool IsInstalled() const noexcept;
    /** @brief Reports stage errors for the most recent slot write or restoration. */
    const MemoryPatchResult& Result() const noexcept { return Patch.Result(); }
    /** @brief Returns the preserved import; caller must keep its module mapped. */
    template <typename T> T Original() const { return reinterpret_cast<T>(OriginalValue); }
    /** @brief Installs or returns false after confirmed rollback; unresolved rollback terminates. */
    bool Install(const ModuleView& module, std::string_view imported_dll, std::string_view imported_name, void* replacement);
    /** @brief Attempts installation; failed writes can retain ownership and replacement dependencies. */
    bool TryInstall(const ModuleView& module, std::string_view imported_dll, std::string_view imported_name, void* replacement);
    /** @brief Restores the import or stops rather than returning with unresolved ownership. */
    void Remove();
    /** @brief Attempts restoration, retaining the original value and patch owner on failure. */
    bool TryRemove() noexcept;
};
}
