# Task 1 report: session file-routing core

## Result

Implemented the generic HelenRuntime session file-routing core on main. The core owns a fresh per-service cache session, validates exact regular originals, initializes redirect overlays, routes native opens and attributes, tracks routed handles, and provides a serialized trusted-write transaction.

## TDD evidence

### RED

Added `tests/HelenRuntime.Tests/FileWriteRoutingServiceTests.cpp`, registered it with the native test project and `TestMain.cpp`, then ran the required Release build before production headers/implementations existed:

```text
rtk proxy powershell.exe -NoProfile -Command "& 'C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe' 'tests/HelenRuntime.Tests/HelenRuntime.Tests.vcxproj' /t:Build /p:Configuration=Release /p:Platform=Win32 /p:PlatformToolset=v143 /m:1 /nodeReuse:false /v:minimal; exit `$LASTEXITCODE"
```

Observed failure:

```text
FileWriteRoutingServiceTests.cpp(1,10): error C1083: Cannot open include file: 'HelenHook/FileReadPolicy.h': No such file or directory
```

### GREEN

After implementing the core, the same build completed successfully, producing:

```text
HelenRuntime.vcxproj -> C:\dev\helenhook\HelenRuntime\bin\Win32\Release\HelenRuntime.lib
HelenRuntime.Tests.vcxproj -> C:\dev\helenhook\bin\Win32\Release\tests\HelenRuntimeTests.exe
```

The required native console invocation then produced:

```text
GENERATED_BATMAN_PROTOCOL_PASS
PASS
```

The tests use real temporary files and exercise redirect reads/writes, original-byte preservation, deny policy, original-read policy, CREATE_ALWAYS truncation, missing-overlay no-recreation, unrelated native pass-through, extended-path recognition, tracked handles, busy trusted transactions, successful synchronization, lock-injected synchronization failure, abandonment fail-closed behavior, no-write cancellation, and fresh-session initialization.

## APIs and implementation files

- `include/HelenHook/FileWritePolicy.h`: `FileWritePolicy::{Deny, Redirect}`.
- `include/HelenHook/FileReadPolicy.h`: `FileReadPolicy::{Original, Redirected}`.
- `include/HelenHook/FileWriteRoute.h`: resolved route value containing `Id`, `OriginalPath`, `WritePolicy`, and `ReadPolicy`.
- `include/HelenHook/FileWriteRoutingService.h` and `HelenRuntime/FileWriteRoutingService.cpp`: initialization, exact/canonical identity matching, real overlay sessions, native `Open`, `GetAttributes`, tracked `Close`, and `BeginTrustedWrite`.
- `include/HelenHook/FileWriteRoutingTransaction.h` and `HelenRuntime/FileWriteRoutingTransaction.cpp`: retained mutex transaction with `Synchronize` and `CancelWithoutWrite`; unsynchronized destruction latches affected routes failed.
- Native project/test registrations and `TestMain.cpp` registration.

## Self-review and concerns

- The service rejects invalid policy combinations, device/alternate-stream/traversal route declarations, reparse traversal, non-regular originals, hard-link ambiguity, duplicate IDs, and duplicate canonical/lexical targets.
- Extended spellings, case/slash normalization, and supported short/identity aliases route to the declared file; aliases that resolve ambiguously or through a reparse form fail closed rather than passing through.
- The transaction has no global bypass and retains one mutex through acquisition, caller-native original writes, and overlay synchronization. Failed synchronization and abandonment latch affected routes so later protected reads fail explicitly.
- This task intentionally does not add mutation hooks, mapping support, duplicate-handle support, or Batman schema/pack adapters; those belong to later tasks.
- Destructor cleanup removes only this service's unique session directory. Crash leftovers are inert and are never reused; process-detach teardown must remain outside loader-lock filesystem work in the runtime owner.
- Routed handles must be closed through the routing adapter's `Close` path so the service can remove tracking; raw external `CloseHandle` calls are outside this core API's observation boundary.

## Commit

The implementation and report are in the scoped commit currently being handed off; its final SHA is recorded in the handoff message after this metadata amendment.

## Round-1 corrections

The original report intentionally disclosed that the first RED run was a missing-header compile failure rather than a behavioral RED. This correction round added behavioral regressions before the fixes. Against the committed core, the focused console run failed with:

```text
Trusted write accepted an unverifiable protected path.
GENERATED_BATMAN_PROTOCOL_PASS
```

The regression used an exclusive real original-file handle and proved that `BeginTrustedWrite` could otherwise acquire a transaction without verifiable metadata access. The regression also asserts busy-save original bytes remain unchanged.

The correction adds:

- `FileWriteRoutingService::PathDisposition` and `ClassifyPath`, with `Rejected` kept distinct from native `Unrelated` pass-through.
- Share-compatible trusted-original preflight used by classification and trusted acquisition, so locked/indeterminate known route aliases fail closed with the native error.
- Replacement-aware synchronization: a verified regular, non-reparse, single-link replacement is accepted, copied to the overlay, and its native identity/canonical alias is refreshed. A real `ReplaceFileW` test covers this path.
- Optional short-name, hard-link, and symbolic reparse alias tests where Windows permits fixture creation.
- Cleanup failure logging for the service-owned session directory.

After correction, the focused routing test executable exited `0`. The final required full Release Win32 build command completed successfully, and the required console invocation produced:

```text
GENERATED_BATMAN_PROTOCOL_PASS
PASS
```

The correction commit SHA is recorded in the final handoff message.

## Evidence command ledger

The round-1 behavioral RED was run with the required Release build command:

```text
rtk proxy powershell.exe -NoProfile -Command "& 'C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe' 'tests/HelenRuntime.Tests/HelenRuntime.Tests.vcxproj' /t:Build /p:Configuration=Release /p:Platform=Win32 /p:PlatformToolset=v143 /m:1 /nodeReuse:false /v:minimal; exit `$LASTEXITCODE"
```

followed by:

```text
powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "& './games/HelenBatmanAA/scripts/Invoke-BatmanConsoleTool.ps1' -FilePath 'C:/dev/helenhook/bin/Win32/Release/tests/HelenRuntimeTests.exe' -Arguments @()"
```

Observed RED output:

```text
GENERATED_BATMAN_PROTOCOL_PASS
Trusted write accepted an unverifiable protected path.
Console tool 'C:/dev/helenhook/bin/Win32/Release/tests/HelenRuntimeTests.exe' exited with code 1.
```

Focused GREEN used the same required console invocation against the same exact executable after temporarily making `TestMain.cpp` call only `RunFileWriteRoutingServiceTests`; the wrapper retained no text output in that run, so the executable was additionally invoked directly:

```text
& 'C:/dev/helenhook/bin/Win32/Release/tests/HelenRuntimeTests.exe'; Write-Output "EXIT=$LASTEXITCODE"
```

with:

```text
EXIT=0
```

The temporary focused entry-point change was reverted before the full run. Final full GREEN used the exact required Release build command above, then the exact required console invocation above, producing:

```text
GENERATED_BATMAN_PROTOCOL_PASS
PASS
```
