#include "VsyncScalar.h"
#include <windows.h>
#include <array>
#include <iostream>
#include <stdexcept>

namespace {
    /** @brief Reports assertion failures through the console entry point, never CRT dialogs. */
    void Expect(bool condition, const char* message) {
        if (!condition) { throw std::runtime_error(message); }
    }
    /** @brief Rejects unsupported read/write representations while retaining every sentinel word. */
    void CheckInvalid(std::uint32_t invalid) {
        std::array<std::uint32_t, 3> words{0x11223344u, invalid, 0x55667788u};
        const std::array<std::uint32_t, 3> before = words;
        helen::VsyncScalar scalar(words[1]);
        bool rejectedRead = false;
        try { static_cast<void>(scalar.Read()); }
        catch (const std::runtime_error&) { rejectedRead = true; }
        Expect(rejectedRead, "invalid read accepted");
        for (bool enabled : {false, true}) {
            bool rejectedWrite = false;
            try { scalar.Write(enabled); }
            catch (const std::runtime_error&) { rejectedWrite = true; }
            Expect(rejectedWrite, "invalid write accepted");
            Expect(words == before, "invalid representation modified storage");
        }
    }
}
/** @brief Checks exact scalar footprint in owned CPU storage; no game or device is accessed. */
int main() {
    SetErrorMode(0x8003);
    try {
        std::array<std::uint32_t, 3> words{0x11223344u, 0u, 0x55667788u};
        helen::VsyncScalar scalar(words[1]);
        Expect(!scalar.Read(), "zero read as enabled");
        scalar.Write(true);
        Expect(words == std::array<std::uint32_t, 3>{0x11223344u, 1u, 0x55667788u}, "enable footprint");
        Expect(scalar.Read(), "one read as disabled");
        scalar.Write(false);
        Expect(words == std::array<std::uint32_t, 3>{0x11223344u, 0u, 0x55667788u}, "disable footprint");
        CheckInvalid(2);
        CheckInvalid(0xFFFFFFFFu);
        std::cout << "VSYNC_SCALAR_PASS\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "VSYNC_SCALAR_FAIL: " << error.what() << '\n';
        return 1;
    }
}
