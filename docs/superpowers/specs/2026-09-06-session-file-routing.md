# Session file-write routing

## Approved outcome

Pack authors can deny writes to exact selected files or redirect them into real, session-owned temporary files. They choose whether later game reads use the original or redirected copy. Batman uses redirect plus redirected reads for its user `BmEngine.ini`. HelenHook can explicitly persist originals and must synchronize the active session copy before reporting an ordinary successful save. User approved specification, planning, and Luna-high implementation while AFK; no further design approval is required.

## Boundaries

- Work on main, preserving `batma/` and the installed working experiment.
- Generic routing belongs in HelenRuntime; Batman-specific path selection and save translation remain in Batman integration.
- No polling, sleeps, retries, engine-address persistence, or runtime repair fallbacks.
- Existing subtitle/menu functionality and old packs without routing declarations remain compatible.
- This is cooperative main-executable Win32 IAT routing, not an OS security sandbox. Dynamically resolved APIs, native syscalls, other processes, and unhooked DLL imports are outside its protection boundary. Document and inspect the first consumer's actual imports before claiming coverage.
- Route initialization and required-hook failure must prevent activating a routing-dependent build hook. Never continue with an unprotected live resize feature.
- One class per file, substantive Doxygen on new declarations, RAII, explicit errors and ownership, project conventions. Tests exercise real temporary files and production routing, not source-string presence.
- Implementers and ordinary task reviewers use `gpt-5.6-luna` at `high`; no special independent-review session is authorized.

## Declaration

Optional build.json `fileWriteRoutes` array, entries:

```json
{"id":"engine-config","root":"documents","path":"Square Enix/Batman Arkham Asylum GOTY/BmGame/Config/BmEngine.ini","writePolicy":"redirect","readPolicy":"redirected","lifetime":"session"}
```

`root` is `documents` (Windows Known Folder) or `game` (installation root). `path` is an exact normalized relative file, never a glob or directory; reject rooted paths, traversal, device paths, alternate streams, empty components/terminal filename, invalid enums, duplicate IDs/targets and overlap with virtual/missing paths. `writePolicy` is `deny` or `redirect`; deny requires `readPolicy: original`. Redirect allows `original` or `redirected`. `lifetime` must be `session`. Missing declaration means no routing, not a default route. No hardcoded username, persistent machine address, or arbitrary pack-selected output directory.

## Session and paths

Create a fresh uniquely named directory under runtime cache; never reuse previous content, including after a crash. Initialize redirected files from originals before installation. Initially require existing regular originals and reject reparse-point traversal and ambiguous aliases. Normalize ordinary case/slash/dot spelling and recognize supported extended/short path aliases or explicitly reject them when they could address a protected file. Do not allow a spelling change, hard-link alias, or parent-directory rename to quietly mutate a known protected original through covered APIs. An implementation may explicitly reject unsupported alias/mutation forms. Ordinary unrelated operations must retain native behavior.

Best-effort cleanup is not a data-path fallback: normal shutdown removes only this session's owned artifacts; cleanup failures are logged and never cause reuse. Crash leftovers are inert, not treated as a new session's data. Do not recursively delete arbitrary cache content.

## Real handles and mutation semantics

Route CreateFileA/W before read-only asset virtualization. Deny creation/truncation/deletion/write-capable opens explicitly; redirect them to session files with original access, sharing, and disposition semantics. Reads select the declared source. Track routed handles until real CloseHandle succeeds. Unrelated and synthetic virtual handles preserve existing behavior.

Cover imported ANSI/Unicode delete, move, move-ex, replace, copy destination, attribute mutations, and handle-based disposition/rename as applicable. Temp-file publication to a protected target must reach only its session file. Protected sources must not escape through rename/replace backup paths. Unsupported compound operations fail explicitly before any mutation. Never let deletion of an overlay fall back to the original on the next read.

Writable mappings, overlapped/inheritable routed handles, and handle duplication may be rejected explicitly in this first version; do not pretend they are synchronized. Reject handle mutation paths that cannot be safely tracked. Real synchronous WriteFile and SetEndOfFile on an already redirected handle operate on the redirected file. Required import installation is transactional and coexists with existing virtual-file hooks.

## Trusted HelenHook writes

A scoped `FileWriteRoutingTransaction` acquired from `FileWriteRoutingService` names the exact original paths being saved. It serializes routing operations across the write and synchronization, and refuses acquisition with `ERROR_SHARING_VIOLATION` while affected routed handles remain open. No waiting for handles to close, deadlines, global bypass switch, or discarded pending game writes. Native HelenHook file operations explicitly inside the transaction use original paths; no arbitrary game caller gains that capability.

After the existing writer completes (including recovery), call transaction synchronization even after a failed save, to mirror the verified resulting original bytes. If synchronization fails, latch affected routes failed for this session: subsequent protected opens/mutations fail explicitly; do not hand out stale data. An abandoned transaction also fails closed. A no-write cancellation releases a transaction without changing files. The API returns Win32 error details and distinguishes original persistence from overlay synchronization.

The original may already be committed when synchronization fails. Batman reports `CommittedSessionSyncFailed = 4` for that case, logs the distinction, and locks further Apply operations. Existing values 0..3 retain meanings; direct frontend already treats values other than 0,1,3 as locked failures, but give 4 an honest explicit message if practical. The experimental no-save engine probe never uses 4. On sync failure following a failed/recovered save, use existing integrity-uncertain behavior and lock. Subtitle saves return false and log a partial failure if original persisted but sync failed. All Batman config service instances share the same routing owner.

## Validation and deliverable

Prove original bytes unchanged by redirected opens/writes, truncate, delete and temp-file replace; deny is real failure; subsequent reads select correct data; session restart discards prior writes; case/alias paths cannot bypass; busy handles prevent trusted save before original mutation; trusted save updates original and overlay; synchronization failure is distinguishable and fails closed; malformed packs and conflicting routes are rejected; packs without routes and existing graphics/subtitle tests remain passing.

Use a real hooked child fixture to exercise API imports (not private-detour visibility hacks), including mixed virtual/routed/unrelated files. Build fresh Release Win32 native artifacts. Generate a separate routing test candidate from committed/current source and verified retail assets, with hashes and runtime schema validation. Do not use an installed pack, historical output, or `batma/` as build input. Do not deploy automatically while user is AFK: preserve installed known-working experiment, deliver candidate plus explicit live-test steps and mark live acceptance pending. Live-resize promotion beyond the existing experiment is not part of this feature.

## API references

Windows sharing/creation semantics: https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-createfilew

Replacement semantics: https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-replacefilew
