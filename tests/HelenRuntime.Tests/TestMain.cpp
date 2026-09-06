#include <exception>
#include <iostream>
#include <windows.h>

/**
 * @brief Runs merge-validation coverage for the active multi-pack runtime builder.
 */
void RunActivePackSetBuilderTests();

/**
 * @brief Runs the Batman supported display-mode catalog coverage.
 */
void RunBatmanDisplayModeServiceTests();

/**
 * @brief Runs Batman dynamic display-provider sequence isolation tests.
 */
void RunBatmanDisplayModeResponseProviderTests();

/**
 * @brief Runs the build runtime coordinator coverage for startup commands and state-observer driven command dispatch.
 */
void RunBuildRuntimeCoordinatorTests();

/**
 * @brief Runs the debug trace service coverage for structured runtime snapshots and JSONL event output.
 */
void RunDebugTraceServiceTests();

/**
 * @brief Runs the export contract coverage for the gameplay callback names consumed by patched assets.
 */
void RunHelenGameHookExportTests();

/**
 * @brief Runs the build hook installer test coverage for generic blob-backed inline jumps.
 */
void RunBuildHookInstallerTests();

/**
 * @brief Runs the command executor test coverage for declarative config-backed commands.
 */
void RunCommandExecutorTests();

/**
 * @brief Runs the command dispatcher test coverage for integer config storage and persistence.
 */
void RunCommandDispatcherTests();

/**
 * @brief Runs the executable fingerprint test coverage used by the pack repository matcher.
 */
void RunExecutableFingerprintTests();

/**
 * @brief Runs the generic file fingerprint test coverage for arbitrary file hashing.
 */
void RunFileFingerprintTests();

/**
 * @brief Runs the hidden-path matcher coverage for normalized absolute and relative game paths.
 */
void RunHiddenPathMatcherTests();

/**
 * @brief Runs the external binding service test coverage for patched gameplay callback resolution.
 */
void RunExternalBindingServiceTests();

/**
 * @brief Runs the hook blob relocator test coverage for native blob patch-site fixups.
 */
void RunHookBlobRelocatorTests();

/**
 * @brief Runs the memory state observer service coverage for bounded signature scans and cached change notifications.
 */
void RunMemoryStateObserverServiceTests();

/**
 * @brief Runs the JSON config store test coverage for persistent runtime settings.
 */
void RunJsonConfigStoreTests();

/**
 * @brief Runs the pack asset resolver test coverage for build-local and shared pack assets.
 */
void RunPackAssetResolverTests();

/**
 * @brief Runs the pack repository test coverage for build and pack selection.
 */
void RunPackRepositoryTests();

/**
 * @brief Runs the explicit pack-selection config coverage for ordered enabled-pack lists.
 */
void RunPackSelectionConfigTests();

/**
 * @brief Runs the proxy bootstrap coordinator coverage for early asynchronous Helen startup.
 */
void RunProxyBootstrapCoordinatorTests();

/**
 * @brief Runs the file API hidden-path helper coverage for common game path forms.
 */
void RunFileApiHookSetTests();

/**
 * @brief Runs the module-load routing service coverage for Bink alias normalization and redirect validation.
 */
void RunModuleLoadRoutingServiceTests();

/**
 * @brief Runs the module-load routing hook coverage for IAT installation and Bink request logging.
 */
void RunModuleLoadRoutingHookSetTests();

/**
 * @brief Runs the runtime layout test coverage for the filesystem paths used by the runtime.
 */
void RunRuntimeLayoutTests();

/**
 * @brief Runs the runtime value store test coverage for declared slot registration, address access, and snapshots.
 */
void RunRuntimeValueStoreTests();

/**
 * @brief Runs the hgdelta container parsing test coverage used by future delta-backed file sources.
 */
void RunHgdeltaFileTests();

/**
 * @brief Runs the texture dump serializer test coverage for DDS extraction of compressed live textures.
 */
void RunTextureDumpSerializerTests();

/**
 * @brief Runs the replacement asset loader test coverage for larger DDS texture swaps.
 */
void RunTextureReplacementAssetLoaderTests();

/**
 * @brief Runs the delta-backed virtual file source coverage for exact base-file reconstruction.
 */
void RunDeltaVirtualFileSourceTests();

/**
 * @brief Runs the virtual file service test coverage for RAM-backed exact-path replacement handles.
 */
void RunVirtualFileServiceTests();

/** @brief Runs real temporary-file session routing and trusted transaction coverage. */
void RunFileWriteRoutingServiceTests();

/** @brief Runs the real imported Win32 IAT routing fixture for session mutations. */
void RunFileWriteRoutingHookFixtureTests();

/**
 * @brief Runs the window behavior config coverage for generic `window.*` keys and derived hook plans.
 */
void RunWindowBehaviorConfigTests();

/**
 * @brief Runs the window behavior hook coverage for focus spoofing and WndProc filtering.
 */
void RunWindowBehaviorHookSetTests();

/** @brief Runs read-only graphics snapshot coverage against real temporary launcher INIs. */
void RunBatmanGraphicsSnapshotTests();

/** @brief Runs final process-lockout checks; no graphics writes may follow this suite. */
void RunBatmanGraphicsIntegrityFailureTests();

/** @brief Runs native tests with console-only failure reporting and no Windows crash dialogs. */
int main()
{
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
    try
    {
        RunBatmanGraphicsSnapshotTests();
        RunActivePackSetBuilderTests();
        RunBatmanDisplayModeServiceTests();
        RunBatmanDisplayModeResponseProviderTests();
        RunBuildHookInstallerTests();
        RunRuntimeLayoutTests();
        RunJsonConfigStoreTests();
        RunCommandDispatcherTests();
        RunRuntimeValueStoreTests();
        RunCommandExecutorTests();
        RunBuildRuntimeCoordinatorTests();
        RunExternalBindingServiceTests();
        RunHelenGameHookExportTests();
        RunFileFingerprintTests();
        RunHiddenPathMatcherTests();
        RunExecutableFingerprintTests();
        RunHgdeltaFileTests();
        RunTextureDumpSerializerTests();
        RunTextureReplacementAssetLoaderTests();
        RunDeltaVirtualFileSourceTests();
        RunPackAssetResolverTests();
        RunPackSelectionConfigTests();
        RunPackRepositoryTests();
        RunProxyBootstrapCoordinatorTests();
        RunFileApiHookSetTests();
        RunModuleLoadRoutingServiceTests();
        RunModuleLoadRoutingHookSetTests();
        RunHookBlobRelocatorTests();
        RunMemoryStateObserverServiceTests();
        RunVirtualFileServiceTests();
        RunFileWriteRoutingServiceTests();
        RunFileWriteRoutingHookFixtureTests();
        RunDebugTraceServiceTests();
        RunWindowBehaviorConfigTests();
        RunWindowBehaviorHookSetTests();
        RunBatmanGraphicsIntegrityFailureTests();
        std::cout << "PASS\n";
        return 0;
    }
    catch (const std::exception& exception)
    {
        std::cerr << exception.what() << "\n";
        return 1;
    }
}
