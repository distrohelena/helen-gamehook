# D3D9 Dispatch and Locking Implementation Plan

> **For agentic workers:** Use superpowers:executing-plans inline, as announced and approved for this session. Preserve the user's main worktree; do not delegate or create a separate review session without permission.

**Goal:** Restore correct internal surface dispatch and eliminate registry-lock reentrancy from replacement creation/reset.

**Architecture:** A separately synchronized original-method registry resolves shared COM calls independently of tracking. Replacement work snapshots state, executes external calls unlocked, and publishes only against a surviving operation identity. Reset uses the same cache path as initial uploads.

**Tech Stack:** C++20, MSVC v143, Win32 D3D9, PowerShell, native console tests.

**Spec:** `docs/superpowers/specs/2026-09-07-d3d9-dispatch-locking-design.md`

## Global constraints

- Work on main; preserve unrelated `batma/` and existing uncommitted reset changes.
- One class per file; substantive Doxygen; RAII; no recursive mutex, timers, polling or silent success on failure.
- Never hold tracking or dispatch locks across COM calls; copied dispatch addresses outlive the lookup lock.
- Keep the no-save probe, session routing, packs, original INIs and backups unchanged.
- Console-only testing, no crash dialogs; hardware fixture needs desktop-capable escalated execution.
- No installation until verification passes and Batman is closed.

## Task 1: Original dispatch registry and forwarding

**Files:** create `include/HelenHook/ComOriginalDispatch.h`, `HelenRuntime/ComOriginalDispatch.cpp`, `tests/HelenRuntime.Tests/ComOriginalDispatchTests.cpp`; modify runtime/test projects and TestMain; modify D3d9TextureReplacementHookSet.

**Interfaces:** `ComOriginalDispatch::Capture(void** table, size_t count)` returns an owned vector copy; `Resolve(void* instance, size_t slot)` returns a copied original address or the live address of an uncaptured table; `Replace(void** table, const std::vector<void*>& originals)` publishes refreshed originals. Invalid arguments/slots are explicit failures. No COM calls inside the class.

- [x] Add console tests exercising originals after shared-table patching, repeated capture, a second object sharing the table, independent tables, refresh and invalid slot detection. Use local table data with distinct address tokens:

```cpp
void* table[] = { &first, &second };
void** object = table;
dispatch.Capture(table, 2);
table[1] = &detour;
Expect(dispatch.Resolve(&object, 1) == &second, "Shared object lost original dispatch");
```

- [x] Observe RED against a minimal implementation, then implement mutex-protected snapshot copying/lookup. Preserve existing snapshots on repeated capture; validate refresh size. Never store references into mutable maps in callers.
- [x] Route shared texture/surface methods through original dispatch even without tracking, preserve original results, and keep cleanup specific to tracked objects. Publish originals before patches. Audit mutex nesting while integrating; do not ship forwarding while the cache still holds tracking locks.
- [x] Run dispatch tests and the existing hardware reproduction against a newly built runtime. Retain the baseline reproduction log from `output/d3d9-reset-fixture-1ae536969ee6434294c52cc685e578ba/fixtures/hook.log`.

## Task 2: Unlocked replacement lifecycle

**Files:** modify `HelenRuntime/D3d9TextureReplacementHookSet.cpp`, its header as needed; create focused operation state class/test files if needed. Update project registration for every new compilation unit.

**Interfaces:** cache operation receives a valid caller-owned device reference and a source identity key; it does not dereference that key during unlocked work. It returns success only for a valid cached/published resource. CPU operation identity is separate from COM ownership.

- [x] Add regression cases for initial upload after surface registration and for duplicate/stale operation invalidation. Cover failure disposal and reentrant lookup without deadlock.
- [x] Implement prepare/execute/publish using scoped locks, owned copies and an explicit in-progress token. The publish predicate must compare the surviving record's operation identity and generation, not just its raw source pointer:

```cpp
if (currentState != capturedState || !capturedState->IsCurrent(operation)) {
    failure_result = D3DERR_INVALIDCALL;
    return false; // RAII disposes the unpublished texture after unlocking.
}
```

- [x] Resolve/CreateTexture/LockRect/UnlockRect and all reference releases outside state locks. Obtain device ownership from a valid caller reference; do not acquire source ownership from a stale raw registry pointer. During reset use source identity only and validate surviving state before publication.
- [x] Remove outer cache locks from texture unlock, surface unlock and reset. Copy source bytes while locked before forwarding UnlockRect; never hash stale post-unlock memory. Ensure every original unlock call is outside the mutex and occurs exactly once.
- [x] Inspect actual bindings outside locks and rebind only stages still referring to the source, removing stale stage-map/last-stage fallback writes. Propagate binding errors explicitly.
- [x] Invalidate operation state at reset/release; collect owned replacements for release outside locks. Keep native failure and pending restoration behavior; adopt changed driver dispatch after native Reset without recording detours as originals.
- [x] Audit touched paths for COM calls under locks, raw context lifetime across external calls and return paths that abandon in-progress state. Run focused tests plus repeated hardware reset/recovery.

## Task 3: Fresh verification and handoff

**Files:** clean `tests/HelenRuntime.Tests/D3d9ReplacementResetTests.cpp`; update `docs/superpowers/reports/2026-09-07-d3d9-replacement-reset-fix.md`; isolated output artifacts only.

- [x] Remove redundant generated DDS before copying Batman's actual subtitle asset. Add content/dimensions assertions and retain all reset/failure/recovery cases.
- [x] Build native tests using MSBuild v143 Release Win32 with `FreshBuildIsolation.targets`, unique output root and `rtk proxy` to normalize the environment. Copy the test executable into `native/tests` for export-test layout, then run with `Invoke-BatmanConsoleTool.ps1`.
- [x] Build a unique no-save candidate using `Prepare-NoSaveProbe.ps1` and `NoSaveProbe.targets`. Run:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tests\Invoke-D3d9ReplacementResetTests.ps1 -RuntimeLibraryPath <fresh-root>\native\HelenRuntime.lib
powershell.exe -NoProfile -ExecutionPolicy Bypass -File games\HelenBatmanAA\experiments\windowed-resolution\Test-NoSaveSession.ps1 -RuntimeLibraryPath <fresh-root>\native\HelenRuntime.lib
```

The fresh root is selected once at execution and recorded in the report; the commands must never silently select an old library.

- [x] Verify package/delta with the unchanged candidate menu assets, fresh library/DLL and pinned fresh hash; run `git diff --check`.
- [x] Record every result honestly. Only after verification, check Batman is closed and prepare a hash-pinned DLL-only install with a new rollback directory and preserved-file inventory. If running, request closure instead of terminating it.
- [x] Ask for live resolution/subtitle acceptance; do not claim the freeze fixed from automated tests alone. User reported "all normal" after the surface-parent candidate and explicitly authorized the commit.

## Execution checkpoint

Implementation and automated verification completed inline on main. The verified DLL was installed after Batman closed; hashes, fixture output roots and rollback paths are recorded in `docs/superpowers/reports/2026-09-07-d3d9-replacement-reset-fix.md`. No implementation commit was made. User live acceptance is the remaining checkpoint.

The first live resize returned, but Continue Game exposed a stale parent-texture
registration after final surface release. A public-API DXT1 allocation regression
reproduced the installed build's failure. Explicit parent ownership around surface
release and tracking all mip surfaces passed fresh regression/reset/subtitle,
failure, native, no-save and package checks. A fresh DLL-only candidate was installed
with rollback and unchanged-file verification; gameplay acceptance is still pending.
