#pragma once

#include <cstdint>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <thread>
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
         * @brief Creates one observer service bound to the supplied observer definitions and update callback.
         * @param definitions Declarative observers that should be evaluated by the service.
         * @param update_callback Callback invoked whenever an observer emits a new mapped value.
         */
        MemoryStateObserverService(std::vector<MemoryStateObserverDefinition> definitions, UpdateCallback update_callback);

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

        /** @brief Declared observers evaluated by this service. */
        std::vector<MemoryStateObserverDefinition> definitions_;
        /** @brief Live debug state that mirrors the declared observer order. */
        std::vector<MemoryStateObserverDebugView> debug_views_;
        /** @brief Last tick count recorded for each observer by the timed polling loop. */
        std::vector<std::uint64_t> last_poll_ticks_;
        /** @brief Callback invoked for newly mapped observer updates. */
        UpdateCallback update_callback_;
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