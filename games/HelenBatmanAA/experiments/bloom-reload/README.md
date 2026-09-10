# Bloom session-INI reload probe

Throwaway feasibility experiment, not production live Apply. Normal builds do
not compile it. Build with the sibling `Build-NoSaveProbe.ps1` switches
`-SameSizeRefresh -BloomReload` and a fresh `-OutputRoot` under repository output.
The previously validated resolution/fullscreen/VSync paths remain available.

## Question and evidence

**Correction discovered during multi-option work:** earlier Bloom PASS logs
proved a Bloom cache change but observed/applied AmbientOcclusion's live field.
Those logs are not valid live Bloom acceptance. The field-binding regression
test now rejects the old offset, and the source uses Bloom data+0x34. No separate
Bloom renderer mirror is claimed. Corrected candidate
`output/bloom-corrected-20260909-a/native` was subsequently installed, with both
DLL hashes checked and 31 original INIs unchanged. Backup and snapshots are in
`output/bloom-corrected-install-20260909-a`. Live Bloom acceptance is pending. The
DynamicShadows binding and its visual acceptance are unaffected.

Can an edited session INI drive a live graphics setting through Batman's own
configuration reload/apply path, without persisting originals?

Static inspection of the pinned executable identifies the relevant chain:

- Settings loader C20130 reads Boolean keys through 6244D0, then 623570, then
  cache lookup 622120. At 6221BE a cache hit branches to 622264, skipping file
  loading. A settings reload alone therefore does not guarantee fresh disk data.
- FConfigFile read 6204A0 clears its sections, calls file-to-string 638320,
  and parses through 61F9F0. The probe explicitly calls this on the cached file.
- Loader key 203D93C is `Bloom`, mapped to settings-data+34 (owner+38).
  Key 203D8B8 is `DirectionalLightmaps`, data+24 (owner+28).
- Apply C40090 receives the complete AB-dword value payload plus save=false.
  It invokes C3FDE0 and publishes a render summary through C24B50.
- That summary includes owner+34, which is AmbientOcclusion, not Bloom. Its
  renderer mirror is 26C0DEC. Bloom is directly consumed at 26C0B70 (for example,
  6508C0 and 835470). The corrected probe checks the entire live payload and
  preserves AmbientOcclusion; it does not label a duplicate read a renderer mirror.

These are static binding observations, not proof of live engine success.

## Runtime experiment

Only a Bloom-only menu edit is accepted. The existing no-save probe verifies
the pinned executable/preferred base, owning game thread, and idle viewport before
dispatch. It shares the existing one-engine-attempt-per-process limit.

The probe resolves the session overlay from the initialized routing service,
requires redirected reads/writes, and checks the engine's INI filename matches
the protected original. It never discovers a scratch copy by directory scanning.
It stages Bloom in that session file, observes the existing cached value, explicitly
rereads the file through the engine, and requires the new cached value before Apply.
It refuses a DirectionalLightmaps mismatch before the broad apply call.

Incoming settings copy the live AB-dword payload with only Bloom changed. The
stock apply call uses save=false. After flushing render commands, the probe
requires the full value payload to match that incoming copy, any genuinely
available renderer mirror to match selection, and the original INI hash to remain unchanged.
Failures are logged, not repaired by writing engine fields back. The existing
UI intentionally reports Apply Failed because no saved baseline is published.
The cache/session edit may remain after a failed attempt; restart before retrying.

## Verification and installed candidate (2026-09-09)

- Draft selector red: `output/bloom-draft-0ad036af926f4563a173e3cd6e433aad`.
  Green: `output/bloom-draft-236f407a77b84aa099ce8ebc26aa9dc4`.
- Real overlay edit red: `output/bloom-draft-7623e1b286f3460f947ee7b3fe1569ef`.
  Final green: `output/bloom-draft-66337d6878b74406a1f10afa652f1de6`.
  Covers ANSI and UTF-16 files, original preservation, unrelated keys,
  on/off staging, original hard-link alias refusal and missing-key refusal.
- Actual runtime/proxy host: `output/runtime-startup-8f63dafc328742d881f147aca4f35ccb` passed.
- Real no-save Commit: system temp `NoSaveSession-d091194ef7694df78002812ebe37b43b`
  passed; foreign executable refusal and original fixture INIs unchanged.
- Fresh build: `output/bloom-reload-20260909-a/native`. Four existing proxy export
  warnings remain. No automated fixture executes the retail reload/apply calls.

Installed matching pair, with hashes verified:

| Artifact | SHA-256 |
| --- | --- |
| HelenGameHook.dll | 51BD212FD6CC110CDDEE89BDF38465ABC4EADBA8FEFFF8F8054F067198C13E8C |
| dinput8.dll | 5EB3E75B24B6A11B2EB796A5D9429F0C03A798CB062F2C52740C6D28C7DD0D29 |
| HelenRuntime.lib (build only) | A63722A940261E8942D0421513CCA0F4A372E7DA8BF84576A04E293F27D71C08 |

Batman/launcher were closed. Prior live-validated VSync pair is backed up at
`output/bloom-install-20260909-a`. Forty pack/config files retained hashes, and
31 original INIs had zero before/after installation differences. No game was
launched automatically. Live acceptance remains pending.

## Live test

### Dynamic Shadows extension

The user approved a one-setting Dynamic Shadows experiment after Bloom's live
test passed: cache `1 -> 0`, live `1 -> 0`, render `1 -> 0`, unrelated value
payload preserved, and all 31 original INIs unchanged
(`output/bloom-live-20260909-b`). Visual Bloom confirmation was inconclusive.

The same opt-in probe now accepts either a Bloom-only or DynamicShadows-only
edit, never both or any mixed settings. Existing class/build flag names are
retained to avoid unrelated integration churn; logs for setting observations
are now `[ini-reload]` with an explicit key. This remains a one-attempt-per-process
experiment, and successful live application intentionally does not mark the
draft saved. The original persistence path remains bypassed.

Pinned DynamicShadows evidence: key string `0x203D81C` is paired by `0xC20130`
with data offset `0x24C` (owner offset `0x250`, absolute `0x26C0D88`).
`0xC24B7E` loads owner+0x250 into the third renderer-transfer field;
`0xC20B9A`/`0xC20B9D` copy that field into renderer mirror `0x26C0DF0`.
The probe reads this mirror only after the synchronous render-command flush.
It does not reuse Bloom's mirror or directly patch either engine field.

New draft tests failed against Bloom-only behavior at
`output/bloom-draft-c558d32bea73450380a9ee88c3fc3180`, then passed at
`output/bloom-draft-c8d44c2b270343d09921d718313dc224`. Real shadow overlay edits
failed against the Bloom-key writer at
`output/bloom-draft-285c130dcbfb40cf9d513407f4fcda6c`, then passed at
`output/bloom-draft-6bf7ca7846894a59bfbc8e59d961a7a2`. Existing path-binding tests
also passed at `output/bloom-draft-22dbc0dffcc94445907520140abe8fcb`.

Visual test: after a fresh launch, find a scene where Batman visibly casts a
shadow. Change only Dynamic Shadows to Off and Apply once, then inspect the
same scene. Static/baked scenery shadows are not the acceptance target. Check
`[ini-reload]` BEFORE, FILE-STAGED, CACHE-RELOADED, AFTER and PASS/STOP alongside
the visual result. The user subsequently enabled shadows and confirmed that
they appeared. Logs show DynamicShadows cache/live/render `0 -> 1`, a PASS,
and preservation of the unrelated value payload and original BmEngine.ini.
Snapshot `output/shadows-live-20260909-a` confirms 30 of 31 original INIs were
byte-identical; the separate BmGame.ini changed by two bytes during the session.
A line-set comparison reported no added/removed lines, but exact text differs;
the cause was not established. Do not claim every INI stayed byte-identical in
this gameplay test. Shadow disabling remains a separate untested live direction.

Fresh candidate `output/shadows-reload-20260909-a/native` built successfully
(four existing proxy export warnings). Runtime/proxy startup passed at
`output/runtime-startup-a65a989bf7c243318dfcc0e2d6bd2206`; the real no-save Commit
fixture passed at system temp `NoSaveSession-76f6604a4db64cfd9175c7fd063fb927`.
Installed with Batman/launcher closed and both hashes verified:

- HelenGameHook.dll: `1B0CE8404317B2C1DBA1E5C314D4FA394A539D7581C82C336A577437C28E9A04`
- dinput8.dll: `D94E10FA3B1A30FE0B5EB5AA84B0776D32DCD9A6BBD84157FDD80C838ED475C2`

Backup of the previously installed Bloom pair and before/after snapshots:
`output/shadows-install-20260909-a`. All 31 original INIs were unchanged, and
118 non-cache/non-log helengamehook files retained their hashes. No game was
launched automatically and nothing was committed.

### First attempt and path-guard correction

The first attempt stopped before staging: `Engine reads a different INI than the
protected user file`. Read-only inspection of PID 3604 found the logical cache
key `..\BmGame\Config\BmEngine.ini`, not a physical Windows filename. The pinned
file manager at `0x026667C0` uses CreateFileReader `0x5504F0`, which calls virtual
slots `+0x50` (`0x5503F0`, expansion) and `+0x54` (`0x554130`, user translation)
before opening the file. Its live FString at `manager+8` is the Documents game
root; `manager+0x14` is the Steam install root. The latter is replaced by the
former at `0x5543A7`. These map the captured logical key to the already protected
Documents INI. Comparing the logical key directly with the original file was
the probe's error; no Bloom reload or live Apply occurred in that attempt.

The correction supports only that captured logical key, validates the file
manager vtable/methods and initialized user-root FString, then checks the mapped
file identity against the routing original. Cache lookup/read still uses the
logical key. It logs all relevant paths before staging; no route is disabled.

Regression test `Test-BloomDraft.ps1 -Binding` failed against the old comparison
(`output/bloom-draft-efa6eb3472af4c54ae70b4f97170e144`) and passed with the correction
(`output/bloom-draft-40fced2ca4e84799a40e3f6aa8d556e5`). Wrong roots, missing roots,
relative roots, other filenames and unsupported absolute cache keys are refused.
Overlay and draft tests also passed freshly. Candidate
`output/bloom-reload-20260909-b/native` built successfully with the same four
existing proxy export warnings. Its real runtime/proxy startup fixture passed
at `output/runtime-startup-78605d86f8cf40c688811d082d225a0c`; its real no-save Commit
fixture passed at system temp `NoSaveSession-58d93436293f4130a96de099a62ba2a2`.
Candidate b was subsequently installed after the user closed Batman. Both
installed hashes matched the fresh build: HelenGameHook.dll
`6B36C29C9CFDAACDB7F65800BCE57F49F321CFA215522837050E76210D9F1B65` and
dinput8.dll `95369AB152A0AD8EA3C99E8907A765040500BDBEA15F92CDCAECE72B8DD1CF6B`.
The previous pair and before/after INI snapshots are preserved at
`output/bloom-install-20260909-b`. All 31 original INIs were unchanged; 118
non-cache/non-log files under helengamehook retained their hashes. Neither the
game nor launcher was running during installation. Live acceptance of the
corrected path binding, cache reload and rendering remains pending.

Launch Batman, change only Bloom and press Apply once. Leave VSync, resolution,
fullscreen and other options unchanged. Inspect `[bloom-reload]` lines in
`Binaries/helengamehook/logs/HelenGameHook.log` for file staging, cache reload,
live/render readback and the final PASS or explicit STOP reason. Visual Bloom
behavior and original INIs after the test must still be checked. Do not infer
support for all other settings from this one-setting experiment.
