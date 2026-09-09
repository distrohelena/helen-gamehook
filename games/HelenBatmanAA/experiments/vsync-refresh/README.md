# Isolated viewport activation fixture

Current opt-in binding: `Build-NoSaveProbe.ps1 -SameSizeRefresh` in the sibling
`windowed-resolution` directory now connects the tested bridge to the pinned
Batman rebuild decision. Normal builds still exclude it. See
`docs/d3d9-presentation-override.md` for the candidate, limits and verification.
The scalar experiment below is historical and is **not** part of that binding.

Run from the repository root:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File games/HelenBatmanAA/experiments/vsync-refresh/Test-Activation.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File games/HelenBatmanAA/experiments/vsync-refresh/Test-Activation.ps1 -DecisionBridge
powershell -NoProfile -ExecutionPolicy Bypass -File games/HelenBatmanAA/experiments/vsync-refresh/Test-Activation.ps1 -Scalar
powershell -NoProfile -ExecutionPolicy Bypass -File games/HelenBatmanAA/experiments/vsync-refresh/Test-Activation.ps1 -Concurrency
powershell -NoProfile -ExecutionPolicy Bypass -File games/HelenBatmanAA/experiments/vsync-refresh/Test-StartupLoader.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File games/HelenBatmanAA/experiments/vsync-refresh/Test-StartupLoader.ps1 -FailureCase
```

This compiles and executes a console-only x86 process in a fresh output directory.
It does not load, patch or call Batman. No normal project imports these sources.

The matcher derives the outer viewport call's return-slot address from the
renderer EBP. For the pinned positive-size existing-viewport chain, the return
slots are EBP+4, +0xC, +0x4C, +0x84 and +0xCC. They correspond to renderer,
RHI viewport, generic viewport, viewport helper and Windows viewport returns.
Expected retail code destinations are EAA0EF, CBBA7D, EB7362, EB95FB (unchanged
mode path), and Helen's own wrapper return. The opt-in binding requires the pinned
executable, preferred base and exact loaded splice bytes. EBP is captured before
the renderer aligns ESP. Fixtures do not establish compatibility with other mods
that rewrite loaded engine callers.

The assembly fixture uses equivalent frame extents (56, 36 and 44 bytes), actual
CALL/RET instructions and the renderer's frame/alignment pattern. A nested call
uses exactly the same five return sites but a different outer return-slot address.
The root publishes its anchor immediately before calling the synthetic viewport;
the nested invocation cannot replace it. These are synthetic engine frames, not
execution of the retail instructions. The synthetic renderer now enters the
shared decision bridge via JMP; its callback receives the real renderer EBP,
runs the matcher, and consumes the request. Destination counters assert one outer
rebuild and one nested skip, independently of callback acceptance counters.

Checks cover actual outer acceptance, nested rejection, balanced returning stacks,
each mismatched return address, missing anchor, insufficient stack extent,
out-of-bounds/misaligned frames and arithmetic overflow refusal. Bounds must refer
to readable current-thread storage; this helper is not an arbitrary memory probe.
The fixture uses NT_TIB bounds for its live frames and owned arrays for malformed
frame tests. It does not test fiber migration, stack corruption or malicious code.

TDD evidence: return-sites-only matching compiled and failed specifically because
the nested call was accepted. Adding exact anchor equality passed. Mutation of
0xCC to 0xC8 failed outer acceptance; omitting the last return comparison failed
the wrong-chain test. Both mutations were restored.

The real-stack fixture now combines origin matching with a CPU-only one-shot
request. Nested arms cannot replace it; wrong renderer/device/thread/anchor,
stale operation numbers and duplicate consumption are refused. The assembly
wrapper disarms the outer request before returning. Consumption evidence survives
disarming until a newer operation is armed. This prototype uses short SRW-locked
transitions. The contention executable tests exactly one Arm and Consume winner
with 16 real threads in each of 32 rounds, using synthetic observed identities.
Atomic wait/notify releases workers without sleeps. Removing the consumption
guard fails the test. This is bounded contention coverage, not exhaustive race
detection or proof of engine-thread/lifetime correctness.

The separate decision fixture enters a naked bridge via JMP and captures both
destinations. It checks original zero/nonzero decisions, identity refusals, a
forced zero-input rebuild, consumption on an already-required rebuild, and
duplicate/disarmed refusal. GPRs, ESP, defined EFLAGS after TEST (AF is undefined),
x87 control/register state, MXCSR and XMM0-7 are compared. Its callback deliberately
clobbers floating-point state. The original-decision-only baseline failed the
forced branch; removing FXRSTOR failed FP preservation, and corrupting ECX failed
GPR preservation. Both mutations were restored. This tests legacy FXSAVE state,
not AVX upper halves or other extended processor state.

Both executables compile the same `DecisionBridge.cpp`. The activation executable
now exercises the complete synthetic call chain through that bridge and matcher;
the decision executable retains independent CPU-state assertions. Supplying
EBP+4 instead of EBP failed outer acceptance, then was restored.

`RefreshRequestScope` owns only a successfully armed request. Tests cover ordinary
C++ exception propagation with disarming, normal unconsumed exit, retained
consumption evidence, repeated-arm refusal and a rejected nested guard that must
not disarm its owner. An empty destructor failed the exception test; unconditional
disarming failed the nested-owner test. Both were replaced with ownership-checked
cleanup. The call-chain fixture now also places a C++ scope outside its native
anchor publisher. An exception injected inside the naked call chain initially
leaked the request; enclosing ownership fixed that regression. Normal disarming
still precedes the publisher's return. Arbitrary retail SEH, driver failures and
fiber migration are not established by these tests.

The scalar executable checks enable/disable and adjacent-word sentinels, rejects
existing 2/0xFFFFFFFF without modifications, and caught both a missing store and
an extra adjacent store. It accesses owned CPU storage only; it is not a live
engine setter or an engine synchronization mechanism.

Remaining work includes the production wrapper's lifetime boundary, concurrent
lifetime tests and engine synchronization/patch-transaction gates. The new loader
tests exercise actual proxy static attach, dynamic/late/reentrant refusal, failed
initialization, failed callback pinning and real process-lifetime pinning. They
use the shared runtime gate and pin classes, with a small fixture initializer.
Removing PIN makes the balanced-release test fail. The bridge's
callback and destinations must be bound before entry and cannot change during a
call; no concurrent rebinding or production installation is tested. An anchor
must never be reused as a persistent identity.
This fixture does not prove all callback side effects safe, modify VSync, execute
Reset, or establish a live Batman success.

## Full lifecycle integration (not a VSync gameplay package)

`Build-StartupHookProbe.ps1 -OutputRoot <fresh-repo-output-path>` builds the real
opt-in runtime and proxy. `-NormalControl` builds without the startup interface.
`Test-RuntimeStartup.ps1 -ArtifactRoot <explicit-native-artifact-directory>` copies
and hash-checks that exact pair into a fresh pack-free host directory, then tests
normal initialization and late startup refusal. Use `-NormalControl` for the
control artifacts. `Test-StartupNativeSuite.ps1 -ArtifactRoot <same-directory>`
links the full existing native suite against the explicitly supplied library.

The recorded candidate is `output/startup-hook-probe-20260908-c/native`; the normal
control is `output/startup-hook-normal-20260908-a/native`. Both real-host tests
passed. The full native suite and three D3D9 regressions also passed (D3D9 needed
GPU access outside the sandbox). Artifact hashes and remaining warnings are in the
static investigation report. No artifacts are installed by these scripts.

The lifecycle extension is opt-in. The VSync decision patch is still NOT linked
or installed. The approved shared-writer revision now adds MemoryPatch ownership,
explicit stage errors and recoverable TryInstall/TryRemove paths; see
`docs/superpowers/specs/2026-09-08-owned-memory-patch-design.md` and run
`tests/HelenRuntime.Tests/Test-MemoryPatchFailures.ps1` for the isolated failure
matrix. Remaining engine binding gates are still open. Do not deploy these
lifecycle-only artifacts as the no-save graphics experiment.
