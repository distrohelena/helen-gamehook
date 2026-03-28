#include <HelenHook/BuildHookInstaller.h>
#include <HelenHook/Memory.h>
#include <HelenHook/PackAssetResolver.h>

#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
    /**
     * @brief Dedicated inline-hook target used by the installer tests.
     * @param value Integer argument supplied by the test harness.
     * @return Deterministic transformed value used to prove the function still works after hook removal.
     */
    __declspec(noinline) int BuildHookInstallerPrimaryTarget(int value)
    {
        return value + 7;
    }

    /**
     * @brief Second dedicated inline-hook target used by the rollback installer scenario.
     * @param value Integer argument supplied by the test harness.
     * @return Deterministic transformed value used to prove the function still works after rollback.
     */
    __declspec(noinline) int BuildHookInstallerRollbackTarget(int value)
    {
        return value + 13;
    }

    /**
     * @brief Throws when one required boolean condition is false so the shared test harness stops at the first failure.
     * @param condition Boolean condition that must evaluate to true.
     * @param message Failure message reported by the shared test runner.
     */
    void Expect(bool condition, const char* message)
    {
        if (!condition)
        {
            throw std::runtime_error(message);
        }
    }

    /**
     * @brief Writes one exact binary payload to disk for hook blob asset scenarios.
     * @param path Destination file path that should be created or replaced.
     * @param bytes Binary payload written into the file.
     */
    void WriteAllBytes(const std::filesystem::path& path, const std::vector<std::uint8_t>& bytes)
    {
        std::ofstream stream(path, std::ios::binary | std::ios::trunc);
        if (!stream)
        {
            throw std::runtime_error("Failed to create a build hook installer test blob.");
        }

        if (!bytes.empty())
        {
            stream.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        }

        if (!stream)
        {
            throw std::runtime_error("Failed to write a build hook installer test blob.");
        }
    }

    /**
     * @brief Copies one contiguous byte range from a function entry so inline patch installation and removal can be compared.
     * @param address Absolute address of the byte range that should be copied.
     * @param byte_count Number of bytes that should be copied.
     * @return Snapshot of the requested bytes in original order.
     */
    std::vector<std::uint8_t> ReadBytes(std::uintptr_t address, std::size_t byte_count)
    {
        const auto* source = reinterpret_cast<const std::uint8_t*>(address);
        return std::vector<std::uint8_t>(source, source + byte_count);
    }

    /**
     * @brief Converts one byte sequence into compact uppercase hexadecimal text compatible with expected-bytes validation.
     * @param bytes Byte sequence that should be converted.
     * @return Compact hexadecimal string with two characters per byte.
     */
    std::string ToCompactHexText(const std::vector<std::uint8_t>& bytes)
    {
        static constexpr char HexDigits[] = "0123456789ABCDEF";

        std::string text;
        text.reserve(bytes.size() * 2);
        for (const std::uint8_t byte : bytes)
        {
            text.push_back(HexDigits[(byte >> 4) & 0x0F]);
            text.push_back(HexDigits[byte & 0x0F]);
        }

        return text;
    }

    /**
     * @brief Returns the main module filename expected by the hook installer module lookup path.
     * @return Main executable filename encoded as narrow text.
     */
    std::string GetMainModuleName()
    {
        const std::optional<helen::ModuleView> main_module = helen::QueryMainModule();
        Expect(main_module.has_value(), "Expected the build hook installer tests to resolve the main module.");
        return main_module->path.filename().string();
    }

    /**
     * @brief Converts one absolute function address into a module-relative RVA inside the main executable image.
     * @param function_address Absolute address of the function that should be hooked.
     * @return 32-bit relative virtual address inside the main module image.
     */
    std::uint32_t GetFunctionRva(std::uintptr_t function_address)
    {
        const std::optional<helen::ModuleView> main_module = helen::QueryMainModule();
        Expect(main_module.has_value(), "Expected the build hook installer tests to resolve the main module before computing an RVA.");
        Expect(function_address >= main_module->base_address, "Function address fell below the main module base address.");
        const std::uintptr_t relative_virtual_address = function_address - main_module->base_address;
        Expect(relative_virtual_address <= static_cast<std::uintptr_t>((std::numeric_limits<std::uint32_t>::max)()), "Function RVA exceeded the 32-bit hook metadata range.");
        return static_cast<std::uint32_t>(relative_virtual_address);
    }

    /**
     * @brief Builds one blob-backed inline-jump hook definition that targets the supplied function RVA.
     * @param id Stable hook identifier assigned to the definition.
     * @param function_rva Exact RVA of the target bytes inside the main executable image.
     * @param expected_bytes Compact hexadecimal text used to validate the target before patching.
     * @param asset_path Build-relative blob asset path loaded by the hook installer.
     * @return Fully populated hook definition for one installer scenario.
     */
    helen::HookDefinition CreateInlineJumpHook(
        const char* id,
        std::uint32_t function_rva,
        const std::string& expected_bytes,
        const std::filesystem::path& asset_path)
    {
        helen::HookDefinition hook;
        hook.Id = id;
        hook.ModuleName = GetMainModuleName();
        hook.RelativeVirtualAddress = function_rva;
        hook.ExpectedBytes = expected_bytes;
        hook.Action = "inline-jump-to-pack-blob";
        hook.OverwriteLength = 5;
        hook.ResumeOffsetFromTarget = 5;
        hook.Blob.AssetPath = asset_path;
        hook.Blob.EntryOffset = 0;
        return hook;
    }
}

/**
 * @brief Verifies that exact-RVA blob-backed hooks install cleanly, expose debug state, and roll back if a later hook fails.
 */
void RunBuildHookInstallerTests()
{
    const std::filesystem::path root = std::filesystem::temp_directory_path() / "HelenRuntimeTests" / "BuildHookInstaller";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);

    try
    {
        const std::filesystem::path pack_root = root / "pack";
        const std::filesystem::path build_root = pack_root / "builds" / "steam-goty-1.0";
        const std::filesystem::path blob_path = build_root / "assets" / "native" / "test-hook.bin";
        std::filesystem::create_directories(blob_path.parent_path());
        WriteAllBytes(blob_path, { 0x90, 0xC3 });

        const helen::PackAssetResolver resolver(pack_root, build_root);
        const helen::RuntimeValueStore runtime_values;

        const std::uintptr_t primary_address = reinterpret_cast<std::uintptr_t>(&BuildHookInstallerPrimaryTarget);
        const std::uintptr_t rollback_address = reinterpret_cast<std::uintptr_t>(&BuildHookInstallerRollbackTarget);
        const std::vector<std::uint8_t> primary_original_bytes = ReadBytes(primary_address, 5);
        const std::vector<std::uint8_t> rollback_original_bytes = ReadBytes(rollback_address, 5);

        const helen::HookDefinition primary_hook = CreateInlineJumpHook(
            "primary-hook",
            GetFunctionRva(primary_address),
            ToCompactHexText(primary_original_bytes),
            "assets/native/test-hook.bin");

        helen::BuildHookInstaller installer(resolver);
        Expect(installer.Install({ primary_hook }, runtime_values), "Expected the primary blob-backed hook to install.");

        const std::vector<std::uint8_t> primary_patched_bytes = ReadBytes(primary_address, 5);
        Expect(primary_patched_bytes[0] == 0xE9, "Expected the primary hook target to start with an inline detour after installation.");

        const std::vector<helen::BuildHookInstaller::InstalledHookDebugView> installed_hooks = installer.GetInstalledHooks();
        Expect(installed_hooks.size() == 1, "Installed hook debug view count mismatch.");
        Expect(installed_hooks[0].Id == "primary-hook", "Installed hook identifier mismatch.");
        Expect(installed_hooks[0].TargetAddress == primary_address, "Installed hook target address mismatch.");
        Expect(installed_hooks[0].BlobAddress != 0, "Installed hook blob address was not recorded.");
        Expect(installed_hooks[0].EntryAddress != 0, "Installed hook entry address was not recorded.");
        Expect(installed_hooks[0].ResumeAddress == primary_address + 5, "Installed hook resume address mismatch.");
        Expect(installed_hooks[0].Relocations.empty(), "Unexpected relocation debug state was captured for a relocation-free blob.");

        installer.Remove();
        Expect(ReadBytes(primary_address, 5) == primary_original_bytes, "Primary hook bytes were not restored after Remove.");
        Expect(BuildHookInstallerPrimaryTarget(5) == 12, "Primary hook target no longer behaved correctly after Remove.");

        const helen::HookDefinition rollback_hook = CreateInlineJumpHook(
            "rollback-hook",
            GetFunctionRva(rollback_address),
            ToCompactHexText(rollback_original_bytes),
            "assets/native/missing-hook.bin");

        helen::BuildHookInstaller failing_installer(resolver);
        Expect(!failing_installer.Install({ primary_hook, rollback_hook }, runtime_values), "Installer unexpectedly succeeded when a later hook blob was missing.");
        Expect(failing_installer.GetInstalledHooks().empty(), "Rollback installer retained debug state after a failed installation.");
        Expect(ReadBytes(primary_address, 5) == primary_original_bytes, "Rollback installer failed to restore the primary hook bytes after the later hook failed.");
        Expect(ReadBytes(rollback_address, 5) == rollback_original_bytes, "Rollback installer changed the secondary hook bytes even though the hook never installed.");
        Expect(BuildHookInstallerRollbackTarget(5) == 18, "Rollback hook target no longer behaved correctly after the failed installation.");
    }
    catch (...)
    {
        std::filesystem::remove_all(root);
        throw;
    }

    std::filesystem::remove_all(root);
}
