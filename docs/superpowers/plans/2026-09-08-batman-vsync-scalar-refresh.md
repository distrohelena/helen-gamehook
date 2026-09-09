# Batman VSync Scalar Refresh Implementation Plan

**Paused by user direction:** The user selected a narrower reusable D3D9
presentation-interval override instead. Implement and test overrides on existing
CreateDevice/Reset calls first; a later real resolution/fullscreen change can
supply the reset. Do not continue the proposed Batman wait guard or instruction
bridge under that approval. This historical plan is not the active work list.

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task, inline as requested. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a separately packaged, opt-in VSync-only no-save candidate using a synchronized single-field setter and Batman-owned presentation refresh.

**Architecture:** A reusable CPU-only request service owns single consumption and RAII disarming. An experiment-only Batman adapter owns the verified scalar, engine rendering-suspension lifetime and decision bridge. The existing session baseline selects VSync-only versus unchanged resolution/fullscreen behavior; both consume one shared process allowance.

**Tech Stack:** C++20, Windows x86/MSVC v143, PowerShell console-tool wrapper, pefile/capstone, existing HelenRuntime inline hooks and D3D9 dispatch.

**Spec:** `docs/superpowers/specs/2026-09-08-batman-vsync-refresh-design.md`, approved revision at `b5e6dea`.

## Global constraints

- Work inline on main. No new agent workflow or independent review is authorized.
- No full-settings apply, configuration reload, SCALE SET command, fabricated device loss or temporary dimensions/mode.
- No other graphics settings, repeated-Apply feature or saved-success UI is included.
- Helen's INI writer stays bypassed. Preserve session-only routing and original INIs; report engine overlay writes separately.
- Keep COM calls, file I/O and engine calls out of the decision callback and request locks.
- One engine attempt per process, shared with resolution/fullscreen. Consume immediately before synchronization entry; no automatic retry or rollback Reset.
- No polling, timers or new engine retry behavior. The existing engine Reset loop can still prevent return.
- Preserve the working installed DLL: SHA-256 `DB757D38C9E02BB9DBD87CD29F6DDABDE77866868C4636956FE9AFB6C7A90752`.
- Build only fresh artifacts. Installation, game launch and live mutation require a later request.
- All native fixtures use `SetErrorMode(0x8003)` and `Invoke-BatmanConsoleTool.ps1`; no MessageBoxes or debugger launch.
- One class per file, substantive Doxygen for every new member, RAII, explicit types and ownership; no corrective fallback writes.
- Preserve unrelated `batma/`. Leave work uncommitted until the user requests a commit.

## Scope and binding stop rule

### Execution checkpoint

Tasks 1-2 were investigated inline; neither is marked fully certified. The
selected suspension entry/destructor, scalar section and viewport-write condition
were decoded against the pinned executable. The callback-origin audit found an
unresolved registered-object virtual dispatch before the intended renderer call.
See the report's "Scalar-plan execution checkpoint" for addresses and limits.
Subsequent isolated work exercises exact stack-activation matching, a shared x86
decision bridge, C++ request-scope cleanup, scalar footprints and bounded request
contention. Those are partial Tasks 3-4 prototypes, not certified production
bindings. The origin identity reaches the intended synthetic destination; the
remaining engine and installation gates are still open. In particular, the
original proxy/runtime interface did not distinguish startup-only installation
from late initialization or pin callback code. The user approved that revision;
the opt-in lifecycle, actual-proxy loader fixtures, fresh real-runtime host and
normal-build control now pass. The complete native suite and D3D9 regressions
also passed against the fresh lifecycle library. The combined native call-chain
fixture now verifies enclosing C++ cleanup on an injected exception.
The shared memory/inline-hook failure contract was the next integration gate.
The user approved its revision in `2026-09-08-owned-memory-patch.md`; the owned
transaction, TryInstall/TryRemove interfaces and real failure-injection matrix
are now implemented. Remaining production gates concern the actual engine
suspension/observation and callback bindings, not permission for this writer fix.
Tasks 5-7 remain blocked by the prerequisite gates. No commit or deployment.

The next read-only pass resolved stock thread-object ownership and the viewport
resource callback. It also confirmed that shutdown ignores the wait result before
closing the handle and freeing thread objects. A post-entry duplicate-handle
check cannot protect that earlier cleanup. See the report's "Render-thread
ownership and wait-result gate" section. A candidate-scoped, pre-cleanup guard
with no-dialog termination on wait failure is proposed, **not approved or
implemented**. It changes engine failure policy and therefore needs the user's
decision before dependent binding work. Resource-destructor/callback and
incoming-edge certification remain open independently of this proposal.

This replaces the obsolete full-settings tasks in `2026-09-08-batman-vsync-binding-gates.md`.
Retain that plan's failed-gate record; do not reclassify the rejected call as safe.
Tasks 1-2 establish engine contracts, not speculative production offsets. If any
required contract cannot be established, stop before the dependent implementation,
record the concrete evidence and request a design revision. Later tasks are
conditional on those gates, not permission to invent an ABI or initialization point.

## File ownership map

New reusable files, registered in `HelenRuntime/HelenRuntime.vcxproj`:

- `include/HelenHook/PresentationRefreshIdentity.h`: required operation/renderer/device/thread identity.
- `include/HelenHook/PresentationRefreshRequest.h`, `HelenRuntime/PresentationRefreshRequest.cpp`: CPU-only arming, match and one-time consumption.
- `include/HelenHook/PresentationRefreshScope.h`, `HelenRuntime/PresentationRefreshScope.cpp`: RAII disarming, no engine ownership.

New experiment files under `games/HelenBatmanAA/experiments/vsync-refresh/`:

- `VsyncScalar.h/.cpp`: typed Boolean field access, exact four-byte write.
- `BatmanRenderSuspension.h/.cpp`: verified engine scope lifetime, noncopyable RAII.
- `BatmanVsyncBinding.h/.cpp`: fingerprint, module-relative addresses and live owner/thread validation.
- `BatmanVsyncDecisionBridge.h/.cpp`: x86 decision bridge and initialization-time hook ownership.
- `BatmanVsyncProbe.h/.cpp`: preflight, synchronized setter/refresh/readback orchestration.
- `VsyncContractTests.cpp`: CPU behavior tests, with any fixture class in its own header.
- `DecisionBridgeTests.cpp`: execution of real bridge instructions against synthetic destinations.
- `Test-VsyncContracts.ps1`, `Test-DecisionBridge.ps1`: isolated x86 compile/run drivers.
- `VsyncProbe.targets`: opt-in build registration; no normal-build dependency on Batman experiment code.

Existing integration files:

- `games/HelenBatmanAA/experiments/windowed-resolution/NoSaveResolutionProbe.h/.cpp`: shared attempt ownership and baseline-aware routing.
- `games/HelenBatmanAA/experiments/windowed-resolution/Prepare-NoSaveProbe.ps1`: exact source substitution passing baseline plus draft.
- `games/HelenBatmanAA/experiments/windowed-resolution/NoSaveProbe.targets`: include new opt-in target only for this candidate.
- `games/HelenBatmanAA/experiments/windowed-resolution/NoSaveSessionTests.cpp`: verify real Commit routing and no publication.
- `HelenGameHook/BatmanGraphicsRuntime.cpp`: candidate-only initialization seam, before dispatch publication, only after Task 2 proves safety.
- `docs/superpowers/reports/2026-09-08-vsync-static-investigation.md`: address/ABI dossier, gate results and verification evidence.

## Task 1: Certify scalar and rendering-suspension contracts

**Consumes:** Pinned retail executable and the existing read-only decoder.
**Produces:** Exact scalar and suspension ABI dossier; no engine calls are executed.

- [ ] Verify identity and inspect the critical renderer read:

```powershell
$retailExe = 'C:\Program Files (x86)\Steam\steamapps\common\Batman Arkham Asylum GOTY\Binaries\ShippingPC-BmGame.exe'
Get-FileHash -LiteralPath $retailExe
python games/HelenBatmanAA/experiments/windowed-resolution/Inspect-Viewport.py --executable $retailExe --address 0xEA65F9 --length 0x4D
```

Require length 38758728 and SHA-256 `4DAC1F5E2AC6710B7378FDCE74601F616F4753E3756CB5FDA63C7519CC2EB028`.

- [ ] Decode complete scope entry/destruction and their callees from exact instruction boundaries:

```powershell
rg -n -A 95 '00732210:' output/ShippingPC-BmGame.disasm.txt
rg -n -A 85 '00724410:' output/ShippingPC-BmGame.disasm.txt
rg -n -A 160 '0072F090:' output/ShippingPC-BmGame.disasm.txt
rg -n -A 110 '0071F6F0:' output/ShippingPC-BmGame.disasm.txt
```

- [ ] Record all scope storage offsets, size/alignment, calling conventions, argument cleanup, globals, callbacks and failure paths. Confirm imported synchronization operations from the PE import table. Verify complete byte ranges with the decoder after determining their boundaries; do not guess storage from the size of a caller's stack frame.
- [ ] Trace nested suspension: adapter entry, viewport entry, renderer entry, reverse destruction. Establish when rendering stops, when it can restart, and what happens if entry fails before scope construction completes. A destructor must not run on unconstructed engine storage.
- [ ] Verify the scalar lies in the expected writable section, is four-byte aligned, and is resolved from the loaded module. Build relocation-aware instruction validation or explicitly reject unsupported load layouts; never accept absolute operands merely because the file hash matches.
- [ ] Record a pass/fail gate. Exact stock stores establish field meaning, not that arbitrary Helen threads may write it. Stop if suspension does not cover all relevant readers through refresh.

## Task 2: Certify refresh origin, initialization and observation

**Consumes:** Task 1 dossier and the existing same-size chain.
**Produces:** Approved patch span/destinations, initialization lifetime and observation contract.

- [ ] Reuse the recorded chain `EB91D0 -> EB7250 -> CBB980 -> EAA0A0 -> EA6310`; verify callback windows before the decision, including `026760EC`, `71A040` and `AFE980`. Record conditions rejecting the single idle viewport. Resolve indirect calls rather than assuming identity/thread equality excludes reentrancy.
- [ ] Trace `C42DA0` and scope exit side effects. Establish that VSync-only refresh preserves unrelated live fields even when INI values differ. Record engine-owned writes under the existing overlay policy; reject any need to broaden routing silently.
- [ ] Inspect initialization around `InitializeBatmanGraphicsRuntime` and build-hook publication in `HelenGameHook/HelenGameHook.cpp`. Prove the target cannot execute while patched; dispatch publication alone is not proof. Define hook/service teardown lifetime so no callback outlives either. Stop if the current loader cannot provide a safe installation point.
- [ ] Inspect the bridge span and all incoming branches:

```powershell
rg -n -B 35 -A 25 '00EA65A6:' output/ShippingPC-BmGame.disasm.txt
rg -n '00EA65A[0-9A-F]|00EA7388' output/ShippingPC-BmGame.disasm.txt
rg -n -A 100 'bool InlineHook::Install' HelenRuntime/Hook.cpp
```

`InlineHook::Install` currently copies overwritten bytes into its trampoline; do
not assume it relocates relative branches. The candidate test/Jcc span is not
approved merely because it exceeds five bytes. Use explicit branch destinations
in the bridge, never execute a copied relative Jcc from an unrelocated trampoline.
If safe use requires a general hook-infrastructure redesign, stop for review.

- [ ] Locate the existing D3D9 Reset owner with `rg -n 'Reset|D3d9ResetDiagnostics' HelenRuntime HelenGameHook`. Define how the candidate correlates before/request/after, existing Reset diagnostics and request consumption without adding a second dispatch registry or COM owner. Final presentation parameters and scalar readback must remain mandatory; return from the viewport function alone is not success.
- [ ] Record the exact approved ABI, addresses, instruction bytes and origin proof. If a same-identity callback can consume the request before the intended call, stop and revise; do not silently add another hook or weaken matching.

## Task 3: CPU-only request and scalar contracts, test first

**Consumes:** Approved scalar representation from Task 1.
**Produces:** Reusable request state and isolated typed scalar writer.

Public interface contract (each named class/struct in its own file; add substantive
Doxygen when implementing):

```cpp
struct PresentationRefreshIdentity {
    std::uint64_t Operation;
    std::uintptr_t Renderer;
    std::uintptr_t Device;
    std::uint32_t Thread;
};
// Construction of identities rejects zero required fields.
// PresentationRefreshRequest starts idle, contains no engine pointers it owns.
// bool Arm(const PresentationRefreshIdentity&) noexcept;
// bool Consume(std::uintptr_t renderer, std::uintptr_t device,
//              std::uint32_t thread) noexcept;
// bool WasConsumed() const noexcept;
// void Disarm() noexcept;
// PresentationRefreshScope(PresentationRefreshRequest&, const PresentationRefreshIdentity&);
// ~PresentationRefreshScope() noexcept; // disarm even when unwinding
// VsyncScalar(std::uint32_t& verifiedField); // adapter validates address/lifetime
// bool VsyncScalar::Read() const;          // reject values other than 0/1
// void VsyncScalar::Write(bool enabled);   // single store; no engine calls
```

- [ ] Read the TDD skill and its writing-good-tests reference. Create console test drivers with fresh GUID output directories, x86 compiler and `/std:c++20 /EHsc /MD /W4 /WX`, following `Test-NoSaveDisplayRequest.ps1`. Print `VSYNC_CONTRACTS_PASS` only after all assertions pass; native failures return nonzero and console text.
- [ ] Add failing CPU tests before implementing behavior. Include this concrete scalar sentinel case using a file-scope `Expect(bool, const char*)` that throws `std::runtime_error`:

```cpp
std::array<std::uint32_t, 3> words{0x11223344u, 0u, 0x55667788u};
VsyncScalar scalar(words[1]);
scalar.Write(true);
Expect(words == std::array<std::uint32_t, 3>{0x11223344u, 1u, 0x55667788u}, "write footprint");
scalar.Write(false);
Expect(words[1] == 0u, "disable");
```

- [ ] Add request tests: idle refuses; nested Arm refuses without replacing identity; wrong renderer/device/thread leaves request armed; exact match consumes once; subsequent match refuses; RAII exit disarms on normal return and exception; operation identities are not reused. Add scalar tests rejecting existing 2 and `0xFFFFFFFF`, with all bytes unchanged on rejection.
- [ ] Run `Test-VsyncContracts.ps1`; first establish compilation, then require failed assertions for stubbed missing behavior rather than accepting a compiler error as the behavior test.
- [ ] Implement the minimal service, RAII scope and scalar writer. Use short request synchronization only; never carry a lock across callbacks. Keep the scalar helper experiment-only, while the address-free request service resides in HelenRuntime.
- [ ] Rerun all tests; require exit zero and the pass marker. Mutate the adjacent write and second-consumption behavior separately to demonstrate the tests catch both, then restore and rerun. Record commands/results.

## Task 4: Execute the x86 bridge in isolation

**Consumes:** Task 2 span/origin contract and Task 3 request interface.
**Produces:** Tested bridge preserving both original destinations and machine state.

- [ ] Create `DecisionBridgeTests.cpp` and `Test-DecisionBridge.ps1` using the console driver pattern from Task 3. Use actual x86 instructions with synthetic skip/rebuild destinations, not source matching or a C++ simulation of a branch.
- [ ] Write failing tests for this exact matrix, saving ESP, GPR, EFLAGS and relevant floating-point/SIMD state at both destinations:

```text
idle, EAX=0                         -> original skip
idle, EAX=1 or 0x80000000            -> original rebuild
matching armed request, EAX=0       -> rebuild; consumed
second matching visit, EAX=0        -> original skip
wrong renderer/device/thread       -> original decision; not consumed
all cases                          -> unchanged stack/non-result registers
idle cases                         -> flags equal original TEST EAX,EAX
callback refusal                   -> no forced branch or stale arming
```

- [ ] Run the unchanged original decision against the matrix and require the forced-request case to fail. Implement only the approved bridge and no-throw CPU callback; no live hook installation in this fixture.
- [ ] Run `Test-DecisionBridge.ps1`; require `VSYNC_DECISION_BRIDGE_PASS` and zero exit. Deliberately corrupt inactive branching, stack restoration and flags separately; prove the matching tests fail, restore and rerun.

## Task 5: Build the gated Batman adapter and orchestration

**Consumes:** Passed Tasks 1-4; no unverified engine ABI is supplied by this plan.
**Produces:** `BatmanVsyncProbe::Apply(const BatmanGraphicsDraftState& baseline, const BatmanGraphicsDraftState& draft)` returning `BatmanGraphicsApplyResult`, with explicit diagnostic outcomes and no saved success.

- [ ] Implement a controllable engine boundary for tests, with separate fixture-class headers. It records entry/revalidation/write/refresh/exit/readback and can reject or throw at each boundary. Use the real Task 3 request and scalar implementation, not duplicate test-only algorithms.
- [ ] Write failure-first orchestration tests asserting this exact successful trace:

```text
Preflight -> ReleaseObservations -> ConsumeAllowance -> EnterSuspension
-> Revalidate -> WriteVsync -> Arm -> Refresh -> Disarm
-> ExitSuspension -> ReadBack
```

- [ ] Add cases for: preflight refusal consumes nothing; failed scope entry never destroys unconstructed storage; post-entry refusal consumes allowance but writes nothing; no-op requires scalar AND actual interval match; mixed edits write nothing; scalar/device disagreement is not no-op; missing consumption/readback failure/mismatch stays explicit; exception after write disarms before scope exit without scalar-only rollback. Simulate live DirectionalLightmaps differing from INI and assert it remains unchanged.
- [ ] Implement `BatmanRenderSuspension` from Task 1's proven ABI with required ownership and RAII cleanup. Implement `BatmanVsyncBinding` from Tasks 1-2, revalidating actual objects each operation. Do not cache borrowed viewport/device pointers across operations.
- [ ] Implement `BatmanVsyncProbe` to satisfy the trace. Actual `GetSwapChain(0)` / `GetPresentParameters` observations must release acquired swapchain references before scope entry. Reacquire after scope exit; compare interval, dimensions, windowed mode, device/viewport identity, live scalar and verified unrelated-field observations. Unsupported representations or lost ownership produce refusal/uncertainty, not defaults.
- [ ] Bind the tested bridge once at the proven safe initialization boundary, only for the opt-in candidate. Keep request service, callback and installed hook alive together. Initialization failure disables this experiment with a specific log reason without making ordinary menu reads unavailable.
- [ ] Run the CPU matrix through the adapter seam before linking real bindings. Require zero exit, no leftover armed state and no writer calls. Build real bindings only after all preceding gates pass; no Batman invocation yet.

## Task 6: Baseline routing and one shared attempt allowance

**Consumes:** Task 5 probe and existing session-owned baseline.
**Produces:** Opt-in Commit routes VSync-only edits without changing normal builds.

- [ ] Extract the existing `Attempted` flag into one experiment-only `NoSaveAttemptAllowance.h/.cpp` in the windowed-resolution directory. Expose `static bool TryConsume() noexcept`; there is no reset API. Both display and VSync paths use this exact instance. Register sources in opt-in targets, not twice in separate DLL/static-library objects.
- [ ] Change the probe interface to `NoSaveResolutionProbe::Apply(const BatmanGraphicsDraftState& baseline, const BatmanGraphicsDraftState& draft)`. Compare all 14 draft fields to baseline, using the existing `BatmanGraphicsField::Vsync`. If VSync changed with any other field, refuse before scope entry; if only VSync changed, delegate to Task 5. Otherwise retain existing display request behavior.
- [ ] Update the exact generated substitution in `Prepare-NoSaveProbe.ps1`:

```powershell
$needle = 'Config.ApplyDraft(attempted.Draft)'
$replacement = 'NoSaveResolutionProbe::Apply(*Session->Baseline, attempted.Draft)'
```

Keep the existing exactly-one-match guard, fresh output directory requirement and
normal source untouched. Do not source the baseline from another INI read.

- [ ] Extend `NoSaveSessionTests.cpp` before integration: VSync-only routes once; VSync plus each other field refuses; reopening the menu cannot reset allowance; display then VSync and VSync then display reject the second attempt; Commit retains intentional NotApplied and does not advance the saved baseline. Use separate processes for allowance-order tests instead of adding a production reset method.
- [ ] Run the existing session and display request tests against the freshly linked candidate library. Assert original fixture INI bytes stay unchanged and normal-build Commit still calls `Config.ApplyDraft`.

## Task 7: Fresh build, regression and package handoff

**Consumes:** Passed Task 6 integration and all preceding gate records.
**Produces:** Fresh opt-in artifacts and honest verification report; no deployment.

- [ ] Register all new files in `VsyncProbe.targets`; preserve normal project behavior. Add a build driver `Build-VsyncProbe.ps1` beside it, consuming mandatory `-OutputRoot`, which prepares the generated no-save source and invokes Win32 Release MSBuild through the console wrapper. Forward `NoSaveOutput` and the opt-in targets; reject existing output roots. Do not resolve runtime libraries by newest timestamp or copy old output into the new root.
- [ ] Build with a new root:

```powershell
$candidateRoot = Join-Path $PWD ('output\vsync-nosave-' + [Guid]::NewGuid().ToString('N'))
& games/HelenBatmanAA/experiments/vsync-refresh/Build-VsyncProbe.ps1 -OutputRoot $candidateRoot
$candidateLibrary = Join-Path $candidateRoot 'native\HelenRuntime.lib'
& games/HelenBatmanAA/experiments/vsync-refresh/Test-VsyncContracts.ps1
& games/HelenBatmanAA/experiments/vsync-refresh/Test-DecisionBridge.ps1
& games/HelenBatmanAA/experiments/windowed-resolution/Test-NoSaveSession.ps1 -RuntimeLibraryPath $candidateLibrary
& games/HelenBatmanAA/experiments/windowed-resolution/Test-NoSaveDisplayRequest.ps1 -RuntimeLibraryPath $candidateLibrary
& tests/Invoke-D3d9ReplacementResetTests.ps1 -RuntimeLibraryPath $candidateLibrary
& tests/Invoke-D3d9ReplacementResetTests.ps1 -RuntimeLibraryPath $candidateLibrary -FailureCases
& tests/Invoke-D3d9ReplacementResetTests.ps1 -RuntimeLibraryPath $candidateLibrary -InitialSurfaceHooks
```

- [ ] Add windowed D3D presentation-interval observation coverage to `tests/HelenRuntime.Tests/D3d9ReplacementResetTests.cpp`: read actual swapchain parameters before/after a supported reset with all temporary references released. Report unsupported driver capability explicitly, not as a passing transition. Preserve exact subtitle-pixel and parent-texture-lifetime assertions. Never switch the desktop to fullscreen automatically.
- [ ] Run the native suite and separate pre-existing subtitle-upsert/concurrent-graphics-apply failures from new failures; focused passes do not mean a green full suite. Suppress crash dialogs through the established wrapper.
- [ ] Stage only fresh DLL/library, build provenance, rollback instructions and test logs inside the new candidate root. Hash every artifact. If a distributable archive is produced, extract to a second fresh directory and compare hashes/file list. Existing graphics/subtitle packs require no asset rebuild for this native-only slice; do not include video skip or claim old assets were newly built.
- [ ] Verify installation remains unchanged and documentation is clean:

```powershell
Get-FileHash 'C:\Program Files (x86)\Steam\steamapps\common\Batman Arkham Asylum GOTY\Binaries\HelenGameHook.dll'
git diff --check
git status --short
```

- [ ] Report exact gate results, artifact hashes, passed/failed/not-run tests and remaining live acceptance. Eventual user-authorized testing must verify actual interval, unchanged size/mode/unrelated settings, and gameplay/subtitles after Continue Game. Do not claim universal GPU coverage, saved success, or a successful Batman live test from fixtures.

## Plan self-review and execution handoff

Spec coverage: synchronization/scalar gates are Tasks 1-2; reusable lifecycle and
four-byte footprint Task 3; inactive ABI preservation Task 4; live ownership,
no-save orchestration, cleanup and uncertainty Task 5; baseline/mixed edits/shared
allowance Task 6; D3D/subtitle regressions, fresh artifacts and deferred live
acceptance Task 7. No binding-gate result or runtime test is asserted by this plan.

The engine ABI is deliberately a verified deliverable, not a guessed signature.
Failure of Tasks 1-2 stops dependent work. Request/scalar interfaces are defined
here; scope storage and machine-code span must come from those executable gates.
Execution remains inline on main as previously selected. No automatic commit,
installation or separate independent-review session is part of execution.
