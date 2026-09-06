# Batman windowed resolution: static discovery checkpoint

## Status

September 5, 2026; starting commit `17e9682` on main. Read-only executable analysis plus a new inspector/test suite. No runtime/frontend changes, injected calls, game launches, or installation changes. Existing dirty worktree files were preserved.

**Gate: insufficient evidence to invoke.** The renderer-to-viewport chain is identified, but a safe, nonpersisting high-level invocation boundary and active window-owner lifetime are not established. This is not a completed live-resize implementation or a failed in-game test.

Executable: `C:\Program Files (x86)\Steam\steamapps\common\Batman Arkham Asylum GOTY\Binaries\ShippingPC-BmGame.exe`, 38,758,728 bytes, SHA256 `4DAC1F5E2AC6710B7378FDCE74601F616F4753E3756CB5FDA63C7519CC2EB028` verified locally. Preferred base 0x00400000. All addresses below are preferred VAs, not reusable runtime pointers. Listed code RVAs also equal file offsets for this image; the inspector nevertheless maps sections explicitly.

## Verified instructions and interpretations

| Entry | RVA / file offset | Observed behavior | Interpretation / limits |
|---|---|---|---|
| 0x715880 | 0x315880 | Tests global 0x26760E8; calls imported GetCurrentThreadId at IAT 0x1E292B4; compares global 0x26760E4 | Game-thread predicate, corroborated by caller assertion string. If initialization flag is zero it returns true, so it alone is insufficient as an initialization guard. |
| 0xEAA0A0 | 0xAAA0A0 | Converts first stack argument by subtracting 8; writes later three arguments to owner offsets 0x10, 0x14, 0x18; calls 0xEA6310 using owner+8 as ECX; ret 0x10 | Renderer viewport update, four stack arguments. Width/height/mode meanings corroborated by callers. It is not a coordinated Windows viewport operation. |
| 0xEA6310 | 0xAA6310 | Checks game thread, creates scope via 0x732210(1), walks viewport list at owner+0x18/count+0x1C, compares dimensions and mode | Device-wide update/resource handling lead; complete failure/device-loss behavior not established. |
| 0x403540 | 0x3540 | Loads global 0x26B0D94, forwards four arguments to virtual slot 0xD8 | Thin renderer interface wrapper; no window sizing. |
| 0xCBB980 | 0x8BB980 | Creates scope via 0x732210(1), updates viewport fields, invokes renderer resize/create/release, then scope destructor 0x724410; ret 0x10 | Higher-level viewport render-resource update with ECX owner and four stack arguments. Not an independently verified public API. |
| 0xEB7250 | 0xAB7250 | Walks window list 0x26CCAB0/count 0x26CCAB4, calls GetClientRect, updates device dimensions, calls 0xCBB980 with this+4, then 0xC42DA0 on normal nonzero-size path | Windows viewport resource update. Also reaches settings serialization: not safe to assume nonpersisting. |
| 0xEB4080 | 0xAB4080 | Reads HWND at this+0x60, uses GetWindowLongW/SetWindowLongW and SetWindowPos; ret 0x18 | Separate window style/geometry helper. Alone it does not resize rendered resources. |
| 0xC42DA0 | 0x842DA0 | Checks this+0x2D4, compares/stores three values at 0x240/0x244/0x248, then calls 0xC3F6B0 with this+4 | Settings-update branch; do not toggle the guard field as a speculative suppression hack. |
| 0xC3F6B0 | 0x83F6B0 | Iterates key/value tables through 0x624AC0, 0x624910, 0x6249A0; invokes 0xC375A0 then 0x621E20 with config singleton 0x26667B0 and filename object 0x266F080 | Config serialization and likely flush. Actual disk write effects require further tracing/live byte comparison; no claim that a file was written during this read-only investigation. |

Pinned evidence windows:

```text
715880: 833DE8606702007411FF15B492E2013B05E4606702740333C0C3B801000000C3
EAA0D2: 8B4C24108B44240C8B542414894E148B4E08894610895618E821C2FFFF5EC21000
CBBA6F: 8B11575553508B82D8000000FFD0
EB7351: 8B4C24105653576A0083C104E81E46E0FF565357B9380B6C02E831BAD8FF
C42DCE: 898140020000A1E8F15D0289B1440200008991480200005083C104E8C2C8FFFF
C3FD86: 8B0DB06766026880F066026A00E888209EFF
EB4080: 538B1D1499E20156578BF98B47606AF0
```

These windows record observations, not a sufficient runtime signature set.

## Call chain and ownership evidence

Vtable begins at 0x2128210. Entry at 0x21282E8 is slot 0xD8 and contains 0xEAA0A0. Constructor-like 0xEAED40 installs the table at instruction 0xEAED6F; teardown-like 0xEAE260 also installs it at 0xEAE289. Factory call at 0xEAFFB2 reaches 0xEAED40 after allocation of 0x12F4 bytes. These identify renderer object construction, not the active Windows viewport's lifetime.

At 0xCBBA71 through 0xCBBA7B, 0xCBB980 passes mode, height, width, renderer viewport reference to slot 0xD8 of global renderer 0x26B0D94. Existing reference is viewport+0x44; dimensions are +0x48/+0x4C and mode is low bit of +0x54. Entry also supports destruction and creation branches; do not conflate it with a pure resize call.

Direct calls to 0xCBB980 occur at 0xEB405B (destruction-like path: first argument 1 and zero dimensions), 0xEB735D, and 0xEB73AB. The latter two are in 0xEB7250. Its normal branch calls the settings updater at 0xEB736A immediately after the resource operation.

Higher Windows code calls 0xEB7250 at 0xEB9599 and 0xEB95F6, and calls window-geometry helper 0xEB4080 at 0xEB95BE/0xEB95E4. The full owning function and every branch/argument have not been established; these interior addresses must not be used as entry points. Nearby code also invokes CreateWindowExW, showing creation/transition responsibilities beyond a simple resize.

0x26B21D8, considered during exploration, is set to 0 or 1 by renderer init/teardown code. It is not the renderer object pointer; do not use it for owner acquisition.

## Synchronization and persistence caveats

0x732210 saves globals 0x26B2198/0x26B2194, and its argument-1 branch calls 0x72F090 before a synchronization import. This resembles render-thread suspension, but its full lifecycle and reentrant safety are not yet proven. Both resource-update layers use it. Existing ExternalInterface forwarding does not establish that resizing is safe from its callback thread.

The settings path is materially broader than three field writes: 0xC3F6B0 iterates 42, 14, and 17 entries in separate serialization loops. Its final 0x621E20 call reaches 0x618390 through 0x621F51. Further tracing is needed to confirm the full file publication path. It must be excluded or explicitly accounted for before claiming that live resize leaves files untouched.

The existing runtime's ResetDetour observes the original device Reset and invalidates texture fingerprints; it does not provide an engine resize API. WindowBehaviorHookSet's window positioning likewise does not establish rendered-size updates. Neither should be repurposed as a fallback.

## Inspector verification

Tests were written first. The plan's unittest discovery command produced **zero tests** because of the hyphenated filename. Running the test file directly then failed because the inspector did not exist, as intended. After implementation, direct execution ran **10 tests, all passing**: identity length/digest rejection, VA-to-file translation, negative/empty/cross-section/zero-fill/truncated-file rejection, complete return decoding, and rejection of a partial final instruction.

```powershell
python -B games/HelenBatmanAA/experiments/windowed-resolution/Test-InspectViewport.py -v
python -B games/HelenBatmanAA/experiments/windowed-resolution/Inspect-Viewport.py --executable 'C:\Program Files (x86)\Steam\steamapps\common\Batman Arkham Asylum GOTY\Binaries\ShippingPC-BmGame.exe' --address 0xEAA0A0 --length 0x53
```

The real-image inspection decoded through `C2 10 00` at 0xEAA0F0. Plan length 0x51 was corrected to 0x53. Unaligned exploratory fragments were treated only as leads, not ABI evidence. One exploratory whole-file pointer scan reached unmapped file data and failed VA conversion; the subsequent direct-call search was bounded to the code section.

## Remaining prerequisites

1. Establish the full Windows resize entry, parameter meanings, and active owner construction/destruction. Distinguish creation/fullscreen-transition branches from same-mode windowed resize.
2. Find a supported nonpersisting engine boundary that retains window/resource/input coordination. Do not temporarily alter settings guards or bypass an untraced serialization call.
3. Establish a safe synchronous execution point relative to rendering and the direct menu callback. If engine completion is deferred, return to the approved design's event-extension checkpoint.
4. Verify actual mode/client/rendered dimensions from live engine ownership. Static viewport fields are not sufficient proof of actual backbuffer size, especially with device-wide maximum dimensions across windows.
5. Resolve config filename object 0x266F080 and compare all affected INI bytes in the eventual controlled proof, including the existing writer's BmEngine.ini and launcher UserEngine.ini targets. No live filename resolution or byte comparison was performed here.

Continue read-only discovery from these specific leads; do not deploy or label the controlled-resize gate passed.
