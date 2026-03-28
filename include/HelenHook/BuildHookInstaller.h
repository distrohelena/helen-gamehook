#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include <HelenHook/Hook.h>
#include <HelenHook/HookBlobRelocator.h>
#include <HelenHook/HookDefinition.h>
#include <HelenHook/PackAssetResolver.h>
#include <HelenHook/RuntimeValueStore.h>

namespace helen
{
    /**
     * @brief Installs declarative blob-backed inline hooks for the active build pack.
     *
     * The installer owns the executable blob allocations and inline hook objects created for the supplied
     * hook definitions. Install either applies every supported hook successfully or removes any partial state
     * that it created before reporting failure.
     */
    class BuildHookInstaller
    {
    public:
        /**
         * @brief Captures one installed hook entry and relocation set for runtime debug snapshots.
         */
        class InstalledHookDebugView
        {
        public:
            /** @brief Stable installed hook identifier. */
            std::string Id;
            /** @brief Absolute address of the overwritten hook target. */
            std::uintptr_t TargetAddress = 0;
            /** @brief Absolute base address of the mutable installed blob bytes. */
            std::uintptr_t BlobAddress = 0;
            /** @brief Absolute address that the inline detour jumps to before the blob payload runs. */
            std::uintptr_t EntryAddress = 0;
            /** @brief Absolute address where execution resumes after the blob returns. */
            std::uintptr_t ResumeAddress = 0;
            /** @brief Relocation results captured while materializing the installed blob. */
            std::vector<HookBlobRelocator::AppliedRelocationView> Relocations;
        };

        /**
         * @brief Creates one hook installer bound to the active pack asset resolver.
         * @param asset_resolver Resolver used to translate declarative blob asset paths into safe filesystem paths.
         */
        explicit BuildHookInstaller(const PackAssetResolver& asset_resolver);

        /**
         * @brief Releases any installed hooks and executable blob allocations owned by this installer.
         */
        ~BuildHookInstaller();

        BuildHookInstaller(const BuildHookInstaller&) = delete;
        BuildHookInstaller& operator=(const BuildHookInstaller&) = delete;
        BuildHookInstaller(BuildHookInstaller&&) = delete;
        BuildHookInstaller& operator=(BuildHookInstaller&&) = delete;

        /**
         * @brief Installs every supplied blob-backed inline hook or rolls back any partial state on the first failure.
         * @param hooks Hook declarations loaded from the selected build metadata.
         * @param runtime_values Declared runtime slots used by blob relocations.
         * @return True when every hook was installed successfully; otherwise false after owned partial state is removed.
         */
        bool Install(const std::vector<HookDefinition>& hooks, const RuntimeValueStore& runtime_values);

        /**
         * @brief Removes every owned inline hook and frees every executable blob allocation created by Install.
         */
        void Remove();

        /**
         * @brief Returns the currently installed hook debug views in installation order.
         * @return Immutable installed hook debug view list.
         */
        const std::vector<InstalledHookDebugView>& GetInstalledHooks() const noexcept;

    private:
        /** @brief Safe pack asset resolver copied from the selected build context. */
        PackAssetResolver asset_resolver_;

        /** @brief Shared relocator used to materialize declarative blob fixups before installation. */
        HookBlobRelocator relocator_;

        /** @brief Owned inline hook objects that restore the original target bytes during removal. */
        std::vector<std::unique_ptr<InlineHook>> installed_hooks_;

        /** @brief Executable blob allocations that must be released when hooks are removed. */
        std::vector<void*> executable_blobs_;

        /** @brief Structured debug views for the hooks currently installed by this instance. */
        std::vector<InstalledHookDebugView> installed_hook_views_;
    };
}