# Batman Bink Loader Routing Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a small IAT-based loader-routing layer that logs Batman Bink DLL loads and keeps the Bink-specific logic out of `HelenGameHook.dll`.

**Architecture:** Add one pure routing service that normalizes module names, classifies Bink aliases, and formats the log line. Add one dedicated hook set that patches the main executable import table for `LoadLibraryA`, `LoadLibraryW`, `LoadLibraryExA`, and `LoadLibraryExW`, then forwards each detour through the routing service and the original Win32 API. Wire the hook set into Batman startup beside the existing file and D3D9 hook sets, then verify the behavior with focused unit tests and a full Win32 build.

**Tech Stack:** C++20, Win32 IAT hooks, Visual Studio 2022, MSBuild, the existing `HelenRuntime.Tests` console harness.

---

## File Map

- `include/HelenHook/ModuleLoadRoutingService.h`: defines the routing decision types and the pure module-name routing API.
- `HelenRuntime/ModuleLoadRoutingService.cpp`: implements name normalization, Bink alias matching, redirect validation, and log-line formatting.
- `include/HelenHook/ModuleLoadRoutingHookSet.h`: defines the loader hook set that owns the four `LoadLibrary*` IAT hooks.
- `HelenRuntime/ModuleLoadRoutingHookSet.cpp`: implements the detours, the main-module IAT install/uninstall flow, and the static active-instance bridge.
- `HelenGameHook/HelenGameHook.cpp`: creates, installs, and removes the new routing hook set during Batman runtime startup and shutdown.
- `tests/HelenRuntime.Tests/ModuleLoadRoutingServiceTests.cpp`: exercises normalization, alias matching, redirect validation, and log formatting.
- `tests/HelenRuntime.Tests/ModuleLoadRoutingHookSetTests.cpp`: exercises hook installation and removal against the test runner's own import table.
- `tests/HelenRuntime.Tests/TestMain.cpp`: registers the new test entry points.
- `HelenRuntime/HelenRuntime.vcxproj`: adds the two new runtime source files and public headers.
- `tests/HelenRuntime.Tests/HelenRuntime.Tests.vcxproj`: adds the two new test source files.

No pack manifest changes are required for this log-only slice.

## Task 1: Add the pure routing service

**Files:**
- Create: `include/HelenHook/ModuleLoadRoutingService.h`
- Create: `HelenRuntime/ModuleLoadRoutingService.cpp`
- Modify: `HelenRuntime/HelenRuntime.vcxproj`
- Create: `tests/HelenRuntime.Tests/ModuleLoadRoutingServiceTests.cpp`
- Modify: `tests/HelenRuntime.Tests/HelenRuntime.Tests.vcxproj`
- Modify: `tests/HelenRuntime.Tests/TestMain.cpp`

- [ ] **Step 1: Write the failing test**

Create `tests/HelenRuntime.Tests/ModuleLoadRoutingServiceTests.cpp` with a pure routing test that proves alias normalization, log formatting, and duplicate redirect rejection:

```cpp
#include <HelenHook/ModuleLoadRoutingService.h>

#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
    void Expect(bool condition, const char* message)
    {
        if (!condition)
        {
            throw std::runtime_error(message);
        }
    }
}

void RunModuleLoadRoutingServiceTests()
{
    {
        const std::vector<helen::ModuleRedirectDefinition> redirects = {
            { L"binkw32.dll", std::filesystem::path(L"helengamehook/deps/bink/binkw32.dll") }
        };

        helen::ModuleLoadRoutingService service(redirects);
        const helen::ModuleLoadRoutingDecision decision =
            service.DescribeRequest(L"LoadLibraryW", L"C:\\Game\\BINKW32.DLL");

        Expect(decision.IsBinkAlias, "Expected Bink DLL requests to be classified as aliases.");
        Expect(decision.NormalizedModuleName == L"binkw32.dll", "Expected module-name normalization to strip the path and lowercase the name.");
        Expect(!decision.RedirectTargetPath.has_value(), "Expected the log-only slice to preserve pass-through behavior.");

        const std::wstring log_line = service.BuildLogMessage(decision);
        Expect(log_line.find(L"api=LoadLibraryW") != std::wstring::npos, "Expected the loader API in the log line.");
        Expect(log_line.find(L"requested=binkw32.dll") != std::wstring::npos, "Expected the normalized module name in the log line.");
        Expect(log_line.find(L"matched=bink alias") != std::wstring::npos, "Expected the alias classification in the log line.");
    }

    {
        bool duplicate_threw = false;
        try
        {
            const std::vector<helen::ModuleRedirectDefinition> duplicate_redirects = {
                { L"binkw32.dll", std::filesystem::path(L"helengamehook/deps/bink/binkw32.dll") },
                { L"BINKW32.DLL", std::filesystem::path(L"helengamehook/deps/bink/alt.dll") }
            };

            helen::ModuleLoadRoutingService service(duplicate_redirects);
            (void)service;
        }
        catch (const std::exception&)
        {
            duplicate_threw = true;
        }

        Expect(duplicate_threw, "Expected duplicate redirect names to fail fast.");
    }
}
```

Run:

```powershell
rtk proxy powershell.exe -NoProfile -Command "& msbuild '.\\HelenGameHook.sln' /p:Configuration=Debug /p:Platform=Win32 /t:HelenRuntime.Tests"
```

Expected: compile fails because `HelenHook/ModuleLoadRoutingService.h` and the new service types do not exist yet.

- [ ] **Step 2: Run the test again after the header exists**

Once the header and source file exist, run:

```powershell
rtk proxy powershell.exe -NoProfile -Command "& '.\\bin\\Win32\\Debug\\HelenRuntime.Tests.exe'"
```

Expected: the new service test fails until normalization, duplicate validation, and log formatting are implemented.

- [ ] **Step 3: Write the minimal implementation**

Implement the service in `HelenRuntime/ModuleLoadRoutingService.cpp` with one small public type and one small routing API:

```cpp
#include <HelenHook/ModuleLoadRoutingService.h>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <optional>
#include <stdexcept>

namespace
{
    std::wstring ToLowerWide(std::wstring text)
    {
        for (wchar_t& character : text)
        {
            character = static_cast<wchar_t>(std::towlower(character));
        }

        return text;
    }
}

namespace helen
{
    ModuleLoadRoutingService::ModuleLoadRoutingService(std::vector<ModuleRedirectDefinition> redirects)
        : redirects_(std::move(redirects))
    {
        // Validate duplicate normalized module names up front so the runtime fails fast.
    }

    std::optional<std::wstring> ModuleLoadRoutingService::TryNormalizeModuleName(std::wstring_view requested_name)
    {
        if (requested_name.empty())
        {
            return std::nullopt;
        }

        const std::filesystem::path requested_path(requested_name);
        std::wstring normalized = requested_path.filename().wstring();
        if (normalized.empty())
        {
            return std::nullopt;
        }

        return ToLowerWide(std::move(normalized));
    }
}
```

The full implementation should keep the redirect list validated in the constructor, classify `binkw32.dll` and `bink2w32.dll` as aliases, and format the final log line from the normalized decision object.

Run:

```powershell
rtk proxy powershell.exe -NoProfile -Command "& msbuild '.\\HelenGameHook.sln' /p:Configuration=Debug /p:Platform=Win32 /t:HelenRuntime.Tests"
```

Expected: the service tests pass.

- [ ] **Step 4: Commit**

```powershell
rtk proxy powershell.exe -NoProfile -Command "git add include/HelenHook/ModuleLoadRoutingService.h HelenRuntime/ModuleLoadRoutingService.cpp HelenRuntime/HelenRuntime.vcxproj tests/HelenRuntime.Tests/ModuleLoadRoutingServiceTests.cpp tests/HelenRuntime.Tests/HelenRuntime.Tests.vcxproj tests/HelenRuntime.Tests/TestMain.cpp; git commit -m 'Add Batman Bink module routing service'"
```

## Task 2: Add the loader hook set

**Files:**
- Create: `include/HelenHook/ModuleLoadRoutingHookSet.h`
- Create: `HelenRuntime/ModuleLoadRoutingHookSet.cpp`
- Modify: `HelenRuntime/HelenRuntime.vcxproj`
- Create: `tests/HelenRuntime.Tests/ModuleLoadRoutingHookSetTests.cpp`
- Modify: `tests/HelenRuntime.Tests/HelenRuntime.Tests.vcxproj`
- Modify: `tests/HelenRuntime.Tests/TestMain.cpp`

- [ ] **Step 1: Write the failing test**

Create `tests/HelenRuntime.Tests/ModuleLoadRoutingHookSetTests.cpp` and force the test binary to import the loader APIs before installing the hook set:

```cpp
#include <HelenHook/ModuleLoadRoutingHookSet.h>
#include <HelenHook/ModuleLoadRoutingService.h>

#include <stdexcept>

namespace
{
    void Expect(bool condition, const char* message)
    {
        if (!condition)
        {
            throw std::runtime_error(message);
        }
    }

    void ForceLoaderImports()
    {
        const HMODULE module_a = LoadLibraryA("kernel32.dll");
        if (module_a != nullptr)
        {
            FreeLibrary(module_a);
        }

        const HMODULE module_w = LoadLibraryW(L"kernel32.dll");
        if (module_w != nullptr)
        {
            FreeLibrary(module_w);
        }

        const HMODULE module_ex_a = LoadLibraryExA("kernel32.dll", nullptr, 0);
        if (module_ex_a != nullptr)
        {
            FreeLibrary(module_ex_a);
        }

        const HMODULE module_ex_w = LoadLibraryExW(L"kernel32.dll", nullptr, 0);
        if (module_ex_w != nullptr)
        {
            FreeLibrary(module_ex_w);
        }
    }
}

void RunModuleLoadRoutingHookSetTests()
{
    ForceLoaderImports();

    helen::ModuleLoadRoutingService routing_service({});
    helen::ModuleLoadRoutingHookSet hook_set(routing_service);

    Expect(hook_set.Install(), "Expected the loader hook set to install on the test runner's main module.");
    Expect(hook_set.IsInstalled(), "Expected the loader hook set to report an installed state.");
    hook_set.Remove();
    Expect(!hook_set.IsInstalled(), "Expected the loader hook set to report a removed state.");
}
```

Run:

```powershell
rtk proxy powershell.exe -NoProfile -Command "& msbuild '.\\HelenGameHook.sln' /p:Configuration=Debug /p:Platform=Win32 /t:HelenRuntime.Tests"
```

Expected: compile fails because `HelenHook/ModuleLoadRoutingHookSet.h` and the new hook set do not exist yet.

- [ ] **Step 2: Run the test again after the header exists**

Once the hook set exists, run:

```powershell
rtk proxy powershell.exe -NoProfile -Command "& '.\\bin\\Win32\\Debug\\HelenRuntime.Tests.exe'"
```

Expected: the hook-set test fails until the `LoadLibrary*` IAT patching and static detour plumbing exist.

- [ ] **Step 3: Write the minimal implementation**

Implement `ModuleLoadRoutingHookSet` as a small IAT hook owner that mirrors the existing file hook set pattern:

```cpp
#include <HelenHook/ModuleLoadRoutingHookSet.h>

#include <HelenHook/Log.h>
#include <HelenHook/Memory.h>

namespace helen
{
    ModuleLoadRoutingHookSet::ModuleLoadRoutingHookSet(ModuleLoadRoutingService& routing_service)
        : routing_service_(routing_service)
    {
    }

    bool ModuleLoadRoutingHookSet::Install()
    {
        const std::optional<ModuleView> main_module = QueryMainModule();
        if (!main_module.has_value())
        {
            return false;
        }

        // Patch LoadLibraryA/W and LoadLibraryExA/W when the current main module imports them.
        // Keep track of whether at least one targeted import was actually present.
        return true;
    }
}
```

The completed implementation should:

- own four `IatHook` members for the four `LoadLibrary*` imports
- keep one static active instance so the detours can reach the routing service
- normalize ANSI and wide module names before passing them to the service
- log the routing decision before forwarding to the original Win32 API
- require at least one targeted loader import to be present, but not require all four imports to exist

Run:

```powershell
rtk proxy powershell.exe -NoProfile -Command "& msbuild '.\\HelenGameHook.sln' /p:Configuration=Debug /p:Platform=Win32 /t:HelenRuntime.Tests"
```

Expected: the hook-set test passes.

- [ ] **Step 4: Commit**

```powershell
rtk proxy powershell.exe -NoProfile -Command "git add include/HelenHook/ModuleLoadRoutingHookSet.h HelenRuntime/ModuleLoadRoutingHookSet.cpp HelenRuntime/HelenRuntime.vcxproj tests/HelenRuntime.Tests/ModuleLoadRoutingHookSetTests.cpp tests/HelenRuntime.Tests/HelenRuntime.Tests.vcxproj tests/HelenRuntime.Tests/TestMain.cpp; git commit -m 'Add Batman Bink loader routing hook set'"
```

## Task 3: Wire the routing hook into Batman startup and verify the log path

**Files:**
- Modify: `HelenGameHook/HelenGameHook.cpp`

- [ ] **Step 1: Write the failing integration check**

Add one runtime smoke check to the startup path so Batman logs are easy to verify after the hook set is enabled:

```cpp
helen::Log(L"[runtime] active-pack init module-load routing install begin.");
if (!g_module_load_routing_hooks->Install())
{
    helen::Log(L"[runtime] failed to install module-load routing hooks.");
    return false;
}
helen::Log(L"[runtime] active-pack init module-load routing hooks installed.");
```

Also add a teardown reset:

```cpp
g_module_load_routing_hooks.reset();
```

Run:

```powershell
rtk proxy powershell.exe -NoProfile -Command "& msbuild '.\\HelenGameHook.sln' /p:Configuration=Debug /p:Platform=Win32 /m"
```

Expected: compile fails until `HelenGameHook.cpp` includes the new header and owns the new hook-set instance.

- [ ] **Step 2: Write the minimal integration**

Update `HelenGameHook/HelenGameHook.cpp` to:

- add `#include <HelenHook/ModuleLoadRoutingHookSet.h>`
- add `std::unique_ptr<helen::ModuleLoadRoutingHookSet> g_module_load_routing_hooks;`
- construct the hook set after `g_file_hooks` or immediately after `g_build_hooks` if that keeps startup logging earlier
- remove the hook set in `ResetPackRuntimeState()` before the process shuts down

Keep the first cut always-on for Batman. Do not add a pack manifest gate in this pass.

Run:

```powershell
rtk proxy powershell.exe -NoProfile -Command "& msbuild '.\\HelenGameHook.sln' /p:Configuration=Debug /p:Platform=Win32 /m"
```

Expected: build passes and the runtime test binary still reports `PASS`.

- [ ] **Step 3: Verify the Batman log path**

Run Batman through the existing Helen bootstrap, load the intro sequence, and inspect the runtime log under the Batman install's `helengamehook/logs/HelenGameHook.log` path.

Expected log line shape:

```text
[runtime] module-load request api=LoadLibraryW requested=binkw32.dll matched=bink alias redirect=<none>
```

If the log never appears, treat that as evidence that Batman is loading Bink through a deeper loader boundary than the four hooked APIs, not as a reason to add a fallback.

- [ ] **Step 4: Commit**

```powershell
rtk proxy powershell.exe -NoProfile -Command "git add HelenGameHook/HelenGameHook.cpp; git commit -m 'Wire Batman Bink loader routing into startup'"
```

## Self-Review Checklist

- The routing service task covers normalization, alias matching, redirect validation, and log formatting.
- The hook-set task covers IAT patching of the four `LoadLibrary*` imports, the static detour bridge, and the install/remove lifecycle.
- The startup task covers the runtime integration point and the actual log verification path in `helengamehook/logs/HelenGameHook.log`.
- There are no placeholder steps, no unnamed helper types, and no unresolved type names across tasks.
- The plan keeps the Bink-specific logic out of `HelenGameHook.dll` and leaves the redirect seam for a later phase.
