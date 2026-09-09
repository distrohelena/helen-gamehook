#include "RefreshActivation.h"
#include <iostream>
#include <stdexcept>

namespace {
    /** @brief Reports contract failures to the console without assertion dialogs. */
    void Expect(bool condition, const char* message) {
        if (!condition) { throw std::runtime_error(message); }
    }
    /** @brief Catches missing publication, wrong-device consumption, duplicates and leaked exceptional state. */
    void CheckActivation() {
        std::array<std::uintptr_t, 52> words{};
        words[1] = 11; words[3] = 22; words[19] = 33; words[33] = 44; words[51] = 55;
        const std::uintptr_t frame = reinterpret_cast<std::uintptr_t>(words.data());
        Expect(!helen::RefreshActivation::ExpectsRenderer(123), "Idle bridge has an active renderer");
        {
            helen::RefreshActivation activation(123, 456, {11,22,33,44});
            Expect(helen::RefreshActivation::Publish(&activation, frame + 204, 55), "Activation publication failed");
            Expect(helen::RefreshActivation::ExpectsRenderer(123), "Published renderer missing");
            Expect(!helen::RefreshActivation::ExpectsRenderer(124), "Wrong renderer accepted");
            Expect(!helen::RefreshActivation::Publish(&activation, frame + 204, 55), "Duplicate publication accepted");
            Expect(!helen::RefreshActivation::Consume(frame, 123, 457), "Wrong device consumed");
            words[19] = 34;
            Expect(!helen::RefreshActivation::Consume(frame, 123, 456), "Wrong return chain consumed");
            words[19] = 33;
            Expect(helen::RefreshActivation::Consume(frame, 123, 456), "Exact activation rejected");
            Expect(!helen::RefreshActivation::Consume(frame, 123, 456), "Activation consumed twice");
            Expect(activation.WasConsumed(), "Consumption evidence missing");
            helen::RefreshActivation::Clear(&activation);
            Expect(!helen::RefreshActivation::ExpectsRenderer(123), "Normal return leaked publication");
        }
        try {
            helen::RefreshActivation activation(123, 456, {11,22,33,44});
            Expect(helen::RefreshActivation::Publish(&activation, frame + 204, 55), "Exception activation refused");
            throw 42;
        } catch (int) {}
        Expect(!helen::RefreshActivation::Consume(frame, 123, 456), "Exception leaked publication");
    }
}
/** @brief Exercises the real activation controller on a bounded x86 stack without Batman. */
int main() {
    SetErrorMode(0x8003);
    try { CheckActivation(); std::cout << "REFRESH_ACTIVATION_PASS\n"; return 0; }
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
