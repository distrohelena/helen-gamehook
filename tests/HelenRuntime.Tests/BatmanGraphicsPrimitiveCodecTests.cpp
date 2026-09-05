#include "BatmanGraphicsPrimitiveCodec.h"

#include <windows.h>
#include <array>
#include <cstring>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
    /** @brief Fails a console test without assertion dialogs even in Release builds. */
    void Require(bool condition, const char* message) {
        if (!condition) {
            throw std::runtime_error(message);
        }
    }

    /** @brief Produces an independent engine-layout fixture, not a value encoded by the production codec. */
    std::array<unsigned char, 16> MakeArgument(std::uint32_t type, double value) {
        std::array<unsigned char, 16> bytes{};
        std::memcpy(bytes.data(), &type, sizeof(type));
        std::memcpy(bytes.data() + 8, &value, sizeof(value));
        return bytes;
    }

    /** @brief Catches wrong numeric payload offsets, integer truncation, and zero being treated as absent. */
    void TestNumericArguments() {
        for (std::uint32_t value : {0u, 1u, 98u, 1920u, 4294967295u}) {
            const auto argument = MakeArgument(3, static_cast<double>(value));
            const auto before = argument;
            Require(helen::BatmanGraphicsPrimitiveCodec::ReadUnsigned(argument.data(), 1, 0) == value,
                "Numeric argument was not decoded exactly from offset eight.");
            Require(argument == before, "Reading modified borrowed argument storage.");
        }
    }

    /** @brief Catches coercion, nonfinite conversion, out-of-range access, and accepting fractional handles. */
    void TestInvalidArguments() {
        for (std::uint32_t type : {0u, 1u, 2u, 4u, 5u, 0x13u, 0x103u}) {
            const auto argument = MakeArgument(type, 1.0);
            Require(!helen::BatmanGraphicsPrimitiveCodec::ReadUnsigned(argument.data(), 1, 0),
                "Non-numeric argument was coerced to an integer.");
        }
        for (double value : {-1.0, 0.5, 4294967296.0, std::numeric_limits<double>::quiet_NaN(),
                std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity()}) {
            const auto argument = MakeArgument(3, value);
            Require(!helen::BatmanGraphicsPrimitiveCodec::ReadUnsigned(argument.data(), 1, 0),
                "Invalid numeric argument accepted as an unsigned integer.");
        }
        const auto argument = MakeArgument(3, 1.0);
        Require(!helen::BatmanGraphicsPrimitiveCodec::ReadUnsigned(nullptr, 1, 0), "Null argument buffer accepted.");
        Require(!helen::BatmanGraphicsPrimitiveCodec::ReadUnsigned(argument.data(), 0, 0), "Empty argument list read.");
        Require(!helen::BatmanGraphicsPrimitiveCodec::ReadUnsigned(argument.data(), 1, 1), "Out-of-range argument read.");
    }

    /** @brief Catches using the return layout for arguments, wrong argument stride, and alignment assumptions. */
    void TestArgumentStride() {
        std::array<unsigned char, 33> bytes{};
        const auto first = MakeArgument(3, 98.0);
        const auto second = MakeArgument(3, 1920.0);
        std::memcpy(bytes.data() + 1, first.data(), first.size());
        std::memcpy(bytes.data() + 17, second.data(), second.size());
        Require(helen::BatmanGraphicsPrimitiveCodec::ReadUnsigned(bytes.data() + 1, 2, 0) == 98u,
            "Unaligned first argument decoded incorrectly.");
        Require(helen::BatmanGraphicsPrimitiveCodec::ReadUnsigned(bytes.data() + 1, 2, 1) == 1920u,
            "Second argument did not use sixteen-byte stride.");
    }

    /** @brief Catches wrong GAS return tags/offsets and overwriting opaque padding around numeric payloads. */
    void TestNumericReturns() {
        for (std::uint32_t value : {0u, 1u, 98u, 1920u, 4294967295u}) {
            helen::BatmanGraphicsPrimitiveResult result;
            std::memset(&result, 0xA5, sizeof(result));
            result.Type = 0;
            helen::BatmanGraphicsPrimitiveCodec::WriteNumber(result, value);
            double decoded = -1.0;
            std::memcpy(&decoded, result.Payload, sizeof(decoded));
            Require(result.Type == 3 && decoded == static_cast<double>(value),
                "Numeric return did not encode a type-three double at offset four.");
            for (unsigned char byte : result.Reserved) {
                Require(byte == 0xA5, "Numeric return modified engine padding.");
            }
            for (unsigned index = 8; index < 12; ++index) {
                Require(result.Payload[index] == 0xA5, "Numeric return exceeded double payload.");
            }
        }
    }
}

/** @brief Exercises the real x86 primitive codec without launching Batman or writing game/config files. */
int main() {
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
    try {
        TestNumericArguments();
        TestInvalidArguments();
        TestArgumentStride();
        TestNumericReturns();
        std::cout << "BATMAN_GRAPHICS_PRIMITIVE_CODEC_PASS\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
