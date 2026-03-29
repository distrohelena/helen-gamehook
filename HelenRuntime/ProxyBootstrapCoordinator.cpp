#include <HelenHook/ProxyBootstrapCoordinator.h>

namespace
{
    /**
     * @brief Closes one Win32 handle when it is valid and then clears the caller's storage.
     * @param handle Handle storage that should be released.
     */
    void CloseOwnedHandle(HANDLE& handle) noexcept
    {
        if (handle != nullptr && handle != INVALID_HANDLE_VALUE)
        {
            CloseHandle(handle);
        }

        handle = nullptr;
    }
}

namespace helen
{
    ProxyBootstrapCoordinator::ProxyBootstrapCoordinator(BootstrapCallback callback, void* callback_context) noexcept
        : callback_(callback)
        , callback_context_(callback_context)
        , state_(State::Idle)
        , completion_event_(nullptr)
        , worker_thread_(nullptr)
    {
    }

    ProxyBootstrapCoordinator::~ProxyBootstrapCoordinator()
    {
        Reset();
    }

    bool ProxyBootstrapCoordinator::BeginBackgroundBootstrap()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (state_ == State::Succeeded || state_ == State::Running)
        {
            return true;
        }

        if (callback_ == nullptr)
        {
            return false;
        }

        if (!EnsureCompletionEventLocked())
        {
            state_ = State::Failed;
            return false;
        }

        ResetEvent(completion_event_);
        state_ = State::Running;
        worker_thread_ = CreateThread(nullptr, 0, &BackgroundBootstrapThreadProc, this, 0, nullptr);
        if (worker_thread_ == nullptr)
        {
            state_ = State::Failed;
            SetEvent(completion_event_);
            return false;
        }

        return true;
    }

    bool ProxyBootstrapCoordinator::EnsureInitialized()
    {
        while (true)
        {
            HANDLE completion_event = nullptr;
            {
                std::lock_guard<std::mutex> lock(mutex_);
                if (state_ == State::Succeeded)
                {
                    return true;
                }

                if (state_ == State::Running)
                {
                    completion_event = completion_event_;
                }
            }

            if (completion_event == nullptr)
            {
                return RunBootstrapSynchronously();
            }

            WaitForSingleObject(completion_event, INFINITE);
        }
    }

    bool ProxyBootstrapCoordinator::IsInitialized() const noexcept
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return state_ == State::Succeeded;
    }

    void ProxyBootstrapCoordinator::Reset() noexcept
    {
        std::lock_guard<std::mutex> lock(mutex_);
        CloseOwnedHandle(worker_thread_);
        CloseOwnedHandle(completion_event_);
        state_ = State::Idle;
    }

    DWORD WINAPI ProxyBootstrapCoordinator::BackgroundBootstrapThreadProc(LPVOID parameter)
    {
        ProxyBootstrapCoordinator& coordinator = *static_cast<ProxyBootstrapCoordinator*>(parameter);
        const bool success = coordinator.callback_ != nullptr && coordinator.callback_(coordinator.callback_context_);
        coordinator.CompleteBootstrap(success, true);
        return 0;
    }

    bool ProxyBootstrapCoordinator::RunBootstrapSynchronously()
    {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (state_ == State::Succeeded)
            {
                return true;
            }

            if (state_ == State::Running)
            {
                return false;
            }

            if (callback_ == nullptr)
            {
                state_ = State::Failed;
                return false;
            }

            if (!EnsureCompletionEventLocked())
            {
                state_ = State::Failed;
                return false;
            }

            ResetEvent(completion_event_);
            state_ = State::Running;
        }

        const bool success = callback_(callback_context_);
        CompleteBootstrap(success, false);
        return success;
    }

    bool ProxyBootstrapCoordinator::EnsureCompletionEventLocked()
    {
        if (completion_event_ != nullptr)
        {
            return true;
        }

        completion_event_ = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        return completion_event_ != nullptr;
    }

    void ProxyBootstrapCoordinator::CompleteBootstrap(bool success, bool close_worker_handle) noexcept
    {
        std::lock_guard<std::mutex> lock(mutex_);
        state_ = success ? State::Succeeded : State::Failed;
        if (completion_event_ != nullptr)
        {
            SetEvent(completion_event_);
        }

        if (close_worker_handle)
        {
            CloseOwnedHandle(worker_thread_);
        }
    }
}
