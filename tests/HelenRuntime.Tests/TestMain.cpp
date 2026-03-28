#include <exception>
#include <iostream>

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
 * @brief Runs the runtime layout test coverage for the filesystem paths used by the runtime.
 */
void RunRuntimeLayoutTests();

/**
 * @brief Runs the runtime value store test coverage for declared slot registration, address access, and snapshots.
 */
void RunRuntimeValueStoreTests();

/**
 * @brief Runs the virtual file service test coverage for RAM-backed exact-path replacement handles.
 */
void RunVirtualFileServiceTests();

/**
 * @brief Runs the native runtime tests and reports the first failure to stderr.
 */
int main()
{
    try
    {
        RunBuildHookInstallerTests();
        RunRuntimeLayoutTests();
        RunJsonConfigStoreTests();
        RunCommandDispatcherTests();
        RunRuntimeValueStoreTests();
        RunCommandExecutorTests();
        RunExternalBindingServiceTests();
        RunHelenGameHookExportTests();
        RunExecutableFingerprintTests();
        RunPackAssetResolverTests();
        RunPackRepositoryTests();
        RunHookBlobRelocatorTests();
        RunMemoryStateObserverServiceTests();
        RunVirtualFileServiceTests();
        RunDebugTraceServiceTests();
        std::cout << "PASS\n";
        return 0;
    }
    catch (const std::exception& exception)
    {
        std::cerr << exception.what() << "\n";
        return 1;
    }
}
