#include "RefreshRequest.h"
#include <atomic>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <vector>

namespace {
    /** @brief Competes on real request synchronization using a shared synthetic observed identity. */
    void Compete(helen::RefreshRequest& request, std::atomic<bool>& start,
        std::atomic<unsigned>& winners, bool consume) {
        start.wait(false);
        const bool accepted = consume ? request.Consume(11, 22, 1, 33) : request.Arm(1, 11, 22, 1, 33);
        if (accepted) { winners.fetch_add(1); }
    }
    /** @brief Releases workers without timing sleeps, including cleanup if thread creation throws. */
    unsigned RunContention(helen::RefreshRequest& request, bool consume) {
        std::atomic<bool> start{false};
        std::atomic<unsigned> winners{0};
        {
            std::vector<std::jthread> workers;
            workers.reserve(16);
            try {
                for (unsigned index = 0; index < 16; ++index) {
                    workers.emplace_back(Compete, std::ref(request), std::ref(start), std::ref(winners), consume);
                }
            } catch (...) {
                start.store(true);
                start.notify_all();
                throw;
            }
            start.store(true);
            start.notify_all();
        }
        return winners.load();
    }
    /** @brief Converts contract violations into console-only failures. */
    void Expect(bool condition, const char* message) {
        if (!condition) { throw std::runtime_error(message); }
    }
}
/** @brief Stresses CPU state serialization, not engine threads, callback rebinding or unload safety. */
int main() {
    SetErrorMode(0x8003);
    try {
        for (unsigned round = 0; round < 32; ++round) {
            helen::RefreshRequest request;
            Expect(RunContention(request, false) == 1, "concurrent arm did not have exactly one winner");
            Expect(RunContention(request, true) == 1, "concurrent consume did not have exactly one winner");
            Expect(request.WasConsumed(1), "concurrent consumption evidence missing");
            request.Disarm(1);
            Expect(!request.Consume(11, 22, 1, 33), "disarmed operation consumed");
        }
        std::cout << "VSYNC_REQUEST_CONCURRENCY_PASS: 32 rounds, 16 contenders per transition\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "VSYNC_REQUEST_CONCURRENCY_FAIL: " << error.what() << '\n';
        return 1;
    }
}
