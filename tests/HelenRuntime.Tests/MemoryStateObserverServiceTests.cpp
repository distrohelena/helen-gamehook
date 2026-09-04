#include <HelenHook/MemoryStateObserverCheckDefinition.h>
#include <HelenHook/MemoryStateObserverDefinition.h>
#include <HelenHook/MemoryStateObserverMapEntryDefinition.h>
#include <HelenHook/MemoryStateObserverService.h>

#include <Windows.h>

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstring>
#include <initializer_list>
#include <string>
#include <unordered_map>
#include <optional>
#include <stdexcept>
#include <thread>
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
     * @brief Waits for a test-owned carrier word to reach an expected response while a worker polls asynchronously.
     * @param address Carrier value address that should be observed.
     * @param expected_value Response value required before the deadline.
     * @param timeout Maximum duration to wait for the response.
     * @return True when the carrier reaches the expected value before the deadline; otherwise false.
     * @remarks The bounded sleep keeps worker tests deterministic without holding a callback or service mutex while polling memory.
     */
    bool WaitForCarrierValue(std::uintptr_t address, int expected_value, std::chrono::milliseconds timeout)
    {
        const std::chrono::steady_clock::time_point deadline = std::chrono::steady_clock::now() + timeout;
        while (ReadInt32(address) != expected_value)
        {
            if (std::chrono::steady_clock::now() >= deadline)
            {
                return false;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        return true;
    }

    /**
     * @brief Waits until one observer debug view reports the requested number of broad discovery scans.
     * @param service Observer service whose live debug views should be sampled while its worker runs.
     * @param observer_index Zero-based observer index whose rescan count is being awaited.
     * @param minimum_rescan_count Smallest rescan count that satisfies the wait.
     * @param timeout Maximum duration to wait before reporting that the worker did not reach the target.
     * @return True when the selected observer reaches the requested count before the deadline; otherwise false.
     * @remarks The loop samples service-owned state until a monotonic deadline and yields between samples, avoiding a fixed sleep that could race the worker's 1 ms polling interval.
     */
    bool WaitForObserverRescanCount(
        const helen::MemoryStateObserverService& service,
        std::size_t observer_index,
        std::uint64_t minimum_rescan_count,
        std::chrono::milliseconds timeout)
    {
        const std::chrono::steady_clock::time_point deadline = std::chrono::steady_clock::now() + timeout;
        while (std::chrono::steady_clock::now() < deadline)
        {
            const std::vector<helen::MemoryStateObserverDebugView> debug_views = service.GetDebugViews();
            if (observer_index < debug_views.size() && debug_views[observer_index].RescanCount >= minimum_rescan_count)
            {
                return true;
            }

            std::this_thread::yield();
        }

        const std::vector<helen::MemoryStateObserverDebugView> debug_views = service.GetDebugViews();
        return observer_index < debug_views.size() && debug_views[observer_index].RescanCount >= minimum_rescan_count;
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
     * @brief Describes the complete request, response, acknowledgement, and failure allocation for one grouped Batman setting.
     * @remarks The two-value response, write, and acknowledgement vectors are ordered as disabled then enabled, matching the normalized config values 0 and 1.
     */
    struct BatmanGroupedObserverProtocol
    {
        /** @brief Stable observer identifier used in emitted updates and diagnostics. */
        const char* Id;
        /** @brief Normalized Helen config key updated by the observer. */
        const char* TargetConfigKey;
        /** @brief Raw request value that asks HelenHook to return the current config value. */
        int ReadRequestValue;
        /** @brief Raw response values corresponding to disabled and enabled config values. */
        std::vector<int> ResponseValues;
        /** @brief Raw write request values corresponding to disabled and enabled config values. */
        std::vector<int> WriteValues;
        /** @brief Raw success acknowledgement values corresponding to disabled and enabled write requests. */
        std::vector<int> AcknowledgementValues;
        /** @brief Raw response written when the update callback rejects a write request. */
        int FailureResponseValue;
        /** @brief Optional command emitted after this observer changes its target config value. */
        const char* CommandId;
    };

    /**
     * @brief Builds the sorted raw-value union accepted by every member of the Batman frontend control group.
     * @return Complete sorted protocol union for VSync, MSAA, PhysX, Stereo, and all seven quality observers.
     * @remarks Keeping this list explicit makes omissions or accidental protocol reuse fail in the grouped fixture rather than being hidden by per-observer mappings.
     */
    std::vector<int> CreateCompleteBatmanGroupedProtocolUnion()
    {
        return {
            4200, 4210, 4211, 4220, 4221, 4230, 4231, 4299,
            4300, 4310, 4311, 4312, 4313, 4314, 4320, 4321, 4322, 4323, 4324, 4330, 4331, 4332, 4333, 4334, 4399,
            4400, 4410, 4411, 4412, 4420, 4421, 4422, 4430, 4431, 4432, 4499,
            4500, 4510, 4511, 4520, 4521, 4530, 4531, 4599,
            4600, 4601, 4602, 4603, 4604, 4605, 4606, 4609,
            4610, 4611, 4612, 4613, 4614, 4615, 4616, 4619,
            4620, 4621, 4622, 4623, 4624, 4625, 4626, 4629,
            4630, 4631, 4632, 4633, 4634, 4635, 4636, 4639,
            4640, 4641, 4642, 4643, 4644, 4645, 4646, 4649,
            4650, 4651, 4652, 4653, 4654, 4655, 4656, 4659,
            4660, 4661, 4662, 4663, 4664, 4665, 4666, 4669
        };
    }

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
     * @brief Creates one full-protocol observer definition for the shared Batman frontend control carrier.
     * @param protocol Request, response, write, acknowledgement, failure, and command values for the setting.
     * @param scan_start Inclusive address at which the bounded carrier search begins.
     * @param scan_end Exclusive address at which the bounded carrier search ends.
     * @param protocol_union Sorted raw values accepted as structurally valid carrier contents by every group member.
     * @return Observer definition with grouped discovery and bidirectional transactional protocol behavior configured.
     */
    helen::MemoryStateObserverDefinition CreateFullBatmanGroupedObserverDefinition(
        const BatmanGroupedObserverProtocol& protocol,
        std::uintptr_t scan_start,
        std::uintptr_t scan_end,
        const std::vector<int>& protocol_union)
    {
        helen::MemoryStateObserverDefinition definition = CreateGraphicsCarrierObserverDefinition(
            protocol.Id,
            scan_start,
            scan_end,
            protocol.TargetConfigKey,
            {});
        definition.AddressGroup = "batmanFrontendControlType";
        definition.AddressMatchValues = protocol_union;
        definition.Mappings.clear();
        definition.ResponseMappings.clear();
        definition.AcknowledgementMappings.clear();
        definition.FailureResponseValue = protocol.FailureResponseValue;
        definition.ResponseRequestValue = protocol.ReadRequestValue;
        definition.CommandId.reset();
        if (protocol.CommandId != nullptr)
        {
            definition.CommandId = protocol.CommandId;
        }

        for (std::size_t value_index = 0; value_index < protocol.WriteValues.size(); ++value_index)
        {
            definition.Mappings.push_back(
                helen::MemoryStateObserverMapEntryDefinition{
                    .Match = protocol.WriteValues[value_index],
                    .Value = static_cast<int>(value_index) });
            definition.AcknowledgementMappings.push_back(
                helen::MemoryStateObserverMapEntryDefinition{
                    .Match = protocol.WriteValues[value_index],
                    .Value = protocol.AcknowledgementValues[value_index] });
        }

        for (std::size_t value_index = 0; value_index < protocol.ResponseValues.size(); ++value_index)
        {
            definition.ResponseMappings.push_back(
                helen::MemoryStateObserverMapEntryDefinition{
                    .Match = static_cast<int>(value_index),
                    .Value = protocol.ResponseValues[value_index] });
        }

        return definition;
    }

    /**
     * @brief Builds a transactional MSAA observer that maps request 4324 to config value 5 and acknowledges native work.
     * @param scan_start Inclusive scan start address.
     * @param scan_end Exclusive scan end address.
     * @return Observer definition with success, failure, and read-response carrier protocols enabled.
     */
    helen::MemoryStateObserverDefinition CreateTransactionalMsaaObserverDefinition(
        std::uintptr_t scan_start,
        std::uintptr_t scan_end)
    {
        helen::MemoryStateObserverDefinition definition = CreateGraphicsCarrierObserverDefinition(
            "graphicsObserverMsaa",
            scan_start,
            scan_end,
            "msaa",
            { 4324 });
        definition.Mappings[0].Value = 5;
        definition.AddressMatchValues.push_back(4300);
        definition.AddressMatchValues.push_back(4324);
        definition.AddressMatchValues.push_back(4334);
        definition.AddressMatchValues.push_back(4399);
        definition.AddressMatchValues.push_back(4970);
        definition.AddressMatchValues.push_back(4960);
        definition.AddressMatchValues.push_back(4969);
        definition.AcknowledgementMappings.push_back(
            helen::MemoryStateObserverMapEntryDefinition{ .Match = 4324, .Value = 4334 });
        definition.FailureResponseValue = 4399;
        definition.ResponseRequestValue = 4300;
        definition.ResponseMappings.push_back(
            helen::MemoryStateObserverMapEntryDefinition{ .Match = 5, .Value = 4314 });
        return definition;
    }

    /**
     * @brief Builds a grouped transactional rollback observer that consumes a rollback request on the shared graphics carrier.
     * @param scan_start Inclusive scan start address.
     * @param scan_end Exclusive scan end address.
     * @return Observer definition mapping raw request 4970 to a successful acknowledgement 4960.
     */
    helen::MemoryStateObserverDefinition CreateRollbackObserverDefinition(
        std::uintptr_t scan_start,
        std::uintptr_t scan_end)
    {
        helen::MemoryStateObserverDefinition definition = CreateGraphicsCarrierObserverDefinition(
            "graphicsObserverRollback",
            scan_start,
            scan_end,
            "rollbackSignal",
            { 4970 });
        definition.AddressGroup = "batmanFrontendControlType";
        definition.AddressMatchValues.push_back(4324);
        definition.AddressMatchValues.push_back(4399);
        definition.AddressMatchValues.push_back(4970);
        definition.AddressMatchValues.push_back(4960);
        definition.AddressMatchValues.push_back(4969);
        definition.AcknowledgementMappings.push_back(
            helen::MemoryStateObserverMapEntryDefinition{ .Match = 4970, .Value = 4960 });
        definition.FailureResponseValue = 4969;
        return definition;
    }

    /**
     * @brief Verifies a successful transactional MSAA update observes the request before acknowledging it and can retry identically.
     */
    void RunTransactionalObserverAcknowledgementTest()
    {
        SYSTEM_INFO system_info{};
        GetSystemInfo(&system_info);
        const std::size_t page_size = system_info.dwPageSize;
        void* const allocation = VirtualAlloc(nullptr, page_size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
        Expect(allocation != nullptr, "Failed to allocate writable memory for the transactional acknowledgement test.");

        const std::uintptr_t page_address = reinterpret_cast<std::uintptr_t>(allocation);
        const std::uintptr_t candidate_address = page_address + 128;
        ConfigureGraphicsCarrierStateBlock(candidate_address, 4324);

        std::vector<int> callback_carrier_values;
        std::vector<int> callback_raw_values;
        helen::MemoryStateObserverService service(
            { CreateTransactionalMsaaObserverDefinition(page_address, page_address + page_size) },
            [&callback_carrier_values, &callback_raw_values, candidate_address](const helen::MemoryStateObserverUpdate& update)
            {
                callback_carrier_values.push_back(ReadInt32(candidate_address + 12));
                callback_raw_values.push_back(update.RawValue);
                return true;
            });

        try
        {
            Expect(service.PollOnce(), "Successful transactional MSAA update unexpectedly failed.");
            Expect(ReadInt32(candidate_address + 12) == 4334, "Successful transactional MSAA update did not write the success acknowledgement.");
            Expect(callback_carrier_values.size() == 1 && callback_carrier_values[0] == 4324, "Transactional callback did not observe the request carrier before acknowledgement.");
            Expect(callback_raw_values.size() == 1 && callback_raw_values[0] == 4324, "Transactional callback received an acknowledged response instead of the raw request.");

            ConfigureGraphicsCarrierStateBlock(candidate_address, 4324);
            Expect(service.PollOnce(), "Identical transactional MSAA retry unexpectedly failed.");
            Expect(ReadInt32(candidate_address + 12) == 4334, "Identical transactional MSAA retry did not write a second success acknowledgement.");
            Expect(callback_carrier_values.size() == 2, "Identical transactional MSAA request was suppressed instead of retried.");
            Expect(callback_raw_values.size() == 2 && callback_raw_values[1] == 4324, "Transactional retry callback received an acknowledged response instead of the raw request.");

            const std::vector<helen::MemoryStateObserverDebugView> debug_views = service.GetDebugViews();
            Expect(debug_views.size() == 1, "Transactional MSAA debug view count mismatch.");
            Expect(debug_views[0].UpdateCount == 2, "Transactional MSAA update count did not increment once per emitted request.");
            Expect(debug_views[0].LastRawValue.has_value() && *debug_views[0].LastRawValue == 4334, "Transactional MSAA debug raw value did not retain the successful acknowledgement.");
            Expect(debug_views[0].LastMappedValue.has_value() && *debug_views[0].LastMappedValue == 5, "Transactional MSAA debug mapped value was not retained for diagnostics.");
            Expect(VirtualFree(allocation, 0, MEM_RELEASE) != FALSE, "Failed to release the transactional acknowledgement allocation.");
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

    /**
     * @brief Verifies a failed transactional callback writes the declared failure response while the poll operation remains healthy.
     */
    void RunTransactionalObserverFailureAcknowledgementTest()
    {
        SYSTEM_INFO system_info{};
        GetSystemInfo(&system_info);
        const std::size_t page_size = system_info.dwPageSize;
        void* const allocation = VirtualAlloc(nullptr, page_size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
        Expect(allocation != nullptr, "Failed to allocate writable memory for the transactional failure acknowledgement test.");

        const std::uintptr_t page_address = reinterpret_cast<std::uintptr_t>(allocation);
        const std::uintptr_t candidate_address = page_address + 128;
        ConfigureGraphicsCarrierStateBlock(candidate_address, 4324);

        int callback_count = 0;
        int callback_carrier_value = 0;
        int callback_raw_value = 0;
        bool callback_values_valid = true;
        helen::MemoryStateObserverService service(
            { CreateTransactionalMsaaObserverDefinition(page_address, page_address + page_size) },
            [&callback_count, &callback_carrier_value, &callback_raw_value, &callback_values_valid, candidate_address](const helen::MemoryStateObserverUpdate& update)
            {
                ++callback_count;
                callback_carrier_value = ReadInt32(candidate_address + 12);
                callback_raw_value = update.RawValue;
                callback_values_valid = callback_values_valid && callback_carrier_value == 4324 && callback_raw_value == 4324;
                return false;
            });

        try
        {
            Expect(service.PollOnce(), "Handled transactional callback failure unexpectedly failed the poll operation.");
            Expect(callback_carrier_value == 4324, "Failed transactional callback carrier capture changed before failure acknowledgement processing completed.");
            Expect(callback_raw_value == 4324 && callback_values_valid, "Failed transactional callback did not observe the raw request before failure acknowledgement.");
            Expect(ReadInt32(candidate_address + 12) == 4399, "Failed transactional callback did not write the declared failure response.");
            Expect(callback_count == 1, "Failed transactional callback invocation count mismatch.");

            ConfigureGraphicsCarrierStateBlock(candidate_address, 4324);
            Expect(service.PollOnce(), "Repeated handled transactional callback failure unexpectedly failed the poll operation.");
            Expect(ReadInt32(candidate_address + 12) == 4399, "Repeated failed transactional callback did not rearm the failure response.");
            Expect(callback_count == 2, "Failed transactional request was suppressed instead of rearmed after failure acknowledgement.");
            Expect(callback_values_valid, "Repeated failed transactional callback did not observe the raw request before failure acknowledgement.");

            Expect(VirtualFree(allocation, 0, MEM_RELEASE) != FALSE, "Failed to release the transactional failure acknowledgement allocation.");
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

    /**
     * @brief Verifies a transactional read request answers from config without emitting an observer update.
     */
    void RunTransactionalObserverReadResponseTest()
    {
        SYSTEM_INFO system_info{};
        GetSystemInfo(&system_info);
        const std::size_t page_size = system_info.dwPageSize;
        void* const allocation = VirtualAlloc(nullptr, page_size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
        Expect(allocation != nullptr, "Failed to allocate writable memory for the transactional read-response test.");

        const std::uintptr_t page_address = reinterpret_cast<std::uintptr_t>(allocation);
        const std::uintptr_t candidate_address = page_address + 128;
        ConfigureGraphicsCarrierStateBlock(candidate_address, 4300);

        std::vector<helen::MemoryStateObserverUpdate> updates;
        helen::MemoryStateObserverService service(
            { CreateTransactionalMsaaObserverDefinition(page_address, page_address + page_size) },
            [&updates](const helen::MemoryStateObserverUpdate& update)
            {
                updates.push_back(update);
                return true;
            },
            [](const std::string& config_key) -> std::optional<int>
            {
                Expect(config_key == "msaa", "Transactional read response queried the wrong config key.");
                return 5;
            });

        try
        {
            Expect(service.PollOnce(), "Transactional read-response poll unexpectedly failed.");
            Expect(ReadInt32(candidate_address + 12) == 4314, "Transactional read request did not write its mapped config response.");
            Expect(updates.empty(), "Transactional read request emitted an observer update.");
            Expect(VirtualFree(allocation, 0, MEM_RELEASE) != FALSE, "Failed to release the transactional read-response allocation.");
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

    /**
     * @brief Verifies a transactional config-read callback exception writes the declared failure response and emits no update.
     * @remarks The exception is raised synchronously by the read-response callback, so PollOnce must handle it without exposing the exception to its caller.
     */
    void RunTransactionalObserverConfigCallbackExceptionTest()
    {
        SYSTEM_INFO system_info{};
        GetSystemInfo(&system_info);
        const std::size_t page_size = system_info.dwPageSize;
        void* const allocation = VirtualAlloc(nullptr, page_size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
        Expect(allocation != nullptr, "Failed to allocate writable memory for the transactional config callback-exception test.");

        const std::uintptr_t page_address = reinterpret_cast<std::uintptr_t>(allocation);
        const std::uintptr_t candidate_address = page_address + 128;
        ConfigureGraphicsCarrierStateBlock(candidate_address, 4300);
        std::vector<helen::MemoryStateObserverUpdate> updates;
        int config_callback_count = 0;
        helen::MemoryStateObserverService service(
            { CreateTransactionalMsaaObserverDefinition(page_address, page_address + page_size) },
            [&updates](const helen::MemoryStateObserverUpdate& update)
            {
                updates.push_back(update);
                return true;
            },
            [&config_callback_count](const std::string&) -> std::optional<int>
            {
                ++config_callback_count;
                if (config_callback_count == 1)
                {
                    throw std::runtime_error("intentional config callback failure");
                }

                throw 7;
            });

        try
        {
            Expect(service.PollOnce(), "Transactional config callback exception was not handled as a failure response.");
            Expect(ReadInt32(candidate_address + 12) == 4399, "Transactional config callback exception did not write the exact failure response.");
            Expect(updates.empty(), "Transactional config callback exception emitted an observer update.");
            ConfigureGraphicsCarrierStateBlock(candidate_address, 4300);
            Expect(service.PollOnce(), "Unknown transactional config callback exception was not handled as a failure response.");
            Expect(ReadInt32(candidate_address + 12) == 4399, "Unknown transactional config callback exception did not write the exact failure response.");
            Expect(config_callback_count == 2, "Transactional config callback exception test did not invoke both exception paths.");
            const std::vector<helen::MemoryStateObserverDebugView> debug_views = service.GetDebugViews();
            Expect(debug_views.size() == 1, "Transactional config callback-exception debug view count mismatch.");
            Expect(debug_views[0].LastRawValue.has_value() && *debug_views[0].LastRawValue == 4399, "Transactional config callback exception did not retain its written failure response.");
            Expect(VirtualFree(allocation, 0, MEM_RELEASE) != FALSE, "Failed to release the transactional config callback-exception allocation.");
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

    /**
     * @brief Verifies a pending request is cleared when a different read request is observed before its config callback fails.
     * @remarks Carrier A's failed acknowledgement remains pending, read request B clears that stale pair before its callback, and returning to A emits the request again.
     */
    void RunTransactionalObserverReadResponsePendingReconciliationTest()
    {
        SYSTEM_INFO system_info{};
        GetSystemInfo(&system_info);
        const std::size_t page_size = system_info.dwPageSize;
        void* const allocation = VirtualAlloc(nullptr, page_size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
        Expect(allocation != nullptr, "Failed to allocate writable memory for the transactional pending reconciliation test.");

        const std::uintptr_t page_address = reinterpret_cast<std::uintptr_t>(allocation);
        const std::uintptr_t candidate_address = page_address + 128;
        ConfigureGraphicsCarrierStateBlock(candidate_address, 4324);
        bool protection_changed = false;
        int callback_count = 0;
        bool callback_values_valid = true;
        DWORD original_protection = 0;
        helen::MemoryStateObserverService service(
            { CreateTransactionalMsaaObserverDefinition(page_address, page_address + page_size) },
            [&protection_changed, &callback_count, &callback_values_valid, &original_protection, page_address, page_size](const helen::MemoryStateObserverUpdate& update)
            {
                callback_values_valid = callback_values_valid && update.RawValue == 4324;
                ++callback_count;
                if (callback_count == 1)
                {
                    protection_changed = VirtualProtect(
                        reinterpret_cast<void*>(page_address),
                        page_size,
                        PAGE_READONLY,
                        &original_protection) != FALSE;
                }

                return true;
            },
            [](const std::string&) -> std::optional<int>
            {
                return std::nullopt;
            });

        try
        {
            Expect(!service.PollOnce(), "Transactional pending reconciliation setup unexpectedly wrote an acknowledgement.");
            Expect(protection_changed, "Transactional pending reconciliation callback did not make carrier A read-only.");
            DWORD restored_protection = 0;
            Expect(
                VirtualProtect(reinterpret_cast<void*>(page_address), page_size, original_protection, &restored_protection) != FALSE,
                "Failed to restore writable protection before transactional pending reconciliation.");
            protection_changed = false;

            ConfigureGraphicsCarrierStateBlock(candidate_address, 4300);
            Expect(!service.PollOnce(), "Transactional pending reconciliation read request unexpectedly succeeded without a config value.");
            Expect(callback_count == 1, "Transactional pending reconciliation read request emitted an update unexpectedly.");

            ConfigureGraphicsCarrierStateBlock(candidate_address, 4324);
            Expect(service.PollOnce(), "Transactional pending reconciliation did not re-emit request A after read request B.");
            Expect(callback_count == 2, "Transactional pending reconciliation retained stale request A suppression after read request B.");
            Expect(callback_values_valid, "Transactional pending reconciliation callback received the wrong request value.");
            Expect(ReadInt32(candidate_address + 12) == 4334, "Transactional pending reconciliation did not write the exact success acknowledgement for request A.");
            Expect(VirtualFree(allocation, 0, MEM_RELEASE) != FALSE, "Failed to release the transactional pending reconciliation allocation.");
        }
        catch (...)
        {
            if (protection_changed)
            {
                DWORD restored_protection = 0;
                VirtualProtect(reinterpret_cast<void*>(page_address), page_size, original_protection, &restored_protection);
            }

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

    /**
     * @brief Verifies a worker survives a throwing transactional config-read callback and processes later setting and rollback requests.
     * @remarks The initial read request fails with 4399; subsequent setting and rollback requests prove the worker remained alive after the handled callback exception.
     */
    void RunTransactionalObserverWorkerConfigCallbackExceptionTest()
    {
        SYSTEM_INFO system_info{};
        GetSystemInfo(&system_info);
        const std::size_t page_size = system_info.dwPageSize;
        void* const allocation = VirtualAlloc(nullptr, page_size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
        Expect(allocation != nullptr, "Failed to allocate writable memory for the transactional config callback-exception worker test.");

        const std::uintptr_t page_address = reinterpret_cast<std::uintptr_t>(allocation);
        const std::uintptr_t candidate_address = page_address + 128;
        ConfigureGraphicsCarrierStateBlock(candidate_address, 4300);
        helen::MemoryStateObserverDefinition setting_definition = CreateTransactionalMsaaObserverDefinition(page_address, page_address + page_size);
        setting_definition.AddressGroup = "batmanFrontendControlType";
        helen::MemoryStateObserverDefinition rollback_definition = CreateRollbackObserverDefinition(page_address, page_address + page_size);
        int config_callback_count = 0;
        std::mutex callback_mutex;
        std::condition_variable callback_condition;
        int setting_callback_count = 0;
        int rollback_callback_count = 0;
        bool callback_values_valid = true;
        helen::MemoryStateObserverService service(
            { setting_definition, rollback_definition },
            [&callback_mutex, &callback_condition, &setting_callback_count, &rollback_callback_count, &callback_values_valid](const helen::MemoryStateObserverUpdate& update)
            {
                {
                    std::lock_guard<std::mutex> lock(callback_mutex);
                    if (update.ObserverId == "graphicsObserverMsaa")
                    {
                        ++setting_callback_count;
                        callback_values_valid = callback_values_valid && update.RawValue == 4324;
                        callback_condition.notify_all();
                        return false;
                    }

                    if (update.ObserverId == "graphicsObserverRollback")
                    {
                        ++rollback_callback_count;
                        callback_values_valid = callback_values_valid && update.RawValue == 4970;
                        callback_condition.notify_all();
                        return true;
                    }

                    callback_values_valid = false;
                }
                callback_condition.notify_all();
                return false;
            },
            [&callback_mutex, &config_callback_count](const std::string&) -> std::optional<int>
            {
                int callback_invocation_count = 0;
                {
                    std::lock_guard<std::mutex> lock(callback_mutex);
                    ++config_callback_count;
                    callback_invocation_count = config_callback_count;
                }
                if (callback_invocation_count == 1)
                {
                    throw std::runtime_error("intentional worker config callback failure");
                }

                return 5;
            });

        try
        {
            Expect(service.Start(), "Transactional config callback-exception worker could not start.");
            Expect(WaitForCarrierValue(candidate_address + 12, 4399, std::chrono::seconds(2)), "Transactional config callback-exception worker did not write the failure response.");
            WriteInt32(candidate_address + 12, 4324);
            {
                std::unique_lock<std::mutex> lock(callback_mutex);
                Expect(
                    callback_condition.wait_for(
                        lock,
                        std::chrono::seconds(2),
                        [&setting_callback_count]()
                        {
                            return setting_callback_count >= 1;
                        }),
                    "Transactional config callback-exception worker did not invoke the setting callback after the read failure.");
            }

            Expect(WaitForCarrierValue(candidate_address + 12, 4399, std::chrono::seconds(2)), "Transactional config callback-exception worker did not write the setting failure response.");
            WriteInt32(candidate_address + 12, 4970);
            {
                std::unique_lock<std::mutex> lock(callback_mutex);
                Expect(
                    callback_condition.wait_for(
                        lock,
                        std::chrono::seconds(2),
                        [&rollback_callback_count]()
                        {
                            return rollback_callback_count >= 1;
                        }),
                    "Transactional config callback-exception worker did not invoke the rollback callback.");
            }

            Expect(WaitForCarrierValue(candidate_address + 12, 4960, std::chrono::seconds(2)), "Transactional config callback-exception worker did not write the rollback success response.");
            {
                std::lock_guard<std::mutex> lock(callback_mutex);
                Expect(setting_callback_count == 1, "Transactional config callback-exception worker invoked the setting callback more than once.");
                Expect(rollback_callback_count == 1, "Transactional config callback-exception worker invoked the rollback callback more than once.");
                Expect(callback_values_valid, "Transactional config callback-exception worker received an incorrect request value.");
            }
            {
                std::lock_guard<std::mutex> lock(callback_mutex);
                Expect(config_callback_count == 1, "Transactional config callback-exception worker invoked config callback more than once for the read request.");
            }

            service.Stop();
            Expect(VirtualFree(allocation, 0, MEM_RELEASE) != FALSE, "Failed to release the transactional config callback-exception worker allocation.");
        }
        catch (...)
        {
            service.Stop();
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

    /**
     * @brief Verifies an acknowledgement write failure returns false without claiming a success response.
     */
    void RunTransactionalObserverWriteFailureTest()
    {
        SYSTEM_INFO system_info{};
        GetSystemInfo(&system_info);
        const std::size_t page_size = system_info.dwPageSize;
        void* const allocation = VirtualAlloc(nullptr, page_size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
        Expect(allocation != nullptr, "Failed to allocate writable memory for the transactional write-failure test.");

        const std::uintptr_t page_address = reinterpret_cast<std::uintptr_t>(allocation);
        const std::uintptr_t candidate_address = page_address + 128;
        ConfigureGraphicsCarrierStateBlock(candidate_address, 4324);
        bool protection_changed = false;
        int callback_count = 0;
        bool callback_values_valid = true;
        DWORD original_protection = 0;
        helen::MemoryStateObserverService service(
            { CreateTransactionalMsaaObserverDefinition(page_address, page_address + page_size) },
            [&protection_changed, &callback_count, &callback_values_valid, &original_protection, page_address, page_size](const helen::MemoryStateObserverUpdate& update)
            {
                callback_values_valid = callback_values_valid && update.RawValue == 4324;
                ++callback_count;
                if (callback_count == 1)
                {
                    protection_changed = VirtualProtect(
                        reinterpret_cast<void*>(page_address),
                        page_size,
                        PAGE_READONLY,
                        &original_protection) != FALSE;
                }
                return true;
            });

        try
        {
            Expect(!service.PollOnce(), "Transactional acknowledgement write failure unexpectedly reported success.");
            Expect(protection_changed, "Transactional write-failure callback did not make the carrier read-only.");
            Expect(callback_values_valid, "Write-failure callback received the wrong raw request.");
            Expect(ReadInt32(candidate_address + 12) == 4324, "Transactional write-failure poll changed the carrier despite rejecting the acknowledgement write.");
            const std::vector<helen::MemoryStateObserverDebugView> debug_views = service.GetDebugViews();
            Expect(debug_views.size() == 1, "Transactional write-failure debug view count mismatch.");
            Expect(debug_views[0].UpdateCount == 1, "Transactional write-failure update count did not record exactly one emitted request.");
            Expect(debug_views[0].LastMappedValue.has_value() && *debug_views[0].LastMappedValue == 5, "Transactional write-failure debug mapped value was not retained.");
            Expect(!debug_views[0].LastRawValue.has_value() || *debug_views[0].LastRawValue != 4334, "Transactional write-failure debug state claimed an unwritten success acknowledgement.");
            Expect(!debug_views[0].LastRawValue.has_value() || *debug_views[0].LastRawValue != 4399, "Transactional write-failure debug state claimed an unwritten failure acknowledgement.");

            DWORD restored_protection = 0;
            Expect(
                VirtualProtect(reinterpret_cast<void*>(page_address), page_size, original_protection, &restored_protection) != FALSE,
                "Failed to restore writable protection after the transactional write-failure test.");
            protection_changed = false;
            Expect(service.PollOnce(), "Transactional acknowledgement suppression poll failed after memory protection was restored.");
            Expect(callback_count == 1, "Transactional acknowledgement re-invoked the callback for an unchanged pending request.");
            Expect(callback_values_valid, "Transactional acknowledgement suppression poll received the wrong raw request.");
            Expect(ReadInt32(candidate_address + 12) == 4324, "Transactional acknowledgement suppression poll claimed an unwritten response.");
            Expect(VirtualFree(allocation, 0, MEM_RELEASE) != FALSE, "Failed to release the transactional write-failure allocation.");
        }
        catch (...)
        {
            if (protection_changed)
            {
                DWORD restored_protection = 0;
                VirtualProtect(reinterpret_cast<void*>(page_address), page_size, original_protection, &restored_protection);
            }

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

    /**
     * @brief Verifies a transactional callback exception is converted to a failure acknowledgement and a later identical request can retry.
     * @remarks The first callback throws before native work completes; the service must still write 4399, retain worker-safe state, and acknowledge the retry with 4334.
     */
    void RunTransactionalObserverCallbackExceptionTest()
    {
        SYSTEM_INFO system_info{};
        GetSystemInfo(&system_info);
        const std::size_t page_size = system_info.dwPageSize;
        void* const allocation = VirtualAlloc(nullptr, page_size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
        Expect(allocation != nullptr, "Failed to allocate writable memory for the transactional callback-exception test.");

        const std::uintptr_t page_address = reinterpret_cast<std::uintptr_t>(allocation);
        const std::uintptr_t candidate_address = page_address + 128;
        ConfigureGraphicsCarrierStateBlock(candidate_address, 4324);
        int callback_count = 0;
        bool callback_values_valid = true;
        helen::MemoryStateObserverService service(
            { CreateTransactionalMsaaObserverDefinition(page_address, page_address + page_size) },
            [&callback_count, &callback_values_valid](const helen::MemoryStateObserverUpdate& update)
            {
                ++callback_count;
                callback_values_valid = callback_values_valid && update.RawValue == 4324;
                if (callback_count == 1)
                {
                    throw std::runtime_error("intentional transactional callback failure");
                }

                return true;
            });

        try
        {
            Expect(service.PollOnce(), "Transactional callback exception was not handled as a failure acknowledgement.");
            Expect(ReadInt32(candidate_address + 12) == 4399, "Transactional callback exception did not write the declared failure response.");
            Expect(callback_values_valid, "Transactional callback-exception test received the wrong raw request.");
            ConfigureGraphicsCarrierStateBlock(candidate_address, 4324);
            Expect(service.PollOnce(), "Transactional request retry after callback exception unexpectedly failed.");
            Expect(callback_count == 2, "Transactional callback exception did not clear the request for retry.");
            Expect(callback_values_valid, "Transactional callback-exception retry received the wrong raw request.");
            Expect(ReadInt32(candidate_address + 12) == 4334, "Transactional retry after callback exception did not write the success acknowledgement.");
            Expect(VirtualFree(allocation, 0, MEM_RELEASE) != FALSE, "Failed to release the transactional callback-exception allocation.");
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

    /**
     * @brief Verifies a transactional observer without an update callback writes its declared failure response and remains retryable.
     */
    void RunTransactionalObserverMissingCallbackTest()
    {
        SYSTEM_INFO system_info{};
        GetSystemInfo(&system_info);
        const std::size_t page_size = system_info.dwPageSize;
        void* const allocation = VirtualAlloc(nullptr, page_size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
        Expect(allocation != nullptr, "Failed to allocate writable memory for the missing transactional callback test.");

        const std::uintptr_t page_address = reinterpret_cast<std::uintptr_t>(allocation);
        const std::uintptr_t candidate_address = page_address + 128;
        ConfigureGraphicsCarrierStateBlock(candidate_address, 4324);
        helen::MemoryStateObserverService service(
            { CreateTransactionalMsaaObserverDefinition(page_address, page_address + page_size) },
            helen::MemoryStateObserverService::UpdateCallback{});

        try
        {
            Expect(service.PollOnce(), "Missing transactional callback was not handled as a failure acknowledgement.");
            Expect(ReadInt32(candidate_address + 12) == 4399, "Missing transactional callback did not write the declared failure response.");
            ConfigureGraphicsCarrierStateBlock(candidate_address, 4324);
            Expect(service.PollOnce(), "Missing transactional callback retry unexpectedly failed.");
            Expect(ReadInt32(candidate_address + 12) == 4399, "Missing transactional callback retry did not write the failure response.");
            Expect(VirtualFree(allocation, 0, MEM_RELEASE) != FALSE, "Failed to release the missing transactional callback allocation.");
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

    /**
     * @brief Verifies a fatal nontransactional callback exception leaves a worker joinable and permits a clean restart.
     * @remarks The first worker pass throws from its callback; Stop must join the completed thread before the second Start creates a replacement worker.
     */
    void RunFatalObserverWorkerStopRestartTest()
    {
        SYSTEM_INFO system_info{};
        GetSystemInfo(&system_info);
        const std::size_t page_size = system_info.dwPageSize;
        void* const allocation = VirtualAlloc(nullptr, page_size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
        Expect(allocation != nullptr, "Failed to allocate writable memory for the fatal worker lifecycle test.");

        const std::uintptr_t page_address = reinterpret_cast<std::uintptr_t>(allocation);
        const std::uintptr_t candidate_address = page_address + 64;
        ConfigureStateBlock(candidate_address, 4101);
        std::mutex callback_mutex;
        std::condition_variable callback_condition;
        int callback_count = 0;
        bool first_callback_entered = false;
        helen::MemoryStateObserverService service(
            { CreateObserverDefinition(page_address, page_address + page_size) },
            [&callback_mutex, &callback_condition, &callback_count, &first_callback_entered](const helen::MemoryStateObserverUpdate&)
            {
                bool is_first_callback = false;
                {
                    std::lock_guard<std::mutex> lock(callback_mutex);
                    ++callback_count;
                    first_callback_entered = true;
                    is_first_callback = callback_count == 1;
                }
                callback_condition.notify_all();
                if (is_first_callback)
                {
                    throw std::runtime_error("intentional fatal observer callback failure");
                }

                return true;
            });

        try
        {
            Expect(service.Start(), "Fatal worker lifecycle test could not start the observer worker.");
            {
                std::unique_lock<std::mutex> lock(callback_mutex);
                Expect(
                    callback_condition.wait_for(
                        lock,
                        std::chrono::seconds(2),
                        [&first_callback_entered]()
                        {
                            return first_callback_entered;
                        }),
                    "Fatal observer worker did not enter its callback.");
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            service.Stop();
            ConfigureStateBlock(candidate_address, 4103);
            Expect(service.Start(), "Observer worker could not restart after a fatal callback exit.");
            {
                std::unique_lock<std::mutex> lock(callback_mutex);
                Expect(
                    callback_condition.wait_for(
                        lock,
                        std::chrono::seconds(2),
                        [&callback_count]()
                        {
                            return callback_count >= 2;
                        }),
                    "Restarted observer worker did not invoke its callback.");
            }

            service.Stop();
            Expect(VirtualFree(allocation, 0, MEM_RELEASE) != FALSE, "Failed to release the fatal worker lifecycle allocation.");
        }
        catch (...)
        {
            service.Stop();
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

    /**
     * @brief Verifies a worker keeps running after a handled transactional failure and processes a later rollback request on the same carrier.
     * @remarks Bounded condition waits coordinate callback entry while bounded carrier waits verify that failure and rollback acknowledgements were committed.
     */
    void RunTransactionalObserverWorkerRecoveryTest()
    {
        SYSTEM_INFO system_info{};
        GetSystemInfo(&system_info);
        const std::size_t page_size = system_info.dwPageSize;
        void* const allocation = VirtualAlloc(nullptr, page_size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
        Expect(allocation != nullptr, "Failed to allocate writable memory for the transactional worker recovery test.");

        const std::uintptr_t page_address = reinterpret_cast<std::uintptr_t>(allocation);
        const std::uintptr_t candidate_address = page_address + 128;
        ConfigureGraphicsCarrierStateBlock(candidate_address, 4324);
        helen::MemoryStateObserverDefinition setting_definition = CreateTransactionalMsaaObserverDefinition(page_address, page_address + page_size);
        setting_definition.AddressGroup = "batmanFrontendControlType";
        helen::MemoryStateObserverDefinition rollback_definition = CreateRollbackObserverDefinition(page_address, page_address + page_size);

        std::mutex callback_mutex;
        std::condition_variable callback_condition;
        int setting_callback_count = 0;
        int rollback_callback_count = 0;
        bool callback_values_valid = true;
        helen::MemoryStateObserverService service(
            { setting_definition, rollback_definition },
            [&callback_mutex, &callback_condition, &setting_callback_count, &rollback_callback_count, &callback_values_valid](const helen::MemoryStateObserverUpdate& update)
            {
                {
                    std::lock_guard<std::mutex> lock(callback_mutex);
                    if (update.ObserverId == "graphicsObserverMsaa")
                    {
                        ++setting_callback_count;
                        callback_values_valid = callback_values_valid && update.RawValue == 4324;
                        callback_condition.notify_all();
                        return false;
                    }

                    if (update.ObserverId == "graphicsObserverRollback")
                    {
                        ++rollback_callback_count;
                        callback_values_valid = callback_values_valid && update.RawValue == 4970;
                        callback_condition.notify_all();
                        return true;
                    }

                    callback_values_valid = false;
                }
                callback_condition.notify_all();
                return false;
            });

        try
        {
            Expect(service.Start(), "Transactional worker recovery test could not start the observer worker.");
            {
                std::unique_lock<std::mutex> lock(callback_mutex);
                Expect(
                    callback_condition.wait_for(
                        lock,
                        std::chrono::seconds(2),
                        [&setting_callback_count]()
                        {
                            return setting_callback_count >= 1;
                        }),
                    "Transactional worker did not invoke the setting callback.");
            }

            Expect(WaitForCarrierValue(candidate_address + 12, 4399, std::chrono::seconds(2)), "Transactional worker did not write the failure acknowledgement.");
            WriteInt32(candidate_address + 12, 4970);
            {
                std::unique_lock<std::mutex> lock(callback_mutex);
                Expect(
                    callback_condition.wait_for(
                        lock,
                        std::chrono::seconds(2),
                        [&rollback_callback_count]()
                        {
                            return rollback_callback_count >= 1;
                        }),
                    "Transactional worker did not invoke the rollback callback after the failure acknowledgement.");
            }

            Expect(WaitForCarrierValue(candidate_address + 12, 4960, std::chrono::seconds(2)), "Transactional worker did not write the rollback success acknowledgement.");
            {
                std::lock_guard<std::mutex> lock(callback_mutex);
                Expect(setting_callback_count == 1, "Transactional worker retried the continuously acknowledged setting request unexpectedly.");
                Expect(rollback_callback_count == 1, "Transactional worker invoked the rollback callback more than once.");
                Expect(callback_values_valid, "Transactional worker callback raw request values were incorrect.");
            }

            service.Stop();
            Expect(VirtualFree(allocation, 0, MEM_RELEASE) != FALSE, "Failed to release the transactional worker recovery allocation.");
        }
        catch (...)
        {
            service.Stop();
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

    /**
     * @brief Verifies a failed acknowledgement at carrier A is not suppressed when grouped cache replacement relocates the same request to carrier B.
     * @remarks The first callback makes the page read-only so the pending request survives an infrastructure failure until carrier A is invalidated.
     */
    void RunTransactionalObserverGroupedCarrierRelocationTest()
    {
        SYSTEM_INFO system_info{};
        GetSystemInfo(&system_info);
        const std::size_t page_size = system_info.dwPageSize;
        void* const allocation = VirtualAlloc(nullptr, page_size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
        Expect(allocation != nullptr, "Failed to allocate writable memory for the transactional relocation test.");

        const std::uintptr_t page_address = reinterpret_cast<std::uintptr_t>(allocation);
        const std::uintptr_t carrier_a_address = page_address + 128;
        const std::uintptr_t carrier_b_address = page_address + 256;
        ConfigureGraphicsCarrierStateBlock(carrier_a_address, 4324);
        ConfigureGraphicsCarrierStateBlock(carrier_b_address, 4324);
        bool protection_changed = false;
        int callback_count = 0;
        bool callback_values_valid = true;
        DWORD original_protection = 0;
        helen::MemoryStateObserverDefinition definition = CreateTransactionalMsaaObserverDefinition(page_address, page_address + page_size);
        definition.AddressGroup = "batmanFrontendControlType";
        helen::MemoryStateObserverService service(
            { definition },
            [&protection_changed, &callback_count, &callback_values_valid, &original_protection, page_address, page_size](const helen::MemoryStateObserverUpdate& update)
            {
                callback_values_valid = callback_values_valid && update.RawValue == 4324;
                ++callback_count;
                if (callback_count == 1)
                {
                    protection_changed = VirtualProtect(
                        reinterpret_cast<void*>(page_address),
                        page_size,
                        PAGE_READONLY,
                        &original_protection) != FALSE;
                }

                return true;
            });

        try
        {
            Expect(!service.PollOnce(), "Transactional relocation setup unexpectedly wrote an acknowledgement on a read-only carrier.");
            Expect(protection_changed, "Transactional relocation callback did not make carrier A read-only.");
            DWORD restored_protection = 0;
            Expect(
                VirtualProtect(reinterpret_cast<void*>(page_address), page_size, original_protection, &restored_protection) != FALSE,
                "Failed to restore writable protection before transactional carrier relocation.");
            protection_changed = false;
            Expect(service.PollOnce(), "Transactional relocation same-address suppression poll failed.");
            Expect(callback_count == 1, "Transactional relocation re-invoked the callback for an unchanged pending request at carrier A.");
            Expect(ReadInt32(carrier_a_address + 12) == 4324, "Transactional relocation same-address suppression poll claimed an unwritten response.");
            WriteInt32(carrier_a_address + 4, 7);
            Expect(service.PollOnce(), "Transactional carrier relocation poll unexpectedly failed.");
            Expect(callback_count == 2, "Transactional carrier relocation suppressed the request pending at carrier A.");
            Expect(callback_values_valid, "Transactional relocation callback received the wrong raw request.");
            Expect(ReadInt32(carrier_b_address + 12) == 4334, "Transactional carrier relocation did not acknowledge the request at carrier B.");
            Expect(VirtualFree(allocation, 0, MEM_RELEASE) != FALSE, "Failed to release the transactional relocation allocation.");
        }
        catch (...)
        {
            if (protection_changed)
            {
                DWORD restored_protection = 0;
                VirtualProtect(reinterpret_cast<void*>(page_address), page_size, original_protection, &restored_protection);
            }

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
                return true;
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

    /**
     * @brief Verifies grouped VSync and MSAA observers reuse the first structurally resolved graphics carrier.
     * @remarks The VSync response is resolved first, then an MSAA request is written into that same carrier before the next complete poll.
     */
    void RunGroupedGraphicsCarrierObserverReuseTest()
    {
        SYSTEM_INFO system_info{};
        GetSystemInfo(&system_info);
        const std::size_t page_size = system_info.dwPageSize;
        void* const allocation = VirtualAlloc(nullptr, page_size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
        Expect(allocation != nullptr, "Failed to allocate writable memory for the grouped graphics carrier reuse test.");

        const std::uintptr_t page_address = reinterpret_cast<std::uintptr_t>(allocation);
        const std::uintptr_t candidate_address = page_address + 128;
        ConfigureGraphicsCarrierStateBlock(candidate_address, 4200);

        helen::MemoryStateObserverDefinition vsync_definition = CreateGraphicsCarrierObserverDefinition(
            "graphicsObserverVsync",
            page_address,
            page_address + page_size,
            "vsync",
            { 4210, 4211 });
        vsync_definition.AddressGroup = "batmanFrontendControlType";

        helen::MemoryStateObserverDefinition msaa_definition = CreateGraphicsCarrierObserverDefinition(
            "graphicsObserverMsaa",
            page_address,
            page_address + page_size,
            "msaa",
            { 4990, 4991 },
            "applyBatmanMsaa");
        msaa_definition.AddressGroup = "batmanFrontendControlType";

        std::vector<helen::MemoryStateObserverUpdate> updates;
        helen::MemoryStateObserverService service(
            { vsync_definition, msaa_definition },
            [&updates](const helen::MemoryStateObserverUpdate& update)
            {
                updates.push_back(update);
                return true;
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
            Expect(service.PollOnce(), "Expected the grouped graphics carrier resolution poll to succeed.");
            Expect(ReadInt32(candidate_address + 12) == 4211, "VSync resolution did not write its configured response into the carrier.");

            std::vector<helen::MemoryStateObserverDebugView> debug_views = service.GetDebugViews();
            Expect(debug_views.size() == 2, "Grouped graphics carrier debug view count mismatch after resolution.");
            Expect(debug_views[0].CachedAddress == candidate_address, "VSync did not resolve the verified graphics carrier.");
            Expect(debug_views[1].CachedAddress == candidate_address, "MSAA did not mirror the grouped graphics carrier address.");
            Expect(debug_views[0].RescanCount == 1, "VSync initial rescan count mismatch for grouped graphics carrier.");
            Expect(debug_views[1].RescanCount == 0, "MSAA rescanned instead of reusing the grouped graphics carrier address.");

            ConfigureGraphicsCarrierStateBlock(candidate_address, 4990);
            Expect(service.PollOnce(), "Expected the grouped MSAA request poll to succeed.");

            debug_views = service.GetDebugViews();
            Expect(debug_views[0].CachedAddress == candidate_address, "VSync lost the grouped graphics carrier address after an MSAA request.");
            Expect(debug_views[1].CachedAddress == candidate_address, "MSAA lost the grouped graphics carrier address after its request.");
            Expect(debug_views[0].RescanCount == 1, "VSync rescanned after the grouped MSAA request.");
            Expect(debug_views[1].RescanCount == 0, "MSAA rescanned after the grouped MSAA request.");
            Expect(updates.size() == 1, "Grouped MSAA request did not emit exactly one mapped update.");
            Expect(updates[0].ObserverId == "graphicsObserverMsaa", "Grouped MSAA request update observer mismatch.");
            Expect(updates[0].RawValue == 4990 && updates[0].MappedValue == 0, "Grouped MSAA request update values mismatch.");
            Expect(updates[0].CommandId.has_value() && *updates[0].CommandId == "applyBatmanMsaa", "Grouped MSAA request command mismatch.");

            Expect(VirtualFree(allocation, 0, MEM_RELEASE) != FALSE, "Failed to release the grouped graphics carrier reuse allocation.");
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

    /**
     * @brief Verifies that unresolved grouped graphics observers perform one discovery scan per manual poll pass.
     * @remarks The first pass has no carrier and therefore elects only the first group member to scan; a carrier created between passes is then discovered once and shared by all three members.
     */
    void RunGroupedGraphicsCarrierSingleScanPerPassTest()
    {
        SYSTEM_INFO system_info{};
        GetSystemInfo(&system_info);
        const std::size_t page_size = system_info.dwPageSize;
        void* const allocation = VirtualAlloc(nullptr, page_size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
        Expect(allocation != nullptr, "Failed to allocate writable memory for the grouped single-scan graphics carrier test.");

        const std::uintptr_t page_address = reinterpret_cast<std::uintptr_t>(allocation);
        const std::uintptr_t candidate_address = page_address + 128;

        helen::MemoryStateObserverDefinition vsync_definition = CreateGraphicsCarrierObserverDefinition(
            "graphicsObserverVsync",
            page_address,
            page_address + page_size,
            "vsync",
            { 4210, 4211 });
        vsync_definition.AddressGroup = "batmanFrontendControlType";

        helen::MemoryStateObserverDefinition msaa_definition = CreateGraphicsCarrierObserverDefinition(
            "graphicsObserverMsaa",
            page_address,
            page_address + page_size,
            "msaa",
            { 4990, 4991 },
            "applyBatmanMsaa");
        msaa_definition.AddressGroup = "batmanFrontendControlType";

        helen::MemoryStateObserverDefinition apply_signal_definition = CreateGraphicsCarrierObserverDefinition(
            "graphicsObserverApplySignal",
            page_address,
            page_address + page_size,
            "applySignal",
            { 4101, 4102 },
            "applyBatmanGraphicsDraft");
        apply_signal_definition.AddressGroup = "batmanFrontendControlType";

        helen::MemoryStateObserverService service(
            { vsync_definition, msaa_definition, apply_signal_definition },
            [](const helen::MemoryStateObserverUpdate&)
            {
                return true;
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
            Expect(service.PollOnce(), "Grouped unresolved observer pass unexpectedly failed.");
            std::vector<helen::MemoryStateObserverDebugView> debug_views = service.GetDebugViews();
            Expect(debug_views.size() == 3, "Grouped single-scan graphics carrier debug view count mismatch.");
            Expect(debug_views[0].RescanCount == 1, "The first grouped observer did not lead the unresolved scan.");
            Expect(debug_views[1].RescanCount == 0, "The second grouped observer duplicated the unresolved scan.");
            Expect(debug_views[2].RescanCount == 0, "The third grouped observer duplicated the unresolved scan.");
            Expect(debug_views[0].CachedAddress == 0 && debug_views[1].CachedAddress == 0 && debug_views[2].CachedAddress == 0,
                "Unresolved grouped observers invented a carrier address.");

            ConfigureGraphicsCarrierStateBlock(candidate_address, 4200);
            Expect(service.PollOnce(), "Late grouped carrier discovery pass unexpectedly failed.");
            Expect(ReadInt32(candidate_address + 12) == 4211, "The late grouped carrier did not receive the VSync response.");

            debug_views = service.GetDebugViews();
            Expect(debug_views[0].RescanCount == 2, "The next pass did not retry grouped discovery.");
            Expect(debug_views[1].RescanCount == 0 && debug_views[2].RescanCount == 0,
                "Grouped observers performed duplicate scans after late carrier creation.");
            Expect(debug_views[0].CachedAddress == candidate_address &&
                   debug_views[1].CachedAddress == candidate_address &&
                   debug_views[2].CachedAddress == candidate_address,
                "The resolved late carrier was not shared across the complete group.");

            Expect(VirtualFree(allocation, 0, MEM_RELEASE) != FALSE, "Failed to release the grouped single-scan graphics carrier allocation.");
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

    /**
     * @brief Verifies timed polling keeps one unresolved grouped graphics observer as the scan leader across repeated worker passes.
     * @remarks Three observers share a one-page empty range and identical 1 ms intervals; the bounded rescan wait proves PollDueObservers executes at least two passes while only the first member scans.
     */
    void RunGroupedGraphicsCarrierTimedSingleScanPerPassTest()
    {
        SYSTEM_INFO system_info{};
        GetSystemInfo(&system_info);
        const std::size_t page_size = system_info.dwPageSize;
        void* const allocation = VirtualAlloc(nullptr, page_size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
        Expect(allocation != nullptr, "Failed to allocate writable memory for the timed grouped single-scan graphics carrier test.");

        const std::uintptr_t page_address = reinterpret_cast<std::uintptr_t>(allocation);

        helen::MemoryStateObserverDefinition vsync_definition = CreateGraphicsCarrierObserverDefinition(
            "graphicsObserverVsync",
            page_address,
            page_address + page_size,
            "vsync",
            { 4210, 4211 });
        vsync_definition.AddressGroup = "batmanFrontendControlType";
        vsync_definition.PollIntervalMs = 1;

        helen::MemoryStateObserverDefinition msaa_definition = CreateGraphicsCarrierObserverDefinition(
            "graphicsObserverMsaa",
            page_address,
            page_address + page_size,
            "msaa",
            { 4990, 4991 },
            "applyBatmanMsaa");
        msaa_definition.AddressGroup = "batmanFrontendControlType";
        msaa_definition.PollIntervalMs = 1;

        helen::MemoryStateObserverDefinition apply_signal_definition = CreateGraphicsCarrierObserverDefinition(
            "graphicsObserverApplySignal",
            page_address,
            page_address + page_size,
            "applySignal",
            { 4101, 4102 },
            "applyBatmanGraphicsDraft");
        apply_signal_definition.AddressGroup = "batmanFrontendControlType";
        apply_signal_definition.PollIntervalMs = 1;

        helen::MemoryStateObserverService service(
            { vsync_definition, msaa_definition, apply_signal_definition },
            [](const helen::MemoryStateObserverUpdate&)
            {
                return true;
            });

        try
        {
            Expect(service.Start(), "Timed grouped observer service could not start.");
            Expect(
                WaitForObserverRescanCount(service, 0, 2, std::chrono::seconds(2)),
                "Timed grouped observer worker did not perform two leader scans.");

            service.Stop();
            const std::vector<helen::MemoryStateObserverDebugView> debug_views = service.GetDebugViews();
            Expect(debug_views.size() == 3, "Timed grouped single-scan graphics carrier debug view count mismatch.");
            Expect(debug_views[0].RescanCount >= 2, "Timed grouped observer leader performed fewer than two scans.");
            Expect(debug_views[1].RescanCount == 0, "Timed grouped second observer performed an independent scan.");
            Expect(debug_views[2].RescanCount == 0, "Timed grouped third observer performed an independent scan.");
            Expect(debug_views[0].CachedAddress == 0 && debug_views[1].CachedAddress == 0 && debug_views[2].CachedAddress == 0,
                "Timed unresolved grouped observers invented a carrier address.");

            Expect(VirtualFree(allocation, 0, MEM_RELEASE) != FALSE, "Failed to release the timed grouped single-scan graphics carrier allocation.");
        }
        catch (...)
        {
            service.Stop();
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

    /**
     * @brief Verifies a stale grouped carrier clears shared state and lets the first observer rescan every group member onto a new carrier.
     * @remarks Carrier A is structurally invalidated before carrier B is activated, so the scan cannot retain stale group state.
     */
    void RunGroupedGraphicsCarrierObserverStaleCacheTest()
    {
        SYSTEM_INFO system_info{};
        GetSystemInfo(&system_info);
        const std::size_t page_size = system_info.dwPageSize;
        void* const allocation = VirtualAlloc(nullptr, page_size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
        Expect(allocation != nullptr, "Failed to allocate writable memory for the grouped graphics carrier stale-cache test.");

        const std::uintptr_t page_address = reinterpret_cast<std::uintptr_t>(allocation);
        const std::uintptr_t carrier_a_address = page_address + 128;
        const std::uintptr_t carrier_b_address = page_address + 256;
        ConfigureGraphicsCarrierStateBlock(carrier_a_address, 4210);

        helen::MemoryStateObserverDefinition vsync_definition = CreateGraphicsCarrierObserverDefinition(
            "graphicsObserverVsync",
            page_address,
            page_address + page_size,
            "vsync",
            { 4210, 4211 });
        vsync_definition.AddressGroup = "batmanFrontendControlType";

        helen::MemoryStateObserverDefinition msaa_definition = CreateGraphicsCarrierObserverDefinition(
            "graphicsObserverMsaa",
            page_address,
            page_address + page_size,
            "msaa",
            { 4990, 4991 },
            "applyBatmanMsaa");
        msaa_definition.AddressGroup = "batmanFrontendControlType";

        helen::MemoryStateObserverService service(
            { vsync_definition, msaa_definition },
            [](const helen::MemoryStateObserverUpdate&)
            {
                return true;
            });

        try
        {
            Expect(service.PollOnce(), "Expected grouped graphics carrier A resolution to succeed.");
            std::vector<helen::MemoryStateObserverDebugView> debug_views = service.GetDebugViews();
            Expect(debug_views.size() == 2, "Grouped graphics carrier stale-cache debug view count mismatch after carrier A resolution.");
            Expect(debug_views[0].CachedAddress == carrier_a_address, "VSync did not resolve grouped graphics carrier A.");
            Expect(debug_views[1].CachedAddress == carrier_a_address, "MSAA did not mirror grouped graphics carrier A.");
            Expect(debug_views[0].RescanCount == 1, "VSync initial rescan count mismatch before stale grouped carrier invalidation.");
            Expect(debug_views[1].RescanCount == 0, "MSAA rescanned before stale grouped carrier invalidation.");

            WriteInt32(carrier_a_address + 4, 7);
            ConfigureGraphicsCarrierStateBlock(carrier_b_address, 4990);

            Expect(service.PollOnce(), "Expected grouped graphics carrier stale-cache replacement poll to succeed.");
            debug_views = service.GetDebugViews();
            Expect(debug_views[0].CachedAddress == carrier_b_address, "VSync retained stale grouped graphics carrier A instead of resolving B.");
            Expect(debug_views[1].CachedAddress == carrier_b_address, "MSAA did not receive the replacement grouped graphics carrier B address.");
            Expect(debug_views[0].CachedAddress != carrier_a_address && debug_views[1].CachedAddress != carrier_a_address, "A stale grouped graphics carrier address remained visible after invalidation.");
            Expect(debug_views[0].RescanCount == 2, "VSync did not perform the grouped replacement rescan.");
            Expect(debug_views[1].RescanCount == 0, "MSAA performed an independent rescan after grouped replacement.");

            Expect(VirtualFree(allocation, 0, MEM_RELEASE) != FALSE, "Failed to release the grouped graphics carrier stale-cache allocation.");
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

    /**
     * @brief Proves late grouped discovery, complete protocol recognition, quality transactions, and atomic carrier rearming.
     * @remarks The carrier is absent for the first pass, every quality mini-range receives a read, successful write, failed write, and retry, and a structurally invalidated carrier is replaced without duplicate group scans.
     */
    void RunGroupedBatmanGraphicsQualityCoverageTest()
    {
        SYSTEM_INFO system_info{};
        GetSystemInfo(&system_info);
        const std::size_t page_size = system_info.dwPageSize;
        void* const allocation = VirtualAlloc(nullptr, page_size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
        Expect(allocation != nullptr, "Failed to allocate writable memory for the complete Batman graphics protocol test.");

        const std::uintptr_t page_address = reinterpret_cast<std::uintptr_t>(allocation);
        const std::uintptr_t carrier_a_address = page_address + 128;
        const std::uintptr_t carrier_b_address = page_address + 256;
        const std::vector<int> protocol_union = CreateCompleteBatmanGroupedProtocolUnion();
        Expect(protocol_union.size() == 100, "The complete Batman graphics protocol union omitted one or more raw values.");
        Expect(std::is_sorted(protocol_union.begin(), protocol_union.end()), "The complete Batman graphics protocol union was not sorted.");
        const std::vector<BatmanGroupedObserverProtocol> protocols = {
            { "graphicsObserverVsync", "vsync", 4200, { 4210, 4211 }, { 4220, 4221 }, { 4230, 4231 }, 4299, nullptr },
            { "graphicsObserverMsaa", "msaa", 4300, { 4310, 4311, 4312, 4313, 4314 }, { 4320, 4321, 4322, 4323, 4324 }, { 4330, 4331, 4332, 4333, 4334 }, 4399, nullptr },
            { "graphicsObserverPhysx", "physx", 4400, { 4410, 4411, 4412 }, { 4420, 4421, 4422 }, { 4430, 4431, 4432 }, 4499, nullptr },
            { "graphicsObserverStereo", "stereo", 4500, { 4510, 4511 }, { 4520, 4521 }, { 4530, 4531 }, 4599, nullptr },
            { "graphicsObserverBloom", "bloom", 4600, { 4601, 4602 }, { 4603, 4604 }, { 4605, 4606 }, 4609, "syncBatmanGraphicsDetailLevel" },
            { "graphicsObserverDynamicShadows", "dynamicShadows", 4610, { 4611, 4612 }, { 4613, 4614 }, { 4615, 4616 }, 4619, "syncBatmanGraphicsDetailLevel" },
            { "graphicsObserverMotionBlur", "motionBlur", 4620, { 4621, 4622 }, { 4623, 4624 }, { 4625, 4626 }, 4629, "syncBatmanGraphicsDetailLevel" },
            { "graphicsObserverDistortion", "distortion", 4630, { 4631, 4632 }, { 4633, 4634 }, { 4635, 4636 }, 4639, "syncBatmanGraphicsDetailLevel" },
            { "graphicsObserverFogVolumes", "fogVolumes", 4640, { 4641, 4642 }, { 4643, 4644 }, { 4645, 4646 }, 4649, "syncBatmanGraphicsDetailLevel" },
            { "graphicsObserverSphericalHarmonicLighting", "sphericalHarmonicLighting", 4650, { 4651, 4652 }, { 4653, 4654 }, { 4655, 4656 }, 4659, "syncBatmanGraphicsDetailLevel" },
            { "graphicsObserverAmbientOcclusion", "ambientOcclusion", 4660, { 4661, 4662 }, { 4663, 4664 }, { 4665, 4666 }, 4669, "syncBatmanGraphicsDetailLevel" }
        };

        std::vector<helen::MemoryStateObserverDefinition> definitions;
        definitions.reserve(protocols.size());
        for (const BatmanGroupedObserverProtocol& protocol : protocols)
        {
            helen::MemoryStateObserverDefinition definition = CreateFullBatmanGroupedObserverDefinition(
                protocol,
                page_address,
                page_address + page_size,
                protocol_union);
            Expect(
                definition.AddressGroup.has_value() && *definition.AddressGroup == "batmanFrontendControlType",
                "A complete Batman graphics observer did not declare the shared address group.");
            Expect(
                definition.AddressMatchValues == protocol_union,
                "A complete Batman graphics observer did not carry the exact sorted protocol union.");
            definitions.push_back(std::move(definition));
        }

        std::unordered_map<std::string, int> config_values;
        for (const BatmanGroupedObserverProtocol& protocol : protocols)
        {
            config_values.emplace(protocol.TargetConfigKey, 1);
        }

        std::vector<helen::MemoryStateObserverUpdate> updates;
        std::string failing_config_key;
        bool force_failure = false;
        helen::MemoryStateObserverService service(
            std::move(definitions),
            [&updates, &failing_config_key, &force_failure](const helen::MemoryStateObserverUpdate& update)
            {
                updates.push_back(update);
                return !(force_failure && update.ConfigKey == failing_config_key);
            },
            [&config_values](const std::string& config_key) -> std::optional<int>
            {
                const auto value = config_values.find(config_key);
                if (value == config_values.end())
                {
                    return std::nullopt;
                }

                return value->second;
            });

        try
        {
            Expect(service.PollOnce(), "The unresolved grouped Batman carrier poll unexpectedly failed before it was seeded.");
            std::vector<helen::MemoryStateObserverDebugView> debug_views = service.GetDebugViews();
            Expect(debug_views.size() == protocols.size(), "Complete Batman grouped observer count mismatch before late discovery.");
            Expect(debug_views[0].RescanCount == 1, "The grouped Batman leader did not perform the initial broad scan.");
            for (std::size_t observer_index = 1; observer_index < debug_views.size(); ++observer_index)
            {
                Expect(
                    debug_views[observer_index].RescanCount == 0,
                    "A nonleader Batman observer duplicated the unresolved grouped broad scan.");
                Expect(
                    debug_views[observer_index].CachedAddress == 0,
                    "An unresolved Batman observer invented a cached carrier address.");
            }

            ConfigureGraphicsCarrierStateBlock(carrier_a_address, protocols[0].ReadRequestValue);
            Expect(service.PollOnce(), "The late grouped Batman carrier discovery pass unexpectedly failed.");
            Expect(ReadInt32(carrier_a_address + 12) == protocols[0].ResponseValues[1], "The VSync read response did not use the exact enabled response code.");

            debug_views = service.GetDebugViews();
            Expect(debug_views[0].RescanCount >= 2, "The grouped Batman leader did not retry late carrier discovery.");
            for (const helen::MemoryStateObserverDebugView& debug_view : debug_views)
            {
                Expect(debug_view.CachedAddress == carrier_a_address, "Late grouped Batman discovery did not share the carrier with every observer.");
            }

            for (std::size_t protocol_index = 4; protocol_index < protocols.size(); ++protocol_index)
            {
                const BatmanGroupedObserverProtocol& protocol = protocols[protocol_index];
                const int read_config_value = static_cast<int>(protocol_index % 2);
                config_values[protocol.TargetConfigKey] = read_config_value;
                WriteInt32(carrier_a_address + 12, protocol.ReadRequestValue);
                Expect(service.PollOnce(), "A quality observer read-response poll unexpectedly failed.");
                Expect(
                    ReadInt32(carrier_a_address + 12) == protocol.ResponseValues[read_config_value],
                    "A quality observer read response wrote the wrong protocol value.");

                const std::size_t successful_update_count = updates.size();
                WriteInt32(carrier_a_address + 12, protocol.WriteValues[1]);
                Expect(service.PollOnce(), "A quality observer enabled write unexpectedly failed.");
                Expect(updates.size() == successful_update_count + 1, "A quality observer enabled write did not emit exactly one update.");
                const helen::MemoryStateObserverUpdate& successful_update = updates.back();
                Expect(successful_update.ObserverId == protocol.Id, "A quality observer enabled update targeted the wrong observer.");
                Expect(successful_update.ConfigKey == protocol.TargetConfigKey, "A quality observer enabled update targeted the wrong config key.");
                Expect(successful_update.RawValue == protocol.WriteValues[1] && successful_update.MappedValue == 1, "A quality observer enabled update carried the wrong values.");
                Expect(
                    successful_update.CommandId.has_value() && *successful_update.CommandId == "syncBatmanGraphicsDetailLevel",
                    "A quality observer enabled update omitted the detail-level synchronization command.");
                Expect(ReadInt32(carrier_a_address + 12) == protocol.AcknowledgementValues[1], "A quality observer enabled write did not receive its exact acknowledgement.");

                const std::size_t failed_update_count = updates.size();
                failing_config_key = protocol.TargetConfigKey;
                force_failure = true;
                WriteInt32(carrier_a_address + 12, protocol.WriteValues[0]);
                Expect(service.PollOnce(), "A handled quality observer failure unexpectedly failed the poll.");
                Expect(updates.size() == failed_update_count + 1, "A failed quality observer write did not emit exactly one update.");
                const helen::MemoryStateObserverUpdate& failed_update = updates.back();
                Expect(failed_update.ObserverId == protocol.Id && failed_update.ConfigKey == protocol.TargetConfigKey, "A failed quality observer update targeted the wrong setting.");
                Expect(failed_update.RawValue == protocol.WriteValues[0] && failed_update.MappedValue == 0, "A failed quality observer update carried the wrong values.");
                Expect(
                    failed_update.CommandId.has_value() && *failed_update.CommandId == "syncBatmanGraphicsDetailLevel",
                    "A failed quality observer update omitted the detail-level synchronization command.");
                Expect(ReadInt32(carrier_a_address + 12) == protocol.FailureResponseValue, "A failed quality observer write did not receive its exact failure response.");

                WriteInt32(carrier_a_address + 12, protocol.WriteValues[0]);
                Expect(service.PollOnce(), "A rearmed quality observer failure unexpectedly failed the poll.");
                Expect(updates.size() == failed_update_count + 2, "A quality observer failure request was not rearmed after its failure response.");
                Expect(ReadInt32(carrier_a_address + 12) == protocol.FailureResponseValue, "A rearmed quality observer write did not repeat its exact failure response.");
                force_failure = false;
                failing_config_key.clear();
            }

            debug_views = service.GetDebugViews();
            Expect(debug_views[0].RescanCount == 2, "The grouped Batman leader performed an unexpected extra scan during quality transactions.");
            for (std::size_t observer_index = 1; observer_index < debug_views.size(); ++observer_index)
            {
                Expect(debug_views[observer_index].RescanCount == 0, "A nonleader Batman observer performed a duplicate broad scan during quality transactions.");
            }

            WriteInt32(carrier_a_address + 4, 7);
            Expect(service.PollOnce(), "The grouped Batman carrier invalidation poll unexpectedly failed.");
            debug_views = service.GetDebugViews();
            Expect(debug_views[0].RescanCount == 3, "The grouped Batman leader did not perform one invalidation rescan.");
            for (const helen::MemoryStateObserverDebugView& debug_view : debug_views)
            {
                Expect(debug_view.CachedAddress == 0, "Grouped Batman invalidation did not clear every observer cache atomically.");
            }

            ConfigureGraphicsCarrierStateBlock(carrier_b_address, protocols[4].ReadRequestValue);
            Expect(service.PollOnce(), "The grouped Batman replacement carrier poll unexpectedly failed.");
            Expect(ReadInt32(carrier_b_address + 12) == protocols[4].ResponseValues[config_values[protocols[4].TargetConfigKey]], "The replacement carrier did not rearm the Bloom read response.");
            debug_views = service.GetDebugViews();
            Expect(debug_views[0].RescanCount == 4, "The grouped Batman leader did not perform one replacement-carrier scan.");
            for (std::size_t observer_index = 1; observer_index < debug_views.size(); ++observer_index)
            {
                Expect(debug_views[observer_index].RescanCount == 0, "A nonleader Batman observer scanned while the replacement carrier was resolved.");
            }
            for (const helen::MemoryStateObserverDebugView& debug_view : debug_views)
            {
                Expect(debug_view.CachedAddress == carrier_b_address, "The replacement grouped Batman carrier was not shared atomically.");
            }

            WriteInt32(carrier_b_address + 12, protocols.back().WriteValues[1]);
            Expect(service.PollOnce(), "The replacement carrier did not accept a quality write after rearming.");
            Expect(ReadInt32(carrier_b_address + 12) == protocols.back().AcknowledgementValues[1], "The replacement carrier quality write did not receive its exact acknowledgement.");

            Expect(VirtualFree(allocation, 0, MEM_RELEASE) != FALSE, "Failed to release the complete Batman graphics protocol allocation.");
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

    /**
     * @brief Verifies a manual poll waits for a worker poll pass whose update callback is still executing.
     * @remarks Condition variables make the overlap deterministic: the worker callback blocks first, then a started manual poll must remain incomplete until that callback is released.
     */
    void RunObserverPollPassSerializationTest()
    {
        SYSTEM_INFO system_info{};
        GetSystemInfo(&system_info);
        const std::size_t page_size = system_info.dwPageSize;
        void* const allocation = VirtualAlloc(nullptr, page_size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
        Expect(allocation != nullptr, "Failed to allocate writable memory for the observer poll serialization test.");

        const std::uintptr_t page_address = reinterpret_cast<std::uintptr_t>(allocation);
        const std::uintptr_t candidate_address = page_address + 64;
        ConfigureStateBlock(candidate_address, 4101);

        std::mutex callback_mutex;
        std::condition_variable callback_entered_condition;
        std::condition_variable callback_release_condition;
        bool callback_entered = false;
        bool callback_release_requested = false;
        int callback_invocation_count = 0;

        std::mutex manual_poll_mutex;
        std::condition_variable manual_poll_condition;
        bool manual_poll_started = false;
        bool manual_poll_completed = false;
        bool manual_poll_result = false;

        helen::MemoryStateObserverService service(
            { CreateObserverDefinition(page_address, page_address + page_size) },
            [&](const helen::MemoryStateObserverUpdate&)
            {
                std::unique_lock<std::mutex> lock(callback_mutex);
                ++callback_invocation_count;
                if (callback_invocation_count == 1)
                {
                    callback_entered = true;
                    callback_entered_condition.notify_all();
                    callback_release_condition.wait(
                        lock,
                        [&callback_release_requested]()
                        {
                            return callback_release_requested;
                        });
                }
                return true;
            });

        std::thread manual_poll_thread;
        try
        {
            Expect(service.Start(), "Expected the observer worker to start for the poll serialization test.");
            {
                std::unique_lock<std::mutex> lock(callback_mutex);
                Expect(
                    callback_entered_condition.wait_for(
                        lock,
                        std::chrono::seconds(2),
                        [&callback_entered]()
                        {
                            return callback_entered;
                        }),
                    "Worker poll did not enter the blocking update callback.");
            }

            manual_poll_thread = std::thread(
                [&service, &manual_poll_mutex, &manual_poll_condition, &manual_poll_started, &manual_poll_completed, &manual_poll_result]()
                {
                    {
                        std::lock_guard<std::mutex> lock(manual_poll_mutex);
                        manual_poll_started = true;
                    }
                    manual_poll_condition.notify_all();

                    const bool poll_result = service.PollOnce();
                    {
                        std::lock_guard<std::mutex> lock(manual_poll_mutex);
                        manual_poll_result = poll_result;
                        manual_poll_completed = true;
                    }
                    manual_poll_condition.notify_all();
                });

            {
                std::unique_lock<std::mutex> lock(manual_poll_mutex);
                Expect(
                    manual_poll_condition.wait_for(
                        lock,
                        std::chrono::seconds(2),
                        [&manual_poll_started]()
                        {
                            return manual_poll_started;
                        }),
                    "Manual poll thread did not start for the poll serialization test.");
                const bool completed_while_callback_blocked = manual_poll_condition.wait_for(
                    lock,
                    std::chrono::milliseconds(100),
                    [&manual_poll_completed]()
                    {
                        return manual_poll_completed;
                    });
                Expect(!completed_while_callback_blocked, "Manual PollOnce completed while the worker callback remained blocked.");
            }

            {
                std::lock_guard<std::mutex> lock(callback_mutex);
                callback_release_requested = true;
            }
            callback_release_condition.notify_all();

            {
                std::unique_lock<std::mutex> lock(manual_poll_mutex);
                Expect(
                    manual_poll_condition.wait_for(
                        lock,
                        std::chrono::seconds(2),
                        [&manual_poll_completed]()
                        {
                            return manual_poll_completed;
                        }),
                    "Manual PollOnce did not complete after the worker callback was released.");
                Expect(manual_poll_result, "Manual PollOnce failed after the worker callback was released.");
            }

            manual_poll_thread.join();
            service.Stop();
            Expect(VirtualFree(allocation, 0, MEM_RELEASE) != FALSE, "Failed to release the observer poll serialization allocation.");
        }
        catch (...)
        {
            {
                std::lock_guard<std::mutex> lock(callback_mutex);
                callback_release_requested = true;
            }
            callback_release_condition.notify_all();
            if (manual_poll_thread.joinable())
            {
                manual_poll_thread.join();
            }

            service.Stop();
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
    RunTransactionalObserverAcknowledgementTest();
    RunTransactionalObserverFailureAcknowledgementTest();
    RunTransactionalObserverReadResponseTest();
    RunTransactionalObserverConfigCallbackExceptionTest();
    RunTransactionalObserverReadResponsePendingReconciliationTest();
    RunTransactionalObserverWorkerConfigCallbackExceptionTest();
    RunTransactionalObserverWriteFailureTest();
    RunTransactionalObserverCallbackExceptionTest();
    RunTransactionalObserverMissingCallbackTest();
    RunFatalObserverWorkerStopRestartTest();
    RunTransactionalObserverWorkerRecoveryTest();
    RunTransactionalObserverGroupedCarrierRelocationTest();
    RunGraphicsCarrierObserverCoexistenceTest();
    RunGroupedGraphicsCarrierObserverReuseTest();
    RunGroupedGraphicsCarrierSingleScanPerPassTest();
    RunGroupedGraphicsCarrierTimedSingleScanPerPassTest();
    RunGroupedGraphicsCarrierObserverStaleCacheTest();
    RunGroupedBatmanGraphicsQualityCoverageTest();
    RunObserverPollPassSerializationTest();

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
            return true;
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
        Expect(ReadInt32(candidate_address) == 4101, "Legacy subtitle observer unexpectedly wrote a transaction response.");

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
        Expect(ReadInt32(candidate_address) == 4103, "Legacy subtitle observer unexpectedly wrote an acknowledgement or failure response.");

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
