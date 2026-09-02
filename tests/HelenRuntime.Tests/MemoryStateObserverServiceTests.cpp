#include <HelenHook/MemoryStateObserverCheckDefinition.h>
#include <HelenHook/MemoryStateObserverDefinition.h>
#include <HelenHook/MemoryStateObserverMapEntryDefinition.h>
#include <HelenHook/MemoryStateObserverService.h>

#include <Windows.h>

#include <cstdint>
#include <cstring>
#include <initializer_list>
#include <optional>
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
     * @brief Reads one signed 32-bit integer from test-owned process memory.
     * @param address Readable address whose integer value should be copied.
     * @return Integer currently stored at the supplied address.
     */
    int ReadInt32(std::uintptr_t address)
    {
        int value = 0;
        std::memcpy(&value, reinterpret_cast<const void*>(address), sizeof(value));
        return value;
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

    /**
     * @brief Writes the complete structured Batman graphics carrier around a candidate base address.
     * @param base_address Candidate observer base address whose surrounding words should be populated.
     * @param raw_value Raw control code written at the carrier value offset.
     */
    void ConfigureGraphicsCarrierStateBlock(std::uintptr_t base_address, int raw_value)
    {
        WriteInt32(base_address - 16, 50);
        WriteInt32(base_address - 12, 100);
        WriteInt32(base_address - 8, 100);
        WriteInt32(base_address - 4, 100);
        WriteInt32(base_address, 4102);
        WriteInt32(base_address + 4, 1);
        WriteInt32(base_address + 8, 0);
        WriteInt32(base_address + 12, raw_value);
        WriteInt32(base_address + 16, 4102);
        WriteInt32(base_address + 20, 2);
        WriteInt32(base_address + 28, 3);
        WriteInt32(base_address + 32, 3);
    }

    /**
     * @brief Describes one structural check used to construct a graphics carrier observer test definition.
     */
    struct GraphicsCarrierCheck
    {
        /** @brief Comparison mode expected by the runtime observer. */
        const char* Comparison;
        /** @brief Signed byte offset relative to the candidate carrier base. */
        int Offset;
        /** @brief Optional constant required by an equals-constant check. */
        std::optional<int> ExpectedValue;
        /** @brief Optional comparison offset required by an equals-value-at-offset check. */
        std::optional<int> CompareOffset;
    };

    /**
     * @brief Builds one graphics carrier observer with a caller-selected disjoint mapping table.
     * @param id Stable observer identifier used by emitted updates.
     * @param scan_start Inclusive scan start address.
     * @param scan_end Exclusive scan end address.
     * @param target_config_key Helen config key associated with mapped values.
     * @param mappings Raw values that this observer alone recognizes.
     * @param command Optional command emitted with mapped updates.
     * @return Fully populated observer definition sharing the graphics carrier structure.
     */
    helen::MemoryStateObserverDefinition CreateGraphicsCarrierObserverDefinition(
        const char* id,
        std::uintptr_t scan_start,
        std::uintptr_t scan_end,
        const char* target_config_key,
        std::initializer_list<int> mappings,
        const char* command = nullptr)
    {
        helen::MemoryStateObserverDefinition definition;
        definition.Id = id;
        definition.ScanStartAddress = scan_start;
        definition.ScanEndAddress = scan_end;
        definition.ScanStride = 4;
        definition.ValueOffset = 12;
        definition.PollIntervalMs = 1;
        definition.TargetConfigKey = target_config_key;
        definition.AddressMatchValues = { 4101, 4102, 4103, 4104, 4105, 4106, 4200, 4210, 4211, 4990, 4991 };
        if (command != nullptr)
        {
            definition.CommandId = command;
        }

        const GraphicsCarrierCheck checks[] = {
            { "equals-constant", -16, 50, std::nullopt },
            { "equals-constant", -12, 100, std::nullopt },
            { "equals-constant", -8, 100, std::nullopt },
            { "equals-constant", -4, 100, std::nullopt },
            { "equals-constant", 4, 1, std::nullopt },
            { "equals-constant", 8, 0, std::nullopt },
            { "equals-constant", 0, 4102, std::nullopt },
            { "equals-constant", 16, 4102, std::nullopt },
            { "equals-constant", 20, 2, std::nullopt },
            { "equals-constant", 28, 3, std::nullopt },
            { "equals-constant", 32, 3, std::nullopt }
        };
        for (const GraphicsCarrierCheck& check : checks)
        {
            helen::MemoryStateObserverCheckDefinition check_definition;
            check_definition.Comparison = check.Comparison;
            check_definition.Offset = check.Offset;
            check_definition.ExpectedValue = check.ExpectedValue;
            check_definition.CompareOffset = check.CompareOffset;
            definition.Checks.push_back(check_definition);
        }

        int mapped_value = 0;
        for (const int raw_value : mappings)
        {
            helen::MemoryStateObserverMapEntryDefinition mapping;
            mapping.Match = raw_value;
            mapping.Value = mapped_value++;
            definition.Mappings.push_back(mapping);
        }

        if (definition.Id == "graphicsObserverVsync")
        {
            definition.ResponseRequestValue = 4200;

            helen::MemoryStateObserverMapEntryDefinition disabled_response;
            disabled_response.Match = 0;
            disabled_response.Value = 4210;
            definition.ResponseMappings.push_back(disabled_response);

            helen::MemoryStateObserverMapEntryDefinition enabled_response;
            enabled_response.Match = 1;
            enabled_response.Value = 4211;
            definition.ResponseMappings.push_back(enabled_response);
        }

        return definition;
    }

    /**
     * @brief Verifies disjoint observer mappings share one cached structural carrier without repeated rescans.
     * @remarks The initial subtitle-like raw code is intentionally unmapped by either observer, exercising the combined subtitle/graphics no-collision case.
     */
    void RunGraphicsCarrierObserverCoexistenceTest()
    {
        SYSTEM_INFO system_info{};
        GetSystemInfo(&system_info);
        const std::size_t page_size = system_info.dwPageSize;
        void* const allocation = VirtualAlloc(nullptr, page_size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
        Expect(allocation != nullptr, "Failed to allocate writable memory for the graphics carrier observer test.");

        const std::uintptr_t page_address = reinterpret_cast<std::uintptr_t>(allocation);
        const std::uintptr_t decoy_address = page_address + 64;
        const std::uintptr_t candidate_address = page_address + 128;
        ConfigureGraphicsCarrierStateBlock(decoy_address, 4000);
        ConfigureGraphicsCarrierStateBlock(candidate_address, 4101);

        std::vector<helen::MemoryStateObserverUpdate> updates;
        helen::MemoryStateObserverService service(
            {
                CreateGraphicsCarrierObserverDefinition(
                    "graphicsObserverVsync",
                    page_address,
                    page_address + page_size,
                    "vsync",
                    { 4210, 4211 }),
                CreateGraphicsCarrierObserverDefinition(
                    "graphicsObserverApplySignal",
                    page_address,
                    page_address + page_size,
                    "applySignal",
                    { 4990, 4991 },
                    "applyBatmanGraphicsDraft")
            },
            [&updates](const helen::MemoryStateObserverUpdate& update)
            {
                updates.push_back(update);
            },
            [](const std::string& config_key) -> std::optional<int>
            {
                if (config_key == "vsync")
                {
                    return 1;
                }

                return std::nullopt;
            });

        try
        {
            Expect(service.PollOnce(), "Expected the initial graphics carrier poll to succeed.");
            Expect(updates.empty(), "Unmapped subtitle-like graphics carrier code emitted an update.");

            std::vector<helen::MemoryStateObserverDebugView> debug_views = service.GetDebugViews();
            Expect(debug_views.size() == 2, "Graphics carrier observer debug view count mismatch.");
            for (const helen::MemoryStateObserverDebugView& debug_view : debug_views)
            {
                Expect(debug_view.CachedAddress == candidate_address, "Graphics carrier observer cached a structurally matching decoy instead of the recognized carrier.");
                Expect(debug_view.CachedAddress != decoy_address, "Graphics carrier observer retained the decoy address.");
                Expect(debug_view.RescanCount == 1, "Graphics carrier observer initial rescan count mismatch.");
            }

            Expect(service.PollOnce(), "Expected the unchanged unmapped graphics carrier poll to succeed.");
            debug_views = service.GetDebugViews();
            for (const helen::MemoryStateObserverDebugView& debug_view : debug_views)
            {
                Expect(debug_view.CachedAddress == candidate_address, "Graphics carrier observer lost its cached address for an unmapped raw code.");
                Expect(debug_view.RescanCount == 1, "Graphics carrier observer rescanned despite an unchanged valid structure.");
            }

            ConfigureGraphicsCarrierStateBlock(candidate_address, 4200);
            Expect(service.PollOnce(), "Expected the VSync initialization request poll to succeed.");
            Expect(ReadInt32(candidate_address + 12) == 4211, "VSync initialization request did not receive the mapped live-config response.");
            Expect(updates.empty(), "VSync initialization request unexpectedly emitted a user-driven config update.");

            Expect(service.PollOnce(), "Expected the VSync initialization response poll to succeed.");
            Expect(updates.size() == 1, "VSync initialization response did not emit exactly one synchronized update.");
            Expect(updates[0].ObserverId == "graphicsObserverVsync", "VSync initialization response observer mismatch.");
            Expect(updates[0].RawValue == 4211 && updates[0].MappedValue == 1, "VSync initialization response values mismatch.");

            ConfigureGraphicsCarrierStateBlock(candidate_address, 4210);
            Expect(service.PollOnce(), "Expected the VSync graphics carrier poll to succeed.");
            Expect(updates.size() == 2, "VSync graphics carrier poll did not emit exactly one additional update.");
            Expect(updates[1].ObserverId == "graphicsObserverVsync", "VSync graphics carrier update observer mismatch.");
            Expect(updates[1].ConfigKey == "vsync", "VSync graphics carrier update config key mismatch.");
            Expect(updates[1].RawValue == 4210 && updates[1].MappedValue == 0, "VSync graphics carrier update values mismatch.");
            Expect(!updates[1].CommandId.has_value(), "VSync graphics carrier update unexpectedly carried a command.");

            ConfigureGraphicsCarrierStateBlock(candidate_address, 4990);
            Expect(service.PollOnce(), "Expected the apply-signal graphics carrier poll to succeed.");
            Expect(updates.size() == 3, "Apply-signal graphics carrier poll did not add exactly one update.");
            Expect(updates[2].ObserverId == "graphicsObserverApplySignal", "Apply-signal graphics carrier update observer mismatch.");
            Expect(updates[2].ConfigKey == "applySignal", "Apply-signal graphics carrier update config key mismatch.");
            Expect(updates[2].RawValue == 4990 && updates[2].MappedValue == 0, "Apply-signal graphics carrier update values mismatch.");
            Expect(updates[2].CommandId.has_value() && *updates[2].CommandId == "applyBatmanGraphicsDraft", "Apply-signal graphics carrier command mismatch.");

            debug_views = service.GetDebugViews();
            for (const helen::MemoryStateObserverDebugView& debug_view : debug_views)
            {
                Expect(debug_view.CachedAddress == candidate_address, "Graphics carrier observer cache changed after a mapped transition.");
                Expect(debug_view.RescanCount == 1, "Graphics carrier observer rescanned after a mapped transition.");
            }

            Expect(VirtualFree(allocation, 0, MEM_RELEASE) != FALSE, "Failed to release the graphics carrier observer allocation.");
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
}

/**
 * @brief Verifies that bounded memory observers find matching state blocks, suppress duplicate emissions, and invalidate stale cached addresses safely.
 */
void RunMemoryStateObserverServiceTests()
{
    RunGraphicsCarrierObserverCoexistenceTest();

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
