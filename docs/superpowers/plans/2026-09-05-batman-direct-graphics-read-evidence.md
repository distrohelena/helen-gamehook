# Direct graphics reads: prerequisite evidence

## State on 2026-09-05

Execution is inline on main as requested. Existing dirty changes and `batma/` are preserved. No game files were written. Installed HelenGameHook.dll SHA256: `3FE2477A99285C366238A6024F0D1CFAFB5EADED893403F4B6341EB073BD25D5`.

Original static dispatch contracts and console probe tests passed freshly: `STATIC_DISPATCH_CONTRACT_PASS`, `DIRECT_READ_CONTRACT_PASS`, `X86_DISPATCH_ABI_PASS`. Scripts require process-local PowerShell `-NoProfile -ExecutionPolicy Bypass`; no machine policy was changed.

## Numeric ABI: static executable evidence

Executable SHA256: `4DAC1F5E2AC6710B7378FDCE74601F616F4753E3756CB5FDA63C7519CC2EB028`. Addresses are preferred VAs; runtime uses module base plus RVA. Inspected read-only with MSVC x86 dumpbin.

- `0x019FD985` calls converter `0x01952F40`; `0x019FD98C` advances argument storage by 16 bytes.
- Converted type is a dword at offset zero, initialized to zero by the caller (`0x019FD96F`). This enters uncoerced source-type dispatch.
- GAS tags 3 and 4 map to numeric argument tag 3 through the table at `0x019530E8`.
- `0x01952FCA` obtains a number through `0x01976720`, stores its double at offset 8, then sets type to 3.
- Return copy `0x01976180` subtracts 2 from the type byte and indexes `0x01976284`. Entry zero selects boolean copy; entry one selects double copy `0x019761CF`. Return type 3 is therefore a double at offset 4, unlike arguments.
- Integer validation must inspect type first and reject nonfinite, fractional, and out-of-range values. Use byte-safe copying for unaligned return payloads; do not coerce strings or booleans.

`Inspect-Dispatch.ps1` pins the corresponding byte windows and jump-table entries. Static evidence is not a live numeric callback test. Numeric codec implementation and red/green console tests remain pending.

## Synchronization prerequisite: not satisfied

- `MemoryStateObserverService::Start` creates a worker; `RunWorkerLoop` calls `PollDueObservers`. Its poll mutex serializes observers, not direct game-thread callbacks.
- `BuildRuntimeCoordinator::HandleObserverUpdate` sets dispatcher keys and runs commands without a graphics transaction lease or catalog generation.
- `BatmanDisplayModeResponseProvider::Resolve` refreshes shared catalogs. Direct reads would publish these from another thread.
- `BatmanDisplayModeService` holds mutable vectors and monitor state without a mutex. `RevalidateMode(kind,index)` references that state while refresh can replace it.
- `BatmanGraphicsConfigService::ApplySelectedResolutionModeToDispatcher` consults the current catalog. The process-wide Apply mutex protects file publication in `ApplyFromDispatcher`, not this lookup or refresh.
- Frontend `FailRollback` clears pending state and unblocks input. `CancelScreen` calls `Destroy`, which clears only frontend state; neither drains an observer command already executing.

This source trace does not prove a live race occurred. It shows that the required native transaction-lifetime exclusion does not exist. A method-level mutex would prevent vector data races but not reinterpretation of a delayed old index against a newly published catalog.

## Decision required

The approved spec explicitly requires stopping if preserving catalog identity needs a write-transaction contract change. Recommend explicit native ownership tying each Apply/rollback sequence to its screen's immutable catalog generation until acknowledgement or explicit cancellation/draining. No timer determines that ownership. Setting encodings and persistence can remain, but the frontend/native lifecycle contract needs design and tests.

Do not silently add that scope or deploy partial synchronization. Tasks 2–6 remain unstarted; Task 1 is incomplete pending this decision and numeric codec tests.
