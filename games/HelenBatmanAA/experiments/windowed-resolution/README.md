# Windowed resolution: throwaway no-Helen-save probe

This experiment is not the production live Apply implementation. Helena approved calling the engine from the existing menu while omitting our writer, then comparing INIs to determine whether Batman saves the change itself. Engine persistence is deliberately left enabled.

Normal projects do not import `NoSaveProbe.targets`. `Prepare-NoSaveProbe.ps1` reads the current session source and generates a copy with exactly one expression replaced: `Config.ApplyDraft(attempted.Draft)` becomes `NoSaveResolutionProbe::Apply(attempted.Draft)`. Staging, catalog validation, transaction consumption, direct reads, and normal source files remain unchanged. The explicitly imported targets compile that generated copy in place of the original.

The experiment returns NotApplied even after an engine call returns: the stock frontend displays **Apply Failed** and does not claim that its saved baseline changed. This is probe-only presentation, not the production result contract. Use logs and INI comparisons to determine what actually happened. Do not test other graphics settings or subtitles during the save-attribution experiment. The probe allows at most one engine invocation per process.

## Newly established static boundary

Full entry is preferred VA `0xEB91D0`, x86 thiscall, five stack arguments: width, height, fullscreen, x, y; `ret 0x14` at `0xEB9632`. The width/height inputs feed the AdjustWindowRect rectangle and 0xEB7250; the fullscreen input selects the fullscreen branch. Existing-window code obtains current position through GetWindowRect, so x/y=-1 are unused on that path. A stock call at 0xEBBB09 supplies all five arguments. This is a Windows engine entry, not a raw D3D Reset.

Windows viewport construction at `0xEBB9A0` installs primary vtable `0x212C408` and secondary vtable `0x212C380` at owner+4. The primary slot at `0x212C40C` contains the resize entry. The engine uses registered array `0x26CCAB0` and count `0x26CCAB4` when routing viewport operations, including its message handler at `0xEBD130`. The probe reacquires this registered owner for the synchronous call; no pointer is cached between calls.

Entry state owner+0x80 rejects recursive resizing; owner+0x60 is HWND; owner+0x64 is parent HWND; secondary viewport's dimensions are owner+0x4C/+0x50 and mode bit is owner+0x58. Runtime checks require exactly one registered viewport, exact vtables, an initialized matching engine thread ID, matching HWND thread/process, top-level windowed state, and no active resize. Renderer vtable and its drawing-viewport field at +0x24 are checked before invoking. The engine path performs its own rendering synchronization. These checks constrain the experiment; successful in-game behavior is still unverified until tested.

For this throwaway proof, rebased executable loading is explicitly rejected because the pinned thread-code bytes contain absolute operands. No dynamic process address is persisted. Production relocation-compatible signatures remain separate work.

## Build and verification

Use a new output root for each source revision. Current build root: `output/batman-no-save-20260905-a`.

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File games/HelenBatmanAA/experiments/windowed-resolution/Prepare-NoSaveProbe.ps1 -OutputRoot C:/dev/helenhook/output/batman-no-save-20260905-a
rtk proxy powershell.exe -NoProfile -Command "& 'C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe' 'HelenGameHook/HelenGameHook.vcxproj' /t:Build /p:Configuration=Release /p:Platform=Win32 /p:PlatformToolset=v143 /p:ForceImportBeforeCppTargets=C:\dev\helenhook\games\HelenBatmanAA\experiments\windowed-resolution\NoSaveProbe.targets /p:NoSaveOutput=C:\dev\helenhook\output\batman-no-save-20260905-a /m:1 /nodeReuse:false /v:minimal; exit `$LASTEXITCODE"
powershell.exe -NoProfile -ExecutionPolicy Bypass -File games/HelenBatmanAA/experiments/windowed-resolution/Test-NoSaveSession.ps1 -RuntimeLibraryPath C:/dev/helenhook/output/batman-no-save-20260905-a/native/HelenRuntime.lib
```

The test runs the actual linked session Commit against newly created temporary INIs. It requires probe rejection in the foreign test executable, no saved-success outcome, unchanged bytes in both INIs, a probe log marker, and consumed transaction rejection. It does not test engine invocation. Build logs confirm the generated session and probe source were compiled, and DLL linker logs point to the isolated runtime library.

The native build succeeded. `NO_SAVE_SESSION_PASS` and all 10 inspector tests passed. A direct MSBuild invocation first failed before compilation due to duplicate PATH/Path environment entries; the established rtk wrapper completed the build. Existing Hook.cpp C4244 warnings remain unrelated to this experiment.

Build the frontend/package using `Build-BatmanDirectGraphicsCandidate.ps1` with this DLL and library as explicit inputs, not any old installed assets. Candidate verification of package structure and frontend does not prove the engine resize or its persistence behavior.

## User test sequence

1. Keep Batman closed for deployment. Preserve working DLL/pack rollback copies and a pre-install snapshot of both relevant config directories.
2. Launch and open graphics options. **Do not Apply yet.** Capture a new `Snapshot-ProbeIni.ps1` baseline now to separate startup writes from resize writes.
3. Change only the windowed resolution and press Apply once. Expect the intentional Apply Failed label. Report whether the window actually resized and whether the menu/input still work. Keep the game open.
4. Check `[resolution-no-save]` logs: require a CALL and RETURNED marker before attributing an INI result to resize. A STOP before CALL is a refused invocation, not evidence about engine saving. BEFORE/AFTER lines report client, engine viewport, and actual D3D backbuffer dimensions.
5. Capture another INI snapshot and compare with the menu-open baseline. Inspect changed keys, not only hashes. Then close Batman and capture again to identify deferred writes.
6. No automatic rollback is performed: this experiment must preserve evidence of engine-owned writes. Restore working code/config only as a separate explicit step after recording the result.

`Snapshot-ProbeIni.ps1` preserves original file copies and hashes without overwriting earlier snapshots. Initial baseline contains 31 INIs across the user and installation config directories.

## Installed experiment checkpoint

- Candidate: `output/batman-direct-graphics/candidate-05e37ad22c904501b2b66162f69d6a6e`.
- Installed DLL SHA256: `8E918BE2B2A86F85271B0FFFE9976100A3E3D0B55FBFE2B8102939FF611E21FF`.
- Working DLL/pack rollback copy: `output/batman-direct-graphics/rollback-b57e6e5ce4574f42a1111c1c158b10b1`.
- Generated and decompiled frontend tests, native package test, exact delta reconstruction, static dispatch contracts, and deployment hash checks passed. Subtitle/proxy/enabled-pack contents were verified unchanged.
- `ini-after-install-verified` versus `ini-before-install`: all 31 INIs unchanged. The first comparison incorrectly wrapped PowerShell 5's parsed JSON array as one element; fixed the parsing and added a regression test. `SNAPSHOT_COMPARISON_PASS` verifies identical snapshots produce zero differences and a test-only modified hash produces exactly one. Neither test changes source INIs.
- September 6 live test completed: the engine call returned after changing client and engine viewport from 1920x1080 to 1600x900. Helena confirmed entering gameplay and correct visual scaling; functional windowed resizing is user-validated.
- The probe's immediate D3D backbuffer observation remained 1920x1080. This measurement does not invalidate the observed functional resize, nor establish which surface controls the effective rendering area. Its interpretation remains separate from the confirmed visual result.
- With Helen's writer bypassed, the menu-open versus after-Apply snapshots show exactly two changed values in user `BmEngine.ini`: SystemSettings ResX 1920 -> 1600 and ResY 1080 -> 900. `UserEngine.ini` stayed 1920x1080. This directly demonstrates engine-owned persistence during the resize path.
- After exit, resolution stayed 1600x900 in `BmEngine.ini`; the only further diff removed the WinDrv.Accessibility section. No other INI changed relative to the after-Apply snapshot.
- Evidence snapshots: `output/batman-no-save-20260905-a/ini-menu-open-20260906`, `ini-after-apply-20260906`, and `ini-after-exit-20260906`. The no-save test DLL remains installed. Reusable interception of game-owned config writes is proposed future work, not implemented here.

This repository checkpoint also preserves earlier polling/probe experiments. The old standard graphics pack/build scripts are not the installed direct no-save candidate. Reproduce this experiment with the explicit opt-in build and fresh candidate pipeline above; do not treat every retained experiment as release-ready.
