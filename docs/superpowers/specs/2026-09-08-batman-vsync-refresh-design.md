# Batman VSync: scoped engine-owned presentation refresh

## Approval and scope

The user approved this architecture in chat after the static investigation.
On 2026-09-08 the user approved revising the settings mutation to a narrowly typed
Helen-owned VSync setter after the full-settings candidate failed its preservation
gate. This revision replaces that candidate; it does not waive the remaining gates.
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

The candidate decision is near preferred VA 0x00EA65A6. The verified renderer
read at 0x00EA65F9 consumes the four-byte UseVsync field at preferred VA
0x026C0D58 (settings owner+0x220). Zero maps to presentation interval 0x80000000;
nonzero maps to 1. Addresses refer only to the pinned retail executable in the
report and must be resolved against its loaded module, not persisted across runs.

Settings-mutation alternatives investigated:

1. **Selected: typed single-field setter in the Batman adapter.** Write only the
   actual UseVsync scalar under verified engine synchronization, then use the
   scoped refresh. This is Helen code, not a discovered stock setter function.
2. **Full-settings apply at 0x00C40090.** Rejected: its inner routine reloads
   configuration and can invert DirectionalLightmaps when live/config values
   differ, even with save=false. Do not copy a full draft or call this routine.
3. **Stock SCALE SET UseVsync command.** Not selected: although its field table
   provides a targeted assignment, the command reaches settings serialization
   at 0x00C42C76 when owner+0x2D4 is zero. It is not a no-save setter. Do not
   manipulate that owner flag or jump into the command's interior to bypass it.

Static evidence supports the scalar's meaning and conditional synchronous
same-size refresh reachability, not the safety of the new combined operation.
The setter's synchronization and lifetime contract still requires verification.

## Components and ownership

### Approved startup-lifetime revision

The user approved extending the shared proxy/runtime lifecycle with startup-only
installation and process-lifetime pinning. This remains opt-in through
`HELEN_ENABLE_STARTUP_HOOKS`; ordinary proxy builds retain their existing entry
path and do not export the new handshake. No installation or game launch is
authorized by this revision.

The real proxy's static process-attach scope publishes its current thread only
until that scope exits. The runtime queries this live context, rejects dynamic
loads/late calls/reentry, pins the proxy, helper and initializer modules, then
opens a thread-specific installation window around its no-throw initializer.
Failure closes or never opens the window; an admitted failed attempt cannot retry.
Pins are deliberately irreversible until process exit, including partial pin
failure. This is a cooperating-DLL lifetime contract, not protection against
malicious in-process code spoofing a proxy export.

Tests must exercise the actual proxy source in static and dynamic loader hosts,
balanced module release with pinning, failed initialization, reentry and late
refusal. A fresh full-runtime host and a normal-build control additionally check
real integration. These tests establish the lifecycle seam, not safety of a
future Batman instruction patch or its engine call chain.

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

### Typed VSync setter and synchronization ownership

The Batman adapter exposes a VSync-specific operation accepting an explicit
enabled/disabled value, not arbitrary addresses, offsets or a writable settings
block. Internally it validates the pinned image, scalar location, required thread
and operation ownership, then stores exactly one four-byte 0 or 1 value. Reject
unexpected existing representations rather than silently normalizing them.
No configuration reload, serializer, full-settings copy, callback or engine call
belongs in the setter. It must not write the unverified refresh flag 0x026B221C
or the renderer's device-error flag. This is a real setting update, not a fabricated
invalidation signal; the scoped hook supplies the explicit rebuild request.

The setter must execute after acquiring a verified engine rendering-suspension
scope, with that same scope held through the synchronous viewport refresh. A
Helen mutex or atomic scalar store alone does not synchronize engine readers.
Candidate scope entry/destruction are 0x00732210(1) and 0x00724410, observed in
the existing viewport and renderer routines. Their ABI, storage, nested lifetime,
callbacks and restart behavior must be established before binding them. Entering
the viewport call's own scope only after writing the scalar is insufficient.

Keep this scope outside the reusable request service. Its entry runs with the
request disarmed; after entry, revalidate the live state before the setting write.
Its exit also runs disarmed. Do not invoke engine code while holding request
locks. If the adapter cannot establish this lifetime safely, reject the binding
and revise the design; do not fall back to an unsynchronized write.

## Apply sequence

1. Determine edited fields from the editor's captured baseline and staged draft,
   not just from current engine values or possibly stale persisted INIs. For this
   first slice, reject VSync combined with size/mode or other graphics edits before
   mutation. A request with no VSync edit continues through the existing path.
2. Validate executable, installed hook, game/viewport/device ownership, correct
   thread, idle rendering state and the shared one-attempt allowance. Read live
   size/mode, settings and swap-chain presentation parameters. Release temporary
   swap-chain/surface references before anything that can Reset.
3. Consume the shared allowance immediately before entering the adapter-owned
   engine synchronization scope: entering it can itself invoke engine callbacks.
   Enter with the request disarmed, then revalidate identity, VSync and unchanged
   dimensions/mode. Record validated unrelated setting values for comparison,
   not for copying back into the engine. Do not assume live values match the INI.
4. With rendering suspended, use the typed setter to change only UseVsync.
   Arm the request immediately afterward, without an intervening engine call,
   and retain both scope and request across the synchronous viewport invocation.
   The callback/reentrancy analysis must prove an unrelated refresh cannot consume
   this request; renderer/device/thread matching alone is not proof of that.
5. Call the verified higher-level viewport entry with identical size/mode.
   Do not fabricate a transition or call an interior renderer address as a function.
   Require the hook to observe and consume the matching request. Disarm before
   releasing the adapter-owned synchronization scope, including failure paths.
6. After scope exit, revalidate ownership and reacquire the current
   swap chain. Require the expected interval, unchanged resolution/fullscreen and
   consistent engine VSync state and unchanged unrelated settings. Log the
   before/request/after values and whether
   Reset and request consumption occurred. Do not use FPS or elapsed time as proof.

An already-effective request requires both the live VSync scalar and actual
presentation interval to match the requested state; it needs no write or forced
Reset. A scalar/device disagreement is not this no-op case. Record the observed
state without claiming persistence. Unexpected interval values are unsupported or
uncertain, not silently coerced. This verifies the API's presentation configuration,
not that a driver override or desktop compositor visibly obeys it.

## Failure and persistence contract

Preflight rejection before synchronization-scope entry does not consume an engine
attempt or mutate settings. Once scope entry is attempted, do not retry
automatically, even if revalidation rejects before the scalar write. Distinguish refusal,
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

Helen's INI writer stays bypassed. The setter itself performs no I/O and uses no
engine save-control argument. This does not make the entire viewport refresh
write-free: the traced normal viewport helper subsequently calls 0x00C42DA0.
Trace and report its writes under the existing routing contract. Preserve the
existing session-only routing policy and
original INIs. Engine-owned writes and session overlays must be reported separately
from Helen publication. Do not weaken routing or silently redirect additional files.

## Required implementation gates

- Prove the same-size/mode higher-level call reaches the decision on the validated
  execution thread with the expected renderer; if it dispatches asynchronously,
  the scoped synchronous design must be revised before implementation proceeds.
- Verify the exact four-byte scalar binding, loaded-module resolution, valid
  representations and setter write footprint. No owning settings snapshot is used.
- Verify synchronization-scope ABI/storage, nested entry/exit and rendering-thread
  lifetime. Establish that suspension covers the scalar write through refresh,
  and exclude reentrant request consumption during viewport callbacks.
- Trace remaining viewport/scope side effects and verify unrelated live settings
  remain unchanged, including when the INI and live state differ. Do not equate
  a one-field setter with proof that the complete engine operation changes one field.
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
Use controllable engine-boundary doubles to test the order: consume allowance,
enter synchronization, revalidate, write one scalar, arm, refresh, disarm, exit
synchronization, read back. Cover failed entry, post-entry refusal, nested/reentrant
calls, missing consumption, scope cleanup and post-call mismatches. Assert the
setter writes exactly four bytes using guarded storage and distinct adjacent-field
sentinels, accepts only supported values and never calls reload/save routines.
Include live/config disagreement for DirectionalLightmaps and other supported
unrelated settings. These fixtures do not certify the real engine's side effects.

The previous binding-gates plan records a legitimate rejection of the old
full-settings approach. Do not mark that gate passed or execute its obsolete
settings-copy tasks against this revision. A revised plan must replace those tasks
with scalar and synchronization-scope verification before production work.

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
