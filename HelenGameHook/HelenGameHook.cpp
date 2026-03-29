#include <HelenHook/BuildRuntimeCoordinator.h>
#include <HelenHook/BuildHookInstaller.h>
#include <HelenHook/CommandDispatcher.h>
#include <HelenHook/CommandExecutor.h>
#include <HelenHook/ExecutableFingerprint.h>
#include <HelenHook/ExternalBindingService.h>
#include <HelenHook/FileApiHookSet.h>
#include <HelenHook/JsonConfigStore.h>
#include <HelenHook/LoadedBuildPack.h>
#include <HelenHook/Log.h>
#include <HelenHook/PackAssetResolver.h>
#include <HelenHook/PackRepository.h>
#include <HelenHook/RuntimeLayout.h>
#include <HelenHook/RuntimeValueStore.h>
#include <HelenHook/VirtualFileService.h>

#include <exception>
#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <windows.h>

namespace
{
    /** @brief Runtime DLL instance handle captured during process attach. */
    HMODULE g_instance{};
    /** @brief Synchronizes initialization, shutdown, and exported callback dispatch. */
    std::mutex g_mutex;
    /** @brief Tracks whether the generic runtime finished initialization. */
    bool g_initialized{};
    /** @brief Resolved runtime directory layout rooted beneath `helengamehook`. */
    std::optional<helen::RuntimeLayout> g_layout;
    /** @brief JSON-backed runtime config store rooted inside the Helen config directory. */
    std::unique_ptr<helen::JsonConfigStore> g_runtime_config;
    /** @brief Typed command and config dispatcher bound to the active runtime config store. */
    std::unique_ptr<helen::CommandDispatcher> g_command_dispatcher;
    /** @brief Active split-pack declaration set matched against the host executable, when available. */
    std::optional<helen::LoadedBuildPack> g_active_pack;
    /** @brief Active runtime slot store that exposes Helen-managed writable slot addresses. */
    std::unique_ptr<helen::RuntimeValueStore> g_runtime_values;
    /** @brief Declarative command executor for the active build declarations. */
    std::unique_ptr<helen::CommandExecutor> g_command_executor;
    /** @brief Generic coordinator that runs build startup commands and hosts declared live state observers. */
    std::unique_ptr<helen::BuildRuntimeCoordinator> g_build_runtime_coordinator;
    /** @brief External callback bridge exported to patched gameplay assets. */
    std::unique_ptr<helen::ExternalBindingService> g_external_bindings;
    /** @brief Build-scoped asset resolver rooted at the active pack and build directories. */
    std::unique_ptr<helen::PackAssetResolver> g_asset_resolver;
    /** @brief RAM-backed virtual file service for declared replacement files. */
    std::unique_ptr<helen::VirtualFileService> g_virtual_files;
    /** @brief Win32 API hook set that redirects declared virtual files into RAM-backed handles. */
    std::unique_ptr<helen::FileApiHookSet> g_file_hooks;
    /** @brief Generic blob-backed native hook installer for the active build. */
    std::unique_ptr<helen::BuildHookInstaller> g_build_hooks;

    /**
     * @brief Resolves the full filesystem path for a loaded module handle.
     * @param module Module handle returned by the Windows loader.
     * @return Absolute module path when the loader reports one; otherwise an empty path.
     */
    std::filesystem::path GetModulePath(HMODULE module)
    {
        std::wstring buffer(MAX_PATH, L'\0');

        while (true)
        {
            const DWORD length = GetModuleFileNameW(module, buffer.data(), static_cast<DWORD>(buffer.size()));
            if (length == 0)
            {
                return {};
            }

            if (length < buffer.size() - 1)
            {
                buffer.resize(length);
                return std::filesystem::path(buffer);
            }

            buffer.resize(buffer.size() * 2);
        }
    }

    /**
     * @brief Ensures a required runtime directory exists before the runtime starts using it.
     * @param directory_path Absolute directory path that must exist for startup to continue.
     * @return True when the directory already exists or was created successfully; otherwise false.
     */
    bool EnsureDirectoryExists(const std::filesystem::path& directory_path)
    {
        std::error_code error;
        std::filesystem::create_directories(directory_path, error);
        return !error;
    }

    /**
     * @brief Creates the standard writable runtime directories under the generic Helen root.
     * @param layout Resolved runtime directory layout derived from the runtime module path.
     * @return True when every required directory is ready; otherwise false.
     */
    bool CreateRuntimeDirectories(const helen::RuntimeLayout& layout)
    {
        if (!EnsureDirectoryExists(layout.PacksDirectory))
        {
            return false;
        }

        if (!EnsureDirectoryExists(layout.ConfigDirectory))
        {
            return false;
        }

        if (!EnsureDirectoryExists(layout.LogsDirectory))
        {
            return false;
        }

        if (!EnsureDirectoryExists(layout.CacheDirectory))
        {
            return false;
        }

        return true;
    }

    /**
     * @brief Converts a narrow diagnostic string into a wide string for the logging API.
     * @param text Narrow diagnostic text emitted by standard exceptions.
     * @return Wide copy of the supplied text for `%ls` logging.
     */
    std::wstring ToWideString(std::string_view text)
    {
        return std::wstring(text.begin(), text.end());
    }

    /**
     * @brief Creates the JSON config store and typed dispatcher for the active runtime layout.
     * @param layout Resolved runtime directory layout derived from the runtime module path.
     * @return True when the command surface was created successfully; otherwise false.
     */
    bool InitializeCommandSurface(const helen::RuntimeLayout& layout)
    {
        try
        {
            const std::filesystem::path config_path = layout.ConfigDirectory / L"runtime.json";
            g_runtime_config = std::make_unique<helen::JsonConfigStore>(config_path);
            g_command_dispatcher = std::make_unique<helen::CommandDispatcher>(*g_runtime_config);
            helen::Logf(L"[runtime] config=%ls", config_path.c_str());
            return true;
        }
        catch (const std::exception& exception)
        {
            g_command_dispatcher.reset();
            g_runtime_config.reset();
            helen::Logf(L"[runtime] command surface initialization failed: %ls", ToWideString(exception.what()).c_str());
            return false;
        }
    }

    /**
     * @brief Attempts to load a matching split pack for the host executable using its strict fingerprint.
     * @param layout Resolved runtime directory layout derived from the runtime module path.
     * @return True when repository discovery completed successfully; otherwise false.
     */
    bool InitializePackRepository(const helen::RuntimeLayout& layout)
    {
        try
        {
            const helen::ExecutableFingerprint executable_fingerprint = helen::ExecutableFingerprint::FromPath(GetModulePath(nullptr));

            helen::PackRepository repository;
            g_active_pack = repository.LoadForExecutable(
                layout.PacksDirectory,
                executable_fingerprint.FileName,
                executable_fingerprint.FileSize,
                executable_fingerprint.Sha256);
            if (g_active_pack)
            {
                helen::Logf(
                    L"[runtime] loaded pack=%ls build=%ls",
                    ToWideString(g_active_pack->Pack.Id).c_str(),
                    ToWideString(g_active_pack->Build.Id).c_str());
            }
            else
            {
                helen::Logf(
                    L"[runtime] no matching pack for executable=%ls size=%llu",
                    ToWideString(executable_fingerprint.FileName).c_str(),
                    static_cast<unsigned long long>(executable_fingerprint.FileSize));
            }

            return true;
        }
        catch (const std::exception& exception)
        {
            g_active_pack.reset();
            helen::Logf(L"[runtime] pack repository initialization failed: %ls", ToWideString(exception.what()).c_str());
            return false;
        }
    }

    /**
     * @brief Registers every declared pack config entry in the typed command dispatcher.
     * @param active_pack Active loaded pack/build declaration set chosen for the host executable.
     * @return True when every declared config entry is an int and registration succeeded; otherwise false.
     */
    bool RegisterDeclaredConfigEntries(const helen::LoadedBuildPack& active_pack)
    {
        if (g_command_dispatcher == nullptr)
        {
            helen::Log(L"[runtime] command dispatcher is not available during config registration.");
            return false;
        }

        for (const helen::ConfigEntryDefinition& entry : active_pack.Pack.ConfigEntries)
        {
            if (entry.Type != "int")
            {
                helen::Logf(
                    L"[runtime] unsupported config type for key=%ls type=%ls",
                    ToWideString(entry.Key).c_str(),
                    ToWideString(entry.Type).c_str());
                return false;
            }

            g_command_dispatcher->RegisterConfigInt(entry.Key, entry.DefaultValue);
        }

        return true;
    }

    /**
     * @brief Registers every declared build runtime slot in the active runtime value store.
     * @param active_pack Active loaded pack/build declaration set chosen for the host executable.
     * @return True when every runtime slot registration succeeds; otherwise false.
     */
    bool RegisterDeclaredRuntimeSlots(const helen::LoadedBuildPack& active_pack)
    {
        if (g_runtime_values == nullptr)
        {
            helen::Log(L"[runtime] runtime value store is not available during slot registration.");
            return false;
        }

        for (const helen::RuntimeSlotDefinition& slot : active_pack.Build.RuntimeSlots)
        {
            if (!g_runtime_values->RegisterSlot(slot))
            {
                helen::Logf(
                    L"[runtime] failed to register runtime slot id=%ls type=%ls",
                    ToWideString(slot.Id).c_str(),
                    ToWideString(slot.Type).c_str());
                return false;
            }
        }

        return true;
    }

    /**
     * @brief Registers every declared build command in the active command executor.
     * @param active_pack Active loaded pack/build declaration set chosen for the host executable.
     * @return True when every command registration succeeds; otherwise false.
     */
    bool RegisterDeclaredCommands(const helen::LoadedBuildPack& active_pack)
    {
        if (g_command_executor == nullptr)
        {
            helen::Log(L"[runtime] command executor is not available during command registration.");
            return false;
        }

        for (const helen::CommandDefinition& command : active_pack.Build.Commands)
        {
            if (!g_command_executor->RegisterCommand(command))
            {
                helen::Logf(
                    L"[runtime] failed to register command id=%ls",
                    ToWideString(command.Id).c_str());
                return false;
            }
        }

        return true;
    }

    /**
     * @brief Registers every declared external binding in the active external binding service.
     * @param active_pack Active loaded pack/build declaration set chosen for the host executable.
     * @return True when every binding registration succeeds; otherwise false.
     */
    bool RegisterDeclaredExternalBindings(const helen::LoadedBuildPack& active_pack)
    {
        if (g_external_bindings == nullptr)
        {
            helen::Log(L"[runtime] external binding service is not available during binding registration.");
            return false;
        }

        for (const helen::ExternalBindingDefinition& binding : active_pack.Build.ExternalBindings)
        {
            try
            {
                g_external_bindings->Register(binding);
            }
            catch (const std::exception& exception)
            {
                helen::Logf(
                    L"[runtime] failed to register external binding id=%ls external=%ls reason=%ls",
                    ToWideString(binding.Id).c_str(),
                    ToWideString(binding.ExternalName).c_str(),
                    ToWideString(exception.what()).c_str());
                return false;
            }
        }

        return true;
    }

    /**
     * @brief Registers every declared virtual file in the active virtual file service.
     * @param active_pack Active loaded pack/build declaration set chosen for the host executable.
     * @return True when every virtual file registration succeeds; otherwise false.
     */
    bool RegisterDeclaredVirtualFiles(const helen::LoadedBuildPack& active_pack)
    {
        if (g_virtual_files == nullptr)
        {
            helen::Log(L"[runtime] virtual file service is not available during virtual file registration.");
            return false;
        }

        for (const helen::VirtualFileDefinition& definition : active_pack.Build.VirtualFiles)
        {
            if (!g_virtual_files->RegisterVirtualFile(definition))
            {
                helen::Logf(
                    L"[runtime] failed to register virtual file id=%ls gamePath=%ls source=%ls",
                    ToWideString(definition.Id).c_str(),
                    definition.GamePath.c_str(),
                    definition.Source.Path.c_str());
                return false;
            }
        }

        return true;
    }

    /**
     * @brief Creates and wires every runtime-owned service for one active build pack.
     * @param active_pack Active loaded pack/build declaration set chosen for the host executable.
     * @return True when every service initializes successfully; otherwise false.
     */
    bool InitializeActivePackRuntime(const helen::LoadedBuildPack& active_pack)
    {
        if (!RegisterDeclaredConfigEntries(active_pack))
        {
            return false;
        }

        g_runtime_values = std::make_unique<helen::RuntimeValueStore>();
        if (!RegisterDeclaredRuntimeSlots(active_pack))
        {
            return false;
        }

        if (g_command_dispatcher == nullptr)
        {
            helen::Log(L"[runtime] command dispatcher is not available for executor creation.");
            return false;
        }

        g_command_executor = std::make_unique<helen::CommandExecutor>(*g_command_dispatcher, *g_runtime_values);
        if (!RegisterDeclaredCommands(active_pack))
        {
            return false;
        }

        g_external_bindings = std::make_unique<helen::ExternalBindingService>(*g_command_dispatcher, *g_command_executor);
        if (!RegisterDeclaredExternalBindings(active_pack))
        {
            return false;
        }

        try
        {
            g_asset_resolver = std::make_unique<helen::PackAssetResolver>(active_pack.PackDirectory, active_pack.BuildDirectory);
        }
        catch (const std::exception& exception)
        {
            helen::Logf(L"[runtime] failed to create pack asset resolver: %ls", ToWideString(exception.what()).c_str());
            return false;
        }

        g_virtual_files = std::make_unique<helen::VirtualFileService>(*g_asset_resolver);
        if (!RegisterDeclaredVirtualFiles(active_pack))
        {
            return false;
        }

        g_build_hooks = std::make_unique<helen::BuildHookInstaller>(*g_asset_resolver);
        if (!g_build_hooks->Install(active_pack.Build.Hooks, *g_runtime_values))
        {
            helen::Log(L"[runtime] failed to install build hooks.");
            return false;
        }

        g_file_hooks = std::make_unique<helen::FileApiHookSet>(*g_virtual_files);
        if (!g_file_hooks->Install())
        {
            helen::Log(L"[runtime] failed to install file API hooks.");
            return false;
        }

        g_build_runtime_coordinator = std::make_unique<helen::BuildRuntimeCoordinator>(
            active_pack.Build.StartupCommandIds,
            active_pack.Build.StateObservers,
            *g_command_dispatcher,
            *g_command_executor);
        if (!g_build_runtime_coordinator->Start())
        {
            helen::Log(L"[runtime] failed to start build runtime coordinator.");
            return false;
        }

        return true;
    }

    /**
     * @brief Releases pack-scoped runtime objects in reverse dependency order.
     */
    void ResetPackRuntimeState()
    {
        g_build_runtime_coordinator.reset();
        g_build_hooks.reset();
        g_file_hooks.reset();
        g_virtual_files.reset();
        g_asset_resolver.reset();
        g_external_bindings.reset();
        g_command_executor.reset();
        g_runtime_values.reset();
    }

    /**
     * @brief Clears process-owned runtime objects outside the Windows loader lock.
     */
    void ResetRuntimeState()
    {
        ResetPackRuntimeState();
        g_active_pack.reset();
        g_command_dispatcher.reset();
        g_runtime_config.reset();
        g_layout.reset();
        g_initialized = false;
    }

    /**
     * @brief Abandons runtime-owned pointers during process detach to avoid hook teardown under loader lock.
     *
     * The runtime intentionally runs for process lifetime after successful initialization. During
     * loader detach, releasing ownership without invoking destructors prevents hook unpatch work
     * from running while the loader lock is held.
     */
    void HandleProcessDetach()
    {
        static_cast<void>(g_build_runtime_coordinator.release());
        static_cast<void>(g_build_hooks.release());
        static_cast<void>(g_file_hooks.release());
        static_cast<void>(g_virtual_files.release());
        static_cast<void>(g_asset_resolver.release());
        static_cast<void>(g_external_bindings.release());
        static_cast<void>(g_command_executor.release());
        static_cast<void>(g_runtime_values.release());
        static_cast<void>(g_command_dispatcher.release());
        static_cast<void>(g_runtime_config.release());
        g_active_pack.reset();
        g_layout.reset();
        g_initialized = false;
        g_instance = nullptr;
    }
}

/**
 * @brief Initializes the generic runtime layout, command surface, active pack services, and exported callback bridge.
 * @return True when runtime startup completes successfully; otherwise false.
 */
extern "C" __declspec(dllexport) BOOL __stdcall HelenInitialize()
{
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_initialized)
    {
        return TRUE;
    }

    if (g_instance == nullptr)
    {
        return FALSE;
    }

    const std::filesystem::path module_path = GetModulePath(g_instance);
    if (module_path.empty())
    {
        return FALSE;
    }

    const helen::RuntimeLayout layout = helen::RuntimeLayout::FromRuntimeModulePath(module_path);
    if (!CreateRuntimeDirectories(layout))
    {
        return FALSE;
    }

    helen::SetLogPath(layout.LogsDirectory / L"HelenGameHook.log");
    if (!InitializeCommandSurface(layout))
    {
        ResetRuntimeState();
        return FALSE;
    }

    if (!InitializePackRepository(layout))
    {
        ResetRuntimeState();
        return FALSE;
    }

    if (g_active_pack && !InitializeActivePackRuntime(*g_active_pack))
    {
        ResetRuntimeState();
        return FALSE;
    }

    helen::Log(L"[runtime] HelenInitialize");
    helen::Logf(L"[runtime] module=%ls", module_path.c_str());
    helen::Logf(L"[runtime] helen_root=%ls", layout.HelenRoot.c_str());

    g_layout = layout;
    g_initialized = true;
    return TRUE;
}

/**
 * @brief Handles explicit runtime shutdown requests as a process-lifetime no-op.
 *
 * The generic runtime stays active for process lifetime after successful initialization so live
 * hook state is not torn down while external callers may still execute patched paths.
 */
extern "C" __declspec(dllexport) void __stdcall HelenShutdown()
{
    std::lock_guard<std::mutex> lock(g_mutex);
    if (!g_initialized)
    {
        return;
    }

    helen::Log(L"[runtime] HelenShutdown");
    helen::Log(L"[runtime] shutdown is a no-op after successful initialization.");
}

/**
 * @brief Receives generic runtime commands and records them until command-line dispatch is implemented.
 * @param command_line Command payload emitted by an external UI bridge or tool.
 * @return False because wide-string command-line dispatch is not implemented.
 */
extern "C" __declspec(dllexport) BOOL __stdcall HelenDispatchCommandW(const wchar_t* command_line)
{
    std::lock_guard<std::mutex> lock(g_mutex);
    if (!g_initialized || !g_layout || g_command_dispatcher == nullptr || command_line == nullptr)
    {
        return FALSE;
    }

    helen::Logf(L"[runtime] command stub: %ls", command_line);
    helen::Log(L"[runtime] command dispatch is not implemented.");
    return FALSE;
}

/**
 * @brief Reads one registered integer config value through the generic external binding bridge.
 * @param key Config key that should be read through the `Helen_GetInt` external callback contract.
 * @param value Receives the resolved config value when the binding and key resolve successfully.
 * @return True when initialization is complete and the binding resolves successfully; otherwise false.
 */
extern "C" __declspec(dllexport) BOOL __stdcall HelenGetIntA(const char* key, int* value)
{
    std::lock_guard<std::mutex> lock(g_mutex);
    if (!g_initialized || g_external_bindings == nullptr || key == nullptr || value == nullptr)
    {
        return FALSE;
    }

    int resolved_value = 0;
    if (!g_external_bindings->TryHandleGetInt("Helen_GetInt", key, resolved_value))
    {
        return FALSE;
    }

    *value = resolved_value;
    return TRUE;
}

/**
 * @brief Writes one registered integer config value through the generic external binding bridge.
 * @param key Config key that should be written through the `Helen_SetInt` external callback contract.
 * @param value Integer value that should be written for the supplied key.
 * @return True when initialization is complete and the binding resolves successfully; otherwise false.
 */
extern "C" __declspec(dllexport) BOOL __stdcall HelenSetIntA(const char* key, int value)
{
    std::lock_guard<std::mutex> lock(g_mutex);
    if (!g_initialized || g_external_bindings == nullptr || key == nullptr)
    {
        return FALSE;
    }

    return g_external_bindings->TryHandleSetInt("Helen_SetInt", key, value);
}

/**
 * @brief Writes one registered integer config value directly through the typed command dispatcher.
 * @param key Config key that should be written by native callers such as blob-backed hooks.
 * @param value Integer value that should be stored for the supplied key.
 * @return True when initialization is complete and the config key was updated successfully; otherwise false.
 */
extern "C" __declspec(dllexport) BOOL __stdcall HelenSetConfigIntA(const char* key, int value)
{
    std::lock_guard<std::mutex> lock(g_mutex);
    if (!g_initialized || g_command_dispatcher == nullptr || key == nullptr)
    {
        return FALSE;
    }

    return g_command_dispatcher->TrySetInt(key, value) ? TRUE : FALSE;
}

/**
 * @brief Runs one registered declarative command through the generic external binding bridge.
 * @param command_id Command identifier that should be dispatched through the `Helen_RunCommand` contract.
 * @return True when initialization is complete and the binding resolves successfully; otherwise false.
 */
extern "C" __declspec(dllexport) BOOL __stdcall HelenRunCommandA(const char* command_id)
{
    std::lock_guard<std::mutex> lock(g_mutex);
    if (!g_initialized || g_external_bindings == nullptr || command_id == nullptr)
    {
        return FALSE;
    }

    return g_external_bindings->TryHandleRunCommand("Helen_RunCommand", command_id);
}

/**
 * @brief Captures the module handle on attach and performs only loader-lock-safe cleanup on detach.
 * @param module Runtime DLL handle supplied by the Windows loader.
 * @param reason Loader notification reason for this callback.
 * @param reserved Reserved loader data that is unused by this runtime.
 * @return Always returns true so the runtime DLL remains loadable.
 */
BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID reserved)
{
    (void)reserved;

    if (reason == DLL_PROCESS_ATTACH)
    {
        g_instance = module;
        DisableThreadLibraryCalls(module);
    }
    else if (reason == DLL_PROCESS_DETACH)
    {
        HandleProcessDetach();
    }

    return TRUE;
}
