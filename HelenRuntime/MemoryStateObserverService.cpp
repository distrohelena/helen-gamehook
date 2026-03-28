#include <HelenHook/MemoryStateObserverService.h>

#include <Windows.h>

#include <algorithm>
#include <chrono>
#include <cstring>
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
        UpdateCallback update_callback)
        : definitions_(std::move(definitions)),
          update_callback_(std::move(update_callback))
    {
        debug_views_.reserve(definitions_.size());
        last_poll_ticks_.assign(definitions_.size(), 0);

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
        std::lock_guard<std::mutex> lock(mutex_);
        if (running_ || definitions_.empty())
        {
            return true;
        }

        stop_requested_ = false;
        running_ = true;
        worker_thread_ = std::thread(&MemoryStateObserverService::RunWorkerLoop, this);
        return true;
    }

    void MemoryStateObserverService::Stop()
    {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (!running_)
            {
                return;
            }

            stop_requested_ = true;
        }

        stop_condition_.notify_all();
        if (worker_thread_.joinable())
        {
            worker_thread_.join();
        }
    }

    bool MemoryStateObserverService::PollOnce()
    {
        for (std::size_t observer_index = 0; observer_index < definitions_.size(); ++observer_index)
        {
            if (!PollObserver(observer_index))
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
        const std::uint64_t now = GetTickCount64();
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

            if (!PollObserver(observer_index))
            {
                return false;
            }
        }

        return true;
    }

    bool MemoryStateObserverService::PollObserver(std::size_t observer_index)
    {
        const MemoryStateObserverDefinition& definition = definitions_[observer_index];
        std::uintptr_t cached_address = 0;
        std::optional<int> previous_mapped_value;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            cached_address = debug_views_[observer_index].CachedAddress;
            previous_mapped_value = debug_views_[observer_index].LastMappedValue;
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
                    if (!TryReadInt32(value_address, cached_raw_value))
                    {
                        cached_raw_value = 0;
                    }

                    const std::optional<int> cached_mapped_value = TryMapObservedValue(definition, cached_raw_value);
                    if (cached_mapped_value.has_value())
                    {
                        bool matches = true;
                        for (const MemoryStateObserverCheckDefinition& check : definition.Checks)
                        {
                            std::uintptr_t check_address = 0;
                            if (!TryApplyOffset(cached_address, check.Offset, check_address))
                            {
                                matches = false;
                                break;
                            }

                            int check_value = 0;
                            if (!TryReadInt32(check_address, check_value))
                            {
                                matches = false;
                                break;
                            }

                            if (check.Comparison == "equals-constant")
                            {
                                if (!check.ExpectedValue.has_value() || check_value != *check.ExpectedValue)
                                {
                                    matches = false;
                                    break;
                                }
                            }
                            else if (check.Comparison == "equals-value-at-offset")
                            {
                                std::uintptr_t compare_address = 0;
                                int compare_value = 0;
                                if (!check.CompareOffset.has_value()
                                    || !TryApplyOffset(cached_address, *check.CompareOffset, compare_address)
                                    || !TryReadInt32(compare_address, compare_value)
                                    || check_value != compare_value)
                                {
                                    matches = false;
                                    break;
                                }
                            }
                            else
                            {
                                matches = false;
                                break;
                            }
                        }

                        if (matches)
                        {
                            resolved_address = cached_address;
                            raw_value = cached_raw_value;
                            mapped_value = cached_mapped_value;
                        }
                    }
                }
            }
        }

        if (!resolved_address.has_value())
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

                    const std::optional<int> candidate_mapped_value = TryMapObservedValue(definition, candidate_raw_value);
                    if (!candidate_mapped_value.has_value())
                    {
                        continue;
                    }

                    bool matches = true;
                    for (const MemoryStateObserverCheckDefinition& check : definition.Checks)
                    {
                        std::uintptr_t check_address = 0;
                        if (!TryApplyOffset(candidate, check.Offset, check_address))
                        {
                            matches = false;
                            break;
                        }

                        int check_value = 0;
                        if (!TryReadInt32(check_address, check_value))
                        {
                            matches = false;
                            break;
                        }

                        if (check.Comparison == "equals-constant")
                        {
                            if (!check.ExpectedValue.has_value() || check_value != *check.ExpectedValue)
                            {
                                matches = false;
                                break;
                            }
                        }
                        else if (check.Comparison == "equals-value-at-offset")
                        {
                            std::uintptr_t compare_address = 0;
                            int compare_value = 0;
                            if (!check.CompareOffset.has_value()
                                || !TryApplyOffset(candidate, *check.CompareOffset, compare_address)
                                || !TryReadInt32(compare_address, compare_value)
                                || check_value != compare_value)
                            {
                                matches = false;
                                break;
                            }
                        }
                        else
                        {
                            matches = false;
                            break;
                        }
                    }

                    if (matches)
                    {
                        resolved_address = candidate;
                        raw_value = candidate_raw_value;
                        mapped_value = candidate_mapped_value;
                        break;
                    }
                }
            }
        }

        std::optional<MemoryStateObserverUpdate> update;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            MemoryStateObserverDebugView& debug_view = debug_views_[observer_index];
            debug_view.CachedAddress = resolved_address.value_or(0);
            if (raw_value.has_value())
            {
                debug_view.LastRawValue = raw_value;
            }

            if (mapped_value.has_value())
            {
                if (!previous_mapped_value.has_value() || *previous_mapped_value != *mapped_value)
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
        }

        if (update.has_value() && update_callback_)
        {
            update_callback_(*update);
        }

        return true;
    }
}