# Batman live windowed resolution

## Scope and approval

Add live resolution changes when Batman is already windowed and the requested mode remains windowed. Arrows edit the draft; Apply Changes performs the resize. Helena approved keeping the previous saved resolution and reporting failure when live resizing fails.

This extends the persistence-only restriction in `2026-09-05-batman-direct-graphics-transactions-design.md` only for this case. Preserve the immediate direct reads, immutable catalogs, existing graphics editing and persistence, subtitles, and current enabled pack set. Live fullscreen transitions and other live graphics settings are excluded. No confirmation countdown, polling, retry timers, or timeout-based success decisions.

## Approach

Use Batman's engine-owned viewport resize flow. It must coordinate the window, render resources, viewport dimensions, and input/UI sizing on the correct engine thread. Bind the verified executable implementation once using module-relative addresses and pinned instruction evidence, with explicit ownership and lifecycle checks.

Moving the window with SetWindowPos alone cannot establish that the rendered resolution changed. Calling D3D9 Reset directly would bypass engine resource ownership and thread requirements. Neither is an acceptable substitute for identifying the engine flow.

Static investigation has identified a possible low-level renderer resize routine at preferred VA 0x00EAA0A0, with a D3D9Viewport.cpp game-thread assertion and apparent dimension writes. This is a discovery lead, not a verified callable interface. The `setres %dx%d%s` reference belongs to wxWidgets-related code and does not establish an active shipping console command. Do not call either based on string references alone.

The known executable is 38,758,728 bytes, SHA256 `4DAC1F5E2AC6710B7378FDCE74601F616F4753E3756CB5FDA63C7519CC2EB028`, preferred base 0x00400000. Runtime bindings use the loaded module base plus verified RVAs, never persisted process addresses.

## Discovery gate and controlled proof

The first implementation milestone is a narrowly scoped, instrumented proof, separate from production Apply integration. Before invoking a candidate, establish its calling convention, parameter meanings, owner acquisition/destruction, higher-level callers, engine thread, and safe invocation boundary. Do not discover ownership through repeated memory scanning.

Prove the chosen flow does not itself publish configuration files before success. Trace the higher-level engine path rather than calling a renderer entry that omits resource synchronization or window/input updates. A missing or stale binding disables only live resizing, with an explicit failure for an eligible live Apply; it must not silently save that requested resolution or make unrelated rows unavailable.

The proof captures authoritative current mode and dimensions from the engine and actual client area, requests one supported windowed size, and verifies both client-area and rendered/backbuffer dimensions. Outer window dimensions do not count as resolution. Verify subsequent rendering, UI/input alignment, a second size change, and restoration of the original size. Compare relevant INI bytes before and after to establish no premature persistence.

Do not assume the ExternalInterface callback is a safe resize boundary. Verify its execution context against the engine requirements. This design selects synchronous completion at a verified safe boundary. If the engine requires deferred work, stop before production integration and propose an engine-completion-event extension; do not invent timer polling or report enqueueing as success.

## Responsibilities and ownership

- Engine binding owns executable compatibility validation and lifecycle-aware access to the active viewport. It exposes explicit unavailable/stale failures, not fabricated dimensions or cached raw object addresses across destruction.
- Live resolution service captures actual state, validates eligibility, invokes the verified engine operation, and verifies its result. It owns no frontend labels or INI publication algorithm.
- Session service retains the existing complete draft, immutable catalog identity, consumed-transaction semantics, and reentrancy guard. It coordinates live work with the existing typed persistence service.
- Persistence service remains responsible for the existing two-file publication, reconciliation, and process-wide disk-integrity latch.
- Frontend keeps draft editing local and distinguishes success, recoverable failure, and uncertain state from an explicit terminal native result.

The commit guard covers live resize, persistence, any compensating resize, and baseline reconciliation. Nested graphics operations are rejected without deadlocking. Stock callbacks retain their forwarding behavior. No operation survives the lifetime of its viewport owner or runtime context.

## Apply ordering

1. Validate and consume the transaction once. Revalidate the complete draft, exact selected resolution pair, monitor identity, and actual current engine mode before any side effect.
2. If actual mode and requested mode are windowed and the requested size differs from the actual size, capture the original live state and invoke the verified live resize. An unchanged size needs no resize. Other mode cases retain existing persistence-only behavior and restart requirements.
3. Verify actual client-area and rendered dimensions. Only a verified successful resize permits publication of the complete requested draft through the existing writer.
4. On verified publication, update native and frontend saved baselines. Successful Apply becomes clean; committed-with-cleanup-failure retains the existing explicit warning.

Persisted and live state are separate facts. The service must not infer the current window mode or resolution from the INI baseline, including after an earlier persistence-only fullscreen change.

## Failure semantics

Validation or binding failure before mutation leaves files and live state untouched. A resize failure with the original live state verified intact leaves the complete saved draft unchanged, retains editable user edits, and reports an explicit recoverable error.

If a resize partially changes the live state, restore the captured original state through the same verified engine path only when that path is still safe to invoke. Verify restoration. Unknown/device-lost state is not permission to issue repeated resets. If original state cannot be verified, leave files unpublished, report live-state uncertainty, and lock further Apply for the process. Reopening the menu cannot clear this latch.

If resize succeeds but persistence reports NotApplied, use one explicit compensating engine resize to restore the original live state and verify it. Report recoverable failure only when both original file bytes and original live state are verified. Failed or unsafe compensation becomes live-state uncertainty; do not claim rollback succeeded.

If persistence reports IntegrityUncertain, retain its process-wide Apply lock and recovery evidence. Do not claim saved resolution is unchanged or blindly compensate against an unknown disk baseline. Report disk uncertainty and the last verified live state. CommittedCleanupFailed means the requested files are committed: keep the successful live size and surface the cleanup warning, without undoing it.

These are deliberate transaction compensations, not hidden fallback success paths. Logs distinguish attempted size, verified live size, disk outcome, and compensation outcome.

## Versioned result contract

Keep CommitV1 and its existing outcomes 0 through 3 persistence-only for compatibility. Add `Helen_Graphics_CommitV2(session, transaction)` for the new frontend. Reuse existing staging calls and strict primitive decoding; both commit versions consume the same transaction, preventing cross-version duplicate execution.

CommitV2 retains outcomes 0 (Committed), 1 (NotApplied), 2 (disk IntegrityUncertain), and 3 (CommittedCleanupFailed), and adds 4 (LiveStateUncertain). In V2, NotApplied additionally guarantees any attempted live change is verified intact or restored. Outcome 4 means disk state is verified unchanged but live state is not verified restored. Disk uncertainty takes precedence if both are uncertain; diagnostics preserve both facts.

Outcomes 0 and 3 reconcile the saved baseline; 1 retains editable edits with an error; 2 and 4 lock Apply. Missing/invalid native results never trigger a V1 fallback or mark success. A live-integrity latch also blocks V1 commits, so compatibility cannot bypass process safety.

## Verification and rollout

Add native tests for live-first ordering, zero persistence on resize rejection/failure, both dimension checks, actual-versus-saved mode, unchanged size, supported-pair revalidation, safe compensation, compensation failure, device loss, stale owners, and reentrancy. Inject every persistence outcome after a successful resize. Verify duplicate commits across versions and process-lifetime latches. Keep existing graphics and subtitle regressions.

Execute the real generated and decompiled frontend for all V2 outcomes, arrow-only staging, clean successful Apply, recoverable edits, and locked uncertainty. Pin the new callback ABI and engine binding in static compatibility checks. No tests infer success from elapsed time.

Build a uniquely identified candidate from current source and the verified retail base. Do not use installed packs, archives, `batma/`, or historical generated assets as build inputs. Require native tests, generated/decompiled frontend tests, package parsing, executable compatibility checks, and exact delta reconstruction before installation. Deploy only with Batman closed, durable rollback artifacts, fresh hashes, and unchanged subtitle/proxy/config checks.

User testing must confirm at least two windowed sizes in both directions without restart, actual rendered-size evidence, correct input/UI layout, Apply becoming clean, reopening/restarting persistence, and existing unrelated settings. Automated evidence and user observations remain separately recorded. Processor speed cannot determine an outcome; synchronous engine work may still take measurable time.

## Execution boundary

This commit contains design only. The working installation remains unchanged. Review this written specification before creating the implementation plan. Execute inline on main, preserve unrelated dirty files, and do not launch an independent review without Helena's approval.
