# D3D9 presentation override

HelenRuntime supports `GameControlled` (default), `ForceOn`, and `ForceOff` on
the existing D3D9 hook set. The policy is session-only and independent of Batman
instructions, settings memory and INI files.

```cpp
hooks.PresentationPolicy().SetVsyncOverride(helen::D3d9VsyncOverride::ForceOff);
```

Enable/install the hook set explicitly, even without texture replacements, and
keep it alive while accessing the policy. Selection is thread-safe but is only
desired state: it neither installs hooks nor creates/resets a device. Unknown enum
values throw without replacing the previous selection.

Intercepted `IDirect3D9::CreateDevice` and `IDirect3DDevice9::Reset` change only
`PresentationInterval`: ONE for on, IMMEDIATE for off, or no change for
game-controlled. The original API receives the same in/out parameter pointer;
native output changes are not discarded through a private copy. The override
does not retry failures, silently substitute another interval, or add resource
ownership. Existing reset cleanup and subtitle replacement restoration remain.

One atomic snapshot applies across the complete parameter array. Adapter-group
array length comes from D3D9 capabilities. Reset uses the count retained at device
registration, without querying a lost device during reset recovery. Failed layout
queries are explicit failures, not a single-adapter fallback. No Direct3D9Ex or
additional-swap-chain interception is added by this slice.

A genuine game-driven resolution/fullscreen change can supply the next reset.
The policy persists across subsequent resets. Returning to game-controlled hands
future requests back to the application; it does not change the current device
immediately. Menu code must distinguish desired policy from observed device state
and saved configuration.

## Tests

- `tests/Test-D3d9PresentationPolicy.ps1`: exact parameter-byte footprint,
  multi-block and repeated application, transparent mode, invalid selection and
  concurrent publication. CPU-only.
- `tests/Invoke-D3d9ReplacementResetTests.ps1 -RuntimeLibraryPath <fresh-lib>
  -PresentationOverride`: actual swap-chain readback at creation and after resets,
  delayed effect, on/off transitions, persistent override, return to game control,
  unchanged size/mode/format, genuine failed-reset HRESULT preservation and exact
  subtitle pixels/texture lifetimes. Observation references end before resets.
- Default, `-FailureCases`, and `-InitialSurfaceHooks` variants remain separate
  regressions against the same explicit runtime library.
- `-PresentationOnly` exercises creation/reset with no texture rules, replacement
  assets, hashing or dumping enabled.

GPU tests use hidden windows and suppress crash dialogs. They never launch Batman
or switch the desktop to fullscreen. Unsupported interval capabilities are
reported as untested/failure, not pass. Multi-adapter rewriting has CPU coverage,
not multi-monitor hardware acceptance. Swap-chain readback verifies API state,
not compositor or driver-override behavior.

The older Batman instruction-bridge/wait-guard plan is paused. Neither is part of
this implementation. The opt-in Batman menu connection is described below;
installation still requires a separate request.

## Recorded verification, 2026-09-08

The CPU test first failed against the no-op implementation with
`ForceOn did not reach every presentation block` in
`output/d3d9-policy-62314868432b4015ad2bbd06649c779a`.
The GPU test first failed before hook integration with
`Actual swap-chain interval did not honor override` in
`output/d3d9-reset-fixture-9f8485568c0741fdb83f39a9e0a44bda`.
An earlier sandboxed GPU attempt could not query HAL capabilities; GPU access
was then authorized for hidden fixtures. That environment failure is not a
behavioral red test or a passing unsupported transition.

Fresh final build: `output/d3d9-policy-final-20260908/native`, using
`Build-StartupHookProbe.ps1 -NormalControl`. The name is a reused isolated build
driver; startup hooks and Batman instruction bridges are not enabled.

| Artifact | SHA-256 |
| --- | --- |
| HelenRuntime.lib | 3E957E3C36C56769880E4162DB2726E25DCE91DF15FDF68000F95A8B33DEDB50 |
| HelenGameHook.dll | 7BCA3B5B828BFCB972A81D8606D84C199B605BF042D4206B248952F81EAFE43D |
| dinput8.dll | 77CB8348BE6253023A0728455FBA2B018D137FDFC0107B3B9C011C659272363B |

Final passes (all fixture names below are inside repository output):

- CPU policy: `d3d9-policy-5dda7cb6623244b0ae3c1df8343b2097`.
- Pack-free GPU: `d3d9-reset-fixture-33f6bd23a16a4a59ba772c5320d0f162`.
- Override GPU: `d3d9-reset-fixture-501b9fc1f51148e89d3aafe46811e671`.
- Default GPU: `d3d9-reset-fixture-2ded8bb176944ea6916dbbe35f4ad3e8`.
- Failure GPU: `d3d9-reset-fixture-4ab98776c8164cfe83c8bc194d4636c0`.
- Initial-surface GPU: `d3d9-reset-fixture-dbc8d15205b54ae1be5c8aec5f09a955`.
- Real runtime/proxy startup: `runtime-startup-a425c49a8d2b4ad09c6f4e896002e04e`.
- Full native suite: final build's
  `tests-2f45476df1894e39a02001f406f7b696/HelenRuntimeTests.exe`, including
  generated Batman protocol and file-routing child passes.

Standalone fixtures compile with /W4 /WX. Full builds retain the four existing
proxy export LNK4104 warnings; standalone library linking reports automatic LTCG.
Review stayed inline; no independent reviewer or subagent was launched.
The installed HelenGameHook.dll remains
`DB757D38C9E02BB9DBD87CD29F6DDABDE77866868C4636956FE9AFB6C7A90752`.
No commit, installation or Batman live test was performed.

## Batman no-save menu connection

The opt-in generated Commit now passes the editor's captured baseline and staged
draft to NoSaveResolutionProbe. If VSync changed, NoSaveVsyncRequest accepts it
only alongside a resolution/fullscreen edit and rejects unrelated mixed edits.
Display-only edits leave the current override untouched. The existing live
NoSaveDisplayRequest validation still requires an actual size/mode transition.

After existing executable, thread, viewport, display and one-attempt checks,
the probe observes the current interval and publishes through
`D3d9TextureReplacementHookSet::TrySetVsyncOverride(device, mode)`. Publication
requires a registered device not currently resetting and holds the registry lock
through the policy write. Active-owner publication/retirement uses that same
lock; no raw hook-owner pointer escapes to the menu. This does not alter the
broader existing detour lifetime contract or add a new device owner.

Then the probe calls the existing engine resize entry, unchanged. It observes
the actual swap-chain interval afterward and logs a mismatch explicitly. An
engine resize is not assumed to imply a device reset. Observation references end
before the engine call. No engine VSync scalar or instructions are patched.

Selection persists if a later resize fails; the probe does not claim rollback.
The existing one engine attempt per process remains. VSync-only Apply is refused
before engine entry. The UI's intentional Apply Failed/NotApplied result remains
because no saved baseline is published; logs distinguish selection from actual
interval readback. Existing file routing is unchanged.

Build with `games/HelenBatmanAA/experiments/windowed-resolution/Build-NoSaveProbe.ps1
-OutputRoot <new-repository-output-directory>`. Preparation requires a new root
and exactly one source substitution; normal Commit source still uses Config.ApplyDraft.

Fresh connected candidate: `output/d3d9-menu-20260908-a/native`:

| Artifact | SHA-256 |
| --- | --- |
| HelenGameHook.dll | 0D831738862EBD84D8FCAE3E3717B246177FC04792288ABE458763E47B37D7CE |
| HelenRuntime.lib | 48A95BD842ECDC7878892047E7E97C3C24C5BE97C0D6325EC79E47A0F58AF800 |
| dinput8.dll | B02E16503402DF25973B40396EDAE6B0F03C07CDCDA3E9365BB58204F4C6A017 |

Verification:

- Request-selection test failed against the stub with `Menu VSync on was not
  selected before display apply` (`vsync-request-dcee2f9a940e478c999c31c664a78438`).
  Final CPU pass: `vsync-request-7866be8be03f45b889cfc867f7d3162f`.
- Device-bound publication failed against its stub in
  `d3d9-reset-fixture-3382715eb8094c19830bbde87991d0c7`. Final pack-free GPU pass:
  `d3d9-reset-fixture-f93c2c139284495796ed71da0640a31e`, including invalid-mode
  rejection, deferred effect and rejection after owner retirement.
- Real generated Commit pass: system temp
  `NoSaveSession-61b459546c7f47d2ab4cd213fcc977ca`. It proves baseline/draft
  propagation, VSync-only refusal, foreign-executable refusal before policy
  publication, single transaction consumption and unchanged fixture INIs.
- Display request pass with GPU access:
  `display-request-d66ec35a26c242bdbb8a649ef540dc44`. The earlier sandboxed
  hidden-device creation failure was environmental, not a passed test.
- Candidate GPU interval/subtitle pass: `d3d9-reset-fixture-e325082290a54214b258ca8e59cd0433`.
- Candidate GPU failure pass: `d3d9-reset-fixture-d53fa6a185e04e999ff5001de76c95dd`.
- Candidate initial-surface pass: `d3d9-reset-fixture-841b33952bfa4633a8206b933ab0744e`.
- Candidate runtime/proxy startup pass: `runtime-startup-1eef1d6091fe4c98b7b58955fc2b75b0`.
- Fresh normal control: `output/d3d9-menu-normal-20260908-a/native`; full native
  suite passed at `tests-cecd3b6852624a049cf3ad7355644725/HelenRuntimeTests.exe`.
  Normal runtime/proxy startup also passed at `runtime-startup-b606baf58e15427f81373d7ba0c0a24a`.
  Normal persistence tests are not asserted to pass against the intentionally
  no-save candidate; its Commit uses the separate no-save fixture above.

The four existing proxy export warnings remain. No Batman launch, installation,
commit, INI mutation in the installed game, or instruction-bridge work occurred.
Installed DLL hash remains DB757D38C9E02BB9DBD87CD29F6DDABDE77866868C4636956FE9AFB6C7A90752.
Actual in-game menu/engine integration still needs live acceptance after an
authorized installation; the fixtures do not execute the retail resize entry.

### Authorized installation

On the user's subsequent installation request, Batman and its launcher were
confirmed closed. Only Binaries/HelenGameHook.dll was replaced with candidate
0D831738862EBD84D8FCAE3E3717B246177FC04792288ABE458763E47B37D7CE and its installed
hash verified. The prior working DLL is backed up at
`output/d3d9-menu-install-a9b31d3e3a9d4dcd8d0e08d998e0ce19/HelenGameHook.dll`.
All 41 protected proxy/pack/config files retained their hashes. Before/after
snapshots of 31 INIs reported zero differences and are stored in that same
rollback directory. No game was launched; live acceptance remains pending.

For the live test, change VSync and resolution/fullscreen in the same Apply, once
per fresh game process. Check `helengamehook/logs/HelenGameHook.log` under Binaries
for `VSYNC AFTER actual-interval=1` (on) or `2147483648` (immediate/off), plus
the explicit matching-selection message. A policy-selected line alone is not
confirmation. Apply Failed remains intentional for this no-save experiment.

### Live combined-resize acceptance and same-size candidate (2026-09-08)

The user tested VSync on to off while resizing windowed Batman from 1280x720 to
2560x1440. Logs show actual interval 1 before, intercepted Reset interval
2147483648, and actual interval 2147483648 afterward. Viewport and backbuffer
both became 2560x1440. This confirms the D3D9 override inside Batman.

The subsequent approved change adds VSync-only Apply using the live dimensions
and mode. No engine VSync scalar write, temporary resolution, polling, extra
suspension scope, or Reset-loop rewrite is included. Build explicitly:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File games/HelenBatmanAA/experiments/windowed-resolution/Build-NoSaveProbe.ps1 -SameSizeRefresh -OutputRoot C:\dev\helenhook\output\<fresh-directory>
```

Normal builds and switchless no-save builds do not install the bridge. The opt-in
requires a matching startup-enabled **runtime and proxy** for a later authorized
installation. It checks the pinned executable, preferred base and loaded splice
bytes inside the pinned proxy startup window. Shared WriteMemory commits an
eight-byte JMP/NOP over `EA65A6: TEST EAX,EAX; JE EA7388`. Destinations are explicit:
skip EA7388 or rebuild EA65AE. There is no copied relative-Jcc trampoline and no
destructor restoring executable bytes at shutdown.

The native wrapper publishes its exact return-slot address before calling
EB91D0 with live width/height/mode and x/y=-1. The thread-local activation checks
original EBX renderer identity, device identity, bounded stack and all five return
sites. Only that invocation consumes the request, once. Normal return clears it
before the return slot disappears; RAII clears it on C++ unwind. Other calls retain
their original decision and legacy x87/SSE state. AVX state is not supported.

Readback requires the selected interval and unchanged viewport/backbuffer size
and mode. The experiment still permits **one engine attempt per game process**,
performs no Helen INI save, and returns intentional Apply Failed. Stock engine
Reset failure/loop behavior remains unchanged. Fixtures do not establish retail
engine completion on every machine or compatibility with other engine-code mods.

Final fresh candidate: `output/same-size-refresh-20260908-b/native`.
The earlier `same-size-refresh-20260908-a` is a development build, not the handoff.

| Artifact | SHA-256 |
| --- | --- |
| HelenGameHook.dll | F2EAF194CDD99E80879339183683C01FD5C76331600D6256943D6CCACA893CF2 |
| dinput8.dll | 9A96A2DDB14834FF9C8AF625ABAABBEEE185A6685B09E7E57AEC36D3FF553ABC |
| HelenRuntime.lib | 55A065C7FE94E4D4D97C13B663CFB88D4C9A5D74F2C243E189F92D4B65671BE2 |

Verification (fixture directories are under repository output unless specified):

- Wrapper red: `vsync-activation-48ee25d7a6f34a38b9d4e286a4bc0e1f`;
  connected optimized native-chain pass: `vsync-activation-c1b460dcdd3b4e0ab4b292214465686f`.
  Tests unchanged arguments, balanced stack, refusal, recursive identical-site
  rejection, and real C++ unwind through the wrapper/controller.
- Controller red: `vsync-activation-d0272aea84fa40c98dd105318c723ee9`;
  pass: `vsync-activation-62e60d2d81684aef8e560c4c01d16161`.
- Register-forwarding red: `vsync-activation-4ee61e0e437d4cb982ebf33be976fa59`;
  optimized bridge/state pass: `vsync-activation-175cbfb8c73342979d0a6cc8df927d54`.
- VSync-only selection red: `vsync-request-0288512a4a74409caffc4752e9b5ace9`;
  pass: `vsync-request-cabecd73c9354155a8c17965ec21fe5f`.
- Request contention pass: `vsync-activation-a2c51813e4f44582aa10e722d888d7f2`.
- Actual candidate DLL/proxy host pass: `runtime-startup-d1b0a97134f0451994ab414eabd9b739`.
- Candidate real Commit pass: system temp `NoSaveSession-080447c002ab423c844abdd4956f83cb`;
  foreign executable refused before policy publication, transactions consumed
  once, and both fixture INIs unchanged.
- Hidden-window D3D9 same-size Reset pass: `d3d9-reset-fixture-2cdbf8eae19448c3bd5b5fd4aa85b3f9`;
  actual interval changed 1 to 2147483648 while backbuffer stayed 800x600.
- Candidate presentation/subtitle/gameplay-texture/reset-recovery pass:
  `d3d9-reset-fixture-2ecf0a0340ed4576b8153d148fc379ff`.
- CPU policy pass: `d3d9-policy-f77bfd11f375404bbc4f2edf6ca76399`.
- Fresh normal control: `output/same-size-normal-20260908-a/native`;
  runtime/proxy host pass: `runtime-startup-c57413cd8ee0417aaeff6fdca6438699`.
  Full native suite passed at
  `tests-d49791bb13d346e48cd5075cffc9c36d/HelenRuntimeTests.exe` under that control.

This candidate has **not been installed or live-tested in Batman**. The installed
runtime was rehashed and remains
0D831738862EBD84D8FCAE3E3717B246177FC04792288ABE458763E47B37D7CE.
No installed game, pack, configuration or INI was modified. Live acceptance must
show startup-hook installation, one consumed decision, matching actual interval
and unchanged dimensions after a VSync-only Apply.

### Authorized same-size installation (2026-09-09)

After the user authorized installation, Batman and its launcher were confirmed
closed. Both matching DLLs from `same-size-refresh-20260908-b/native` were
installed and their hashes verified against the table above. The prior working
pair is backed up at `output/same-size-install-20260909-a`. All 40 pack/config
files retained their hashes; before/after snapshots of 31 INIs reported zero
differences. No game was launched. The next live test is one VSync-only toggle
and Apply, without touching resolution or fullscreen. Apply Failed remains
intentional; actual interval and unchanged dimensions must be confirmed in logs.

### Same-size live acceptance (2026-09-09)

The user completed the VSync-only test and confirmed success. The installed
Batman log records startup hook installation, actual interval 1 before Apply,
and an engine call with unchanged 1280x720 windowed dimensions. The intercepted
Reset uses interval 2147483648. The decision was consumed exactly once and the
stock resize returned. Actual interval readback is 2147483648 afterward; client,
viewport and backbuffer all remain 1280x720 windowed. This validates the same-size
refresh in Batman, beyond the earlier synthetic and hidden-window GPU fixtures.
The no-save/one-attempt experimental limits remain in place.
