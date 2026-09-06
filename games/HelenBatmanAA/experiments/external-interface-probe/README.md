# Batman direct ExternalInterface proof — experimental opt-in build

This directory is an isolated feasibility prototype, not a shipping bridge. Only a build explicitly importing `ProbeBuild.targets` includes it in HelenGameHook. Ordinary builds exclude it. Offline tests must not be reported as a successful in-game call.

## Verified static evidence

Inspected `ShippingPC-BmGame.exe` SHA256:
`4DAC1F5E2AC6710B7378FDCE74601F616F4753E3756CB5FDA63C7519CC2EB028`.

Preferred image base is `0x00400000`. The following addresses are preferred VAs, **not reusable live pointers**. A future installer must validate the supported executable and instruction bytes, then use loaded module base plus RVA.

- `0x019FD890`: ExternalInterface call implementation, located through the reference at `0x019FDA13` to its missing-handler warning.
- `0x019FD997`–`0x019FD9A8`: obtains movie-owned GAS result at movie + `0x9DC`, releases its previous contents through `0x01976050`, and sets its type to undefined.
- `0x019FD9CA`: obtains the handler at movie + `0xD4`.
- `0x019FD9D0`: five-byte vtable lookup `8B 11 8B 52 04` obtains virtual slot 1. This is the candidate interception site, RVA `0x015FD9D0`.
- `0x019FD9D5`–`0x019FD9E1`: pushes argument count, converted argument array, method-name pointer, and movie pointer, then invokes the handler with its object in ECX.
- `0x019FD9E3`: copies the movie-owned result back to the ActionScript call result via `0x01976180`.
- In that copy routine, type 2 selects the boolean branch at `0x019761AF`, copying payload byte +4. The prototype handles only this primitive; it owns no strings or GFx objects.

`Inspect-Dispatch.ps1` checks the relevant byte sequences read-only. It does not authorize arbitrary builds or install a hook.

## What the console proof establishes

`Test-Probe.ps1` compiles x86 with warnings treated as errors, then executes:

- Actual read-only INI reads for True and False.
- Missing file/key and invalid values stay undefined, not fabricated Off.
- Exact callback-name matching; unknown calls leave the result untouched.
- Invalid argument counts are not treated as valid reads or forwarded to stock.
- The adapter is invoked with a thiscall function pointer, exercising ECX plus four stack arguments and callee stack cleanup.
- 100 direct calls and 100 stock calls alternate between two movie buffers. Stock this/movie/argument identity and results are checked, and the other movie must remain untouched.

The stock virtual handler is a test double because the engine is not running. The boolean reader is intentionally limited to this experiment; it is not a replacement for the shipping graphics parser/transaction service. `ProbeRuntime.cpp` binds the reader synchronously before hook publication and catches/logs exceptions from owned probe calls. Stock calls retain their existing exception behavior.

## Prepared and installed live probe (2026-09-05)

The opt-in DLL and staged pack were installed with `Deploy-Probe.ps1`. Native package parsing, delta reconstruction, console x86 tests, and execution of the decompiled frontend methods passed. Deployment verified exact DLL/pack hashes, unchanged subtitles, unchanged proxy, and unchanged enabled-pack configuration. The first deployment attempt stopped before writing anything because an imported helper's default game path overrode the caller; the helper now receives the explicit Steam path.

Current staged package: `output/batman-direct-probe/package-c118fdbb40df47789afff3f3a1e5d0b0` beneath the repository. Durable pre-probe rollback copy: `output/batman-direct-probe/rollback-d84e9ca32d514bddb5cef01df7daa8b3`. Existing logs were retained and copied into that backup.

The frontend calls `Helen_ProbeGetFullscreen` once per initialization and accepts only a boolean. Fullscreen remains read-only. Its old observer is removed from the probe pack, and the frontend starts the remaining carrier reads at VSync regardless of direct-call success. This prevents the old route from disguising a broken direct call. Other settings still use the previous protocol during this isolated test.

The runtime log records `[direct-probe] synchronous Fullscreen`, movie identity, returned GAS type, and boolean payload. **In-game success is still awaiting user verification.**

## Remaining live checkpoint

Prepare a separately enabled, executable-pinned probe that installs the dispatch adapter once, only after its reader is initialized. It must preserve the stock handler and hook lifetime, contain native exceptions, and refuse mismatched bytes. Patch an isolated frontend test to call `Helen_ProbeGetFullscreen` once and report its returned type and value explicitly. Do not enable the current Fullscreen polling request for that test, or count its reply as proof of the direct path.

Acceptance requires an actual synchronous ActionScript boolean result, intact stock menu behavior, and success after menu destruction/recreation and relaunch. No deadline, carrier scan, worker polling, or automatic fallback to the old route is part of this prototype.
