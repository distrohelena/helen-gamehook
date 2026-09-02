#include <HelenHook/MemoryStateObserverCheckDefinition.h>
#include <HelenHook/MemoryStateObserverDefinition.h>
#include <HelenHook/MemoryStateObserverMapEntryDefinition.h>
#include <HelenHook/MemoryStateObserverService.h>

#include <Windows.h>

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstring>
#include <initializer_list>
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
    RunGroupedGraphicsCarrierObserverStaleCacheTest();
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
