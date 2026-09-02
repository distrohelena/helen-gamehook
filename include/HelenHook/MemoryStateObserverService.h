#pragma once

#include <cstdint>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include <HelenHook/MemoryStateObserverDebugView.h>
#include <HelenHook/MemoryStateObserverDefinition.h>
#include <HelenHook/MemoryStateObserverUpdate.h>

namespace helen
{
    /**
     * @brief Watches bounded process-memory ranges for declarative state signatures and emits eligible mapped requests and updates.
     */
    class MemoryStateObserverService
    {
    public:
        /**
         * @brief Applies one mapped observer update synchronously inside the serialized poll pass and reports whether all required native work succeeded.
         * @return True when all required native work for the update completed; false when the update could not be applied fully.
         * @remarks The implementation must not call PollOnce(), Start(), Stop(), or another observer lifecycle method on the same service from this callback because poll_mutex_ remains held until the callback returns.
         */
        using UpdateCallback = std::function<bool(const MemoryStateObserverUpdate&)>;

        /**
         * @brief Resolves the current integer value for a registered config key synchronously during a serialized carrier response.
         * @remarks The implementation must not call PollOnce(), Start(), Stop(), or another observer lifecycle method on the same service from this callback because poll_mutex_ remains held until the callback returns.
         */
        using ConfigValueCallback = std::function<std::optional<int>(const std::string&)>;

        /**
         * @brief Creates one observer service bound to the supplied observer definitions and update callback.
         * @param definitions Declarative observers that should be evaluated by the service.
         * @param update_callback Callback invoked synchronously for each eligible mapped request or update; it applies the mapped update and reports whether all required native work succeeded, and it must not re-enter this service through PollOnce(), Start(), Stop(), or another observer lifecycle method.
         * @param config_value_callback Optional callback used synchronously by request-response observers to read current config; when supplied, it must not re-enter this service through PollOnce(), Start(), Stop(), or another observer lifecycle method.
         */
        MemoryStateObserverService(
            std::vector<MemoryStateObserverDefinition> definitions,
            UpdateCallback update_callback,
            ConfigValueCallback config_value_callback = {});

        /**
         * @brief Stops the background polling thread before the service is destroyed.
         */
        ~MemoryStateObserverService();

        /**
         * @brief Starts the background polling thread when at least one observer is declared.
         * @return True when startup succeeds or no observers were declared; otherwise false.
         */
        bool Start();

        /**
         * @brief Stops the background polling thread and waits for it to exit.
         */
        void Stop();

        /**
         * @brief Polls every observer immediately, bypassing background timer throttling.
         * @return True when the poll completes successfully; otherwise false.
         * @remarks The complete pass, including ConfigValueCallback and UpdateCallback execution, holds poll_mutex_. Do not call PollOnce() from either callback on this same service.
         */
        bool PollOnce();

        /**
         * @brief Returns the current live debug state for every declared observer.
         * @return Debug views ordered the same way as the declared observers.
         */
        std::vector<MemoryStateObserverDebugView> GetDebugViews() const;

    private:
        /**
         * @brief Runs the timed background polling loop until Stop is requested.
         */
        void RunWorkerLoop();

        /**
         * @brief Polls only the observers whose polling interval has elapsed while holding the pass serialization lock.
         * @return True when the timed poll completes successfully; otherwise false.
         * @remarks poll_mutex_ remains held for the complete due-observer pass, including response and update callbacks, so callbacks must not re-enter this service.
         */
        bool PollDueObservers();

        /**
         * @brief Polls one observer and optionally emits an eligible mapped request or update.
         * @param observer_index Zero-based observer index inside the stored definition array.
         * @return True when the observer poll completed successfully; otherwise false.
         * @remarks Callers must hold poll_mutex_ for the complete enclosing pass; this method intentionally does not acquire that serialization mutex itself.
         */
        bool PollObserver(std::size_t observer_index);

        /**
         * @brief Returns the address currently cached for one observer, consulting its shared address group when declared.
         * @param observer_index Zero-based observer index inside the stored definition array.
         * @return Structurally validated carrier base address, or zero when no address is cached.
         * @remarks The caller must not hold mutex_; this method acquires the service lock while reading the observer or group cache.
         */
        std::uintptr_t GetCachedAddress(std::size_t observer_index) const;

        /**
         * @brief Stores one structurally validated carrier address for an observer and mirrors grouped addresses to matching debug views.
         * @param observer_index Zero-based observer index whose definition determines whether the cache is grouped.
         * @param address Carrier base address that passed the observer's structural and raw-value checks.
         * @return This method does not return a value; it updates the selected cache under mutex_.
         * @remarks The caller must not hold mutex_; ungrouped addresses update only the selected debug view, while grouped addresses replace the group entry and every matching view.
         */
        void CacheResolvedAddress(std::size_t observer_index, std::uintptr_t address);

        /**
         * @brief Removes one observer's cached carrier address and invalidates every matching grouped view when applicable.
         * @param observer_index Zero-based observer index whose definition determines whether the cache is grouped.
         * @return This method does not return a value; it clears the selected cache under mutex_.
         * @remarks The caller must not hold mutex_; ungrouped invalidation affects only the selected debug view, while grouped invalidation erases the group entry and clears every matching view.
         */
        void ClearCachedAddress(std::size_t observer_index);

        /** @brief Declared observers evaluated by this service. */
        std::vector<MemoryStateObserverDefinition> definitions_;
        /** @brief Live debug state that mirrors the declared observer order. */
        std::vector<MemoryStateObserverDebugView> debug_views_;
        /** @brief Structurally validated carrier base addresses keyed by declarative address-group ID. */
        std::unordered_map<std::string, std::uintptr_t> grouped_addresses_;
        /** @brief Last tick count recorded for each observer by the timed polling loop. */
        std::vector<std::uint64_t> last_poll_ticks_;
        /** @brief Raw transactional request remembered while its response has not yet been written, so only the same continuously pending request is suppressed. */
        std::vector<std::optional<int>> pending_transaction_requests_;
        /** @brief Resolved carrier address paired by index with each pending transactional request, preventing stale-address suppression after relocation. */
        std::vector<std::optional<std::uintptr_t>> pending_transaction_addresses_;
        /** @brief Callback invoked for eligible mapped observer requests and updates, including retryable transactional requests. */
        UpdateCallback update_callback_;
        /** @brief Optional callback that supplies current config values for bidirectional carrier responses. */
        ConfigValueCallback config_value_callback_;
        /** @brief Protects debug views, cached addresses, and thread start-stop state. */
        mutable std::mutex mutex_;
        /** @brief Serializes complete observer poll passes so manual and worker polling cannot overlap state validation, responses, or callbacks; it remains held through both callback types and therefore requires callbacks to avoid re-entering this service. */
        std::mutex poll_mutex_;
        /** @brief Coordinates timed wakeups and stop requests for the background worker thread. */
        std::condition_variable stop_condition_;
        /** @brief Background polling thread owned by the service. */
        std::thread worker_thread_;
        /** @brief Returns whether the background worker thread is currently running. */
        bool running_ = false;
        /** @brief Returns whether the background worker thread should exit as soon as possible. */
        bool stop_requested_ = false;
    };
}
