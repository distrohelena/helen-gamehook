# Session File Routing Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Reusable pack-selected deny/session-redirection with explicit synchronized HelenHook persistence.

**Architecture:** Real redirected files and tracked real handles in HelenRuntime. Existing IAT virtualization delegates selected paths to the routing service. Scoped trusted transactions coordinate Batman persistence; immutable pack declarations select routes.

**Tech Stack:** C++20/Win32, VS MSBuild v143 Release Win32, existing native tests, PowerShell packaging.

**Spec:** `docs/superpowers/specs/2026-09-06-session-file-routing.md`

## Global Constraints

- Work on main, preserving `batma/` and the installed working experiment.
- No polling, sleeps, retries, engine-address persistence, or runtime repair fallbacks.
- Existing subtitle/menu functionality and old packs without routing declarations remain compatible.
- Implementers and ordinary task reviewers use `gpt-5.6-luna` at `high`; no special independent-review session is authorized.
- One class per file, substantive Doxygen on new declarations, RAII, explicit errors and ownership, project conventions. Tests exercise real temporary files and production routing, not source-string presence.
- No deployment while AFK. Scoped commits allowed; no push. All local edits use apply_patch. Do not spawn agents from workers.

## Shared build/test commands

Run native build through the PATH/Path-safe child PowerShell wrapper:

```powershell
rtk proxy powershell.exe -NoProfile -Command "& 'C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe' 'tests/HelenRuntime.Tests/HelenRuntime.Tests.vcxproj' /t:Build /p:Configuration=Release /p:Platform=Win32 /p:PlatformToolset=v143 /m:1 /nodeReuse:false /v:minimal; exit `$LASTEXITCODE"
```

Resolve the test executable from project OutDir and run through `games/HelenBatmanAA/scripts/Invoke-BatmanConsoleTool.ps1` (inspect its parameters first) to suppress crash dialogs. Add tests to project and main registration. Capture actual test counts/output and commands in task report, not just exit status. Compile API scaffolding first if needed for a meaningful behavioral RED, but do not implement behavior before its failing test. Each task updates its own affected tests, runs focused red/green then the complete native suite once before committing. Git metadata writes may require a narrowly scoped escalation.

### Task 1: Session storage, real opens, and trusted transaction core

**Files:** Create `include/HelenHook/FileWritePolicy.h`, `FileReadPolicy.h`, `FileWriteRoute.h`, `FileWriteRoutingService.h`, `FileWriteRoutingTransaction.h`; corresponding implementation sources `HelenRuntime/FileWriteRoutingService.cpp`, `FileWriteRoutingTransaction.cpp`; tests `tests/HelenRuntime.Tests/FileWriteRoutingServiceTests.cpp`; modify native project/header registrations and test main. FileWriteRoute is the resolved runtime type; pack declaration comes later.

**Interfaces:** Produce `FileWritePolicy::{Deny,Redirect}`, `FileReadPolicy::{Original,Redirected}` and `FileWriteRoute` with `std::string Id`, `std::filesystem::path OriginalPath`, `FileWritePolicy WritePolicy`, `FileReadPolicy ReadPolicy`. Service constructor takes cache directory and request-base directory; `bool Initialize(const std::vector<FileWriteRoute>&, DWORD& error)` creates unique session and validates all paths atomically. Methods `bool IsProtectedPath(const std::filesystem::path&) const`, `bool IsTrackedHandle(HANDLE) const`, `HANDLE Open(const std::filesystem::path&, DWORD access, DWORD share, LPSECURITY_ATTRIBUTES, DWORD disposition, DWORD flags, HANDLE templateFile)`, `BOOL Close(HANDLE)`, `DWORD GetAttributes(const std::filesystem::path&)`, `std::unique_ptr<FileWriteRoutingTransaction> BeginTrustedWrite(const std::vector<std::filesystem::path>&, DWORD& error)`. Open/GetAttributes handle both selected and unrelated paths natively; Close is for tracked handles. Transaction `bool Synchronize(DWORD& error)` and `void CancelWithoutWrite()`; destructor latches unsynchronized abandonment. Keep service internals private with transaction friendship; implementation state may be a separately declared private state class/file to keep headers minimal.

- [ ] Write real-temp-file tests for redirect (`original=A`, game writes `B`, real original stays `A`, game read is `B`), deny, original-read policy, CREATE_ALWAYS/truncate, deleted overlay missing (do not recreate), and unrelated pass-through. Use literal byte expectations:
  ```cpp
  assert(ReadOriginal() == "A");
  assert(ReadThroughRouter() == "B");
  ```
- [ ] Run behavioral RED with minimal compiled interface, record failure/output.
- [ ] Implement fresh unique session files and canonical exact matching. Reject unsafe paths/reparse/hard-link ambiguity; matching extended/short aliases must normalize or fail closed, not passthrough. Preserve valid native LastError on API results. Cleanup owns only this unique session directory. Reject overlapped, inheritable, writable mapping support not yet provided. Track all routed handles; one mutex serializes mutation and transactions.
- [ ] Write/run RED for busy transaction (open routed handle => sharing violation and unchanged original); trusted transaction updates original then Synchronize => next routed read updated; failure injection via real filesystem lock => synchronization fails and protected reads fail; abandoned transaction fails closed; no-write cancellation remains usable. Initialize second service/session => starts from original, not prior overlay. No test-only production methods.
- [ ] Implement transaction: atomically validate all named paths and zero affected open handles, retain lock through original write and sync. Synchronize copies verified current originals even after writer recovery; failure latches affected routes. No global bypass, no wait/retry. Empty/unprotected-only transaction must remain a valid no-routing save scope for old configurations.
- [ ] Run complete native tests, self-review ownership/LastError, commit scoped files, write report with exact API and tests.

### Task 2: Mutation-safe IAT adapter and real hooked fixture

**Files:** Modify `include/HelenHook/FileApiHookSet.h`, `HelenRuntime/FileApiHookSet.cpp`, Task 1 routing service files; create `tests/HelenRuntime.Tests/FileWriteRoutingHookFixture.cpp` and a dedicated fixture vcxproj or console-runner PowerShell script; extend `FileApiHookSetTests.cpp`. New adapter helper classes each get their own file if needed.

**Interfaces:** Consume Task 1 service API. Preserve existing FileApiHookSet constructor and add overload taking `FileWriteRoutingService&`. Route selected opens before synthetic read virtualization; route tracked CloseHandle before native close. Add service methods with native-style arguments for Delete, Move (flags), Replace (backup/options), Copy (failIfExists), SetAttributes. Methods operate under the same lock and route every protected operand; unsupported combinations fail before mutation.

- [ ] Add child-fixture behavioral tests that install real IAT hooks then use imported CreateFile/WriteFile/CloseHandle against temp fixtures, failing before adapters exist. Ensure compiler does not optimize tested imports out. Child must load production routing from DLL or equivalent so real helper API calls bypass fixture IAT. Console runner inherits no-dialog ErrorMode.
- [ ] Wire optional routing into FileApiHookSet without changing old constructor behavior. Install required present imports transactionally; missing optional unused API imports are not errors, failed hook for present relevant import is an error. Inspect actual Batman PE imports with pefile before final coverage decisions; record them.
- [ ] Test then implement A/W DeleteFile, MoveFile, MoveFileEx, ReplaceFile, CopyFile and SetFileAttributes paths where imported. Destination selected => redirect; deny => ACCESS_DENIED; selected source moved outside route => explicit failure. Protected backup operands and unsafe multi-route mutations => explicit failure. Cross-volume/async delayed move to selected path => reject before mutation. Do not overwrite original when routing temp publication.
- [ ] Test then reject unsafe DuplicateHandle, writable/named mappings, SetFileInformationByHandle rename/disposition and parent directory mutations affecting selected files. Preserve unrelated native behavior. Handle mapping policy in both A/W imports. Prevent existing original-read handles gaining write access through supported APIs. Never forward an unsupported protected operation to native original.
- [ ] Run hooked fixture covering redirect write/truncate/delete/recreate/temp rename, deny failure, original-read, active-handle trusted-save rejection, unprotected file and synthetic read virtualization coexistence. Add alias/compound-operand regressions. Run native suite and commit; report exact coverage/non-goals.

### Task 3: Validated pack declaration and runtime ownership

**Files:** Create `include/HelenHook/FileWriteRouteDefinition.h`; modify `BuildDefinition.h`, `ActivePackSet.h`, `HelenRuntime/PackRepository.cpp`, `ActivePackSetBuilder.cpp`, `HelenGameHook/HelenGameHook.cpp`, projects and corresponding parser/builder tests. Generic path-root resolver can be new `FileWriteRouteResolver.h/.cpp` with its own tests.

**Interfaces:** Definition fields `Id`, `Root`, `Path`, `WritePolicy`, `ReadPolicy`; parse optional build.json `fileWriteRoutes` with exact spec strings and required lifetime session. BuildDefinition and ActivePackSet expose `std::vector<FileWriteRouteDefinition> FileWriteRoutes`. Resolver converts `game` and `documents` roots to absolute FileWriteRoute entries using installation root and Known Folder Documents; allow injected roots for temp-fixture tests. Service lifetime is shared ownership at runtime to allow direct callback contexts to outlive retirement safely.

- [ ] Parser RED: valid redirect/deny fixtures; invalid/missing fields, invalid policies/lifetime, traversal/ADS/device paths, duplicate IDs/targets fail. Old packs load unchanged.
- [ ] Implement strict schema and active-set merge conflict rejection, including conflicting roots once resolved and virtual/missing overlap. Never silently skip a bad route.
- [ ] Resolver RED: supplied temp Documents root yields exact declared path; game root matches existing virtual-file installation semantics; missing known-folder/unsafe physical target fails initialization.
- [ ] Initialize service and file hooks before routing-dependent build hooks or callbacks can execute. Roll back failures; retired contexts must retain service ownership. Do not expose partial service and do not initialize game-specific routes in generic core.
- [ ] Run native parser/builder/resolver suite, build HelenGameHook Release Win32, commit and document service ownership/injection handoff for Task 4.

### Task 4: Explicit Batman persistence and partial failure

**Files:** Modify `BatmanGraphicsConfigService.h/.cpp`, `BatmanGraphicsApplyOutcome.h`, `BatmanGraphicsSessionService.cpp` if needed; `HelenGameHook/BatmanGraphicsRuntimeContext.h/.cpp`, `BatmanGraphicsRuntime.h/.cpp`, runtime initialization; graphics/subtitle tests. Optional focused persistence adapter class in its own file, not helpers hidden inside methods.

**Interfaces:** Add config-service overload with shared routing ownership, preserve existing constructors by delegating to a valid empty-routing service or explicit optional domain absence. Every runtime Batman service instance gets the same routing owner. Add enum `CommittedSessionSyncFailed = 4`; existing 0..3 unchanged. Consume BeginTrustedWrite/Synchronize/CancelWithoutWrite.

- [ ] Write RED: real two-file save under protected BmEngine route changes both originals and routed BmEngine; busy game handle means NotApplied before either original changes; subtitle save to protected original synchronizes; no-route existing saves remain unchanged.
- [ ] Acquire transaction before any persistence mutation, keep existing two-file recovery logic unchanged inside it, synchronize after both success and recovery, and release only afterward. Reads for HelenHook persistence use originals explicitly, not session scratch values. Do not erase UserEngine/BmEngine deliberate fixture differences except fields intentionally saved.
- [ ] Add fault tests: original commits then overlay copy blocked => enum4, route fail-closed, Apply locked; failed original save+failed sync => integrity uncertain; fully recovered failure+successful sync remains NotApplied. Log original-vs-session failure separately. Existing cleanup failure outcome3 only survives successful synchronization.
- [ ] Ensure direct frontend reports code4 as locked partial failure without claiming saved success. Update real generated frontend behavioral tests if message/branch changes; no polling/deadlines.
- [ ] Run full native suite and affected generated frontend test, build DLL, commit and report.

### Task 5: Separate fresh Batman routing candidate and acceptance guide

**Files:** Add dedicated opt-in routing support to `games/HelenBatmanAA/scripts/Build-BatmanDirectGraphicsCandidate.ps1` and package verifiers, or a focused wrapper `Build-BatmanFileRoutingCandidate.ps1`; tests under same scripts; author `games/HelenBatmanAA/experiments/file-routing/README.md`. Keep normal no-route candidate behavior unchanged.

**Interfaces:** Candidate's build.json adds spec's exact BmEngine documents route to existing direct graphics pack; no other files protected. Runtime DLL and HelenRuntime.lib explicitly passed from fresh Release Win32 build; provenance identifies source commit, artifact hashes, routing policy and live-test pending. Do not promote throwaway no-save probe into normal code.

- [ ] Test candidate verifier rejects malformed/duplicate/wrong-target route and stale DLL using real parser/package load, not text presence. Existing no-route tests remain valid.
- [ ] Build all native sources freshly with isolated output and intermediate dirs, create routing candidate from verified retail Frontend.umap using existing direct pipeline. Run emitted and decompiled frontend behavior tests plus package/delta/rejection tests against new native library.
- [ ] Record hashes, exact commands, imported API coverage, successful native test counts, cleanup/partial-failure semantics, rollback and live-test steps. User test: game automatic resize writes should affect only overlay; explicit Helen save should persist both original and overlay; restart should ignore abandoned scratch. No claim this has been live-tested.
- [ ] Leave installed working package untouched. Run git diff --check, commit source/docs, report candidate path and any genuine limitations; controller requests ordinary final whole-change review then completes handoff.
