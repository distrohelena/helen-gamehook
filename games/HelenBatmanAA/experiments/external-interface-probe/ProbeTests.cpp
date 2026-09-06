#include "FullscreenProbe.h"
#include "DispatchAdapter.h"
#include <array>
#include <windows.h>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <cstring>

/** Stops the console test with a readable failure rather than a GUI assertion dialog. */
void Require(bool condition, const char* message) {
    if (!condition) { throw std::runtime_error(message); }
}

/** Counts actual stock-target invocations to detect swallowed or duplicate forwarding. */
unsigned StockCalls = 0;
/** Expected borrowed handler pointer for the stock thiscall fixture. */
void* ExpectedHandler = nullptr;
/** Expected movie identity; a new instance must not reuse the first movie's return slot. */
void* ExpectedMovie = nullptr;
/** Expected untouched argument buffer forwarded by the adapter. */
const void* ExpectedArguments = nullptr;

/** Emulates the unavailable engine virtual method and validates every argument at the ABI boundary. */
void __fastcall StockHandler(void* handler, void*, void* movie, const char* name, const void* arguments, unsigned count) {
    Require(handler == ExpectedHandler && movie == ExpectedMovie, "Stock this/movie identity changed.");
    Require(name != nullptr && std::strcmp(name, "FE_GetControlType") == 0, "Custom callback leaked into stock dispatch.");
    Require(arguments == ExpectedArguments && count == 3, "Stock arguments changed.");
    ++StockCalls;
    PrimitiveResult& result = *reinterpret_cast<PrimitiveResult*>(static_cast<unsigned char*>(movie) + 0x9DC);
    result.Type = 2;
    result.Payload[0] = 0;
}

/** Calls the adapter as Batman does: ECX handler and four stack arguments, not an ordinary cdecl call. */
void TestDispatchAbi(const FullscreenProbe& probe) {
    DispatchAdapter::Bind(probe);
    using OriginalMethod = void (__thiscall*)(void*, void*, const char*, const void*, unsigned);
    const std::array<OriginalMethod, 2> table{nullptr, reinterpret_cast<OriginalMethod>(&StockHandler)};
    const OriginalMethod* handler = table.data();
    ExpectedHandler = &handler;
    const OriginalMethod dispatch = reinterpret_cast<OriginalMethod>(&DispatchAdapter::Dispatch);
    const std::array<int, 3> arguments{7, 9, 11};
    ExpectedArguments = arguments.data();
    std::array<std::array<unsigned char, 0xA00>, 2> movies{};
    for (unsigned iteration = 0; iteration < 100; ++iteration) {
        std::array<unsigned char, 0xA00>& movie = movies[iteration % 2];
        const std::array<unsigned char, 0xA00> otherBefore = movies[(iteration + 1) % 2];
        ExpectedMovie = movie.data();
        PrimitiveResult& result = *reinterpret_cast<PrimitiveResult*>(movie.data() + 0x9DC);
        result.Type = 0;
        dispatch(&handler, movie.data(), "Helen_ProbeGetFullscreen", nullptr, 0);
        Require(result.Type == 2 && result.Payload[0] == 1, "ABI adapter did not return direct Fullscreen.");
        result.Type = 0;
        dispatch(&handler, movie.data(), "FE_GetControlType", arguments.data(), 3);
        Require(result.Type == 2 && result.Payload[0] == 0, "Stock handler result did not survive forwarding.");
        Require(movies[(iteration + 1) % 2] == otherBefore, "Dispatch wrote into another movie's result slot.");
    }
    Require(StockCalls == 100, "Stock calls were swallowed, duplicated, or used for the probe.");
    std::cout << "X86_DISPATCH_ABI_PASS\n";
}

/** Exercises real INI reads and the primitive response contract without launching or patching Batman. */
int wmain(int argc, wchar_t** argv) {
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
    try {
        Require(argc == 2, "Expected absolute fixture directory.");
        const std::filesystem::path fixtures(argv[1]);
        for (const wchar_t* file : {L"on.ini", L"off.ini"}) {
            FullscreenProbe probe((fixtures / file).wstring());
            PrimitiveResult result{};
            result.Reserved[1] = 0xAB;
            result.Payload[11] = 0xCD;
            Require(probe.TryHandle("Helen_ProbeGetFullscreen", 0, result), "Direct Fullscreen call was not handled.");
            Require(result.Type == 2, "Valid Fullscreen must return a GAS boolean.");
            Require(result.Payload[0] == (std::wstring(file) == L"on.ini" ? 1 : 0), "Fullscreen value was not read from this INI.");
            Require(result.Reserved[1] == 0xAB && result.Payload[11] == 0xCD, "Probe touched opaque result bytes.");
        }
        for (const wchar_t* file : {L"missing.ini", L"invalid.ini", L"absent.ini"}) {
            FullscreenProbe probe((fixtures / file).wstring());
            PrimitiveResult result{};
            Require(probe.TryHandle("Helen_ProbeGetFullscreen", 0, result), "Failed probe read fell through to a stock call.");
            Require(result.Type == 0, "Missing or invalid Fullscreen was fabricated as a boolean.");
        }
        FullscreenProbe probe((fixtures / L"on.ini").wstring());
        for (const char* name : {"FE_GetControlType", "Helen_ProbeGetFullscreenExtra", "helen_ProbeGetFullscreen", ""}) {
            PrimitiveResult result{};
            result.Type = 5;
            const PrimitiveResult before = result;
            Require(!probe.TryHandle(name, 0, result), "Non-probe callback was intercepted.");
            Require(std::memcmp(&before, &result, sizeof(result)) == 0, "Forwarded callback result was modified.");
        }
        PrimitiveResult result{};
        Require(probe.TryHandle("Helen_ProbeGetFullscreen", 1, result) && result.Type == 0, "Malformed probe call was accepted or forwarded.");
        std::cout << "DIRECT_READ_CONTRACT_PASS\n";
        TestDispatchAbi(probe);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
