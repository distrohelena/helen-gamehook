#include <HelenHook/MemoryStateObserverCheckDefinition.h>
#include <HelenHook/MemoryStateObserverDefinition.h>
#include <HelenHook/MemoryStateObserverMapEntryDefinition.h>
#include <HelenHook/MemoryStateObserverService.h>

#include <Windows.h>

#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <vector>

namespace
{
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
     * @brief Writes one signed 32-bit integer into writable process memory for observer test setup.
     * @param address Writable address that should receive the integer value.
     * @param value Integer value written to the supplied address.
     */
    void WriteInt32(std::uintptr_t address, int value)
    {
        std::memcpy(reinterpret_cast<void*>(address), &value, sizeof(value));
    }

    /**
     * @brief Configures one candidate Batman-style subtitle state block around the supplied base address.
     * @param base_address Candidate observer base address that should satisfy the configured checks.
     * @param raw_value Raw subtitle-size state value written at the observer value offset.
     */
    void ConfigureStateBlock(std::uintptr_t base_address, int raw_value)
    {
        WriteInt32(base_address - 16, 50);
        WriteInt32(base_address, raw_value);
        WriteInt32(base_address + 16, raw_value);
    }

    /**
     * @brief Builds one bounded observer definition that mirrors a Batman-style subtitle state block.
     * @param scan_start Inclusive scan start address.
     * @param scan_end Exclusive scan end address.
     * @return Fully populated observer definition that maps raw subtitle values into Helen config.
     */
    helen::MemoryStateObserverDefinition CreateObserverDefinition(std::uintptr_t scan_start, std::uintptr_t scan_end)
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

        helen::MemoryStateObserverCheckDefinition constant_check;
        constant_check.Comparison = "equals-constant";
        constant_check.Offset = -16;
        constant_check.ExpectedValue = 50;
        definition.Checks.push_back(constant_check);

        helen::MemoryStateObserverCheckDefinition mirror_check;
        mirror_check.Comparison = "equals-value-at-offset";
        mirror_check.Offset = 16;
        mirror_check.CompareOffset = 0;
        definition.Checks.push_back(mirror_check);

        helen::MemoryStateObserverMapEntryDefinition small_mapping;
        small_mapping.Match = 4101;
        small_mapping.Value = 0;
        definition.Mappings.push_back(small_mapping);

        helen::MemoryStateObserverMapEntryDefinition large_mapping;
        large_mapping.Match = 4103;
        large_mapping.Value = 2;
        definition.Mappings.push_back(large_mapping);

        return definition;
    }
}

/**
 * @brief Verifies that bounded memory observers find matching state blocks, suppress duplicate emissions, and invalidate stale cached addresses safely.
 */
void RunMemoryStateObserverServiceTests()
{
    SYSTEM_INFO system_info{};
    GetSystemInfo(&system_info);
    const std::size_t page_size = system_info.dwPageSize;

    void* const allocation = VirtualAlloc(nullptr, page_size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    Expect(allocation != nullptr, "Failed to allocate writable memory for the observer tests.");

    const std::uintptr_t page_address = reinterpret_cast<std::uintptr_t>(allocation);
    const std::uintptr_t candidate_address = page_address + 64;
    ConfigureStateBlock(candidate_address, 4101);

    std::vector<helen::MemoryStateObserverUpdate> updates;
    helen::MemoryStateObserverService service(
        { CreateObserverDefinition(page_address, page_address + page_size) },
        [&updates](const helen::MemoryStateObserverUpdate& update)
        {
            updates.push_back(update);
        });

    try
    {
        Expect(service.PollOnce(), "Expected the first observer poll to succeed.");
        Expect(updates.size() == 1, "Expected the first observer poll to emit one update.");
        Expect(updates[0].ObserverId == "subtitleUiStateObserver", "Observer identifier mismatch.");
        Expect(updates[0].ConfigKey == "ui.subtitleSize", "Observer target config key mismatch.");
        Expect(updates[0].RawValue == 4101, "Observer raw value mismatch.");
        Expect(updates[0].MappedValue == 0, "Observer mapped value mismatch.");
        Expect(updates[0].CommandId.has_value() && *updates[0].CommandId == "applySubtitleSize", "Observer command mismatch.");

        std::vector<helen::MemoryStateObserverDebugView> debug_views = service.GetDebugViews();
        Expect(debug_views.size() == 1, "Observer debug view count mismatch after the first poll.");
        Expect(debug_views[0].CachedAddress == candidate_address, "Observer cached address mismatch after the first poll.");
        Expect(debug_views[0].UpdateCount == 1, "Observer update count mismatch after the first poll.");
        Expect(debug_views[0].LastRawValue.has_value() && *debug_views[0].LastRawValue == 4101, "Observer last raw value mismatch after the first poll.");
        Expect(debug_views[0].LastMappedValue.has_value() && *debug_views[0].LastMappedValue == 0, "Observer last mapped value mismatch after the first poll.");

        Expect(service.PollOnce(), "Expected the duplicate observer poll to succeed.");
        Expect(updates.size() == 1, "Duplicate observer poll unexpectedly emitted another update.");

        ConfigureStateBlock(candidate_address, 4103);
        Expect(service.PollOnce(), "Expected the changed observer poll to succeed.");
        Expect(updates.size() == 2, "Changed observer poll did not emit the second update.");
        Expect(updates[1].RawValue == 4103, "Changed observer raw value mismatch.");
        Expect(updates[1].MappedValue == 2, "Changed observer mapped value mismatch.");

        debug_views = service.GetDebugViews();
        Expect(debug_views[0].CachedAddress == candidate_address, "Observer cached address changed unexpectedly after the second update.");
        Expect(debug_views[0].UpdateCount == 2, "Observer update count mismatch after the second update.");
        Expect(debug_views[0].LastRawValue.has_value() && *debug_views[0].LastRawValue == 4103, "Observer last raw value mismatch after the second update.");
        Expect(debug_views[0].LastMappedValue.has_value() && *debug_views[0].LastMappedValue == 2, "Observer last mapped value mismatch after the second update.");

        Expect(VirtualFree(allocation, 0, MEM_RELEASE) != FALSE, "Failed to release the observer test allocation.");
        Expect(service.PollOnce(), "Observer poll after freeing the cached block unexpectedly failed.");

        debug_views = service.GetDebugViews();
        Expect(debug_views[0].CachedAddress == 0, "Observer cached address was not cleared after the backing memory was released.");
    }
    catch (...)
    {
        if (allocation != nullptr)
        {
            MEMORY_BASIC_INFORMATION memory_info{};
            if (VirtualQuery(allocation, &memory_info, sizeof(memory_info)) != 0 && memory_info.State == MEM_COMMIT)
            {
                VirtualFree(allocation, 0, MEM_RELEASE);
            }
        }

        throw;
    }
}
