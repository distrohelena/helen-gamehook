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
     * @brief Watches bounded process-memory ranges for declarative state signatures and emits mapped config updates when the observed value changes.
     */
    class MemoryStateObserverService
    {
    public:
        /**
         * @brief Receives mapped observer updates detected by the service.
         */
        using UpdateCallback = std::function<void(const MemoryStateObserverUpdate&)>;

        /**
         * @brief Resolves the current integer value for a registered config key during a carrier response.
         */
        using ConfigValueCallback = std::function<std::optional<int>(const std::string&)>;

        /**
         * @brief Creates one observer service bound to the supplied observer definitions and update callback.
         * @param definitions Declarative observers that should be evaluated by the service.
         * @param update_callback Callback invoked whenever an observer emits a new mapped value.
         * @param config_value_callback Optional callback used by request-response observers to read current config.
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
         * @brief Polls only the observers whose polling interval has elapsed.
         * @return True when the timed poll completes successfully; otherwise false.
         */
        bool PollDueObservers();

        /**
         * @brief Polls one observer and optionally emits a mapped update when the observed value changed.
         * @param observer_index Zero-based observer index inside the stored definition array.
         * @return True when the observer poll completed successfully; otherwise false.
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
        /** @brief Callback invoked for newly mapped observer updates. */
        UpdateCallback update_callback_;
        /** @brief Optional callback that supplies current config values for bidirectional carrier responses. */
        ConfigValueCallback config_value_callback_;
        /** @brief Protects debug views, cached addresses, and thread start-stop state. */
        mutable std::mutex mutex_;
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
