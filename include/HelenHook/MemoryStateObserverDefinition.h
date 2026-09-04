#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <HelenHook/MemoryStateObserverCheckDefinition.h>
#include <HelenHook/MemoryStateObserverMapEntryDefinition.h>

namespace helen
{
    /**
     * @brief Declares one bounded in-process memory observer that watches for a validated state block and maps its value into Helen config.
     */
    class MemoryStateObserverDefinition
    {
    public:
        /** @brief Stable observer identifier used by diagnostics and debug output. */
        std::string Id;
        /** @brief Inclusive start address of the bounded scan range. */
        std::uintptr_t ScanStartAddress = 0;
        /** @brief Exclusive end address of the bounded scan range. */
        std::uintptr_t ScanEndAddress = 0;
        /** @brief Byte stride applied when scanning for a candidate state block. */
        std::size_t ScanStride = 0;
        /** @brief Signed byte offset, relative to each candidate base address, that contains the observed raw integer value. */
        int ValueOffset = 0;
        /** @brief Poll interval used by the background observer thread when it is active. */
        int PollIntervalMs = 0;
        /** @brief Config key that receives the mapped observer value after a change is detected. */
        std::string TargetConfigKey;
        /** @brief Optional follow-up command executed after the config key is updated. */
        std::optional<std::string> CommandId;
        /** @brief Validation checks that a candidate state block must satisfy before it is accepted. */
        std::vector<MemoryStateObserverCheckDefinition> Checks;
        /** @brief Optional compatible observer address reuse identifier that lets related observers share a resolved carrier address. */
        std::optional<std::string> AddressGroup;
        /** @brief Raw values that identify this observer's carrier address; an empty list preserves legacy mapping-based recognition. */
        std::vector<int> AddressMatchValues;
        /** @brief Provider identifier used by a response-only observer to obtain dynamic scalar responses. */
        std::optional<std::string> DynamicResponseProviderId;
        /** @brief Positive raw request values that a response-only observer may answer dynamically. */
        std::vector<int> DynamicResponseRequestValues;
        /** @brief Inclusive lower bound for scalar values returned by a response-only observer provider. */
        int DynamicResponseMinimumValue = 0;
        /** @brief Inclusive upper bound for scalar values returned by a response-only observer provider. */
        int DynamicResponseMaximumValue = 0;
        /** @brief Raw-to-config mappings that translate observed state codes into Helen-owned config values. */
        std::vector<MemoryStateObserverMapEntryDefinition> Mappings;
        /** @brief Raw request-to-success-response mappings whose response is written only after the associated config update and optional follow-up command both succeed. */
        std::vector<MemoryStateObserverMapEntryDefinition> AcknowledgementMappings;
        /** @brief Raw response value written when the associated config or command update fails. */
        std::optional<int> FailureResponseValue;
        /** @brief Optional raw request code that asks HelenHook to answer through the resolved carrier value address. */
        std::optional<int> ResponseRequestValue;
        /** @brief Config-to-raw mappings used to encode a requested current config value back into process memory. */
        std::vector<MemoryStateObserverMapEntryDefinition> ResponseMappings;
    };
}
