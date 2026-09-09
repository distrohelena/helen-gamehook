# Owned Memory Patch Implementation Plan

> **For agentic workers:** Use superpowers:executing-plans inline, as requested.

**Goal:** Preserve patch ownership across Win32 partial failures.
**Architecture:** MemoryPatch owns bytes and protection recovery; hooks retain
that owner; legacy entry points roll back or stop before dependent state is freed.
**Tech Stack:** C++20, Win32 x86, MSVC, PowerShell console fixtures.
**Spec:** `docs/superpowers/specs/2026-09-08-owned-memory-patch-design.md`

## Global constraints

- Inline on main; no commit or installed game changes.
- No polling, concurrent patch execution, arbitrary instruction relocation or SEH recovery.
- One class per new file and substantive Doxygen documentation.
- Test-only Win32 interception stays under tests, not in production controls.

## Task 1: Demonstrate and repair owned writes

- [x] Add `tests/HelenRuntime.Tests/MemoryPatchFailureTests.cpp`, test-only Win32
  interception and `Test-MemoryPatchFailures.ps1`. First reproduce ignored cache
  failure against existing code: `Expect(!WriteMemory(...), "flush failure reported success")`.
- [x] Run fixture and record the expected failure, not a compiler failure.
- [x] Add `MemoryPatch`, `MemoryPatchResult`, `MemoryProtectionRegion` headers
  and `HelenRuntime/MemoryPatch.cpp`. `Apply(void*, const void*, size_t)` and
  `Restore()` return a result; `HasOwnership()` is the cleanup obligation;
  `Commit()` only accepts a successful application. Preserve initial protections.
- [x] Wire project sources. Replace legacy writes with `Apply`, `Commit` on
  success, otherwise `Restore`; stop if restoration cannot complete.
- [x] Test real bytes/protections and injected initial-protect/cache/restore
  failures, including cross-region ranges and explicit cleanup retry.

## Task 2: Retain hook ownership

- [x] Reproduce existing removal loss with injected VirtualProtect refusal;
  assert the hook still owns its trampoline and original target bytes.
- [x] Split InlineHook/IatHook declarations into individual headers, retaining
  `Hook.h` as the compatible include. Add TryInstall/TryRemove and owned patches.
  Preallocate trampoline and patch arrays before publication. Legacy Install
  rolls back on false; legacy Remove cannot return after failed recovery.
- [x] Test inline/IAT partial installation, restoration failure, retry, and
  callable healthy paths. Child cases verify unrecoverable legacy failures stop
  before dependent state can be released, with no dialog.

## Task 3: Integration and evidence

- [x] Build fresh opt-in and normal runtime/proxy pairs with existing isolated
  build scripts. Run lifecycle fixtures and full native suite against fresh lib.
- [x] Run targeted D3D regressions as permitted by the environment.
- [x] Inspect diff and failure contracts, record exact outcomes and remaining
  VSync gates; verify installed DLL SHA256. Leave changes uncommitted.

## Completion evidence

Final artifacts are under `output/owned-patch-probe-20260908-b/native` and
`output/owned-patch-normal-20260908-b/native`, not the earlier lifecycle-only
directories. Both full builds and real-runtime hosts passed. The full native
suite under `tests-5c04a7656033402f85a04a8811514035` and the three D3D9 variants
passed against the final opt-in library. The standalone failure suite is
`output/memory-patch-e24f8218ecf040a4ac3d4dd5917a3771` (six groups, five expected
fatal children), compiled after both deliberate regression mutations were removed.
Detailed hashes, red/green evidence and limits are in the static investigation
report. Review stayed inline as requested; no separate independent review ran.
The approved shared-writer amendment is complete; retail VSync binding gates
remain outside this amendment. No commit, package deployment or game launch.
