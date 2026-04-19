#include <HelenHook/BatmanGraphicsConfigService.h>
#include <HelenHook/BuildRuntimeCoordinator.h>
#include <HelenHook/CommandDefinition.h>
#include <HelenHook/CommandDispatcher.h>
#include <HelenHook/CommandExecutor.h>
#include <HelenHook/CommandMapEntryDefinition.h>
#include <HelenHook/CommandStepDefinition.h>
#include <HelenHook/MemoryStateObserverCheckDefinition.h>
#include <HelenHook/MemoryStateObserverDefinition.h>
#include <HelenHook/MemoryStateObserverMapEntryDefinition.h>
#include <HelenHook/RuntimeSlotDefinition.h>
#include <HelenHook/RuntimeValueStore.h>

#include <Windows.h>

#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <stdexcept>

namespace
{
    /**
     * @brief Throws when one required boolean condition is false so the shared test runner stops at the first regression.
     * @param condition Boolean condition that must evaluate to true.
     * @param message Failure message reported by the shared runtime test harness.
     */
    void Expect(bool condition, const char* message)
    {
        if (!condition)
        {
            throw std::runtime_error(message);
        }
    }

    /**
     * @brief Throws when two floating-point values differ by more than the supplied tolerance.
     * @param actual Value produced by the runtime coordinator.
     * @param expected Value required by the current test scenario.
     * @param tolerance Maximum permitted absolute difference.
     * @param message Failure message reported by the shared runtime test harness.
     */
    void ExpectNear(double actual, double expected, double tolerance, const char* message)
    {
        if (std::fabs(actual - expected) > tolerance)
        {
            throw std::runtime_error(message);
        }
    }

    /**
     * @brief Writes one signed 32-bit integer into writable process memory for observer test setup.
     * @param address Writable address that should receive the integer value.
     * @param value Integer value written to the supplied address.
     */
    void WriteInt32(std::uintptr_t address, int value)
    {
        std::memcpy(reinterpret_cast<void*>(address), &value, sizeof(value));
    }

    /**
     * @brief Configures one Batman-style subtitle-size state block around the supplied base address.
     * @param base_address Candidate observer base address that should satisfy the configured checks.
     * @param raw_value Raw subtitle-size state code written at the observer value offset.
     */
    void ConfigureStateBlock(std::uintptr_t base_address, int raw_value)
    {
        WriteInt32(base_address - 16, 50);
        WriteInt32(base_address, raw_value);
        WriteInt32(base_address + 4, 1);
        WriteInt32(base_address + 8, 0);
        WriteInt32(base_address + 12, 1);
        WriteInt32(base_address + 16, raw_value);
        WriteInt32(base_address + 20, 2);
        WriteInt32(base_address + 28, 3);
        WriteInt32(base_address + 32, 3);
    }

    /**
     * @brief Builds the runtime slot definition used by the startup-command and observer coverage.
     * @return Float32 runtime slot definition for the live subtitle scale target.
     */
    helen::RuntimeSlotDefinition CreateSubtitleScaleSlot()
    {
        helen::RuntimeSlotDefinition definition;
        definition.Id = "subtitle.scale";
        definition.Type = "float32";
        definition.InitialValue = 1.5;
        return definition;
    }

    /**
     * @brief Builds the standard subtitle-size apply command used by the coordinator coverage.
     * @param id Stable command identifier assigned to the definition.
     * @return Fully populated command definition that maps config state into the live subtitle scale slot.
     */
    helen::CommandDefinition CreateApplySubtitleSizeCommand(const char* id)
    {
        helen::CommandDefinition command;
        command.Id = id;
        command.Name = "Apply Subtitle Size";

        helen::CommandStepDefinition read_step;
        read_step.Kind = "read-config-int";
        read_step.ConfigKey = "ui.subtitleSize";
        read_step.ValueName = "subtitleSizeState";
        command.Steps.push_back(read_step);

        helen::CommandStepDefinition map_step;
        map_step.Kind = "map-int-to-double";
        map_step.InputValueName = "subtitleSizeState";
        map_step.OutputValueName = "subtitleScale";
        map_step.Mappings.push_back(helen::CommandMapEntryDefinition{ .Match = 0, .Value = 2.0 });
        map_step.Mappings.push_back(helen::CommandMapEntryDefinition{ .Match = 1, .Value = 4.0 });
        map_step.Mappings.push_back(helen::CommandMapEntryDefinition{ .Match = 2, .Value = 8.0 });
        command.Steps.push_back(map_step);

        helen::CommandStepDefinition set_live_step;
        set_live_step.Kind = "set-live-double";
        set_live_step.Target = "subtitle.scale";
        set_live_step.ValueName = "subtitleScale";
        command.Steps.push_back(set_live_step);

        return command;
    }

    /**
     * @brief Builds one startup command that simply runs the main subtitle apply command.
     * @return Nested command definition used to verify startup command execution.
     */
    helen::CommandDefinition CreateApplySavedSubtitleSizeCommand()
    {
        helen::CommandDefinition command;
        command.Id = "applySavedSubtitleSize";
        command.Name = "Apply Saved Subtitle Size";

        helen::CommandStepDefinition run_step;
        run_step.Kind = "run-command";
        run_step.CommandId = "applySubtitleSize";
        command.Steps.push_back(run_step);

        return command;
    }

    /**
     * @brief Builds the bounded subtitle-size observer used to verify live menu updates.
     * @param scan_start Inclusive scan start address.
     * @param scan_end Exclusive scan end address.
     * @return Fully populated observer definition that maps Batman raw codes into Helen config values.
     */
    helen::MemoryStateObserverDefinition CreateSubtitleObserver(std::uintptr_t scan_start, std::uintptr_t scan_end)
    {
        helen::MemoryStateObserverDefinition definition;
        definition.Id = "subtitleUiStateObserver";
        definition.ScanStartAddress = scan_start;
        definition.ScanEndAddress = scan_end;
        definition.ScanStride = 4;
        definition.ValueOffset = 0;
        definition.PollIntervalMs = 1;
        definition.TargetConfigKey = "ui.subtitleSize";
        definition.CommandId = "applySubtitleSize";

        const int constant_offsets[] = { -16, 4, 8, 12, 20, 28, 32 };
        const int constant_values[] = { 50, 1, 0, 1, 2, 3, 3 };
        for (int index = 0; index < 7; ++index)
        {
            helen::MemoryStateObserverCheckDefinition check;
            check.Comparison = "equals-constant";
            check.Offset = constant_offsets[index];
            check.ExpectedValue = constant_values[index];
            definition.Checks.push_back(check);
        }

        helen::MemoryStateObserverCheckDefinition mirror_check;
        mirror_check.Comparison = "equals-value-at-offset";
        mirror_check.Offset = 16;
        mirror_check.CompareOffset = 0;
        definition.Checks.push_back(mirror_check);

        definition.Mappings.push_back(helen::MemoryStateObserverMapEntryDefinition{ .Match = 4101, .Value = 0 });
        definition.Mappings.push_back(helen::MemoryStateObserverMapEntryDefinition{ .Match = 4102, .Value = 1 });
        definition.Mappings.push_back(helen::MemoryStateObserverMapEntryDefinition{ .Match = 4103, .Value = 2 });
        return definition;
    }

    /**
     * @brief Builds a unique temporary `BmEngine.ini` path for the current test process.
     * @return Absolute temporary path reserved for the current test run.
     */
    std::filesystem::path CreateTemporaryBatmanGraphicsIniPath()
    {
        const DWORD process_id = GetCurrentProcessId();
        return
            std::filesystem::temp_directory_path() /
            "HelenRuntimeTests" /
            ("runtime-coordinator-" + std::to_string(process_id)) /
            "BmEngine.ini";
    }
}

/**
 * @brief Verifies that the build runtime coordinator executes startup commands and applies state-observer updates through the generic command surface.
 */
void RunBuildRuntimeCoordinatorTests()
{
    SYSTEM_INFO system_info{};
    GetSystemInfo(&system_info);
    const std::size_t page_size = system_info.dwPageSize;
    void* const allocation = VirtualAlloc(nullptr, page_size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    Expect(allocation != nullptr, "Failed to allocate writable memory for the build runtime coordinator tests.");

    const std::uintptr_t page_address = reinterpret_cast<std::uintptr_t>(allocation);
    const std::uintptr_t candidate_address = page_address + 64;
    ConfigureStateBlock(candidate_address, 4101);

    helen::CommandDispatcher dispatcher;
    dispatcher.RegisterConfigInt("ui.subtitleSize", 1);
    Expect(dispatcher.TrySetInt("ui.subtitleSize", 2), "Failed to seed the saved subtitle size config.");

    helen::RuntimeValueStore runtime_values;
    Expect(runtime_values.RegisterSlot(CreateSubtitleScaleSlot()), "Failed to register the subtitle scale runtime slot.");

    const std::filesystem::path unused_batman_ini_path = CreateTemporaryBatmanGraphicsIniPath();
    helen::BatmanGraphicsConfigService graphics_config_service(unused_batman_ini_path);
    helen::CommandExecutor executor(dispatcher, runtime_values, graphics_config_service);
    Expect(executor.RegisterCommand(CreateApplySubtitleSizeCommand("applySubtitleSize")), "Failed to register applySubtitleSize.");
    Expect(executor.RegisterCommand(CreateApplySavedSubtitleSizeCommand()), "Failed to register applySavedSubtitleSize.");

    helen::BuildRuntimeCoordinator coordinator(
        { "applySavedSubtitleSize" },
        { CreateSubtitleObserver(page_address, page_address + page_size) },
        dispatcher,
        executor);

    try
    {
        Expect(coordinator.Start(), "Build runtime coordinator failed to start.");

        std::optional<double> slot_value = runtime_values.TryGetDouble("subtitle.scale");
        Expect(slot_value.has_value(), "Startup command removed the subtitle scale slot.");
        ExpectNear(*slot_value, 8.0, 0.001, "Startup commands did not apply the saved subtitle scale.");

        ConfigureStateBlock(candidate_address, 4101);
        Expect(coordinator.PollStateObserversOnce(), "Observer poll for the initial subtitle state failed.");
        const std::optional<int> saved_small = dispatcher.TryGetInt("ui.subtitleSize");
        Expect(saved_small.has_value() && *saved_small == 0, "Observer did not mirror the small subtitle selection into config.");

        slot_value = runtime_values.TryGetDouble("subtitle.scale");
        Expect(slot_value.has_value(), "Observer update removed the subtitle scale slot.");
        ExpectNear(*slot_value, 2.0, 0.001, "Observer update did not apply the small subtitle scale.");

        ConfigureStateBlock(candidate_address, 4103);
        Expect(coordinator.PollStateObserversOnce(), "Observer poll for the large subtitle state failed.");
        const std::optional<int> saved_large = dispatcher.TryGetInt("ui.subtitleSize");
        Expect(saved_large.has_value() && *saved_large == 2, "Observer did not mirror the large subtitle selection into config.");

        slot_value = runtime_values.TryGetDouble("subtitle.scale");
        Expect(slot_value.has_value(), "Large observer update removed the subtitle scale slot.");
        ExpectNear(*slot_value, 8.0, 0.001, "Observer update did not apply the large subtitle scale.");
    }
    catch (...)
    {
        VirtualFree(allocation, 0, MEM_RELEASE);
        throw;
    }

    VirtualFree(allocation, 0, MEM_RELEASE);
}
