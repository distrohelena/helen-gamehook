#pragma once

#include <Windows.h>

#include <mutex>

namespace helen
{
    /**
     * @brief Coordinates one optional background bootstrap so a later caller can wait for or reuse the same initialization attempt.
     *
     * The coordinator allows the proxy DLL to start Helen initialization during process attach and
     * then let later exported entry points observe that same work instead of launching a second bootstrap.
     * The callback may be retried synchronously when an earlier background attempt fails.
     */
    class ProxyBootstrapCoordinator
    {
    public:
        /**
         * @brief Function signature used to perform one bootstrap attempt.
         * @param context Untyped caller-owned context forwarded back to the callback.
         * @return True when the bootstrap completed successfully; otherwise false.
         */
        using BootstrapCallback = bool(*)(void* context);

        /**
         * @brief Creates one coordinator bound to the callback that performs Helen bootstrap work.
         * @param callback Callback invoked for both background and synchronous bootstrap attempts.
         * @param callback_context Untyped caller-owned context forwarded to callback.
         */
        ProxyBootstrapCoordinator(BootstrapCallback callback, void* callback_context) noexcept;

        /**
         * @brief Releases any event or thread handles still owned by the coordinator.
         */
        ~ProxyBootstrapCoordinator();

        ProxyBootstrapCoordinator(const ProxyBootstrapCoordinator&) = delete;
        ProxyBootstrapCoordinator& operator=(const ProxyBootstrapCoordinator&) = delete;
        ProxyBootstrapCoordinator(ProxyBootstrapCoordinator&&) = delete;
        ProxyBootstrapCoordinator& operator=(ProxyBootstrapCoordinator&&) = delete;

        /**
         * @brief Starts one background bootstrap when initialization has not already succeeded or started.
         * @return True when initialization is already complete, already running, or the background worker was scheduled successfully.
         */
        bool BeginBackgroundBootstrap();

        /**
         * @brief Ensures one successful bootstrap has completed, waiting for an existing background attempt when necessary.
         * @return True when initialization completed successfully; otherwise false.
         */
        bool EnsureInitialized();

        /**
         * @brief Reports whether one successful bootstrap has already completed.
         * @return True when the coordinator is in the succeeded state.
         */
        bool IsInitialized() const noexcept;

        /**
         * @brief Releases owned synchronization handles and returns the coordinator to the idle state.
         *
         * This method is intended for test cleanup and explicit shutdown flows that know no background
         * bootstrap remains in flight.
         */
        void Reset() noexcept;

    private:
        /**
         * @brief Internal bootstrap state tracked by the coordinator.
         */
        enum class State
        {
            /** @brief No bootstrap attempt has started yet. */
            Idle,
            /** @brief A background or synchronous bootstrap is currently running. */
            Running,
            /** @brief A bootstrap attempt completed successfully. */
            Succeeded,
            /** @brief The most recent bootstrap attempt failed. */
            Failed
        };

        /**
         * @brief Thread entry point that executes one background bootstrap attempt.
         * @param parameter Coordinator instance that owns the bootstrap state.
         * @return Always returns zero after recording the bootstrap result.
         */
        static DWORD WINAPI BackgroundBootstrapThreadProc(LPVOID parameter);

        /**
         * @brief Runs one synchronous bootstrap attempt when no background attempt is currently active.
         * @return True when the synchronous attempt succeeded; otherwise false.
         */
        bool RunBootstrapSynchronously();

        /**
         * @brief Creates the completion event on first use.
         * @return True when the event already exists or was created successfully; otherwise false.
         */
        bool EnsureCompletionEventLocked();

        /**
         * @brief Records one bootstrap result, signals waiters, and closes the background thread handle when requested.
         * @param success Result returned by the bootstrap callback.
         * @param close_worker_handle True when the current background worker handle should be closed.
         */
        void CompleteBootstrap(bool success, bool close_worker_handle) noexcept;

        /** @brief Callback invoked to perform one bootstrap attempt. */
        BootstrapCallback callback_;

        /** @brief Untyped caller-owned context forwarded to callback_. */
        void* callback_context_;

        /** @brief Current bootstrap state observed by the proxy and background worker. */
        State state_;

        /** @brief Manual-reset event signaled whenever the current bootstrap attempt finishes. */
        HANDLE completion_event_;

        /** @brief Live background worker handle used only so the coordinator can close it after completion. */
        HANDLE worker_thread_;

        /** @brief Synchronizes state transitions and handle ownership updates. */
        mutable std::mutex mutex_;
    };
}
