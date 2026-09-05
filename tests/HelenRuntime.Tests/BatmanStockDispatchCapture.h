#pragma once

#include <stdexcept>

/** @brief Emulates only the verified two-slot engine handler vtable to exercise the real x86 forwarding boundary. */
class BatmanStockDispatchCapture {
public:
    /** @brief Last borrowed movie received by the original handler slot, absent before its first call. */
    void* Movie = nullptr;
    /** @brief Last borrowed method name, including a legitimate null stock name. */
    const char* Name = nullptr;
    /** @brief Last borrowed converted argument buffer; no test-owned result decoding is performed. */
    const void* Arguments = nullptr;
    /** @brief Number of engine arguments received without coercion. */
    unsigned Count = 0;
    /** @brief Number of entries into the original handler slot. */
    unsigned Calls = 0;
    /** @brief Selects a stock exception used to verify that graphics containment does not swallow stock failures. */
    bool ThrowOnCall = false;

    /** @brief Occupies the engine's unused first virtual slot; invoking it is an ABI failure. */
    virtual void Reserved() {
        throw std::logic_error("Dispatch used stock vtable slot zero.");
    }
    /** @brief Receives original thiscall arguments at vtable slot one and optionally throws a stock failure. */
    virtual void Invoke(void* movie, const char* name, const void* arguments, unsigned count) {
        Movie = movie;
        Name = name;
        Arguments = arguments;
        Count = count;
        ++Calls;
        if (ThrowOnCall) {
            throw std::logic_error("Stock dispatch failure.");
        }
    }
};
