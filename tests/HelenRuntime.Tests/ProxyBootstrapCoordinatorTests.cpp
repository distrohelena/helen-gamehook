#include <HelenHook/ProxyBootstrapCoordinator.h>

#include <Windows.h>

#include <atomic>
#include <chrono>
#include <stdexcept>
#include <thread>

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
     * @brief Simple bootstrap callback context used to count invocations and control completion timing.
     */
    struct BootstrapCallbackContext
    {
        /**
         * @brief Number of times the coordinator asked the callback to perform initialization.
         */
        std::atomic<int> Calls{ 0 };

        /**
         * @brief Event that allows the test to delay callback completion until the background state is observable.
         */
        HANDLE Gate = nullptr;

        /**
         * @brief Result returned by the callback once the gate opens.
         */
        bool Result = true;
    };

    /**
     * @brief Test callback that blocks on the supplied gate so the coordinator can be observed in the running state.
     * @param context Untyped callback context provided by the test.
     * @return Result requested by the test after the gate opens.
     */
    bool DelayedBootstrapCallback(void* context)
    {
        BootstrapCallbackContext& callback_context = *static_cast<BootstrapCallbackContext*>(context);
        callback_context.Calls.fetch_add(1);
        if (callback_context.Gate != nullptr)
        {
            WaitForSingleObject(callback_context.Gate, INFINITE);
        }

        return callback_context.Result;
    }
}

/**
 * @brief Verifies that a background bootstrap completes through EnsureInitialized without starting a duplicate initialization attempt.
 */
void RunProxyBootstrapCoordinatorTests()
{
    BootstrapCallbackContext context;
    context.Gate = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (context.Gate == nullptr)
    {
        throw std::runtime_error("Failed to create the proxy bootstrap test gate.");
    }

    try
    {
        helen::ProxyBootstrapCoordinator coordinator(&DelayedBootstrapCallback, &context);
        Expect(coordinator.BeginBackgroundBootstrap(), "Expected background bootstrap scheduling to succeed.");

        for (int attempt = 0; attempt < 50 && context.Calls.load() == 0; ++attempt)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        Expect(context.Calls.load() == 1, "Expected the background bootstrap callback to run exactly once before release.");
        SetEvent(context.Gate);

        Expect(coordinator.EnsureInitialized(), "Expected EnsureInitialized to observe the background bootstrap success.");
        Expect(context.Calls.load() == 1, "EnsureInitialized unexpectedly launched a duplicate bootstrap attempt.");
    }
    catch (...)
    {
        CloseHandle(context.Gate);
        throw;
    }

    CloseHandle(context.Gate);
}
