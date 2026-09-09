#include <windows.h>
#include <iostream>
#include <stdexcept>
#if !defined(FIXTURE_DYNAMIC_HOST)
/** @brief Forces a real static proxy import, which receives Windows' startup attach context. */
extern "C" __declspec(dllimport) DWORD __stdcall HelenQueryStartupThread() noexcept;
/** @brief Reads evidence from the statically imported fixture runtime. */
extern "C" __declspec(dllimport) unsigned __stdcall FixtureReadState();
/** @brief Calls the same startup export again after the actual attach window has closed. */
extern "C" __declspec(dllimport) BOOL __stdcall HelenInitializeAtStartup(HMODULE proxy);
#endif
namespace {
    /** @brief Reports contract failures to the console and a failing exit code. */
    void Expect(bool condition, const char* message) {
        if (!condition) { throw std::runtime_error(message); }
    }
}
/** @brief Runs startup or dynamic loader tests in disposable processes without Batman. */
int main(int argc, char**) {
    SetErrorMode(0x8003);
    try {
#if defined(FIXTURE_DYNAMIC_HOST)
        const bool pin = argc > 1;
        const wchar_t* name = pin ? L"FixtureRuntime.dll" : L"dinput8.dll";
        HMODULE module = LoadLibraryExW(name, nullptr, LOAD_LIBRARY_SEARCH_APPLICATION_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
        Expect(module != nullptr, "fixture DLL did not load");
        HMODULE runtime = pin ? module : GetModuleHandleW(L"FixtureRuntime.dll");
        Expect(runtime != nullptr, "fixture runtime dependency missing");
        using ReadFunction = unsigned (__stdcall*)();
        const ReadFunction read = reinterpret_cast<ReadFunction>(GetProcAddress(runtime, "FixtureReadState"));
        Expect(read != nullptr && read() == 0, "dynamic load admitted startup initialization");
        if (pin) {
            using PinFunction = BOOL (__stdcall*)();
            const PinFunction pinModule = reinterpret_cast<PinFunction>(GetProcAddress(runtime, "FixturePin"));
            Expect(pinModule != nullptr && pinModule(), "module pin failed");
        } else {
            using StartupFunction = BOOL (__stdcall*)(HMODULE);
            const StartupFunction startup = reinterpret_cast<StartupFunction>(GetProcAddress(runtime, "HelenInitializeAtStartup"));
            Expect(startup != nullptr && !startup(module), "late dynamic call admitted installation");
            Expect(read() == 0, "late refusal executed initializer");
        }
        Expect(FreeLibrary(module) != FALSE, "balanced fixture release failed");
        Expect((GetModuleHandleW(name) != nullptr) == pin, "module lifetime did not match pin policy");
        if (pin) { Expect(read() == 0, "pinned callback not callable after release"); }
#else
        static_cast<void>(argc);
        Expect(HelenQueryStartupThread() == 0, "proxy startup window leaked");
#if defined(FIXTURE_FAIL_INIT)
        Expect(FixtureReadState() == 0x1901, "failed initializer did not close installation window");
#else
        Expect(FixtureReadState() == 0x1D01, "startup callback missing or installation window leaked");
#endif
        Expect(!HelenInitializeAtStartup(GetModuleHandleW(L"dinput8.dll")), "late static call reopened installation");
        Expect(FixtureReadState() == 0x1901, "late call changed callback count or leaked permission");
#endif
        std::cout << "STARTUP_LOADER_PASS\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "STARTUP_LOADER_FAIL: " << error.what() << '\n';
        return 1;
    }
}
