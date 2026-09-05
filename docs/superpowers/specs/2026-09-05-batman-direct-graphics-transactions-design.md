# Batman direct graphics reads and transactions

## Status and scope

Helena approved extending the direct-read migration to native transaction ownership and direct Apply communication. This specification supersedes the read-only restrictions and shared mutable Apply-catalog design in `2026-09-05-batman-direct-graphics-reads-design.md`. The existing read-only implementation plan must be revised before execution; its unchanged-write acceptance criteria are obsolete.

Keep all graphics rows, layout, arrow behavior, local editing, Detail presets, supported fullscreen resolutions, configured windowed sizes, and Back/discard behavior. Preserve subtitle functionality and the enabled subtitle-plus-graphics pack set. Do not add video skip or change unrelated runtime protocols.

Replace both graphics read and write memory-carrier communication with executable-pinned, synchronous ExternalInterface callbacks. No graphics request polling, response toggles, scan discovery, sleeps, retry timers, or timeout-based ownership remains. This does not remove unrelated subtitle or runtime observers.

## Architecture and alternatives

Use a native graphics session holding captured settings, immutable resolution catalogs, and a separately owned staged Apply draft. Direct calls carry explicit session and transaction identities. Persistence retains the existing Batman INI encoding, normalization, two-file publication, and reconciliation logic.

The rejected alternative was keeping the carrier transport while adding transaction tracking and a drain barrier around it. Existing numeric messages do not identify their originating screen/catalog, making late-message exclusion another protocol to build and verify. A lock on the existing catalog alone would prevent data races but not stale-index reinterpretation.

Synchronous commit is deliberately selected instead of introducing a new asynchronous task/event subsystem. File and display work may briefly block the menu; execution speed cannot decide success. Do not describe this as zero-latency or guaranteed nonblocking. If profiling later demonstrates unacceptable stalls, propose a separate event-completion design rather than reintroducing timers.

## Responsibilities

- Read/parser service: decode the authoritative launcher `UserEngine.ini` once per session through the existing parser. Preserve valid independent fields and explicit failures. Getters perform no I/O and never populate missing values from dispatcher defaults.
- Display service: build catalogs using existing rules and produce owned immutable catalog data including monitor identity, configured pair, and desktop pair. Exact-pair revalidation consumes that owned catalog, not whichever mutable catalog was refreshed last.
- Session/transaction service: validate identities and state transitions, own bounded snapshots/drafts, and serialize commit with session replacement and cancellation. It contains no Scaleform layout knowledge.
- Persistence service: accept a validated complete draft without relying on a sequence of writes to global dispatcher keys; share the existing writer rather than duplicate its file algorithm. Existing callers may delegate through an adapter.
- ExternalInterface adapter: strict primitive decoding/encoding, allowlisted operations, owned-exception containment, and unchanged stock forwarding. Initialize required services before publishing the hook.
- Frontend: keep edits local until Apply, transfer them to a native draft, commit once, and update labels from the returned outcome. It does not infer native completion from elapsed time.

## Session and transfer lifetime

Opening graphics creates a session with a positive, never-reused-in-process numeric ID. A new open invalidates an older idle session and any uncommitted draft; it cannot replace a committing transaction's catalog. Reentrant/conflicting operations fail explicitly rather than deadlock or use stale state. The supported workflow remains one active graphics editor.

Read transfer and session lifetime are distinct. EndRead releases read-transfer scratch data, not the session's settings baseline or immutable catalogs needed by Apply. Close releases an idle session and discards staged, uncommitted work without writing files. Missing Close cannot grow an unbounded movie map: a subsequent permitted Open retires idle state. Never retain dereferenceable engine objects; identity checks do not depend on a movie address alone.

Generation overflow fails explicitly rather than reusing a handle. Every operation checks argument count, primitive type, finite integer range, handle validity, and permitted state. Invalid input has no persistence side effect. Read failures remain undefined and distinguishable from valid zero.

## Direct transaction flow

The versioned allowlist provides Open, scalar/catalog getters, EndRead, BeginApply, SetField, SetResolution, Commit, CancelApply, and Close. Exact exported names and shared enum values belong in the revised implementation plan and native/frontend parity tests.

1. BeginApply(session) creates a transaction from the valid captured baseline and returns a unique transaction ID. Only one transaction may be staged. Missing required baseline fields prevent Apply with an explicit failure, while independently valid rows remain readable; do not invent a complete draft.
2. SetField(session, transaction, field, value) changes only the owned draft. Use normalized config values, not ambiguous display positions: MSAA's menu position and stored value differ. Preset edits must produce consistent leaf values and derived Detail state through shared normalization rules.
3. SetResolution(session, transaction, kind, index) resolves against the session's immutable catalog and stores the exact pair and monitor identity. Reject missing catalogs, invalid indices, and inconsistent Fullscreen/kind combinations before commit. No later lookup may reinterpret that index against another screen's catalog.
4. Commit(session, transaction) validates the complete draft, revalidates the selected exact pair against the current display environment, and performs the existing persistence transaction synchronously. No side effects occur while merely staging fields.
5. Return a terminal outcome only after persistence and any required reconciliation finish. On success, update native baseline and frontend Initial/Draft state to the committed values; Apply disables until another edit. On failure, keep displayed committed values separate from the attempted draft and present the failure explicitly.

The session retains its owned catalog through every persistence/reconciliation phase. Do not keep a mutex acquired across separate frontend calls: use explicit state ownership, with scoped synchronization for each entry and the full Commit operation. Unexpected reentrancy must be rejected before trying to reacquire a nonrecursive operation lock. Ordinary stock callbacks are forwarded without entering graphics transaction locks.

Duplicate Commit for a consumed transaction must not write twice. Stale transactions and calls from a retired session fail. CancelApply discards only an uncommitted draft; it cannot pretend to cancel completed disk writes. Because commit is synchronous, normal same-thread navigation cannot interleave with it; reentrant Close/Open/Cancel during commit fails without releasing owned state.

## Persistence, rollback, and failure reporting

Preserve existing handling of `BmEngine.ini` and launcher `UserEngine.ini`, their encodings, unrelated INI content, and the current publication/reconciliation protections. Extract a typed result from the existing boolean writer where needed: success, failure with original state verified intact/restored, and failure with state not verified consistent. Do not infer successful rollback from a false return value or merely reload the INI and call it restored.

For validation or staging failure, discard the native attempted draft; no file rollback is necessary because nothing was published. For publication failure, complete the existing writer's reconciliation and verify the resulting state before returning its outcome. Preserve diagnostics and recovery artifacts if restoration cannot be verified. Never report a generic recoverable Apply failure as though both files are known unchanged.

Uncertain disk state locks further Apply for the process and remains visible after closing/reopening graphics; retiring a session cannot clear a persistence integrity failure. No automatic retry, fabricated baseline, or hidden repair. An explicit diagnosed recovery is separate work. Exceptions after possible publication must retain the same uncertainty distinction instead of becoming a harmless undefined result.

Do not broaden this work into new live engine-setting functionality. Preserve the actual behavior of the existing graphics commands and file persistence, including restart requirements where they already exist. Test any retained dispatcher/runtime synchronization explicitly; publishing a staged draft into globally persistent config one field at a time is not an acceptable transaction implementation.

## Frontend, runtime integration, and packaging

The graphics frontend no longer emits legacy FE_SetControlType read/write requests or polls FE_GetControlType for graphics results. Remove graphics-only observer mappings, acknowledgement codes, Apply toggles, and deadline branches after their replacement has behavioral coverage. Retain unrelated stock calls and subtitle mechanisms.

Both native runtime and frontend change in one staged candidate. Do not hot-swap into a running process with old observer workers. Build from current source and verified retail base; installed packs, old archives, `batma/`, and historical generated assets are not inputs. The standalone generated-AS probe transform is not a production build step.

Preserve the installed working probe until native tests, compiled/decompiled frontend checks, native pack parsing, executable signatures, and byte-for-byte delta reconstruction pass. Deploy only while Batman is closed, with durable rollback artifacts, exact live hashes, unchanged subtitle/proxy/config checks, and verified cleanup of temporary staging directories.

## Verification and acceptance

- Numeric ABI: pin executable bytes, argument stride/tag/payload, and return tag/payload; test strict argument validation and stock forwarding. Static evidence currently establishes double arguments at offset 8 and double returns at offset 4; it is not itself a live expanded callback test.
- Reads: one capture per open; no getter I/O; malformed/missing fields and catalog errors isolated; valid zero preserved; slow operations eventually return their actual outcome without a deadline.
- Ownership: retire stale sessions, EndRead versus Close, duplicate Commit, stale transactions, generation exhaustion, abandoned staged drafts, and reentrant Open/Close/Cancel during Commit.
- Resolution identity: change published catalog ordering between sessions and prove old transactions cannot target the new ordering. Delay commit under test control and attempt replacement/cancellation; retain the exact selected pair throughout revalidation and reconciliation.
- Persistence: existing writer regression suite plus injected failures before publication, between the two files, and during reconciliation. Assert original bytes when recovery is reported successful; assert locked Apply and preserved evidence when consistency is uncertain.
- Draft isolation: no dispatcher/config/file mutation before commit; failed staging does not leak values. Preserve MSAA mappings, Detail presets/leaves, Fullscreen/kind consistency, custom windowed sizes, and supported-only fullscreen selection.
- Frontend: execute actual generated and decompiled AS; test arrows, Back/discard, successful Apply becoming clean, explicit failures, and absence of every graphics carrier request/polling branch. A missing direct handler never falls back to the old protocol.
- Live: compare all values with launcher settings, reopen/restart, edit/discard, apply controlled changes, and verify persistence/logs. Separate automated results from user-confirmed game behavior.

## Execution boundary

Only this design is being added now. Revise the implementation plan after written-spec review; do not execute its superseded read-only/shared-catalog tasks. Keep work inline on main as requested and preserve all unrelated dirty changes. No independent review session is authorized.
