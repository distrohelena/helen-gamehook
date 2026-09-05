#include <HelenHook/ActivePackSet.h>
#include <HelenHook/ActivePackSetBuilder.h>
#include <HelenHook/BatmanDisplayModeService.h>
#include <HelenHook/BatmanDisplayModeResponseProvider.h>
#include <HelenHook/BatmanGraphicsConfigService.h>
#include <HelenHook/BuildRuntimeCoordinator.h>
#include <HelenHook/BuildHookInstaller.h>
#include <HelenHook/CommandDispatcher.h>
#include <HelenHook/CommandExecutor.h>
#include <HelenHook/D3d9TextureReplacementHookSet.h>
#include <HelenHook/ExecutableFingerprint.h>
#include <HelenHook/ExternalBindingService.h>
#include <HelenHook/FileApiHookSet.h>
#include <HelenHook/JsonConfigStore.h>
#include <HelenHook/LoadedBuildPack.h>
#include <HelenHook/LoadedBuildPackSet.h>
#include <HelenHook/Log.h>
#include <HelenHook/PackRepository.h>
#include <HelenHook/PackSelectionConfig.h>
#include <HelenHook/ModuleLoadRoutingHookSet.h>
#include <HelenHook/ModuleLoadRoutingService.h>
#include <HelenHook/RuntimeLayout.h>
#include <HelenHook/RuntimeValueStore.h>
#include <HelenHook/VirtualFileService.h>
#include <HelenHook/WindowBehaviorConfig.h>
#include <HelenHook/WindowBehaviorHookSet.h>

#include <exception>
#include <array>
#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <system_error>
#include <functional>
#include <shlobj.h>
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
    /** @brief Merged active runtime pack set consumed by runtime services after validation succeeds. */
    std::optional<helen::ActivePackSet> g_active_runtime_pack_set;
    /** @brief Active runtime slot store that exposes Helen-managed writable slot addresses. */
    std::unique_ptr<helen::RuntimeValueStore> g_runtime_values;
    /** @brief Batman-specific graphics config bridge bound to the user `BmEngine.ini` file. */
    std::unique_ptr<helen::BatmanGraphicsConfigService> g_batman_graphics_config_service;
    /** @brief Batman display-mode catalog service shared by graphics draft validation and dynamic observers. */
    std::unique_ptr<helen::BatmanDisplayModeService> g_batman_display_mode_service;
    /** @brief Declarative command executor for the active build declarations. */
    std::unique_ptr<helen::CommandExecutor> g_command_executor;
    /** @brief Generic coordinator that runs build startup commands and hosts declared live state observers. */
    std::unique_ptr<helen::BuildRuntimeCoordinator> g_build_runtime_coordinator;
    /** @brief Pure loader-routing service that classifies intercepted module-load requests. */
    std::unique_ptr<helen::ModuleLoadRoutingService> g_module_load_routing_service;
    /** @brief Loader hook set that logs Batman Bink requests through the routing service with runtime-controlled shallow and deep loader paths. */
    std::unique_ptr<helen::ModuleLoadRoutingHookSet> g_module_load_routing_hooks;
    /** @brief External callback bridge exported to patched gameplay assets. */
    std::unique_ptr<helen::ExternalBindingService> g_external_bindings;
    /** @brief RAM-backed virtual file service for declared replacement files. */
    std::unique_ptr<helen::VirtualFileService> g_virtual_files;
    /** @brief Win32 API hook set that redirects declared virtual files into RAM-backed handles. */
    std::unique_ptr<helen::FileApiHookSet> g_file_hooks;
    /** @brief Optional Direct3D 9 texture replacement hook set driven by build texture metadata. */
    std::unique_ptr<helen::D3d9TextureReplacementHookSet> g_d3d9_texture_hooks;
    /** @brief Optional window behavior hook set driven by generic `window.*` runtime config keys. */
    std::unique_ptr<helen::WindowBehaviorHookSet> g_window_behavior_hooks;
    /** @brief Generic blob-backed native hook installer for the active build. */
    std::unique_ptr<helen::BuildHookInstaller> g_build_hooks;

    /** @brief Batman graphics config keys logged before and after each native apply command. */
    constexpr std::array<std::string_view, 15> BatmanGraphicsDraftKeys = {
        "fullscreen",
        "resolutionWidth",
        "resolutionHeight",
        "vsync",
        "msaa",
        "detailLevel",
        "bloom",
        "dynamicShadows",
        "motionBlur",
        "distortion",
        "fogVolumes",
        "sphericalHarmonicLighting",
        "ambientOcclusion",
        "physx",
        "stereo"
    };

    /**
     * @brief Exact dynamic provider identifier declared by the Batman graphics package.
     *
     * Runtime callback dispatch is intentionally an exact ordinal string comparison so an
     * unknown provider cannot accidentally query or mutate the Batman display catalog.
     */
    constexpr std::string_view BatmanDisplayModeProviderId = "batmanDisplayModes";
    /**
     * @brief Dynamic request that refreshes the display catalog and snapshots the current resolution pair.
     *
     * The callback stores the width/height pair only after both the config read and catalog refresh
     * succeed, ensuring the two current-resolution responses describe one catalog generation.
     */
    constexpr int BatmanDisplayModeCatalogRequest = 4700;
    /**
     * @brief Dynamic request that returns the captured current resolution width.
     *
     * This request is valid only after BatmanDisplayModeCatalogRequest has produced a successful
     * snapshot; it never falls back to a default or a newly-read config value.
     */
    constexpr int BatmanDisplayModeCurrentWidthRequest = 4897;
    /**
     * @brief Dynamic request that returns and consumes the captured current resolution height.
     *
     * Consuming the snapshot after height prevents an out-of-sequence later request from reusing
     * an old width/height pair after the catalog response sequence has completed.
     */
    constexpr int BatmanDisplayModeCurrentHeightRequest = 4898;
    /** @brief First dynamic request reserved for the mode-specific windowed catalog. */
    constexpr int BatmanWindowedCatalogRequest = 5200;
    /** @brief Last dynamic request reserved for the windowed catalog's current pair. */
    constexpr int BatmanWindowedCatalogCurrentHeightRequest = 5398;
    /** @brief First dynamic request reserved for the mode-specific fullscreen catalog. */
    constexpr int BatmanFullscreenCatalogRequest = 5400;
    /** @brief Last dynamic request reserved for the fullscreen catalog's current pair. */
    constexpr int BatmanFullscreenCatalogCurrentHeightRequest = 5598;
    /** @brief Dynamic request returning the actual current desktop width for explicit fullscreen fallback. */
    constexpr int BatmanDesktopWidthRequest = 5600;
    /** @brief Dynamic request returning the actual current desktop height for explicit fullscreen fallback. */
    constexpr int BatmanDesktopHeightRequest = 5601;

    /** @brief Runtime config key that enables the main-module LoadLibrary routing hooks. */
    constexpr std::string_view ModuleLoadRoutingLoadLibraryHooksEnabledKey = "moduleLoadRouting.loadLibraryHooksEnabled";

    /** @brief Runtime config key that enables the deeper ntdll LdrLoadDll hook. */
    constexpr std::string_view ModuleLoadRoutingLdrLoadDllHookEnabledKey = "moduleLoadRouting.ldrLoadDllHookEnabled";

    /** @brief Default state for the main-module LoadLibrary routing hooks. */
    constexpr int ModuleLoadRoutingLoadLibraryHooksEnabledDefault = 1;

    /** @brief Default state for the deeper ntdll LdrLoadDll hook. */
    constexpr int ModuleLoadRoutingLdrLoadDllHookEnabledDefault = 0;

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
     * @brief Converts one external narrow UI message into a wide string for safe logging.
     * @param message Narrow message text supplied by the external UI bridge.
     * @return Wide message text when conversion succeeds; otherwise a fixed fallback string.
     *
     * The conversion uses UTF-8 semantics because the gameplay assets pass UTF-8-compatible
     * payloads through the `Helen_Log` bridge. The helper deliberately avoids `printf`-style
     * formatting so malformed text cannot surface as a formatting exception.
     */
    std::wstring ConvertUiMessageToWide(const char* message)
    {
        if (message == nullptr)
        {
            return L"<null>";
        }

        const int required_length = MultiByteToWideChar(CP_UTF8, 0, message, -1, nullptr, 0);
        if (required_length <= 0)
        {
            return L"<unprintable>";
        }

        std::wstring wide_message(static_cast<std::size_t>(required_length - 1), L'\0');
        const int actual_length = MultiByteToWideChar(
            CP_UTF8,
            0,
            message,
            -1,
            wide_message.data(),
            required_length);
        if (actual_length <= 0)
        {
            return L"<unprintable>";
        }

        return wide_message;
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
     * @brief Builds one readable Batman graphics draft snapshot from the active config dispatcher.
     * @param dispatcher Dispatcher that stores the normalized graphics draft values.
     * @return One comma-delimited snapshot string that includes every Batman graphics draft key.
     */
    std::wstring BuildBatmanGraphicsDraftSnapshot(const helen::CommandDispatcher& dispatcher)
    {
        std::wstring snapshot;
        for (std::size_t index = 0; index < BatmanGraphicsDraftKeys.size(); ++index)
        {
            const std::string_view key = BatmanGraphicsDraftKeys[index];
            if (index > 0)
            {
                snapshot += L",";
            }

            snapshot += std::wstring(key.begin(), key.end());
            snapshot += L"=";
            const std::optional<int> value = dispatcher.TryGetInt(std::string(key));
            if (value.has_value())
            {
                snapshot += std::to_wstring(*value);
            }
            else
            {
                snapshot += L"<missing>";
            }
        }

        return snapshot;
    }

    /**
     * @brief Registers the loader-routing runtime flags with their default values.
     */
    void RegisterModuleLoadRoutingFlags()
    {
        if (g_command_dispatcher == nullptr)
        {
            return;
        }

        g_command_dispatcher->RegisterConfigInt(
            std::string(ModuleLoadRoutingLoadLibraryHooksEnabledKey),
            ModuleLoadRoutingLoadLibraryHooksEnabledDefault);
        g_command_dispatcher->RegisterConfigInt(
            std::string(ModuleLoadRoutingLdrLoadDllHookEnabledKey),
            ModuleLoadRoutingLdrLoadDllHookEnabledDefault);
    }

    /**
     * @brief Reads one loader-routing runtime flag from the dispatcher.
     * @param key Config key that should contain a registered integer flag.
     * @return Flag value when the key is registered; otherwise no value.
     */
    std::optional<bool> TryGetModuleLoadRoutingFlag(std::string_view key)
    {
        if (g_command_dispatcher == nullptr)
        {
            return std::nullopt;
        }

        const std::optional<int> value = g_command_dispatcher->TryGetInt(std::string(key));
        if (!value.has_value())
        {
            return std::nullopt;
        }

        return *value != 0;
    }

    /**
     * @brief Writes one Batman graphics draft snapshot into the runtime log when the dispatcher is available.
     * @param stage Stable label describing where in the native apply flow the snapshot was captured.
     */
    void LogBatmanGraphicsDraftSnapshot(std::wstring_view stage)
    {
        const std::wstring stage_text(stage);
        if (g_command_dispatcher == nullptr)
        {
            helen::Logf(L"[runtime] %ls dispatcher=<missing>", stage_text.c_str());
            return;
        }

        const std::wstring snapshot = BuildBatmanGraphicsDraftSnapshot(*g_command_dispatcher);
        helen::Logf(L"[runtime] %ls %ls", stage_text.c_str(), snapshot.c_str());
    }

    /**
     * @brief Resolves the user-documents `BmEngine.ini` path used by Batman Arkham Asylum graphics settings.
     * @return Absolute user-documents `BmEngine.ini` path when the Windows known-folder lookup succeeds; otherwise no value.
     */
    std::optional<std::filesystem::path> TryGetBatmanUserEngineIniPath()
    {
        PWSTR documents_path = nullptr;
        const HRESULT result = SHGetKnownFolderPath(FOLDERID_Documents, KF_FLAG_DEFAULT, nullptr, &documents_path);
        if (FAILED(result) || documents_path == nullptr)
        {
            if (documents_path != nullptr)
            {
                CoTaskMemFree(documents_path);
            }

            return std::nullopt;
        }

        const std::filesystem::path ini_path = std::filesystem::path(documents_path) /
            "Square Enix" /
            "Batman Arkham Asylum GOTY" /
            "BmGame" /
            "Config" /
            "BmEngine.ini";
        CoTaskMemFree(documents_path);
        return ini_path;
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
            RegisterModuleLoadRoutingFlags();
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
     * @brief Wraps one fallback single-pack match in the shared loaded-pack-set shape.
     * @param loaded_pack Loaded pack/build selection produced by the repository fallback path.
     * @return One ordered pack set containing only the supplied pack.
     */
    helen::LoadedBuildPackSet CreateSinglePackSet(const helen::LoadedBuildPack& loaded_pack)
    {
        helen::LoadedBuildPackSet loaded_pack_set;
        loaded_pack_set.Packs.push_back(loaded_pack);
        return loaded_pack_set;
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
            helen::LoadedBuildPackSet loaded_pack_set;

            helen::PackRepository repository;
            const helen::PackSelectionConfig pack_selection_config(layout.ConfigDirectory / L"packs.json");
            const std::optional<std::vector<std::string>> enabled_pack_ids = pack_selection_config.TryGetEnabledPacks(executable_fingerprint.FileName);
            if (enabled_pack_ids.has_value())
            {
                const std::optional<helen::LoadedBuildPackSet> selected_pack_set = repository.LoadPackSetForExecutable(
                    layout.PacksDirectory,
                    executable_fingerprint.FileName,
                    executable_fingerprint.FileSize,
                    executable_fingerprint.Sha256,
                    *enabled_pack_ids);
                if (!selected_pack_set.has_value())
                {
                    helen::Logf(
                        L"[runtime] enabled pack set failed for executable=%ls",
                        ToWideString(executable_fingerprint.FileName).c_str());
                    return false;
                }

                loaded_pack_set = *selected_pack_set;
                helen::Logf(
                    L"[runtime] loaded explicit pack set executable=%ls count=%zu",
                    ToWideString(executable_fingerprint.FileName).c_str(),
                    loaded_pack_set.Packs.size());
            }
            else
            {
                const std::optional<helen::LoadedBuildPack> selected_pack = repository.LoadForExecutable(
                    layout.PacksDirectory,
                    executable_fingerprint.FileName,
                    executable_fingerprint.FileSize,
                    executable_fingerprint.Sha256);
                if (!selected_pack.has_value())
                {
                    helen::Logf(
                        L"[runtime] no matching pack for executable=%ls size=%llu",
                        ToWideString(executable_fingerprint.FileName).c_str(),
                        static_cast<unsigned long long>(executable_fingerprint.FileSize));
                    return true;
                }

                loaded_pack_set = CreateSinglePackSet(*selected_pack);
            }

            helen::ActivePackSet active_pack_set;
            std::string failure_reason;
            const helen::ActivePackSetBuilder active_pack_set_builder;
            if (!active_pack_set_builder.TryBuild(loaded_pack_set, active_pack_set, failure_reason))
            {
                helen::Logf(
                    L"[runtime] active pack-set validation failed executable=%ls reason=%ls",
                    ToWideString(executable_fingerprint.FileName).c_str(),
                    ToWideString(failure_reason).c_str());
                return false;
            }

            g_active_runtime_pack_set = std::move(active_pack_set);
            helen::Logf(
                L"[runtime] active pack set ready executable=%ls count=%zu startup=%zu hidden=%zu",
                ToWideString(executable_fingerprint.FileName).c_str(),
                g_active_runtime_pack_set->LoadedPacks.size(),
                g_active_runtime_pack_set->StartupCommandIds.size(),
                g_active_runtime_pack_set->MissingPaths.size());
            return true;
        }
        catch (const std::exception& exception)
        {
            g_active_runtime_pack_set.reset();
            helen::Logf(L"[runtime] pack repository initialization failed: %ls", ToWideString(exception.what()).c_str());
            return false;
        }
    }

    /**
     * @brief Installs the loader-routing hook using the registered runtime flags before pack initialization begins.
     * @return True when the routing service and hook set are created and installed successfully; otherwise false.
     */
    bool InitializeModuleLoadRouting()
    {
        const std::optional<bool> enable_load_library_hooks =
            TryGetModuleLoadRoutingFlag(ModuleLoadRoutingLoadLibraryHooksEnabledKey);
        if (!enable_load_library_hooks.has_value())
        {
            helen::Log(L"[runtime] failed to read the LoadLibrary routing flag.");
            return false;
        }

        const std::optional<bool> enable_ldr_load_dll_hook =
            TryGetModuleLoadRoutingFlag(ModuleLoadRoutingLdrLoadDllHookEnabledKey);
        if (!enable_ldr_load_dll_hook.has_value())
        {
            helen::Log(L"[runtime] failed to read the LdrLoadDll routing flag.");
            return false;
        }

        helen::Logf(
            L"[runtime] module load routing flags loadLibrary=%d ldrLoadDll=%d",
            static_cast<int>(*enable_load_library_hooks),
            static_cast<int>(*enable_ldr_load_dll_hook));

        g_module_load_routing_service = std::make_unique<helen::ModuleLoadRoutingService>();
        helen::Log(L"[runtime] module load routing service created.");

        g_module_load_routing_hooks = std::make_unique<helen::ModuleLoadRoutingHookSet>(
            *g_module_load_routing_service,
            *enable_load_library_hooks,
            *enable_ldr_load_dll_hook);
        helen::Log(L"[runtime] module load routing hook set created.");
        helen::Log(L"[runtime] module load routing hooks install begin.");
        if (!g_module_load_routing_hooks->Install())
        {
            helen::Log(L"[runtime] failed to install module load routing hooks.");
            return false;
        }

        helen::Log(L"[runtime] module load routing hooks installed.");
        return true;
    }

    /**
     * @brief Registers every merged config entry in the typed command dispatcher.
     * @param config_entries Unified config entries selected for the host executable.
     * @return True when every declared config entry is an int and registration succeeded; otherwise false.
     */
    bool RegisterDeclaredConfigEntries(const std::vector<helen::ConfigEntryDefinition>& config_entries)
    {
        if (g_command_dispatcher == nullptr)
        {
            helen::Log(L"[runtime] command dispatcher is not available during config registration.");
            return false;
        }

        for (const helen::ConfigEntryDefinition& entry : config_entries)
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
     * @brief Registers every merged runtime slot in the active runtime value store.
     * @param runtime_slots Unified runtime slots selected for the host executable.
     * @return True when every runtime slot registration succeeds; otherwise false.
     */
    bool RegisterDeclaredRuntimeSlots(const std::vector<helen::RuntimeSlotDefinition>& runtime_slots)
    {
        if (g_runtime_values == nullptr)
        {
            helen::Log(L"[runtime] runtime value store is not available during slot registration.");
            return false;
        }

        for (const helen::RuntimeSlotDefinition& slot : runtime_slots)
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
     * @brief Registers every merged command in the active command executor.
     * @param commands Unified commands selected for the host executable.
     * @return True when every command registration succeeds; otherwise false.
     */
    bool RegisterDeclaredCommands(const std::vector<helen::CommandDefinition>& commands)
    {
        if (g_command_executor == nullptr)
        {
            helen::Log(L"[runtime] command executor is not available during command registration.");
            return false;
        }

        for (const helen::CommandDefinition& command : commands)
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
     * @brief Registers every merged external binding in the active external binding service.
     * @param external_bindings Unified external bindings selected for the host executable.
     * @return True when every binding registration succeeds; otherwise false.
     */
    bool RegisterDeclaredExternalBindings(const std::vector<helen::ExternalBindingDefinition>& external_bindings)
    {
        if (g_external_bindings == nullptr)
        {
            helen::Log(L"[runtime] external binding service is not available during binding registration.");
            return false;
        }

        for (const helen::ExternalBindingDefinition& binding : external_bindings)
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
     * @brief Registers every merged virtual file declaration in the active virtual file service.
     * @param virtual_files Pack-scoped virtual files selected for the host executable.
     * @return True when every virtual file registration succeeds; otherwise false.
     */
    bool RegisterDeclaredVirtualFiles(const std::vector<helen::PackScopedVirtualFileRegistration>& virtual_files)
    {
        if (g_virtual_files == nullptr)
        {
            helen::Log(L"[runtime] virtual file service is not available during virtual file registration.");
            return false;
        }

        for (const helen::PackScopedVirtualFileRegistration& registration : virtual_files)
        {
            if (!g_virtual_files->RegisterVirtualFile(registration))
            {
                helen::Logf(
                    L"[runtime] failed to register virtual file id=%ls gamePath=%ls source=%ls",
                    ToWideString(registration.Definition.Id).c_str(),
                    registration.Definition.GamePath.c_str(),
                    registration.Definition.Source.Path.c_str());
                return false;
            }
        }

        return true;
    }

    /**
     * @brief Creates and wires every runtime-owned service for one merged active pack set.
     * @param layout Resolved runtime directory layout that owns writable cache and config directories.
     * @param active_pack_set Merged runtime view chosen for the host executable.
     * @return True when every service initializes successfully; otherwise false.
     */
    bool InitializeActivePackRuntime(const helen::RuntimeLayout& layout, const helen::ActivePackSet& active_pack_set)
    {
        helen::Logf(
            L"[runtime] active-pack init begin packs=%zu commands=%zu virtualFiles=%zu hooks=%zu textures=%zu",
            active_pack_set.LoadedPacks.size(),
            active_pack_set.Commands.size(),
            active_pack_set.VirtualFiles.size(),
            active_pack_set.Hooks.size(),
            active_pack_set.TextureReplacements.size());

        if (!RegisterDeclaredConfigEntries(active_pack_set.ConfigEntries))
        {
            return false;
        }

        helen::RegisterWindowBehaviorConfigKeys(*g_command_dispatcher);
        helen::Log(L"[runtime] active-pack init config entries ready.");

        g_runtime_values = std::make_unique<helen::RuntimeValueStore>();
        helen::Log(L"[runtime] active-pack init runtime value store created.");
        if (!RegisterDeclaredRuntimeSlots(active_pack_set.RuntimeSlots))
        {
            return false;
        }

        if (g_command_dispatcher == nullptr)
        {
            helen::Log(L"[runtime] command dispatcher is not available for executor creation.");
            return false;
        }

        const std::optional<std::filesystem::path> batman_engine_ini_path = TryGetBatmanUserEngineIniPath();
        if (!batman_engine_ini_path.has_value())
        {
            helen::Log(L"[runtime] failed to resolve the Batman user BmEngine.ini path.");
            return false;
        }

        g_batman_display_mode_service = std::make_unique<helen::BatmanDisplayModeService>();
        g_batman_graphics_config_service = std::make_unique<helen::BatmanGraphicsConfigService>(
            *batman_engine_ini_path,
            *g_batman_display_mode_service);
        helen::Logf(L"[runtime] active-pack init batman ini=%ls", batman_engine_ini_path->c_str());
        g_command_executor = std::make_unique<helen::CommandExecutor>(
            *g_command_dispatcher,
            *g_runtime_values,
            *g_batman_graphics_config_service);
        helen::Log(L"[runtime] active-pack init command executor created.");
        if (!RegisterDeclaredCommands(active_pack_set.Commands))
        {
            return false;
        }

        g_external_bindings = std::make_unique<helen::ExternalBindingService>(*g_command_dispatcher, *g_command_executor);
        helen::Log(L"[runtime] active-pack init external binding service created.");
        if (!RegisterDeclaredExternalBindings(active_pack_set.ExternalBindings))
        {
            return false;
        }

        g_virtual_files = std::make_unique<helen::VirtualFileService>(layout.CacheDirectory);
        helen::Log(L"[runtime] active-pack init virtual file service created.");
        if (!RegisterDeclaredVirtualFiles(active_pack_set.VirtualFiles))
        {
            return false;
        }

        g_build_hooks = std::make_unique<helen::BuildHookInstaller>();
        helen::Log(L"[runtime] active-pack init build hook installer created.");
        if (!g_build_hooks->Install(active_pack_set.Hooks, *g_runtime_values))
        {
            helen::Log(L"[runtime] failed to install build hooks.");
            return false;
        }
        helen::Log(L"[runtime] active-pack init build hooks installed.");

        g_file_hooks = std::make_unique<helen::FileApiHookSet>(
            *g_virtual_files,
            layout.GameRoot.parent_path(),
            layout.GameRoot,
            active_pack_set.MissingPaths);
        helen::Log(L"[runtime] active-pack init file api hook set created.");
        if (!g_file_hooks->Install())
        {
            helen::Log(L"[runtime] failed to install file API hooks.");
            return false;
        }
        helen::Log(L"[runtime] active-pack init file API hooks installed.");

        g_d3d9_texture_hooks = std::make_unique<helen::D3d9TextureReplacementHookSet>(
            active_pack_set.EnableD3d9TextureReplacementHooks,
            active_pack_set.EnableD3d9TextureHashLogging,
            active_pack_set.EnableD3d9TextureImageDumping,
            layout.LogsDirectory / L"d3d9-textures",
            active_pack_set.TextureReplacements);
        helen::Logf(
            L"[runtime] active-pack init d3d9 hooks created enable=%d hash=%d dump=%d replacements=%zu",
            static_cast<int>(active_pack_set.EnableD3d9TextureReplacementHooks),
            static_cast<int>(active_pack_set.EnableD3d9TextureHashLogging),
            static_cast<int>(active_pack_set.EnableD3d9TextureImageDumping),
            active_pack_set.TextureReplacements.size());
        helen::Log(L"[runtime] active-pack init d3d9 hooks install begin.");
        if (!g_d3d9_texture_hooks->Install())
        {
            helen::Log(L"[runtime] failed to install D3D9 texture replacement hooks.");
            return false;
        }
        helen::Log(L"[runtime] active-pack init d3d9 hooks install returned.");

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
            helen::Log(L"[runtime] active-pack init window behavior hook set created.");
            if (!g_window_behavior_hooks->Install())
            {
                helen::Log(L"[runtime] window behavior hook set failed to install; continuing without window behavior overrides.");
                g_window_behavior_hooks.reset();
            }
            else
            {
                helen::Log(L"[runtime] active-pack init window behavior hook set installed.");
            }
        }

        helen::Log(L"[runtime] active-pack init build runtime coordinator create begin.");
        const std::shared_ptr<helen::BatmanDisplayModeResponseProvider> display_mode_response_provider =
            std::make_shared<helen::BatmanDisplayModeResponseProvider>(
                *g_batman_display_mode_service,
                *g_command_dispatcher);
        const helen::MemoryStateObserverDynamicResponseCallback dynamic_response_callback =
            [display_mode_response_provider](
                const std::string& provider_id,
                int raw_request) -> std::optional<int>
        {
            return display_mode_response_provider->Resolve(provider_id, raw_request);
        };
        g_build_runtime_coordinator = std::make_unique<helen::BuildRuntimeCoordinator>(
            active_pack_set.StartupCommandIds,
            active_pack_set.StateObservers,
            *g_command_dispatcher,
            *g_command_executor,
            dynamic_response_callback);
        helen::Log(L"[runtime] active-pack init build runtime coordinator created.");
        helen::Log(L"[runtime] active-pack init build runtime coordinator start begin.");
        if (!g_build_runtime_coordinator->Start())
        {
            helen::Log(L"[runtime] failed to start build runtime coordinator.");
            return false;
        }

        helen::Log(L"[runtime] active-pack init complete.");

        return true;
    }

    /**
     * @brief Releases pack-scoped runtime objects in reverse dependency order.
     */
    void ResetPackRuntimeState()
    {
        g_build_runtime_coordinator.reset();
        g_window_behavior_hooks.reset();
        g_d3d9_texture_hooks.reset();
        g_file_hooks.reset();
        g_build_hooks.reset();
        g_virtual_files.reset();
        g_external_bindings.reset();
        g_command_executor.reset();
        g_batman_graphics_config_service.reset();
        g_batman_display_mode_service.reset();
        g_runtime_values.reset();
    }

    /**
     * @brief Clears process-owned runtime objects outside the Windows loader lock.
     */
    void ResetRuntimeState()
    {
        g_module_load_routing_hooks.reset();
        g_module_load_routing_service.reset();
        ResetPackRuntimeState();
        g_active_runtime_pack_set.reset();
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
        static_cast<void>(g_window_behavior_hooks.release());
        static_cast<void>(g_d3d9_texture_hooks.release());
        static_cast<void>(g_build_hooks.release());
        static_cast<void>(g_file_hooks.release());
        static_cast<void>(g_virtual_files.release());
        static_cast<void>(g_external_bindings.release());
        static_cast<void>(g_command_executor.release());
        static_cast<void>(g_runtime_values.release());
        static_cast<void>(g_module_load_routing_hooks.release());
        static_cast<void>(g_module_load_routing_service.release());
        static_cast<void>(g_command_dispatcher.release());
        static_cast<void>(g_runtime_config.release());
        g_active_runtime_pack_set.reset();
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
    helen::Log(L"[runtime] HelenInitialize enter.");
    helen::Logf(L"[runtime] module=%ls", module_path.c_str());
    helen::Logf(L"[runtime] helen_root=%ls", layout.HelenRoot.c_str());
    helen::Logf(L"[runtime] logs=%ls", layout.LogsDirectory.c_str());
    if (!InitializeCommandSurface(layout))
    {
        ResetRuntimeState();
        return FALSE;
    }
    helen::Log(L"[runtime] command surface initialized.");

    if (!InitializeModuleLoadRouting())
    {
        ResetRuntimeState();
        return FALSE;
    }
    helen::Log(L"[runtime] module load routing ready.");

    if (!InitializePackRepository(layout))
    {
        ResetRuntimeState();
        return FALSE;
    }
    helen::Log(L"[runtime] pack repository initialized.");

    if (g_active_runtime_pack_set.has_value() && !InitializeActivePackRuntime(layout, *g_active_runtime_pack_set))
    {
        ResetRuntimeState();
        return FALSE;
    }

    helen::Log(L"[runtime] HelenInitialize complete.");

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
 * @return The resolved config value when initialization is complete and the binding resolves successfully; otherwise 0.
 */
extern "C" __declspec(dllexport) int __stdcall HelenGetIntA(const char* key)
{
    std::lock_guard<std::mutex> lock(g_mutex);
    if (!g_initialized || g_external_bindings == nullptr || key == nullptr)
    {
        helen::Log(L"[runtime] Helen_GetInt rejected before binding resolution.");
        return 0;
    }

    int resolved_value = 0;
    if (!g_external_bindings->TryHandleGetInt("Helen_GetInt", key, resolved_value))
    {
        helen::Logf(L"[runtime] Helen_GetInt key=%hs failed", key);
        return 0;
    }

    helen::Logf(L"[runtime] Helen_GetInt key=%hs value=%d", key, resolved_value);
    return resolved_value;
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
        helen::Log(L"[runtime] Helen_SetInt rejected before binding resolution.");
        return FALSE;
    }

    const BOOL succeeded = g_external_bindings->TryHandleSetInt("Helen_SetInt", key, value) ? TRUE : FALSE;
    helen::Logf(L"[runtime] Helen_SetInt key=%hs value=%d result=%d", key, value, static_cast<int>(succeeded));
    return succeeded;
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
        helen::Log(L"[runtime] Helen_RunCommand rejected before binding resolution.");
        return FALSE;
    }

    const BOOL succeeded = g_external_bindings->TryHandleRunCommand("Helen_RunCommand", command_id) ? TRUE : FALSE;
    helen::Logf(L"[runtime] Helen_RunCommand command=%hs result=%d", command_id, static_cast<int>(succeeded));
    return succeeded;
}

/**
 * @brief Applies the Batman graphics draft through the native command executor and logs the full draft before and after execution.
 * @return True when the runtime is initialized and `applyBatmanGraphicsDraft` completes successfully; otherwise false.
 */
extern "C" __declspec(dllexport) BOOL __stdcall HelenApplyBatmanGraphicsDraftA()
{
    std::lock_guard<std::mutex> lock(g_mutex);
    helen::Log(L"[runtime] Helen_ApplyBatmanGraphicsDraft enter.");
    if (!g_initialized)
    {
        helen::Log(L"[runtime] Helen_ApplyBatmanGraphicsDraft rejected: runtime not initialized.");
        return FALSE;
    }

    if (g_command_executor == nullptr)
    {
        helen::Log(L"[runtime] Helen_ApplyBatmanGraphicsDraft rejected: command executor missing.");
        return FALSE;
    }

    if (g_command_dispatcher == nullptr)
    {
        helen::Log(L"[runtime] Helen_ApplyBatmanGraphicsDraft rejected: command dispatcher missing.");
        return FALSE;
    }

    LogBatmanGraphicsDraftSnapshot(L"Helen_ApplyBatmanGraphicsDraft before");
    const BOOL succeeded = g_command_executor->RunCommand("applyBatmanGraphicsDraft") ? TRUE : FALSE;
    helen::Logf(L"[runtime] Helen_ApplyBatmanGraphicsDraft result=%d", static_cast<int>(succeeded));
    LogBatmanGraphicsDraftSnapshot(L"Helen_ApplyBatmanGraphicsDraft after");
    return succeeded;
}

/**
 * @brief Writes one UI-originated diagnostic message into the runtime log stream.
 * @param message UTF-8-compatible narrow string supplied by an external UI callback such as `Helen_Log`.
 * @return True when the message is non-null and was accepted for logging; otherwise false.
 *
 * The callback is intentionally non-throwing so malformed UI text cannot crash the game process.
 */
extern "C" __declspec(dllexport) BOOL __stdcall HelenLogA(const char* message)
{
    try
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        if (message == nullptr)
        {
            helen::Log(L"[runtime] Helen_Log rejected null message.");
            return FALSE;
        }

        const std::wstring wide_message = ConvertUiMessageToWide(message);
        helen::Log(L"[ui] " + wide_message);
        return TRUE;
    }
    catch (...)
    {
        return FALSE;
    }
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
