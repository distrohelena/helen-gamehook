# Batman VSync: scoped engine-owned presentation refresh

## Approval and scope

The user approved this architecture in chat after the static investigation.
This document is the design-review checkpoint, not implementation authorization.
Work remains on main, based on 9dc4358. The installed fullscreen candidate and
unrelated `batma/` directory stay untouched. No independent review or substantial
multi-agent workflow is authorized by this design.

Enable a VSync-only live experiment through Batman's existing menu while retaining
the no-Helen-save behavior and one engine attempt per process. The current
resolution/fullscreen path must retain its behavior when VSync is not edited.
No other graphics settings, repeated-Apply feature or saved-success UI is included.

## Evidence and alternatives

See `docs/superpowers/reports/2026-09-08-vsync-static-investigation.md` for pinned
executable evidence. The renderer maps UseVsync into its presentation interval,
but its rebuild decision does not compare VSync. The queued settings-update payload
does not carry VSync. A settings writer or same-size resize alone is not sufficient
evidence of a live change.

1. **Selected: scoped invalidation at the engine's rebuild decision.** The hook
   supplies the missing reason to rebuild; Batman retains synchronization,
   resource teardown, Reset and resource recreation.
2. **Direct Helen-owned D3D Reset.** Rejected for this slice: it bypasses the engine's
   broader resource/viewport lifecycle and duplicates behavior already implicated
   in previous freezes.
3. **Temporary size/mode changes or spoofing device loss.** Rejected: those alter
   unrelated live state and use the wrong engine contract to trigger a refresh.

The candidate decision is near preferred VA 0x00EA65A6. The settings-apply candidate
is 0x00C40090. These are research leads until their complete contracts pass the
implementation gates below; copying these addresses is not sufficient validation.

## Components and ownership

### Reusable request lifecycle

A small CPU-only service owns one explicitly armed refresh request. It contains
an operation identity, expected renderer/device identity, permitted execution
thread and whether the request was consumed. States distinguish idle, armed,
consumed and terminal result. Reject nested/concurrent arming and stale identities.
RAII disarms the request on every returning or exception path. Do not persist
runtime addresses or cache a borrowed engine pointer across operations.

The service does not know Batman addresses, mutate engine fields or own Direct3D
resources. Reuse must follow actual ownership needs; do not create a general
graphics framework or promise that MSAA and other settings use the same contract.

### Batman binding and hook

A Batman-specific adapter owns executable/signature validation, settings layout,
engine calls and the instruction-level bridge. Install the hook once during safe
runtime initialization, before the target can execute concurrently. Reuse the
existing hook infrastructure only after verifying it correctly relocates the
overwritten instructions and branch targets. Never patch/unpatch around each Apply.

With no matching armed request, reproduce the original decision exactly. A
matching request can force the rebuild branch once; it must not turn subsequent
engine-internal visits into additional Helen requests. Preserve registers, flags,
stack alignment and calling convention at both original branch destinations.
No exception may escape the machine-code bridge. Keep COM calls, file I/O and
engine calls out of the hook's short decision callback and out of request locks.

The hook and its service must outlive all possible callbacks. Initialization
failure leaves the VSync experiment unavailable, with an explicit reason; it must
not make unrelated menu values unavailable or weaken existing fingerprint checks.

## Apply sequence

1. Determine edited fields from the editor's captured baseline and staged draft,
   not just from current engine values or possibly stale persisted INIs. For this
   first slice, reject VSync combined with size/mode or other graphics edits before
   mutation. A request with no VSync edit continues through the existing path.
2. Validate executable, installed hook, game/viewport/device ownership, correct
   thread, idle rendering state and the shared one-attempt allowance. Read live
   size/mode, settings and swap-chain presentation parameters. Release temporary
   swap-chain/surface references before anything that can Reset.
3. Build a validated current-settings snapshot and alter only VSync. The engine
   settings layout must be proven safe to copy; a raw size observed in disassembly
   is not enough to assume ownership-free fields. Preserve all unrelated values.
4. Consume the shared allowance immediately before the first possible engine
   mutation. Use the validated settings-apply entry with saving disabled and arm
   the refresh only across the verified synchronous viewport-refresh invocation.
   The exact ordering must be proven to prevent an intervening unrelated refresh
   from consuming the request. If that cannot be established, do not deploy.
5. Enter the engine's higher-level synchronization path without fabricating a
   size/mode transition. Do not call an interior renderer address as a function.
   Require the hook to observe and consume the matching request.
6. After engine return, disarm, revalidate ownership and reacquire the current
   swap chain. Require the expected interval, unchanged resolution/fullscreen and
   consistent engine VSync state. Log the before/request/after values and whether
   Reset and request consumption occurred. Do not use FPS or elapsed time as proof.

An already-effective VSync request needs no forced Reset; record the observed
state without claiming persistence. Unexpected interval values are unsupported or
uncertain, not silently coerced. This verifies the API's presentation configuration,
not that a driver override or desktop compositor visibly obeys it.

## Failure and persistence contract

Preflight rejection does not consume an engine attempt or mutate settings. Once
engine mutation is attempted, do not retry automatically. Distinguish refusal,
verified live application, known failure and uncertain live state in diagnostics;
none is equivalent to a saved transaction. Keep the probe's intentional NotApplied
frontend result for completed no-save attempts. Retain an explicit process lockout
for an uncertain attempt; reopening the menu must not reset it.

Do not restore only the settings variable and claim rollback of the D3D device.
Do not add automatic rollback resets in this slice. If state has changed partially,
report that fact and preserve evidence. A returning failure must clear the armed
request without hiding the original engine/driver error.

The engine has an existing Reset failure loop. This design adds no polling,
timers, sleeps or retries, but cannot bound that loop or recover control if the
engine never returns. It must not advertise timeout safety. Changing that engine
failure behavior requires a separately approved design.

Helen's INI writer stays bypassed. Validate the candidate engine save-control
argument and trace transitive writers; its name or a gated call is not proof that
all writes are disabled. Preserve the existing session-only routing policy and
original INIs. Engine-owned writes and session overlays must be reported separately
from Helen publication. Do not weaken routing or silently redirect additional files.

## Required implementation gates

- Prove the same-size/mode higher-level call reaches the decision on the validated
  execution thread with the expected renderer; if it dispatches asynchronously,
  the scoped synchronous design must be revised before implementation proceeds.
- Verify settings-call ABI, snapshot layout/ownership and save side effects,
  including that unrelated live settings are not changed by reload/normalization.
- Verify exact patch boundaries, all inbound/outbound control flow and instruction
  relocation. An inactive hook must be indistinguishable from the original code.
- Establish how actual Reset success/failure and request consumption are observed
  without adding a second owner to the existing D3D9 hook/dispatch registries.

Failure to satisfy a gate is a design issue, not permission for a fallback hack.

## Verification and handoff

Use TDD for request ownership, single consumption, no-op/refusal behavior, stale
identity/thread rejection, reentrancy and cleanup on every returning failure.
Execute the machine-code bridge in an isolated x86 fixture to test both branches,
registers, flags and stack behavior; source-text matching alone is insufficient.
Use controllable engine-boundary doubles to test call order, no-save arguments,
unrelated-field preservation, missing consumption and post-call mismatches.

Real windowed D3D fixtures verify presentation-interval observation and retain
existing reset, failure, exact subtitle-pixel and parent-texture-lifetime checks.
No automated desktop fullscreen switch or live Batman mutation is required for
the development checkpoint. Run no-save session and package/delta checks against
explicit fresh artifacts and record hashes. Report the existing full native
suite failures honestly if they persist; do not label the whole suite green from
focused tests.

Automated fixtures cannot prove Batman's live thread/lifecycle integration. Build
and report the candidate separately; installation requires a later user request,
Batman closed, a verified rollback copy and unchanged-pack/config/INI checks.
Eventual live acceptance verifies VSync in the actual swap-chain parameters,
unchanged mode/size, and rendering/subtitles after entering gameplay. Do not claim
that all GPUs or driver settings are covered by one test.
