#include <HelenHook/MemoryStateObserverService.h>

#include <HelenHook/Log.h>

#include <Windows.h>

#include <algorithm>
#include <chrono>
#include <cstring>
#include <exception>
#include <limits>
#include <optional>

namespace
{
    /**
     * @brief Returns whether one committed memory page protection flag allows plain read access.
     * @param protection Raw Win32 page protection flags reported by VirtualQuery.
     * @return True when the page can be read safely; otherwise false.
     */
    bool IsReadableProtection(DWORD protection)
    {
        if ((protection & PAGE_GUARD) != 0 || (protection & PAGE_NOACCESS) != 0)
        {
            return false;
        }

        const DWORD readable_mask =
            PAGE_READONLY |
            PAGE_READWRITE |
            PAGE_WRITECOPY |
            PAGE_EXECUTE_READ |
            PAGE_EXECUTE_READWRITE |
            PAGE_EXECUTE_WRITECOPY;
        return (protection & readable_mask) != 0;
    }

    /**
     * @brief Adds one signed byte offset to an address while rejecting overflow and underflow.
     * @param base Base address that should be adjusted.
     * @param offset Signed byte offset relative to base.
     * @param adjusted_address Receives the adjusted address when the addition is valid.
     * @return True when the adjusted address is representable; otherwise false.
     */
    bool TryApplyOffset(std::uintptr_t base, int offset, std::uintptr_t& adjusted_address)
    {
        if (offset >= 0)
        {
            const std::uintptr_t unsigned_offset = static_cast<std::uintptr_t>(offset);
            if (base > (std::numeric_limits<std::uintptr_t>::max)() - unsigned_offset)
            {
                return false;
            }

            adjusted_address = base + unsigned_offset;
            return true;
        }

        const std::uintptr_t unsigned_offset = static_cast<std::uintptr_t>(-static_cast<long long>(offset));
        if (base < unsigned_offset)
        {
            return false;
        }

        adjusted_address = base - unsigned_offset;
        return true;
    }

    /**
     * @brief Returns the smallest byte offset read by one observer definition.
     * @param definition Observer definition whose read footprint should be summarized.
     * @return Smallest signed byte offset read by the observer.
     */
    int GetMinimumReadOffset(const helen::MemoryStateObserverDefinition& definition)
    {
        int minimum_offset = definition.ValueOffset;
        for (const helen::MemoryStateObserverCheckDefinition& check : definition.Checks)
        {
            minimum_offset = (std::min)(minimum_offset, check.Offset);
            if (check.CompareOffset.has_value())
            {
                minimum_offset = (std::min)(minimum_offset, *check.CompareOffset);
            }
        }

        return minimum_offset;
    }

    /**
     * @brief Returns the largest byte offset read by one observer definition.
     * @param definition Observer definition whose read footprint should be summarized.
     * @return Largest signed byte offset read by the observer.
     */
    int GetMaximumReadOffset(const helen::MemoryStateObserverDefinition& definition)
    {
        int maximum_offset = definition.ValueOffset;
        for (const helen::MemoryStateObserverCheckDefinition& check : definition.Checks)
        {
            maximum_offset = (std::max)(maximum_offset, check.Offset);
            if (check.CompareOffset.has_value())
            {
                maximum_offset = (std::max)(maximum_offset, *check.CompareOffset);
            }
        }

        return maximum_offset;
    }

    /**
     * @brief Attempts to read one signed 32-bit integer from the current process without allowing access violations to escape.
     * @param address Address whose four bytes should be copied.
     * @param value Receives the copied integer when the read succeeds.
     * @return True when the address remained readable for the full copy; otherwise false.
     */
    bool TryReadInt32(std::uintptr_t address, int& value) noexcept
    {
        __try
        {
            std::memcpy(&value, reinterpret_cast<const void*>(address), sizeof(value));
            return true;
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            value = 0;
            return false;
        }
    }

    /**
     * @brief Attempts to write one signed 32-bit integer into current-process memory without leaking access violations.
     * @param address Address whose four bytes should receive the supplied value.
     * @param value Integer value that should be copied.
     * @return True when the write completes; otherwise false.
     */
    bool TryWriteInt32(std::uintptr_t address, int value) noexcept
    {
        __try
        {
            std::memcpy(reinterpret_cast<void*>(address), &value, sizeof(value));
            return true;
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            return false;
        }
    }

    /**
     * @brief Evaluates only the structural checks that identify one observer's state block.
     * @param definition Observer definition whose checks should be evaluated.
     * @param base_address Candidate base address to validate.
     * @return True when every declared check passes; otherwise false.
     * @remarks Mapping tables intentionally do not participate here. A structurally valid carrier remains cached while another subsystem temporarily owns its raw control code; mappings only decide whether an update is emitted.
     */
    bool MatchesObserverChecks(const helen::MemoryStateObserverDefinition& definition, std::uintptr_t base_address) noexcept
    {
        for (const helen::MemoryStateObserverCheckDefinition& check : definition.Checks)
        {
            std::uintptr_t check_address = 0;
            if (!TryApplyOffset(base_address, check.Offset, check_address))
            {
                return false;
            }

            int check_value = 0;
            if (!TryReadInt32(check_address, check_value))
            {
                return false;
            }

            if (check.Comparison == "equals-constant")
            {
                if (!check.ExpectedValue.has_value() || check_value != *check.ExpectedValue)
                {
                    return false;
                }
            }
            else if (check.Comparison == "equals-value-at-offset")
            {
                std::uintptr_t compare_address = 0;
                int compare_value = 0;
                if (!check.CompareOffset.has_value()
                    || !TryApplyOffset(base_address, *check.CompareOffset, compare_address)
                    || !TryReadInt32(compare_address, compare_value)
                    || check_value != compare_value)
                {
                    return false;
                }
            }
            else
            {
                return false;
            }
        }

        return true;
    }

    /**
     * @brief Maps one raw observed value through the observer's declarative integer table.
     * @param definition Observer definition whose mappings should be evaluated.
     * @param raw_value Raw observed integer value.
     * @return Mapped integer value when the observer knows this raw value; otherwise no value.
     */
    std::optional<int> TryMapObservedValue(const helen::MemoryStateObserverDefinition& definition, int raw_value)
    {
        for (const helen::MemoryStateObserverMapEntryDefinition& mapping : definition.Mappings)
        {
            if (mapping.Match == raw_value)
            {
                return mapping.Value;
            }
        }

        return std::nullopt;
    }

    /**
     * @brief Maps one current config value into the raw response code declared by an observer.
     * @param definition Observer whose config-to-raw response mappings should be evaluated.
     * @param config_value Current integer value read from the observer's target config key.
     * @return Raw response code when the config value has a declared mapping; otherwise no value.
     */
    std::optional<int> TryMapResponseValue(const helen::MemoryStateObserverDefinition& definition, int config_value)
    {
        for (const helen::MemoryStateObserverMapEntryDefinition& mapping : definition.ResponseMappings)
        {
            if (mapping.Match == config_value)
            {
                return mapping.Value;
            }
        }

        return std::nullopt;
    }

    /**
     * @brief Maps one transactional observer raw request to its declared native success acknowledgement code.
     * @param definition Observer definition whose acknowledgement mappings should be searched.
     * @param raw_request_value Raw request value carried by the emitted transaction update.
     * @return Native success acknowledgement code when the raw request has a declared mapping; otherwise no value.
     */
    std::optional<int> TryMapAcknowledgementValue(const helen::MemoryStateObserverDefinition& definition, int raw_request_value)
    {
        for (const helen::MemoryStateObserverMapEntryDefinition& mapping : definition.AcknowledgementMappings)
        {
            if (mapping.Match == raw_request_value)
            {
                return mapping.Value;
            }
        }

        return std::nullopt;
    }

    /**
     * @brief Returns whether one raw value is eligible to identify an observer's carrier address.
     * @param definition Observer definition whose explicit or legacy recognition values should be used.
     * @param raw_value Raw integer read from a candidate value offset.
     * @return True when the raw value is an explicit address match or a legacy mapping match.
     * @remarks Explicit AddressMatchValues intentionally separate address recognition from update mappings. Definitions created before that field existed retain mapping-based recognition when the explicit list is empty.
     */
    bool IsAddressMatchValue(const helen::MemoryStateObserverDefinition& definition, int raw_value) noexcept
    {
        if (!definition.AddressMatchValues.empty())
        {
            return std::find(definition.AddressMatchValues.begin(), definition.AddressMatchValues.end(), raw_value) != definition.AddressMatchValues.end();
        }

        return TryMapObservedValue(definition, raw_value).has_value();
    }

    /**
     * @brief Returns the smallest positive poll interval declared by the active observer set.
     * @param definitions Observers whose timed poll intervals should be examined.
     * @return Smallest declared poll interval in milliseconds.
     */
    int GetMinimumPollInterval(const std::vector<helen::MemoryStateObserverDefinition>& definitions)
    {
        int minimum_interval = 0;
        for (const helen::MemoryStateObserverDefinition& definition : definitions)
        {
            if (minimum_interval == 0 || definition.PollIntervalMs < minimum_interval)
            {
                minimum_interval = definition.PollIntervalMs;
            }
        }

        return minimum_interval;
    }
}

namespace helen
{
    /**
     * @brief Returns whether the supplied address range is fully readable in the current process.
     * @param address Range start address that should be validated.
     * @param byte_count Number of bytes that must be readable from address.
     * @return True when the full range is readable; otherwise false.
     */
    bool IsReadableAddressRange(std::uintptr_t address, std::size_t byte_count)
    {
        if (byte_count == 0)
        {
            return true;
        }

        if (address > (std::numeric_limits<std::uintptr_t>::max)() - (byte_count - 1))
        {
            return false;
        }

        std::uintptr_t current = address;
        const std::uintptr_t end = address + byte_count;
        while (current < end)
        {
            MEMORY_BASIC_INFORMATION memory_info{};
            if (VirtualQuery(reinterpret_cast<const void*>(current), &memory_info, sizeof(memory_info)) == 0)
            {
                return false;
            }

            if (memory_info.State != MEM_COMMIT || !IsReadableProtection(memory_info.Protect))
            {
                return false;
            }

            const std::uintptr_t region_start = reinterpret_cast<std::uintptr_t>(memory_info.BaseAddress);
            const std::uintptr_t region_end = region_start + static_cast<std::uintptr_t>(memory_info.RegionSize);
            if (region_end <= current)
            {
                return false;
            }

            current = (std::min)(region_end, end);
        }

        return true;
    }

    MemoryStateObserverService::MemoryStateObserverService(
        std::vector<MemoryStateObserverDefinition> definitions,
        UpdateCallback update_callback,
        ConfigValueCallback config_value_callback)
        : definitions_(std::move(definitions)),
          update_callback_(std::move(update_callback)),
          config_value_callback_(std::move(config_value_callback))
    {
        debug_views_.reserve(definitions_.size());
        last_poll_ticks_.assign(definitions_.size(), 0);
        pending_transaction_requests_.assign(definitions_.size(), std::nullopt);
        pending_transaction_addresses_.assign(definitions_.size(), std::nullopt);

        for (const MemoryStateObserverDefinition& definition : definitions_)
        {
            MemoryStateObserverDebugView debug_view;
            debug_view.Id = definition.Id;
            debug_views_.push_back(std::move(debug_view));
        }
    }

    MemoryStateObserverService::~MemoryStateObserverService()
    {
        Stop();
    }

    bool MemoryStateObserverService::Start()
    {
        std::thread completed_worker;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (running_ || definitions_.empty())
            {
                if (definitions_.empty())
                {
                    Logf(L"[observer] start skipped because no observers were declared.");
                }
                return true;
            }

            completed_worker = std::move(worker_thread_);
        }

        if (completed_worker.joinable())
        {
            completed_worker.join();
        }

        std::lock_guard<std::mutex> lock(mutex_);
        if (running_)
        {
            return true;
        }

        stop_requested_ = false;
        running_ = true;
        Logf(L"[observer] starting service with %zu observer(s).", definitions_.size());
        worker_thread_ = std::thread(&MemoryStateObserverService::RunWorkerLoop, this);
        return true;
    }

    void MemoryStateObserverService::Stop()
    {
        std::thread worker_to_join;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (running_)
            {
                stop_requested_ = true;
            }

            worker_to_join = std::move(worker_thread_);
        }

        stop_condition_.notify_all();
        if (worker_to_join.joinable())
        {
            worker_to_join.join();
        }
    }

    bool MemoryStateObserverService::PollOnce()
    {
        std::lock_guard<std::mutex> poll_lock(poll_mutex_);
        std::unordered_set<std::string> scanned_address_groups;
        for (std::size_t observer_index = 0; observer_index < definitions_.size(); ++observer_index)
        {
            if (!PollObserver(observer_index, scanned_address_groups))
            {
                return false;
            }
        }

        return true;
    }

    std::vector<MemoryStateObserverDebugView> MemoryStateObserverService::GetDebugViews() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return debug_views_;
    }

    void MemoryStateObserverService::RunWorkerLoop()
    {
        const int minimum_interval = GetMinimumPollInterval(definitions_);
        while (true)
        {
            if (!PollDueObservers())
            {
                break;
            }

            std::unique_lock<std::mutex> lock(mutex_);
            if (stop_requested_)
            {
                break;
            }

            stop_condition_.wait_for(
                lock,
                std::chrono::milliseconds(minimum_interval),
                [this]()
                {
                    return stop_requested_;
                });

            if (stop_requested_)
            {
                break;
            }
        }

        std::lock_guard<std::mutex> lock(mutex_);
        running_ = false;
        stop_requested_ = false;
    }

    bool MemoryStateObserverService::PollDueObservers()
    {
        std::lock_guard<std::mutex> poll_lock(poll_mutex_);
        const std::uint64_t now = GetTickCount64();
        std::unordered_set<std::string> scanned_address_groups;
        for (std::size_t observer_index = 0; observer_index < definitions_.size(); ++observer_index)
        {
            std::uint64_t last_poll_tick = 0;
            int poll_interval_ms = 0;
            {
                std::lock_guard<std::mutex> lock(mutex_);
                last_poll_tick = last_poll_ticks_[observer_index];
                poll_interval_ms = definitions_[observer_index].PollIntervalMs;
            }

            if (last_poll_tick != 0 && now - last_poll_tick < static_cast<std::uint64_t>(poll_interval_ms))
            {
                continue;
            }

            {
                std::lock_guard<std::mutex> lock(mutex_);
                last_poll_ticks_[observer_index] = now;
            }

            if (!PollObserver(observer_index, scanned_address_groups))
            {
                return false;
            }
        }

        return true;
    }

    std::uintptr_t MemoryStateObserverService::GetCachedAddress(std::size_t observer_index) const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        const std::optional<std::string>& address_group = definitions_[observer_index].AddressGroup;
        if (address_group.has_value())
        {
            const auto grouped_address = grouped_addresses_.find(*address_group);
            if (grouped_address != grouped_addresses_.end())
            {
                return grouped_address->second;
            }

            return 0;
        }

        return debug_views_[observer_index].CachedAddress;
    }

    void MemoryStateObserverService::CacheResolvedAddress(std::size_t observer_index, std::uintptr_t address)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        const std::optional<std::string>& address_group = definitions_[observer_index].AddressGroup;
        if (!address_group.has_value())
        {
            if (debug_views_[observer_index].CachedAddress != 0 && debug_views_[observer_index].CachedAddress != address)
            {
                pending_transaction_requests_[observer_index].reset();
                pending_transaction_addresses_[observer_index].reset();
            }

            debug_views_[observer_index].CachedAddress = address;
            return;
        }

        const auto previous_grouped_address = grouped_addresses_.find(*address_group);
        if (previous_grouped_address != grouped_addresses_.end() && previous_grouped_address->second != address)
        {
            for (std::size_t matching_index = 0; matching_index < definitions_.size(); ++matching_index)
            {
                if (definitions_[matching_index].AddressGroup == address_group)
                {
                    pending_transaction_requests_[matching_index].reset();
                    pending_transaction_addresses_[matching_index].reset();
                }
            }
        }

        grouped_addresses_[*address_group] = address;
        for (std::size_t matching_index = 0; matching_index < definitions_.size(); ++matching_index)
        {
            if (definitions_[matching_index].AddressGroup == address_group)
            {
                debug_views_[matching_index].CachedAddress = address;
            }
        }
    }

    void MemoryStateObserverService::ClearCachedAddress(std::size_t observer_index)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        const std::optional<std::string>& address_group = definitions_[observer_index].AddressGroup;
        if (!address_group.has_value())
        {
            debug_views_[observer_index].CachedAddress = 0;
            pending_transaction_requests_[observer_index].reset();
            pending_transaction_addresses_[observer_index].reset();
            return;
        }

        grouped_addresses_.erase(*address_group);
        for (std::size_t matching_index = 0; matching_index < definitions_.size(); ++matching_index)
        {
            if (definitions_[matching_index].AddressGroup == address_group)
            {
                debug_views_[matching_index].CachedAddress = 0;
                pending_transaction_requests_[matching_index].reset();
                pending_transaction_addresses_[matching_index].reset();
            }
        }
    }

    void MemoryStateObserverService::ReconcilePendingTransaction(
        std::size_t observer_index,
        const std::optional<std::uintptr_t>& resolved_address,
        const std::optional<int>& raw_value)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        const bool pending_pair_present =
            pending_transaction_requests_[observer_index].has_value() ||
            pending_transaction_addresses_[observer_index].has_value();
        const bool current_pair_matches =
            resolved_address.has_value() &&
            raw_value.has_value() &&
            pending_transaction_requests_[observer_index].has_value() &&
            pending_transaction_addresses_[observer_index].has_value() &&
            *pending_transaction_requests_[observer_index] == *raw_value &&
            *pending_transaction_addresses_[observer_index] == *resolved_address;
        if (pending_pair_present && !current_pair_matches)
        {
            pending_transaction_requests_[observer_index].reset();
            pending_transaction_addresses_[observer_index].reset();
        }
    }

    bool MemoryStateObserverService::WriteReadFailureResponse(
        std::size_t observer_index,
        const MemoryStateObserverDefinition& definition,
        std::uintptr_t resolved_address,
        int raw_value,
        const char* reason)
    {
        if (!definition.FailureResponseValue.has_value())
        {
            Logf(
                L"[observer] response failed id=%hs key=%hs raw=%d reason=%hs-failure-response-missing",
                definition.Id.c_str(),
                definition.TargetConfigKey.c_str(),
                raw_value,
                reason);
            return false;
        }

        std::uintptr_t response_address = 0;
        if (!TryApplyOffset(resolved_address, definition.ValueOffset, response_address))
        {
            Logf(
                L"[observer] response failed id=%hs key=%hs raw=%d response=%d reason=%hs-address-overflow",
                definition.Id.c_str(),
                definition.TargetConfigKey.c_str(),
                raw_value,
                *definition.FailureResponseValue,
                reason);
            return false;
        }

        if (!TryWriteInt32(response_address, *definition.FailureResponseValue))
        {
            Logf(
                L"[observer] response failed id=%hs key=%hs raw=%d response=%d address=0x%08llX reason=%hs-write-failed",
                definition.Id.c_str(),
                definition.TargetConfigKey.c_str(),
                raw_value,
                *definition.FailureResponseValue,
                static_cast<unsigned long long>(response_address),
                reason);
            return false;
        }

        {
            std::lock_guard<std::mutex> lock(mutex_);
            debug_views_[observer_index].LastRawValue = definition.FailureResponseValue;
            pending_transaction_requests_[observer_index].reset();
            pending_transaction_addresses_[observer_index].reset();
        }

        Logf(
            L"[observer] response failed handled id=%hs key=%hs raw=%d response=%d address=0x%08llX reason=%hs",
            definition.Id.c_str(),
            definition.TargetConfigKey.c_str(),
            raw_value,
            *definition.FailureResponseValue,
            static_cast<unsigned long long>(response_address),
            reason);
        return true;
    }

    bool MemoryStateObserverService::PollObserver(
        std::size_t observer_index,
        std::unordered_set<std::string>& scanned_address_groups)
    {
        const MemoryStateObserverDefinition& definition = definitions_[observer_index];
        const std::uintptr_t cached_address = GetCachedAddress(observer_index);
        const bool is_transactional =
            !definition.AcknowledgementMappings.empty() && definition.FailureResponseValue.has_value();
        std::optional<int> previous_mapped_value;
        std::uint64_t previous_rescan_count = 0;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            previous_mapped_value = debug_views_[observer_index].LastMappedValue;
            previous_rescan_count = debug_views_[observer_index].RescanCount;
        }

        const int minimum_offset = GetMinimumReadOffset(definition);
        const int maximum_offset = GetMaximumReadOffset(definition);

        std::optional<std::uintptr_t> resolved_address;
        std::optional<int> raw_value;
        std::optional<int> mapped_value;

        if (cached_address != 0)
        {
            std::uintptr_t minimum_address = 0;
            std::uintptr_t maximum_address = 0;
            if (TryApplyOffset(cached_address, minimum_offset, minimum_address)
                && TryApplyOffset(cached_address, maximum_offset, maximum_address)
                && maximum_address <= (std::numeric_limits<std::uintptr_t>::max)() - sizeof(int)
                && IsReadableAddressRange(minimum_address, (maximum_address - minimum_address) + sizeof(int)))
            {
                std::uintptr_t value_address = 0;
                if (TryApplyOffset(cached_address, definition.ValueOffset, value_address))
                {
                    int cached_raw_value = 0;
                    if (TryReadInt32(value_address, cached_raw_value)
                        && IsAddressMatchValue(definition, cached_raw_value)
                        && MatchesObserverChecks(definition, cached_address))
                    {
                        resolved_address = cached_address;
                        raw_value = cached_raw_value;
                        mapped_value = TryMapObservedValue(definition, cached_raw_value);
                    }
                }
            }
        }

        if (!resolved_address.has_value())
        {
            if (cached_address != 0)
            {
                ClearCachedAddress(observer_index);
            }

            bool may_rescan = true;
            if (definition.AddressGroup.has_value())
            {
                may_rescan = scanned_address_groups.insert(*definition.AddressGroup).second;
            }

            if (may_rescan)
            {
                {
                    std::lock_guard<std::mutex> lock(mutex_);
                    ++debug_views_[observer_index].RescanCount;
                }

                std::uintptr_t region_cursor = definition.ScanStartAddress;
                while (region_cursor < definition.ScanEndAddress && !resolved_address.has_value())
                {
                    MEMORY_BASIC_INFORMATION memory_info{};
                    if (VirtualQuery(reinterpret_cast<const void*>(region_cursor), &memory_info, sizeof(memory_info)) == 0)
                    {
                        break;
                    }

                    const std::uintptr_t region_start = reinterpret_cast<std::uintptr_t>(memory_info.BaseAddress);
                    const std::uintptr_t region_end = region_start + static_cast<std::uintptr_t>(memory_info.RegionSize);
                    if (region_end <= region_cursor)
                    {
                        break;
                    }

                    const std::uintptr_t scan_region_start = (std::max)(region_cursor, region_start);
                    const std::uintptr_t scan_region_end = (std::min)(definition.ScanEndAddress, region_end);
                    region_cursor = scan_region_end;

                    if (memory_info.State != MEM_COMMIT || !IsReadableProtection(memory_info.Protect))
                    {
                        continue;
                    }

                    std::uintptr_t first_candidate = scan_region_start;
                    if (minimum_offset < 0)
                    {
                        const std::uintptr_t minimum_candidate = scan_region_start + static_cast<std::uintptr_t>(-minimum_offset);
                        first_candidate = (std::max)(first_candidate, minimum_candidate);
                    }

                    std::uintptr_t last_candidate_exclusive = scan_region_end;
                    if (maximum_offset >= 0)
                    {
                        const std::uintptr_t required_trailing_bytes = static_cast<std::uintptr_t>(maximum_offset) + sizeof(int);
                        if (scan_region_end < required_trailing_bytes)
                        {
                            continue;
                        }

                        last_candidate_exclusive = scan_region_end - required_trailing_bytes + 1;
                    }

                    if (first_candidate >= last_candidate_exclusive)
                    {
                        continue;
                    }

                    const std::uintptr_t stride = static_cast<std::uintptr_t>(definition.ScanStride);
                    for (std::uintptr_t candidate = first_candidate; candidate < last_candidate_exclusive; candidate += stride)
                    {
                        std::uintptr_t value_address = 0;
                        if (!TryApplyOffset(candidate, definition.ValueOffset, value_address))
                        {
                            continue;
                        }

                        int candidate_raw_value = 0;
                        if (!TryReadInt32(value_address, candidate_raw_value))
                        {
                            continue;
                        }

                        if (IsAddressMatchValue(definition, candidate_raw_value)
                            && MatchesObserverChecks(definition, candidate))
                        {
                            resolved_address = candidate;
                            raw_value = candidate_raw_value;
                            mapped_value = TryMapObservedValue(definition, candidate_raw_value);
                            break;
                        }
                    }
                }
            }
        }

        ReconcilePendingTransaction(observer_index, resolved_address, raw_value);

        bool response_written = false;
        if (resolved_address.has_value() &&
            raw_value.has_value() &&
            definition.ResponseRequestValue.has_value() &&
            *raw_value == *definition.ResponseRequestValue)
        {
            if (!config_value_callback_)
            {
                Logf(L"[observer] response failed id=%hs reason=config-callback-missing", definition.Id.c_str());
                return false;
            }

            std::optional<int> config_value;
            try
            {
                config_value = config_value_callback_(definition.TargetConfigKey);
            }
            catch (const std::exception& exception)
            {
                Logf(
                    L"[observer] response failed id=%hs key=%hs raw=%d reason=config-callback-exception message=%hs",
                    definition.Id.c_str(),
                    definition.TargetConfigKey.c_str(),
                    *raw_value,
                    exception.what());
                return WriteReadFailureResponse(
                    observer_index,
                    definition,
                    *resolved_address,
                    *raw_value,
                    "config-callback-exception");
            }
            catch (...)
            {
                Logf(
                    L"[observer] response failed id=%hs key=%hs raw=%d reason=config-callback-exception-unknown",
                    definition.Id.c_str(),
                    definition.TargetConfigKey.c_str(),
                    *raw_value);
                return WriteReadFailureResponse(
                    observer_index,
                    definition,
                    *resolved_address,
                    *raw_value,
                    "config-callback-exception-unknown");
            }

            if (!config_value.has_value())
            {
                Logf(
                    L"[observer] response failed id=%hs key=%hs reason=config-value-missing",
                    definition.Id.c_str(),
                    definition.TargetConfigKey.c_str());
                return false;
            }

            const std::optional<int> response_value = TryMapResponseValue(definition, *config_value);
            std::uintptr_t response_address = 0;
            if (!response_value.has_value() ||
                !TryApplyOffset(*resolved_address, definition.ValueOffset, response_address) ||
                !TryWriteInt32(response_address, *response_value))
            {
                Logf(
                    L"[observer] response failed id=%hs key=%hs config=%d reason=write-or-mapping-failed",
                    definition.Id.c_str(),
                    definition.TargetConfigKey.c_str(),
                    *config_value);
                return false;
            }

            raw_value = response_value;
            mapped_value = std::nullopt;
            response_written = true;
            Logf(
                L"[observer] response id=%hs key=%hs config=%d raw=%d address=0x%08llX",
                definition.Id.c_str(),
                definition.TargetConfigKey.c_str(),
                *config_value,
                *response_value,
                static_cast<unsigned long long>(response_address));
        }

        if (resolved_address.has_value())
        {
            CacheResolvedAddress(observer_index, *resolved_address);
        }

        std::optional<MemoryStateObserverUpdate> update;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            MemoryStateObserverDebugView& debug_view = debug_views_[observer_index];
            debug_view.CachedAddress = resolved_address.value_or(0);

            if (mapped_value.has_value() && !response_written)
            {
                bool should_emit_update = false;
                if (is_transactional)
                {
                    const bool same_pending_request =
                        pending_transaction_requests_[observer_index].has_value() &&
                        pending_transaction_addresses_[observer_index].has_value() &&
                        raw_value.has_value() &&
                        *pending_transaction_requests_[observer_index] == *raw_value &&
                        *pending_transaction_addresses_[observer_index] == *resolved_address;
                    if (!same_pending_request)
                    {
                        pending_transaction_requests_[observer_index] = raw_value;
                        pending_transaction_addresses_[observer_index] = resolved_address;
                        should_emit_update = true;
                    }
                }
                else if (!previous_mapped_value.has_value() || *previous_mapped_value != *mapped_value)
                {
                    should_emit_update = true;
                }

                if (should_emit_update)
                {
                    debug_view.LastMappedValue = mapped_value;
                    ++debug_view.UpdateCount;

                    MemoryStateObserverUpdate emitted_update;
                    emitted_update.ObserverId = definition.Id;
                    emitted_update.ConfigKey = definition.TargetConfigKey;
                    emitted_update.RawValue = *raw_value;
                    emitted_update.MappedValue = *mapped_value;
                    emitted_update.CommandId = definition.CommandId;
                    update = std::move(emitted_update);
                }
            }

            if (raw_value.has_value() && (!is_transactional || !mapped_value.has_value() || response_written))
            {
                debug_view.LastRawValue = raw_value;
            }
        }

        if (resolved_address.has_value())
        {
            if (cached_address == 0 || cached_address != *resolved_address)
            {
                Logf(
                    L"[observer] resolved id=%hs address=0x%08llX raw=%d mapped=%d",
                    definition.Id.c_str(),
                    static_cast<unsigned long long>(*resolved_address),
                    raw_value.value_or(0),
                    mapped_value.value_or(0));
            }
        }
        else if (cached_address != 0)
        {
            Logf(
                L"[observer] unresolved id=%hs previousAddress=0x%08llX",
                definition.Id.c_str(),
                static_cast<unsigned long long>(cached_address));
        }
        else if (previous_rescan_count == 0)
        {
            Logf(
                L"[observer] initial scan found no match for id=%hs range=0x%08llX..0x%08llX stride=%d valueOffset=%d",
                definition.Id.c_str(),
                static_cast<unsigned long long>(definition.ScanStartAddress),
                static_cast<unsigned long long>(definition.ScanEndAddress),
                definition.ScanStride,
                definition.ValueOffset);
        }

        if (!update.has_value())
        {
            return true;
        }

        bool update_succeeded = true;
        if (update_callback_)
        {
            try
            {
                update_succeeded = update_callback_(*update);
            }
            catch (const std::exception& exception)
            {
                Logf(
                    L"[observer] update failed id=%hs raw=%d reason=callback-exception message=%hs",
                    definition.Id.c_str(),
                    update->RawValue,
                    exception.what());
                update_succeeded = false;
            }
            catch (...)
            {
                Logf(
                    L"[observer] update failed id=%hs raw=%d reason=callback-exception-unknown",
                    definition.Id.c_str(),
                    update->RawValue);
                update_succeeded = false;
            }
        }
        else if (is_transactional)
        {
            Logf(
                L"[observer] update failed id=%hs raw=%d reason=callback-missing",
                definition.Id.c_str(),
                update->RawValue);
            update_succeeded = false;
        }

        if (!is_transactional)
        {
            return update_succeeded;
        }

        std::optional<int> response_value;
        if (update_succeeded)
        {
            response_value = TryMapAcknowledgementValue(definition, update->RawValue);
            if (!response_value.has_value())
            {
                Logf(
                    L"[observer] acknowledgement failed id=%hs raw=%d result=1 reason=success-mapping-missing",
                    definition.Id.c_str(),
                    update->RawValue);
                return false;
            }
        }
        else
        {
            response_value = definition.FailureResponseValue;
        }

        std::uintptr_t response_address = 0;
        if (!TryApplyOffset(*resolved_address, definition.ValueOffset, response_address))
        {
            Logf(
                L"[observer] acknowledgement failed id=%hs raw=%d result=%d reason=address-overflow",
                definition.Id.c_str(),
                update->RawValue,
                static_cast<int>(update_succeeded));
            return false;
        }

        if (!response_value.has_value() || !TryWriteInt32(response_address, *response_value))
        {
            Logf(
                L"[observer] acknowledgement failed id=%hs raw=%d result=%d response=%d address=0x%08llX reason=write-failed",
                definition.Id.c_str(),
                update->RawValue,
                static_cast<int>(update_succeeded),
                response_value.value_or(0),
                static_cast<unsigned long long>(response_address));
            return false;
        }

        {
            std::lock_guard<std::mutex> lock(mutex_);
            debug_views_[observer_index].LastRawValue = response_value;
            pending_transaction_requests_[observer_index].reset();
            pending_transaction_addresses_[observer_index].reset();
        }

        Logf(
            L"[observer] acknowledgement id=%hs raw=%d result=%d response=%d address=0x%08llX",
            definition.Id.c_str(),
            update->RawValue,
            static_cast<int>(update_succeeded),
            *response_value,
            static_cast<unsigned long long>(response_address));

        return true;
    }
}
