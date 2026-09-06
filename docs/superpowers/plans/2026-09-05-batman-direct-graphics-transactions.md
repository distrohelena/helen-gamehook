# Batman Direct Graphics Transactions Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking. Helena already selected inline execution on main; do not ask again or launch subagents.

**Goal:** Replace graphics reads and Apply polling with synchronous, validated callbacks and native-owned sessions, catalogs, and transactions.

**Architecture:** Read once into session-owned state, stage edits without global side effects, and commit through the existing INI publication/reconciliation algorithm with typed outcomes. Resolution indices resolve only within immutable session catalogs. Direct callbacks return completed outcomes and reject stale or conflicting operations.

**Tech Stack:** x86 C++20/MSVC v143, generated ActionScript 2/C# builder, PowerShell, Node.js, FFDec, native pack/delta verification.

**Spec:** `docs/superpowers/specs/2026-09-05-batman-direct-graphics-transactions-design.md`, approved commit `d2b32ca`.

## Execution checkpoint: direct test candidate installed, 2026-09-05

The native session/transaction service, allowlisted primitive adapter, owned runtime binding, and synchronous frontend are implemented. Native aggregate tests pass, including same-thread/concurrent session rejection, duplicate transaction consumption, stock x86 forwarding, stock exception propagation and runtime retirement. The DLL export test now resolves the DLL beside the active test executable instead of accidentally loading an older Debug output. MSVC `/EHsc` required explicit `noexcept(false)` on the C-linkage dispatch boundary to preserve the stock exception test.

Actual emitted and compiled/FFDec-decompiled ActionScript pass `Test-BatmanDirectGraphicsFrontend.js`: no graphics timer/carrier calls, strict scalar decoding, normalized MSAA, directional edits, catalog switching, successful/failed/uncertain commit states, missing callbacks, rejected staging cancellation and native close ownership. The numeric codec and all nine static executable byte contracts passed again. These are automated tests, not live gameplay acceptance.

An isolated test-candidate path was added rather than activating the legacy release package while its validators are still being migrated:

- `Build-BatmanDirectGraphicsCandidate.ps1` recompiles managed builders, generates into a unique directory, patches the verified retail frontend and creates fresh manifests/delta; it takes an explicitly separately built native DLL/library.
- `Test-BatmanDirectGraphicsPackage.ps1` uses the production native parser and delta reader, checks the exact eight-file pack, one production dispatch hook/export, and absence of graphics observers/commands/config/slots or unrelated behavior.
- Rejection tests passed for duplicate hooks, a probe export, a wrong signature, stale delta metadata and legacy commands.
- `Deploy-BatmanDirectGraphicsCandidate.ps1` reuses the existing rollback-safe four-artifact deployment helper. Its regression suite passed. It requires explicit GameBin and preserves the subtitle pack, proxy and enabled-pack config hashes.

Clean native outputs: `output/batman-direct-transactions-20260905/native/`. The linker command log confirms the candidate DLL linked that directory's freshly rebuilt `HelenRuntime.lib`. The clean build reported existing C4244 warnings in generic `Hook.cpp` and a project-reference intermediate-directory warning; no compilation/link failure occurred.

Installed candidate: `output/batman-direct-graphics/candidate-03187d8e3aef470f965c8caa8e91d6a0/`.
Installed DLL SHA256: `A9A5FA68B61E943A77DB97F51EC2E7ADF16E48B222A21AD45FF62D95601E0CA2`.
Exact reconstructed frontend SHA256: `F73FD07D5205EAE39D58495E1FCC395761E5E99C9E083A519133220FA133474C`.
Durable pre-install backup: `output/batman-direct-graphics/rollback-3dd583e991d9412594b9e32a58d86779/`.
Only subtitles and graphics remain enabled. Batman was closed at installation. Installed files passed exact hash/snapshot verification; temporary deployment staging was verified empty and removed.

### User-confirmed live acceptance

After installation of the candidate recorded above, Helena confirmed that the graphics menu works and loads immediately. She then confirmed the requested VSync change/Apply/reopen test, followed by the requested Fullscreen/Resolution change/Apply/restart test. These are user-reported gameplay results, distinct from automated verification. Implementation and candidate tooling are committed on main as `d18059d`.

At the acceptance commit, the frontend behavior test passed again. The native aggregate suite initially failed the subtitle upsert fixture, then passed in a subsequent process without source changes. Inspection found that the PID-only temporary fixture directory selected by the failed run already contained `BmGame.ini` and other files created on September 4. The subtitle resolver prefers that existing sibling, whereas the upsert fixture expects no sibling. This identifies an outstanding test-isolation issue with reused process IDs; no game files or implementation code were changed to obtain the rerun. Do not treat this as consistently clean native-suite execution until fixture isolation is corrected.

Still pending: log review; migration of the original stable pack/rebuild/release validators; shared generated field/outcome constants; remaining exhaustive failure/identity/capture-count coverage listed below; correction of the PID-reuse fixture issue. Do not interpret this checkpoint as completion of every checkbox or a release-ready package. Do not use the old stable rebuild/deploy route to reproduce this direct test candidate.

## Global Constraints

- No graphics request polling, response toggles, scan discovery, sleeps, retry timers, or timeout-based ownership remains.
- This does not remove unrelated subtitle or runtime observers.
- No side effects occur while merely staging fields.
- Duplicate Commit for a consumed transaction must not write twice.
- Uncertain disk state locks further Apply for the process and remains visible after closing/reopening graphics.
- Do not broaden this work into new live engine-setting functionality.
- Work inline on main, preserve all unrelated dirty changes, and never consume `batma/`, installed packs, old archives, or historical generated assets as build inputs.
- One class per file; substantive Doxygen for every added member; explicit required initialization; no nullable substitutes for required services, local helper functions, or tuple abstractions.
- Never deploy while Batman runs. Keep the working direct Fullscreen probe until the whole candidate passes automated checks.
- Console test failures only. Native test mains use `SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX)`; preserve managed no-dialog execution wrappers.

This plan replaces `2026-09-05-batman-direct-graphics-reads.md`; do not execute its shared mutable catalog or unchanged-write tasks. Task 1 evidence in `2026-09-05-batman-direct-graphics-read-evidence.md` remains valid. The synchronization blocker is resolved architecturally by removing the graphics carrier writer, not by claiming its old lifecycle safe.

## File map and test/build convention

New public domain units go in `include/HelenHook/<Name>.h`, implementations in `HelenRuntime/<Name>.cpp`. Register sources in `HelenRuntime/HelenRuntime.vcxproj`; update matching filters if present. New tests go in `tests/HelenRuntime.Tests/<Name>Tests.cpp`, registered in its `.vcxproj` and `TestMain.cpp` using the existing `Run<Name>Tests()` convention.

Units: `BatmanGraphicsField` (enum), `BatmanGraphicsSnapshot` (partial read state), `BatmanGraphicsDraftState` (validated complete draft extracted from the config service's anonymous namespace), `BatmanDisplayCatalog` (immutable owned catalog), `BatmanGraphicsApplyResult` (typed persistence result), `BatmanGraphicsSessionService` (state/lifetime), and `BatmanGraphicsTransaction` (staged state). Keep each in its own file.

Hook-only units: `HelenGameHook/BatmanGraphicsPrimitiveCodec.h/.cpp` and `HelenGameHook/BatmanGraphicsExternalInterface.h/.cpp`. Register in `HelenGameHook/HelenGameHook.vcxproj`; bind from `HelenGameHook/HelenGameHook.cpp` before hook installation.

Existing sources requiring focused changes: `HelenRuntime/BatmanGraphicsConfigService.cpp`, `HelenRuntime/BatmanDisplayModeService.cpp`, their public headers, and `games/HelenBatmanAA/builder/tools/NativeSubtitleExePatcher/SubtitleSizeModBuilder/GraphicsOptionsShellScriptTemplates.cs`.

For every task: write one behavior test, run it red, implement the smallest passing change, run that test and affected regressions, then commit only reviewed task hunks. Repeat inside a task instead of writing its entire implementation before tests. A missing declaration may be introduced as scaffolding; the red test must then exercise an unimplemented behavior, not merely fail compilation.

Native build command (from repo root; check every exit code):

```powershell
rtk proxy powershell.exe -NoProfile -Command "& 'C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe' 'tests/HelenRuntime.Tests/HelenRuntime.Tests.vcxproj' /t:Build /p:Configuration=Release /p:Platform=Win32 /p:PlatformToolset=v143 /m:1 /nodeReuse:false /v:minimal; exit `$LASTEXITCODE"
& bin/Win32/Release/tests/HelenRuntimeTests.exe
```

For isolated candidate builds use explicit distinct OutDir/IntDir per project; build runtime before hook with `/p:BuildProjectReferences=false` on the second build. Do not use an unevaluated `$(MSBuildProjectName)` in command-line IntDir. PowerShell scripts need process-local `powershell.exe -NoProfile -ExecutionPolicy Bypass -File <path>`; never change machine policy.

## Task 1: Verify and test the primitive ABI

**Execution checkpoint:** Numeric codec implemented and tested red/green on 2026-09-05. The first runnable test failed on argument decoding; after implementing reads it failed on return encoding; the completed codec then passed under x86 `/W4 /WX`. Existing boolean/stock-dispatch tests and all nine pinned executable byte contracts passed freshly. No live expanded callback or deployment has occurred. Hook project registration remains with Task 6, when the adapter consumes this unit.

The codec has a dedicated console runner in `tests/HelenRuntime.Tests/BatmanGraphicsPrimitiveCodecTests.cpp`, compiled by `games/HelenBatmanAA/scripts/Test-BatmanGraphicsPrimitiveCodec.ps1`; it is deliberately not added to the aggregate test executable because it has its own no-dialog main. The existing probe runner remains unchanged. This isolates native primitive tests from runtime and game dependencies.

**Files:** Create hook primitive codec, extend `games/HelenBatmanAA/experiments/external-interface-probe/ProbeTests.cpp` and `Test-Probe.ps1`; use existing `Inspect-Dispatch.ps1` without changing the installed package.

**Interface:** `BatmanGraphicsPrimitiveCodec::ReadUnsigned(const void* arguments, unsigned count, unsigned index) -> std::optional<uint32_t>`; `WriteNumber(PrimitiveResult&, uint32_t) -> void`. `PrimitiveResult` is the experiment's verified 16-byte return structure, promoted to a separately named production header `HelenGameHook/BatmanGraphicsPrimitiveResult.h` and shared with test code. Count/arity is also checked by the adapter.

- [ ] Record branch/status/index and hashes of installed DLL and all enabled graphics-pack files. Preserve the starting diff outside build inputs. Read AGENTS.md and no-dialog test instructions before compilation.
- [ ] Run static signature inspection and existing probe tests. Expected executable SHA256: `4DAC1F5E2AC6710B7378FDCE74601F616F4753E3756CB5FDA63C7519CC2EB028`; existing markers `STATIC_DISPATCH_CONTRACT_PASS`, `DIRECT_READ_CONTRACT_PASS`, `X86_DISPATCH_ABI_PASS`.
- [ ] Add an actual codec test using aligned raw byte storage, not a mock codec:

```cpp
std::array<std::byte, 16> argument{};
const uint32_t type = 3;
const double value = 1920.0;
std::memcpy(argument.data(), &type, sizeof(type));
std::memcpy(argument.data() + 8, &value, sizeof(value));
assert(BatmanGraphicsPrimitiveCodec::ReadUnsigned(argument.data(), 1, 0) == 1920);
```

Use the suite's throwing assertion mechanism rather than disabled Release `assert` in final tests. Test values 0, 1, 98, 1920, UINT32_MAX; reject type 2/4, negative, fractional, NaN, infinity, overflow, null storage with positive count, and out-of-range index. Return tests inspect tag byte 3 and memcpy a double from offset 4, including untouched guard bytes.
- [ ] Implement strict type-first decoding using the pinned 16-byte stride and offset-8 argument double, `std::isfinite`, integer/range checks before conversion, and memcpy for unaligned result writes. No managed object/string allocations.
- [ ] Run red/green codec tests and stock-forwarding ABI tests; document static versus console versus live evidence. Commit only codec/test/evidence files.

## Task 2: Read snapshots and side-effect-free complete drafts

**Files:** Create field/snapshot/draft headers, `tests/HelenRuntime.Tests/BatmanGraphicsSnapshotTests.cpp`; modify config service header/source and test registration.

**Interface:** `CaptureReadSnapshot() const -> BatmanGraphicsSnapshot`; `BatmanGraphicsSnapshot::Get(BatmanGraphicsField) const -> std::optional<int>`; `TryCreateDraft() const -> std::optional<BatmanGraphicsDraftState>`. Draft setters validate normalized field values and maintain shared Detail leaf/preset semantics; no default construction of an invalid complete draft.

Field IDs: Fullscreen=0, Vsync=1, Msaa=2, Bloom=3, DynamicShadows=4, MotionBlur=5, Distortion=6, FogVolumes=7, SphericalHarmonicLighting=8, AmbientOcclusion=9, Physx=10, Stereo=11, PersistedWidth=12, PersistedHeight=13, DesktopWidth=14, DesktopHeight=15, CanApply=16. Desktop/CanApply are session projections, not parsed INI keys. Detail is derived from leaves, not independently transported.

- [ ] Write fixture tests for valid single-byte and UTF-16 INIs, missing file, missing one field, invalid MSAA/PhysX, inconsistent dimension pair, and partial Detail leaves. Verify exact file bytes and dispatcher values before/after capture remain equal.

```text
Capture valid Windowed/1600x900 fixture -> Fullscreen=0, Width=1600.
Remove VSync only -> Vsync absent, Fullscreen still 0, TryCreateDraft absent.
Capture valid 16x MSAA -> normalized Msaa=5, not menu position 4.
```

- [ ] Extract existing parsing/normalization into shared per-field operations; the current all-or-nothing LoadIntoDispatcher delegates to a complete draft conversion. Capture reads UserEngine.ini once and getters perform no I/O. Preserve existing writer/preset tests.
- [ ] Test draft mutation with no dispatcher, config store, or file changes. A rejected field leaves the draft unchanged. Preserve configured resolution even when its fullscreen catalog is unavailable.
- [ ] Run native suite, inspect the focused diff, and commit snapshot/parser extraction.

## Task 3: Typed persistence outcomes without a second writer

**Files:** Create `BatmanGraphicsApplyResult.h`; modify config service's `PublishBatmanGraphicsIniPair`, reconciliation helpers, and public Apply API; extend `tests/HelenRuntime.Tests/CommandExecutorTests.cpp` and create `BatmanGraphicsPersistenceTests.cpp`.

**Interface:** `ApplyDraft(const BatmanGraphicsDraftState&) const -> BatmanGraphicsApplyResult`. Result contains `Outcome` and recovery diagnostics; enum values: Committed=0, NotApplied=1, IntegrityUncertain=2, CommittedCleanupFailed=3. NotApplied means no target publication or both original target byte sequences verified restored; it is not a blanket mapping from old false returns. Existing bool ApplyFromDispatcher delegates and retains its caller contract deliberately.

- [ ] Add failure injection around actual filesystem operations, reusing the existing command-executor test interception infrastructure after inspecting it. If that infrastructure cannot cover an operation, introduce a narrow required file-operation interface in `BatmanGraphicsFileOperations.h` with a Win32 implementation; do not add production methods that exist only to configure test failures.
- [ ] Write tests against real temp INI pairs with operation failures injected at these points:

```text
First stage write fails -> NotApplied, original targets unchanged.
Second target publish fails; reconciliation succeeds -> NotApplied, exact originals.
Second target publish fails; reconciliation fails -> IntegrityUncertain, recovery files retained.
Both target replacements succeed; cleanup fails -> CommittedCleanupFailed, new target bytes.
Both replacements and cleanup succeed -> Committed, new target bytes, no owned residue.
Exception after possible publication -> reconcile or IntegrityUncertain; never harmless undefined.
```

- [ ] Implement result propagation in the existing writer, preserving encoding, unrelated lines, sibling file naming, and exact-byte reconciliation. Only clean recovery evidence when the applicable outcome permits it. Verify new target bytes before reporting committed outcomes; cleanup failure after verified publication must not trigger a second Apply.
- [ ] Add a process-lifetime integrity latch to the owning persistence service, queried by `IsApplyLocked() const -> bool`. Set it when consistency is uncertain; no session reset clears it. Serializing graphics persistence still uses the existing transaction lock, with documented lock ordering.
- [ ] Run existing publication/rollback tests and new cases red/green. Commit typed persistence changes independently.

## Task 4: Immutable display catalogs

**Files:** Create `BatmanDisplayCatalog.h/.cpp`; modify display service header/source; extend `BatmanDisplayModeServiceTests.cpp`.

**Interface:** `CaptureCatalog(BatmanDisplayModeCatalogKind, int width, int height) -> std::optional<BatmanDisplayCatalog>`; `RevalidateMode(const BatmanDisplayCatalog&, size_t) -> std::optional<BatmanDisplayMode>`. Catalog has const accessors for kind, modes, device identity, original configured pair, and desktop pair. It owns its data; no references into service scratch vectors.

- [ ] Write the ownership regression test:

```text
Capture catalog A; record pair at index i.
Change enumeration and capture catalog B with different ordering.
RevalidateMode(A,i) must return A's pair if still supported, never B[i].
Remove A's pair or change device -> explicit failure.
```

- [ ] Extract catalog construction and revalidation from existing service logic; legacy APIs can adapt to it without changing their tests. Capture doesn't sort again during getters. Preserve maximum 98 entries, exact custom windowed size, work-area rules, and supported-only fullscreen rules.
- [ ] Verify failure in either catalog doesn't corrupt the other or scalar reads. Commit after the display suite passes.

## Task 5: Session and transaction state machine

**Files:** Create session service and transaction units and `BatmanGraphicsSessionServiceTests.cpp`.

**Interfaces:** Session service owns required config/display service references. Methods `Open() -> optional<uint32_t>`, `Get(session,field) -> optional<int>`, `ModeCount(session,kind)`, `ModeWidth/ModeHeight(session,kind,index) -> optional<int>`, `EndRead(session) -> bool`, `BeginApply(session) -> optional<uint32_t>`, `SetField(session,transaction,field,value) -> bool`, `SetResolution(session,transaction,kind,index) -> bool`, `Commit(session,transaction) -> optional<BatmanGraphicsApplyResult>`, `CancelApply(session,transaction) -> bool`, `Close(session) -> bool`.

Use a separate nonwrapping uint32 identity source for sessions and transactions. State is at most one session plus one staged transaction; no per-movie map. Use a scoped nonblocking entry lock so concurrent or reentrant entries reject immediately rather than deadlock; retain the entry through Commit/reconciliation, never across frontend calls. Stock callbacks do not acquire it. Identity exhaustion fails explicitly.

- [ ] Write state-machine tests using real parser/catalog services and the narrow persistence dependency from Task 3. Test Open replacement, stale IDs, EndRead disabling getters but retaining Apply baseline/catalogs, Close/Cancel discarding without writes, missing baseline preventing BeginApply, and duplicate Commit producing zero further publication calls.
- [ ] Stage from captured complete baseline; accept only writable fields 0–11. Set Fullscreen before SetResolution; display-kind changes require explicit valid resolution selection. Scalar-only Apply with unchanged display settings preserves the configured pair and does not require inventing a catalog entry. Any explicitly selected display pair is revalidated at commit.
- [ ] Test concurrent/reentrant behavior deterministically with a test-controlled operation barrier, not sleeps:

```text
Commit enters persistence barrier and holds session A/catalog A.
Other entry attempts Open, Close, CancelApply -> explicit rejection, no state replacement.
Release barrier; Commit returns outcome; completed transaction cannot run again.
IntegrityUncertain -> CanApply=0 after Close/Open; getters remain usable.
```

- [ ] On NotApplied keep committed baseline and consume failed transaction; allow a fresh attempt with a fresh ID. On committed outcomes update baseline to the submitted normalized draft. On IntegrityUncertain lock Apply and retain diagnostics through reopen. Cancel of an already consumed transaction fails, never claims to undo publication.
- [ ] Add capture/enumeration counters and delayed-capture tests proving one INI capture per Open and no getter I/O or elapsed-time validity decision. Run native suite and commit the service.

## Task 6: Production callback adapter and frontend

**Files:** Create hook adapter and `GraphicsTransactionContract.cs` beside the source template; modify hook initialization/project; create `games/HelenBatmanAA/scripts/Test-BatmanDirectGraphicsFrontend.js`; update shell contract tests.

**Wire interface:** Exact callback prefix `Helen_Graphics_`, suffix `V1`; names OpenV1, GetV1, ModeCountV1, ModeWidthV1, ModeHeightV1, EndReadV1, BeginApplyV1, SetFieldV1, SetResolutionV1, CommitV1, CancelApplyV1, CloseV1. Arguments follow Task 5 in order. Count/width/height/get return numeric values or undefined; mutators return true or undefined; Open/BeginApply return IDs or undefined; Commit returns Task 3's numeric Outcome or undefined for a rejected invocation. Undefined after an invoked writer must never conceal an uncertain persistence result.

Export `extern "C" void __fastcall HelenGraphicsDispatch(void* handler, void* unusedEdx, void* movie, const char* name, const void* arguments, unsigned count)` with x86 decorated name `@HelenGraphicsDispatch@24`. Resolve the existing interception from loaded module base plus RVA `0x015FD9D0`, verify `8B118B5204`, overwrite/resume 5 bytes.

- [ ] Test all callback arities/types, unknown operation names, stale IDs, every result mapping, exception containment, and stock forwarding with unchanged pointers/count on alternating movies. No generic command-name execution API is exposed.
- [ ] Bind services before hook publication, then implement allowlisted dispatch using Task 1's codec. Keep the adapter free of INI parsing and UI behavior. Remove the experiment-only hook binding from the production candidate; never install both dispatch hooks.
- [ ] Generate shared constants for field IDs/outcomes and test native/C# parity. Frontend maps normalized MSAA values `[0,1,2,3,5]` to menu indices, and Detail edits submit their leaf states.
- [ ] Add tests executing extracted actual generated AS with recorded ExternalInterface calls:

```javascript
assert.equal(calls.filter(c => c[0] === 'Helen_Graphics_OpenV1').length, 1);
assert.equal(calls.filter(c => c[0] === 'Helen_Graphics_EndReadV1').length, 1);
assert.ok(!calls.some(c => c[0] === 'FE_SetControlType' || c[0] === 'FE_GetControlType'));
```

The harness uses the experiment's method extraction approach, not a rewritten controller. Test invalid scalar responses, BeginApply failure, SetField/SetResolution failure followed by CancelApply, every Commit outcome, missing callback, and cleanup on initialization failure. EndRead runs after valid Open even if value validation fails; Close runs on screen destruction. A clean read transfer does not close the session.
- [ ] Replace initialization and Apply queue polling with direct transfer/staging/commit. Send complete scalar draft after BeginApply, followed by resolution when required, then Commit exactly once. NotApplied shows failure with attempted draft retained for correction; committed outcomes copy Draft to Initial, disabling Apply. CommittedCleanupFailed also shows a cleanup warning. IntegrityUncertain disables Apply across reopened sessions. No transport failure falls back to carrier requests.
- [ ] Keep arrows/layout/Back/Detail behavior and existing restart semantics. Remove graphics-only deadlines/toggles and failure handlers that could infer cancellation from time. Rebuild fresh GFX and run the same JS tests on emitted and FFDec-decompiled controller code. Commit source/tests after native ABI and both frontend checks pass.

## Task 7: Fresh integrated package and live verification

**Files:** Modify graphics pack `builds/steam-goty-1.0/hooks.json`, fresh target/delta/files metadata; update `Rebuild-BatmanGraphicsOptionsExperiment.ps1`, `Deploy-BatmanGraphicsOptionsExperiment.ps1`, and package/shell/deployment validators under `games/HelenBatmanAA/scripts`. Extend `tests/HelenRuntime.Tests/PackRepositoryTests.cpp`.

- [ ] Add tests rejecting graphics carrier observers, simultaneous probe/production hooks, missing export relocations, wrong executable signatures, and stale target metadata. Use relocation JSON keys `module` and `export`, not moduleName/exportName. Remove only graphics read/write mappings; preserve subtitle and unrelated observers.
- [ ] Build native runtime and hook into fresh separate candidate outputs, compile source builder, and produce target GFX/Frontend/delta from verified retail base. Record source status and SHA256 provenance. Do not invoke the generated-AS probe patcher in the production pipeline.
- [ ] Run fresh native tests, codec/dispatch tests, emitted/decompiled AS tests, native pack parser, and byte-for-byte delta reconstruction. Run the existing scripts below after inspecting their parameter declarations:

```text
Test-BatmanGraphicsOptionsShellContract.ps1
Test-BatmanGraphicsOptionsPackage.ps1
Test-BatmanGraphicsOptionsRetailBaseContract.ps1
Test-BatmanGraphicsOptionsLayout.ps1
Test-BatmanGraphicsOptionsShellDeployment.ps1
Test-BatmanGraphicsOptionsDeployBaseCompatibility.ps1
```

Revise `Test-BatmanGraphicsCatalogPolling.ps1` to assert the replacement architecture or retire it explicitly; do not leave a required validator enforcing obsolete polling. Keep unrelated regression assertions.
- [ ] Verify Batman is closed. Back up working DLL/graphics pack/config/logs durably; deploy with explicit GameBin and existing rollback-safe helper under required elevation. Hash-verify installed candidate, unchanged subtitle assets, proxy and config, exact enabled pack set, and verified-empty staging cleanup.
- [ ] Ask for live tests: values versus launcher, reopen/relaunch, both arrow directions, windowed/fullscreen and supported resolutions, edit/discard, controlled Apply and persistence. Inspect logs; separate automated evidence from user confirmation. Do not claim failure injection was tested live against user files.
- [ ] Commit only verified task changes/generated artifacts with explicit staging. Report unresolved issues honestly, retain diagnostic evidence, and leave the installation recoverable. No independent review session without asking Helena.

## Self-review coverage

Tasks 1/6 cover numeric ABI and strict dispatch; 2 covers parsing and valid partial reads; 3 covers persistence/reconciliation and sticky integrity failure; 4/5 cover exact catalog identity and transaction ownership; 6 covers direct-only UI behavior; 7 covers provenance, regression verification, deployment, and live acceptance. The old synchronization prerequisite is replaced, not bypassed.

Read spec and this plan together before execution. No test or implementation task is marked complete by this document. Execution remains inline, as already selected.
