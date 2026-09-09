# VSync live-apply investigation (static evidence only)

Read-only investigation approved after fullscreen commit 9dc4358. No runtime
code, installed files, process memory or configuration was changed. No game was
launched. Addresses below are preferred VAs in the SHA-256-pinned retail image
4DAC1F5E2AC6710B7378FDCE74601F616F4753E3756CB5FDA63C7519CC2EB028.

## Established leads

- UTF-16 UseVsync keys occur at 0x0203DAB0, 0x0203ED00, 0x0203FEA0 and
  0x02041188. The first two referenced tables associate the key with settings-data
  offset 0x21C (e.g. 0x00C20226/0x00C20337 and 0x00C3F7A5/0x00C3F8B6).
  The full settings owner includes a four-byte prefix, giving owner+0x220.
- Renderer code reads global 0x026C0D58 at 0x00EA65F9, consistent with settings
  owner 0x026C0B38 + 0x220. The arithmetic through 0x00EA663F produces 1 for
  nonzero and 0x80000000 for zero, then stores it at offset 0x34 within the
  0x38-byte presentation-parameter block initialized at 0x00EA65AE.
- The enclosing renderer routine starts at 0x00EA6310. Its existing-device path
  at 0x00EA6529–0x00EA65A8 can return without rebuilding parameters when mode,
  window and dimensions do not require a rebuild. Merely changing the setting
  and calling the same-size viewport resize is therefore not a proven VSync apply.
- 0x00C40090 is a candidate high-level settings apply wrapper. Stock callers pass
  a full settings-data pointer and another argument; its return is ret 8. It calls
  0x00C3FDE0 when the global at 0x026C3CDC is present and performs a 0xAB-dword
  copy on the alternative path. Nonzero second argument gates calls to
  0x00C3F6B0 and later 0x00C24710. The inner routine also gates the settings
  serialization call at 0x00C3FF66–0x00C3FF7D. This is evidence for a save-control
  argument, not proof that zero suppresses every transitive engine-owned write.
- The settings apply route calls 0x00C24B50, which has a thread check and a
  queued-command path. Its complete effects and command execution contract still
  require tracing before binding it from Helen's menu callback.
- A stock settings reader writes the VSync global at 0x01517291, compares its
  previous value, and writes 1 to 0x026B221C at 0x015172A7 when changed. Another
  stock path saves/disables/restores VSync around 0x016D4C6A–0x016D4CB4 and also
  writes that flag. No consumer of this flag was established by this investigation.
  It must not be treated as a verified refresh request.
- Renderer owner+0x28 is set after device errors at 0x00EA7C2D, 0x00EAA28E and
  0x00EAA29F. Do not repurpose this device-error state to force VSync application.

## Reproduction and recommendation

Search evidence in `output/ShippingPC-BmGame.disasm.txt`; verify critical sequences
against the executable using the existing `Inspect-Viewport.py`. The latter was
run successfully for VA 0x00EA65F9, length 0x4D and checked the whole-file identity.

VSync is not ready to enable from these findings alone. The next required work is
to establish the stock settings-to-renderer refresh boundary and its scheduling,
then verify the actual presentation interval changed without changing resolution
or fullscreen. Preserve the no-save experiment and the working viewport path.
Do not write global flags, spoof device loss, use timers, or claim that a stored
setting proves the current device adopted it.

## Follow-up: queued update and refresh boundary

The queued command from 0x00C24B50 copies nine dwords, not the complete settings
block. Its single-thread path calls 0x00C20B30 directly with the same payload.
That consumer compares/updates globals 0x026C0DE8 through 0x026C0E08 and calls
0x00715940 on change. It does not update/compare VSync at 0x026C0D58. Consequently
this command is not the missing VSync invalidation mechanism.

A raw whole-image search for the little-endian absolute address 0x026B221C found
only the three previously identified writer operands. This does not exclude an
indirect reader, but provides no usable live-refresh contract. The renderer's
owner+0x28 stores were checked and are device-error handling, not a setting API.

The existing-device refresh decision at 0x00EA659C–0x00EA65AE is a concrete
candidate for a narrowly scoped hook: it skips to 0x00EA7388 when its accumulated
rebuild decision is false. The actual engine reset path at 0x00EA664C visits
resource-release callbacks, invokes device Reset at 0x00EA66BE with its own
presentation block, then visits resource-recreation callbacks at 0x00EA67AB.
It includes the engine's pre-existing failure retry behavior; we must not claim
that binding this path adds no possible engine-side wait or guarantees success.

Recommended next design is an explicit, scoped presentation-settings invalidation
request honored at the engine's own rebuild decision, not spoofed device loss or
temporary resolution/fullscreen changes. Apply should use a full current-settings
snapshot with only VSync changed and the candidate engine save-control flag off,
then enter through a verified higher-level viewport synchronization route. Read
back the real swap-chain interval after return; any failure/uncertainty must remain
visible. The exact same-size call chain, calling convention, hook register/flags
preservation, save side effects and reset failure handling need implementation-time
verification. This is a new hook design, not an already safe drop-in binding.

No implementation or deployment was performed during this follow-up.

## Binding-gates checkpoint: 2026-09-08

**Decision: do not implement the bridge yet.** The same-size route has a
synchronous renderer call, but the proposed full-settings apply does not provide
the required unconditional preservation of unrelated live settings. This finding
supersedes the earlier recommendation to use that candidate with only a modified
VSync field. It is static executable evidence, not a reproduced Batman failure.

### Same-size call chain

The pinned image's renderer vtable at 0x02128210 resolves slot 0xD8 to
0x00EAA0A0 and slot 0x1BC to 0x00E9C070. Windows viewport secondary vtable
0x0212C380 resolves slot 0x4C to 0x00CC6410 (fullscreen-bit accessor).

The existing 0x00EB91D0 route calls 0x00EB7250, which updates renderer maximum
dimensions through slot 0x1BC and calls 0x00CBB980 with ECX=Windows owner+4
and stack arguments (release=false, width, height, fullscreen). Positive dimensions
take the normal branch at 0x00EB7345; zero dimensions take special branches and
are not an approved VSync input. The normal branch subsequently calls 0x00C42DA0,
so synchronous reachability does not establish a write-free viewport operation.

0x00CBB980 stores the requested dimensions/mode and, with release=false and an
existing RHI viewport handle at secondary-owner+0x44, calls renderer slot 0xD8
with (handle, width, height, fullscreen). There is no equal-size/mode no-op test
before that call. Missing handles take creation, and release=true takes destruction;
neither is the intended path. The outer Windows call also rejects a busy owner
(owner+0x80) and has fullscreen-specific window-class validation. This is a
conditional existing-viewport path, not a promise for arbitrary engine state.

0x00EAA0A0 derives its viewport container from handle-8, checks the game-thread
predicate 0x00715880, stores width/height/fullscreen at container+0x10/+0x14/+0x18,
and directly calls 0x00EA6310 at 0x00EAA0EA. The final existing-device decision
remains test EAX,EAX at 0x00EA65A6, skip to 0x00EA7388 or rebuild at 0x00EA65AE.
The same-size arguments can therefore reach that decision synchronously.

0x00715880 compares the current thread against 0x026760E4 when thread checking
is initialized (0x026760E8). 0x00CBB980 and 0x00EA6310 both enter rendering
suspension through 0x00732210(1). That scope calls 0x0072F090, which flushes via
0x00729290 and stops/waits for the rendering thread when active. Its destructor
0x00724410 restores the saved rendering state and conditionally restarts the
thread. This is not the queued settings-update path. The engine Reset retry loop
still prevents any guarantee that the synchronous call will return successfully.

Result: the static synchronous-reachability question is supported. Complete
runtime identity checks, callback/reentrancy analysis and patch-span approval
remain pending; no request has been armed or executed in Batman.

### Rejected settings-preservation contract

0x00C40090 uses ECX for the settings owner and two callee-cleaned stack arguments
(incoming settings data, save control), returning with ret 8. With 0x026C3CDC
present, it calls 0x00C3FDE0, whose relevant order is:

1. Synchronize through 0x00729290 and reload configuration into owner+4 through
   0x00C20130 at 0x00C3FE4B.
2. Compare reloaded owner+0x28 against incoming-data+0x24 at 0x00C3FEF1.
   Remember whether they differ, then copy 0xAB dwords from the incoming draft
   to owner+4 at 0x00C3FF03.
3. With save=false, skip 0x00C3F6B0, but continue at 0x00C3FF82. If the comparison
   differed, replace owner+0x28 with its logical inverse at 0x00C3FF90.

The loader table maps data+0x24 to UTF-16 DirectionalLightmaps at 0x0203D8B8:
0x00C20194/0x00C20197 store the field pointer, the push at 0x00C20232 shifts its
stack slot, and 0x00C2028B supplies the adjacent key. Thus owner+0x28 is an
unrelated graphics setting, not VSync.

For example, a live/draft DirectionalLightmaps value of 1 with a reloaded value
of 0 is copied as 1 and then changed to 0 despite save=false. This contradicts
the proposed preservation guarantee even though only VSync was edited in the
draft. The following texture-group restoration covers owner+0x70 onward, not
owner+0x28. With the first data field unchanged, the later component-update
branch is skipped; no direct restoration of owner+0x28 follows in the inner
routine. The wrapper's 0x00C24B50 payload does not carry this field either.

This does not prove every VSync-only call changes lighting: the demonstrated
condition is a live/config mismatch. It is enough to reject the unconditional
contract; live settings must not silently be assumed equal to persisted settings.
Other texture-group, viewport and component side effects also occur outside the
save gate. Full ownership and transitive physical-write certification was stopped
after this rejection and is not claimed complete. 0x00C24710 forms a compact
settings summary; its presence alone is not evidence of a physical INI writer.

Next design work must establish a narrower VSync mutation contract or explicitly
validate a restricted full-apply contract before accepting this entry point.
Do not repair an unrelated setting after the call, skip into an interior address,
or alter the live/config baseline merely to satisfy this gate.

### Reproduction and unchanged installation

The existing decoder completed with exit zero and the pinned SHA-256 for these
exact ranges: 0x00EA659C length 0x12, 0x00EA65F9 length 0x4D,
0x00C20B30 length 0xAE, 0x00C3FEF1 length 0x20, and 0x00C3FF66 length 0x2D.
Use `Inspect-Viewport.py --executable <retail-exe> --address <VA> --length <size>`.
Vtable dwords and the DirectionalLightmaps key were read directly using pefile.
These checks decode instructions; they are not executable bridge or live tests.

The installed HelenGameHook.dll still hashes to
DB757D38C9E02BB9DBD87CD29F6DDABDE77866868C4636956FE9AFB6C7A90752.
No bridge fixture, production implementation, package, deployment or commit was
made during this gate execution. The approved design requires revision before
proceeding beyond the failed settings gate.

## Scalar-plan execution checkpoint

The revised scalar design at b5e6dea was investigated inline. No runtime code,
build, installation or live game call was performed. Tasks 1-2 are **not fully
certified**; dependent implementation remains paused at the origin gate below.

### Established suspension and scalar details

For the selected argument 1, entry 0x00732210 uses ECX as scope storage and one
callee-cleaned dword argument (ret 4). It writes three dwords: saved 0x026B2198
at +0, saved 0x026B2194 at +4, and the argument at +8. This selected branch uses
12 bytes of storage, with dword accesses; no owning pointer or vtable is stored.
It clears 0x026B2198, calls 0x0072F090, then increments 0x026B21A8. The PE imports
confirm 0x01E290CC is InterlockedIncrement and 0x01E290D0 is InterlockedDecrement.
The argument-zero branch is different and is not the proposed binding.

Destructor 0x00724410 receives the storage in ECX, restores 0x026B2198 and
decrements the counter. It restarts through 0x0071F6F0 only when both saved flags
were nonzero. In the ordinary outer-scope/viewport-scope/renderer-scope nesting,
inner scopes save zero flags and do not restart rendering; only the outer exit
can restore the original active state. Entry must complete before a wrapper may
consider the scope constructed. Partial failure through an engine callback has
not been certified as recoverable and cannot be hidden by calling the destructor
on partially constructed storage.

0x0072F090 calls the global callback before its flush and render-thread shutdown.
It clears the active-rendering flag, invokes thread virtual slots +0x10 and +0x0C,
then destroys/releases thread state. Scope exit creates new thread state through
0x0071F6F0. The transitive virtual-call failure/lifetime audit is incomplete; these
observations are not a complete synchronization gate pass.

The UseVsync scalar lies in the pinned image's writable .data section
(characteristics 0xC0000040) and is four-byte aligned. Address resolution remains
module-relative with loaded-instruction validation required.

### More precise viewport persistence condition

0x00C42DA0 compares incoming width/height/fullscreen to settings-owner offsets
0x240/0x244/0x248. If all match, it returns without serialization. If any differs,
it updates those three fields and calls 0x00C3F6B0 (unless owner+0x2D4 bypasses the
routine). Matching the actual viewport alone does not prove the settings-owner
fields also match. This condition must be covered by preflight and preservation
tests, not silently corrected. No claim of globally write-free refresh is made.

### Unresolved request-origin gate

The indirect callback at 0x00CBB9A9 is still executed even when the adapter has
already suspended rendering. The assignment at 0x01509932 gives the callback
global 0x026760EC the function 0x00788170. That function dispatches through
global object 0x026B4380, virtual slot +8. Its stock initialization installs
vtable 0x022D3900 at 0x0150A270/0x0150A28C; the slot resolves to 0x0078F230.
That routine iterates registered objects and calls each object's virtual slot +8
at 0x0078F27A. Registration at 0x0150A291 includes the object constructed by
0x00792400 with vtable 0x01F161C0. The complete transitive callback effects have
not been established by this checkpoint.

The renderer routine also has distinct direct callers at 0x00EA9EEE,
0x00EA9F5F, 0x00EA9FC8, 0x00EAA0EA and 0x00EAA12D. The intended path returns
to 0x00EAA0EF. A renderer/device/thread-only match does not encode which call
activation reached the decision. These facts do **not** prove a stock callback
actually takes a competing path; they mean the required exclusion proof remains
open. Do not label the approach broken, or mark origin safety passed.

Recommended design consideration: bind request consumption to the intended
viewport-call activation, not just object/thread identity. Merely checking one
return address would exclude other direct callers but would not distinguish a
nested invocation of the same caller. A concrete activation check needs separate
instruction/stack validation and approval before changing the request contract.
Alternatively continue the complete registered-callback audit under the current
design. Neither alternative has been implemented.

### Initialization and verification observations

The actual proxy in HelenProxyDInput8/HelenProxyDInput8.cpp calls HelenInitialize
synchronously from process attach and before forwarding DirectInput calls. The
pinned executable imports DINPUT8.dll normally, not as a delay import. The separate
ProxyBootstrapCoordinator background worker is not used by this proxy. This
supports an early startup path, but dynamic-load/teardown and loader-lock safety
must still be accounted for before declaring the hook lifetime certified.

Fresh decoder runs verified the pinned SHA-256 and exact ranges:
0x00732210 length 0x74 (selected entry branch), 0x00724410 length 0x34 (destructor),
0x0078F230 length 0x57 (callback iteration), and 0x00C42DA0 length 0x52 (viewport
settings update). Import names, section flags and vtable destinations were read
directly from the PE. No executable bridge tests or native regressions were run.

### Subsequent isolated x86 request and decision tests

The `experiments/vsync-refresh` fixtures now test exact outer stack activation
with one-shot request consumption, including a nested invocation sharing all
return sites. The wrapper disarms before returning. Separate JMP-entry decision
tests check original and forced exits, duplicate refusal, GPR/ESP/defined TEST
flags, and legacy x87/MXCSR/XMM preservation around a clobbering callback.
The missing-consumption and missing-forced-branch baselines failed before their
implementations. Deliberately removing FX restoration or corrupting ECX also
failed their respective assertions, then was restored.

This is isolated evidence only: the decision callback and real-frame matcher are
not yet composed in one fixture. Production RAII/exception cleanup, concurrent
lifetime tests, loaded-image validation and engine synchronization/initialization
gates remain open. No Batman code or installed files were changed, and no live
VSync result is claimed. See the fixture README for commands and limitations.

### Combined decision path and C++ scope cleanup follow-up

The activation executable now enters the same extracted `DecisionBridge.cpp`
used by CPU-state tests. The callback receives actual renderer EBP, matches the
synthetic return chain and consumes the request. Separate destination counters
verify one outer rebuild and one nested skip. Before wiring this path, the new
test failed because no rebuild destination was reached. An EBP+4 argument mutation
then failed outer acceptance; the correct argument was restored.

`RefreshRequestScope` tests ordinary C++ exception unwinding and normal cleanup,
retains consumption evidence and does not release an owner when a nested arm is
refused. The empty-destructor baseline failed leaked-request detection; an
unconditional-disarm mutation failed owner preservation. Both fixes are verified
by the fixture. The assembly call wrapper still performs explicit normal-return
cleanup: these tests do not establish exception propagation through naked or
retail frames, SEH cleanup, concurrent lifetime safety or live engine behavior.
Installed Batman files remain outside the experiment's scope.

### Scalar footprint, contention and installation-lifetime gate

The experiment-only `VsyncScalar` binds required owned test storage by reference,
rejects existing values other than zero/one, and writes exactly one `uint32_t`.
The no-write baseline failed `enable footprint`; writing the next word also
failed that sentinel test. Invalid values 2 and 0xFFFFFFFF are rejected on reads
and both requested writes without modifying storage. No engine address is bound
by these tests, and this helper does not provide engine synchronization.

`RequestConcurrencyTests` releases 16 real threads using atomic wait/notify for
32 fresh-request rounds. Each round requires exactly one Arm winner and exactly
one Consume winner. Removing the consumed check fails the latter assertion.
Workers deliberately supply the same synthetic observed thread identity; this
tests request-state serialization, not authority to execute on a Batman thread.
It is not exhaustive race detection or callback/module teardown certification.

Fresh pinned decoding confirms the candidate eight-byte decision span is
`85 C0 0F 84 DA 0D 00 00` at 0x00EA65A6. The preceding JNE at 0x00EA65A4 targets
0x00EA65AE, outside that span; it bypasses request consumption. No copied relative
Jcc trampoline may be executed. This local decode is not an all-inbound-edge proof.

**Installation/lifetime gate remains NO-GO with the current interface.**
`HelenProxyDInput8.cpp:170` discards `reserved`, calls `HelenInitialize` on attach,
and also initializes before forwarding exports. `HelenGameHook.cpp:1037` accepts
initialization whenever not initialized, without a startup-only proof. Atomic
graphics-context publication does not exclude execution of the renderer during a
late instruction patch. `HelenShutdown` is a no-op and `HandleProcessDetach`
releases hook owners without removal; neither keeps executable DLL pages mapped.
No module pinning was found in the runtime, game hook or proxy sources inspected.

Microsoft documents that [DllMain's attach context](https://learn.microsoft.com/en-us/windows/win32/dlls/dllmain)
distinguishes static startup from dynamic loading, and that
[GetModuleHandleEx PIN](https://learn.microsoft.com/en-us/windows/win32/api/libloaderapi/nf-libloaderapi-getmodulehandleexa)
keeps a module loaded until process termination. These are available mechanisms,
not evidence that the current code already enforces them.

Proposed scope revision: an explicit proxy-to-runtime startup-only installation
handshake, rejecting late installation of this experiment, and process-lifetime
ownership/pinning for its callback code and state. It must include real loader
fixtures for startup acceptance, late-call refusal, repeated initialization and
unload behavior; do not simply add a Boolean caller claim or pin in an unreviewed
loader-lock path. This changes the shared proxy/runtime lifecycle contract and
therefore follows the plan's design-revision stop rule. No shared lifecycle code
has been changed. Scalar/suspension ABI, transitive callback effects and loaded
binding checks still require their remaining gates; the proposal does not waive
them. There is no installable VSync candidate at this checkpoint.

### Approved startup lifecycle implementation and verification

The approved revision is implemented with `ProxyStartupScope`,
`StartupHookLifetime`, `ProcessModulePin`, and the opt-in `StartupHookRuntime`
adapter. Only the proxy's actual static-attach scope exposes its current thread.
The gate queries that scope, admits one attempt, pins code before invoking the
initializer and closes the installation window on returning success or failure.
Dynamic loading and late/reentrant calls do not open a window. No new worker,
polling or timer was added. Pins deliberately survive failed initialization;
they cannot be undone. The cooperating-proxy query is not an adversarial security
boundary. The normal proxy compilation excludes this interface.

Real loader tests use the actual `HelenProxyDInput8.cpp`, not a replacement proxy.
The initial refusing gate failed startup acceptance. Static startup, dynamic-load
refusal, late refusal, reentry refusal, invalid callback pin refusal and initializer
failure now pass. Replacing PIN with UNCHANGED_REFCOUNT caused the balanced-release
test to fail; PIN was restored. Both success and failure drivers run disposable
processes with console-only failures.

Fresh full opt-in build: `output/startup-hook-probe-20260908-c/native`.

| Artifact | SHA-256 |
| --- | --- |
| HelenGameHook.dll | C6BA09FD9DAAB240CC560371B56A21CF4DDD10CB97A26C78E143BE3C61027F05 |
| dinput8.dll | 7BA82D48DB9934FA429D0E99F9D8B7164045C24F9CFFDE08105755FECB797C32 |
| HelenRuntime.lib | 6F256F999708CA86544AAF0F09E77901A08658E864B588A231042A9728F06201 |

Normal control build: `output/startup-hook-normal-20260908-a/native`. PE export
inspection found the startup/query exports only in the opt-in pair. Both real
runtime/proxy pairs passed the pack-free host test. The first host omitted the
LoadLibrary import required by the existing default module-routing policy; adding
that genuine import to the fixture resolved initialization in both controls.
No production fallback or routing-policy change was made.

Build attempts a/b stopped before compilation because MSBuild inherited a PATH
key conflicting with MSVC's Path entry. Normalizing only the build process's key
spelling resolved this. Existing Hook.cpp conversion warnings and proxy COM export
warnings remain. The full native suite freshly linked against the opt-in library
reported PASS; no pre-existing failure was reproduced in that run. Hidden-window
D3D9 reset, failure and initial-surface variants passed with GPU access. The first
sandboxed D3D run returned D3DERR_NOTAVAILABLE at device creation; the same fixture
passed outside that restriction without changing its behavior or display mode.

The activation fixture now encloses its real naked call chain in a C++ request
scope. An injected C++ exception before the root renderer initially leaked an
armed request; after moving ownership outside the native frame, it propagates
and disarms. Normal cleanup still occurs before the anchor publisher returns.
This adds concrete native-frame unwind coverage but does not certify arbitrary
retail SEH, driver faults, fiber migration or engine failure recovery.

### Continued suspension audit and remaining patch-transaction gate

The startup factory at 0x01509B69 selects object 0x025EA234, whose vtable
0x022D36BC creates threads via 0x005F0230. That constructor installs thread vtable
0x01E9F82C. Its +0x10 slot is 0x005EC380, confirmed by fresh pinned decoding to call
WaitForSingleObject(thread+4, INFINITE); IAT 0x01E290F8 resolves to that API.
The shutdown path 0x0072F090 invokes this wait after clearing active rendering,
then +0x0C (0x005F0160), which closes the handle. The wait result is not checked by
stock code. A production preflight therefore cannot equate a returned scope entry
with verified thread termination without further ownership/observation checks.

The registered callback 0x007960E0 iterates Texture2D objects, resolved through the
class constructor 0x00733250 and vtable 0x01EEF318, calling slot +0x124. The stock
implementation is 0x0072FA80 and can complete streaming work; the caller waits
while it remains busy. These are existing engine waits, not new Helen polling.
The complete live subclass/resource callback inventory remains unverified.
The pinned executable's TLS directory is empty. None of this is a live VSync test.

**Next integration gate:** `HelenRuntime/Memory.cpp:127` reports true after memcpy
regardless of FlushInstructionCache or the protection-restoration result.
`InlineHook::Install` relies on that Boolean before publishing its ownership;
`Remove` ignores the restoration result and releases its trampoline. Consequently
the current interface cannot represent a partially completed patch or retain
ownership conditionally on confirmed removal. No such OS failure was reproduced
in the successful fixtures; this is a source-proven failure-contract gap, not a
claim that the installed Batman pack is broken.

Adding only a false return after writing is insufficient: callers could release
state while patched bytes remain. A shared patch transaction/result and hook-owner
failure contract needs its own failure-injection tests and approval under the
plan's hook-infrastructure revision rule. No common memory-writer or hook-installer
behavior was changed. VSync patch installation remains disabled; the working
installed files are unchanged and this lifecycle build is not a gameplay package.

### Approved shared patch-ownership revision

The user approved this broader correction after the preceding gate report. The
historical memory-writer limitation above is now addressed by `MemoryPatch`:
it captures original bytes and distinct VirtualQuery-region protections before
modification, reports write-access/cache/protection failures independently, and
retains recovery ownership until Restore confirms all stages. No background retry
or relaxed engine-binding assumption was added. The source range must be readable
and the caller must exclude concurrent execution/writes/unmapping; arbitrary SEH
recovery and instruction relocation are explicitly outside this contract.

InlineHook and IatHook expose TryInstall/TryRemove for owners able to retain their
dependencies after failure. Legacy Install rolls back before returning false;
legacy Remove/destruction cannot return while restoration is unresolved. The
legacy raw byte/fill writers likewise roll back before false. Unrecoverable
cleanup calls the no-dialog process-stop path (exit code ERROR_WRITE_FAULT, 29),
not a success fallback or a silent trampoline leak. Hook declarations are split
into one class per header; Hook.h remains the compatible umbrella include.

Failure tests compile the real Memory.cpp, MemoryPatch.cpp and Hook.cpp with only
test-local Win32 protection/cache/free interception. All uninjected operations
call Windows, including allocation, writes, real IAT replacement, original
trampoline execution and restoration. New per-run output is mandatory.

Recorded red/green evidence:

- Baseline `memory-patch-a9c7914ed91740e59692900d2fcb49a2` failed with
  `flush failure reported success`.
- Baseline `memory-patch-d520938029c14b94a35cee855bfc1f22` failed with
  `failed removal discarded ownership`.
- Deliberately discarding ownership after failed Restore failed with
  `failed retry discarded owner` in `memory-patch-0497e4c953c7435898c90f512ceab975`.
- Deliberately restoring RWX instead of the original protections failed with
  `mixed protections were flattened` in `memory-patch-4264dcfcd87e41949441930d33b2c83c`.
- A last-result regression was first reproduced in
  `memory-patch-97a2be222da04388904afa61dd137d8b`, then corrected.
- Final source fixture `memory-patch-e24f8218ecf040a4ac3d4dd5917a3771` passes all
  six groups, including five expected-termination child scenarios. Compilation
  uses `/W4 /WX` without suppressing conversion warnings. Both deliberate mutants
  were restored before this run.

The initially unmanifested fixture was refused before main. Console debugger
launch reported elevation required; embedding an explicit asInvoker manifest
resolved it without elevation or a machine-wide setting change. The fixture
runner adds the SDK resource compiler to its own process PATH. A temporary static
CRT diagnostic build did not solve the launch issue; the final fixture uses MD.

Initial integration artifacts under `owned-patch-probe-20260908-a` and
`owned-patch-normal-20260908-a` built successfully. Both exact runtime/proxy pairs
passed pack-free startup tests; the complete native suite and all three D3D9
variants passed against the opt-in library. Final rebuilds include the additional
last-result correction; their evidence is recorded below when complete.

#### Final corrected integration results

Fresh full opt-in build: `output/owned-patch-probe-20260908-b/native`.

| Artifact | SHA-256 |
| --- | --- |
| HelenGameHook.dll | B4E0EF483E5B5CDF7BA458F7C4A02A420A0EEFD439A7187561C561CBE62DB13D |
| dinput8.dll | 71007659B0B710F8C921CC90C0E7F3EA8334F2E8EC605FCB3BB08D9D1771765C |
| HelenRuntime.lib | 39402AA07CDC81537A38A4C8398CD571D3AD18968C2AF051D08A59F75B2C8859 |

Fresh normal control: `output/owned-patch-normal-20260908-b/native`. Both builds
exit zero; the four existing proxy COM-export LNK4104 warnings remain. No compiler
warning suppression is used in the standalone failure test. Final source changes
were compiled from fresh output trees, not copied from an installed pack.

- Opt-in real runtime host: `runtime-startup-9d900e98ef3641239b34babc2d45185d`, PASS.
- Normal real runtime host: `runtime-startup-a7abe4698d514729af67bbd3388a75ef`, PASS.
- Full native suite: final opt-in directory's
  `tests-5c04a7656033402f85a04a8811514035/HelenRuntimeTests.exe`, PASS, including
  generated Batman protocol and file-routing child markers.
- D3D9 reset: `d3d9-reset-fixture-ca51390bac6f4cfda1ef9185fdbf9435`, PASS.
- D3D9 failure: `d3d9-reset-fixture-61f67f190e7d4c5fabe6c85c5fbf7cf7`, PASS.
- D3D9 initial surfaces: `d3d9-reset-fixture-4815d94ed5904b4d926a8fdd5104409e`, PASS.
  Each reports the exact final library hash above. GPU access was granted for
  hidden-window fixtures; none launches Batman or changes the display mode.
- Actual-proxy normal/failing loader fixtures:
  `startup-loader-664d34fd778a4956b86a7d89bff264a9` and
  `startup-loader-01e8a3547ee248e382557e0a7add5fd8`, all six PASS markers.

The final diff check reports no whitespace errors (Git notes configured LF/CRLF
conversion). The installed HelenGameHook.dll still hashes to
`DB757D38C9E02BB9DBD87CD29F6DDABDE77866868C4636956FE9AFB6C7A90752`.
Everything remains uncommitted on main; unrelated batma/ was preserved.

The shared patch transaction gate is closed within its exclusive-memory-access
contract. No retail decision patch was installed or invoked. Remaining VSync
gates still include verified render-thread stop observation, callback/resource
binding and inbound-edge/engine-side-effect certification. These test results
must not be presented as a live VSync success or a deployable gameplay package.

### Render-thread ownership and wait-result gate (2026-09-08 continuation)

Read-only inspection reverified the retail file's 38,758,728-byte length and
SHA-256 `4DAC1F5E2AC6710B7378FDCE74601F616F4753E3756CB5FDA63C7519CC2EB028`.
The existing Inspect-Viewport decoder consumed every byte in the ranges below;
end addresses are exclusive. These are static observations, not live acceptance.
Python ran through Invoke-BatmanConsoleTool with crash dialogs suppressed. An
initial inline command failed on Windows argument quoting before analysis; the
corrected command completed with exit zero.

| Range | Decoded instructions | Role |
| --- | ---: | --- |
| 005F0230..005F02CC | 64 | Thread-object allocation and factory failure |
| 005F0070..005F015F | 75 | CreateThread, handle and ID publication |
| 005EC290..005EC336 | 60 | Runnable execution and conditional self-deletion |
| 005EC380..005EC394 | 10 | Wait, padding and thread-ID accessor |
| 005F0160..005F0226 | 64 | Stop, optional wait, close and conditional deletion |
| 0071F6F0..0071F767 | 33 | Render-thread startup |
| 0072F090..0072F124 | 45 | Render-thread shutdown |
| 0073DA90..0073DD67 | 208 | Render-command execution loop |
| 0073DD70..0073DDF5 | 42 | Runnable exception wrapper |
| 0071F770..0071F7E0 | 29 | Engine render-health reporting and tail call |
| 00CC3340..00CC3383 | 22 | Viewport resource-reference release |
| 00EAE930..00EAE956 | 13 | RHI reference decrement and virtual destruction |
| 00EBB9C5..00EBB9F8 | 13 | Constructor excerpt installing viewport vtables |
| 00EA656A..00EA65AE | 23 | Decision predecessors and proposed patch span |

The PE import table resolves 01E290B8=CreateThread, 01E290C0=Sleep,
01E290F8=WaitForSingleObject, 01E29134=CloseHandle and
01E292B4=GetCurrentThreadId, all from KERNEL32.dll.

The factory allocates 0x1C bytes and installs vtable 01E9F82C. Object offsets
are +4 HANDLE, +8 runnable, +0C thread auto-delete, +10 runnable auto-delete,
+14 priority and +18 thread ID. Startup passes zero for both auto-delete flags,
stack size and priority. CreateThread writes the ID through object+18; the
returned handle is stored at +4. The stock thread therefore does not normally
free itself or close its handle in 005EC290. This supports ownership analysis;
it does not authorize caching the object across shutdown. Startup publishes the
active flag before factory completion and does not check the returned object
before global publication, so active=true is not a sufficient preflight.

Vtable 01E9F82C has +0C=005F0160, +10=005EC380 and +14=005EC390.
The wait helper's exact bytes are `8B41046AFF50FF15F890E201C3`:
read object+4, call WaitForSingleObject(handle, INFINITE), return its EAX.
At 0072F0BD shutdown clears 026B2194 before calling that helper. The caller
does not inspect EAX. It next invokes 005F0160 with arguments 0 and FFFFFFFF;
that first argument skips the helper's optional second wait. It closes the
original handle, clears it, then shutdown frees the thread and runnable objects
and clears their globals. Returning from this whole routine or observing its
cleared globals therefore does not independently prove a successful wait.

Windows distinguishes WAIT_OBJECT_0 from WAIT_FAILED; INFINITE does not convert
an API failure into successful synchronization. A zero-time check returns
immediately, and the handle must stay open during the check. See
[WaitForSingleObject](https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-waitforsingleobject).
A separately owned duplicate refers to the same kernel object; it could preserve
an observation handle across the original handle's closure. See
[DuplicateHandle](https://learn.microsoft.com/en-us/windows/win32/api/handleapi/nf-handleapi-duplicatehandle).
However, checking that duplicate only after suspension entry returns is too late
to prevent the engine's intervening cleanup if its original wait failed. Do not
implement an observation helper and call this failure contract solved.

Runnable vtable 01EEE414 has Run=0073DD70 and no-op Stop/Exit targets
00719C20/00719C10. The execution loop tests 026B2194 but can still execute queued
commands before exiting. Its exception wrapper writes zero to 025D7E2C; that
field's initial file value is one. Both normal and handled-exception paths return
zero from Run. Engine health reporter 0071F770 checks this separate flag.
Neither a zero thread exit code nor a signaled thread alone proves healthy
rendering. This does not establish that such a failure caused any earlier freeze.

The stock render-thread predicate 00715850 accepts a null thread global, and the
main-thread predicate 00715880 accepts an uninitialized 026760E8 flag. Eventual
preflight must validate initialized, concrete thread identities rather than
treating either permissive predicate as a complete ownership check.

#### Viewport callback and incoming-edge progress

The Windows viewport constructor writes vtable 0212C360 at viewport+0C.
The CBB980 -> 71A040 resource argument resolves to that subobject on this stock
path. Vtable +18 is 00719E50. When the resource is initialized and RHI is active,
that method calls +10=00656320 (RET), then +8=00CC3340, before unlinking it.
00CC3340 clears and releases resource+24 and resource-4, corresponding to Windows
viewport+30 and +8. Both releases use RHI vtable+230. Stock target 00EAE930
decrements the reference count and can tail-call the concrete object's virtual
destructor. The direct viewport callback is now resolved; the possible resource
destructors and broader callback inventory are not all certified.

The exact decision bytes remain `85C00F84DA0D0000` at 00EA65A6..00EA65AE.
There is a direct incoming `753A` JNE at 00EA656A targeting the start, 00EA65A6.
The `7508` JNE at 00EA65A4 bypasses the span to 00EA65AE. Both must be preserved.
Textual branch and raw pointer searches are discovery aids, not proof that no
computed indirect entry can target an instruction inside the span.

#### Gate result and proposed revision, not approval

Task 1's successful stock shutdown path is substantially better identified, but
its checked-failure boundary is not supplied by the current adapter design.
Task 2 still has resource/callback and incoming-edge proof outstanding.
Production Tasks 5-7 remain stopped. No experiment was installed or invoked.

Recommended revision for approval: add a candidate-only, operation-scoped guard
at the verified thread-wait boundary, before engine cleanup. Preserve the stock
wait and inactive behavior; validate exact operation/thread ownership. On a
failed or unexpected wait result, preserve diagnostics and terminate without a
MessageBox rather than return into cleanup or unwind into render-thread restart.
This would deliberately end the game and could lose unsaved progress, so it is
a failure-policy change requiring approval, not an ordinary refusal result.
Do not add polling, a timeout, forced thread termination, a second Reset, or a
global Win32 wait hook. The exact patch/ABI and failure-injection fixture would
need their own design and verification before integration. This guard would not
solve an engine wait that never returns or close the other binding gates.

Alternatives considered: a post-entry duplicate-handle check is too late for this
failure; silently assuming every wait succeeds weakens the approved contract;
reimplementing shutdown would take over engine lifecycle responsibilities and is
a broader revision. Leaving live VSync disabled remains safe. No production code
or failure policy was changed during this continuation.
