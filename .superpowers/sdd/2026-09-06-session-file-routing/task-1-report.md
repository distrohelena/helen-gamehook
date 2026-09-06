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
