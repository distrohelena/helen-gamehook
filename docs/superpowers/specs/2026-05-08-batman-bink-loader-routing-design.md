# Batman Bink Loader Routing Design

## Goal

Add a small loader-routing layer to Helen GameHook so Batman Arkham Asylum can report when it attempts to load Bink DLLs, while keeping the Bink-specific implementation outside `HelenGameHook.dll`.

The first slice is observability only:

- detect Bink DLL load requests from Batman
- log the request clearly
- preserve the original load behavior

The design must also leave a clean seam for a later redirect implementation that loads an alternate DLL from a Helen-controlled path instead of replacing the game's Bink DLLs on disk.

## Scope

In scope:

- a dedicated runtime service for module-load routing
- IAT interception of `LoadLibraryA`, `LoadLibraryW`, `LoadLibraryExA`, and `LoadLibraryExW`
- normalized matching for Bink module names
- runtime logging when Batman requests a Bink DLL through the hook
- a structured redirect target model for future use
- fail-fast behavior when the hook cannot be installed
- tests for name matching, logging, and install/uninstall behavior

Out of scope:

- the actual H265 decode bridge
- Bink API emulation
- replacing the Bink DLLs in the Batman game directory
- process-wide global loader patching beyond the runtime's own hook installation model
- special-case support for every possible module loading API on day one

## User-Facing Behavior

When Batman attempts to load a Bink DLL through a hooked loader call, Helen logs the request and continues with the original path resolution:

- requested module name
- loader API that made the request
- whether the requested name matched a Bink alias
- whether a redirect target is configured for that alias

Example log shape:

```text
[runtime] module-load request api=LoadLibraryW requested=binkw32.dll matched=bink alias redirect=<none>
```

The initial behavior is deliberately non-invasive:

- if a Bink alias is detected, the request is logged
- if no redirect target is configured, the original load continues unchanged
- if a redirect target is configured in a later build, the hook will load that target instead of the original request

If Batman loads Bink through a path that does not pass through the hooked loader functions, Helen does not invent a synthetic match or a best-effort fallback. The log should make that absence obvious so the next boundary can be chosen from evidence rather than guesswork.

## Runtime Design

### Small Routing Service

Add one dedicated runtime component whose only job is to decide whether a module-load request should be observed or rewritten.

This service should:

- normalize requested module names
- match them against a small alias set
- optionally resolve a configured replacement target
- produce one structured routing decision for the hook layer

The service must not own H265 decode logic, Bink ABI emulation, or any Batman gameplay behavior. Its responsibility ends at routing and logging.

### Hook Ownership

The runtime already has an IAT hook facility, so the new loader-routing layer should build on that mechanism instead of introducing a separate patching system.

The first slice should follow the same main-module installation pattern already used by the file hook set and patch the loader IAT entries for:

- `LoadLibraryA`
- `LoadLibraryW`
- `LoadLibraryExA`
- `LoadLibraryExW`

This keeps the first slice focused and testable. A deeper loader hook such as `LdrLoadDll` is out of scope for the initial implementation and should only be considered if Batman bypasses the hooked APIs.

### Module Name Normalization

Matching should operate on a canonical module name, not on raw input strings.

Normalization rules:

- compare case-insensitively
- strip directory components when present
- treat `binkw32.dll` and `bink2w32.dll` as the initial Batman aliases
- reject empty normalized names as invalid input rather than inventing a match

The router should treat the request name as a DLL identifier, not as a filesystem path contract. If the requested string contains a path, the path is preserved for logging, but alias matching uses the final module name.

### Redirect Target Model

Define a structured redirect mapping, even though phase 1 only logs and passes through.

Suggested shape:

```json
{
  "moduleRedirects": [
    {
      "requestedName": "binkw32.dll",
      "replacementPath": "helengamehook/deps/bink/binkw32.dll"
    }
  ]
}
```

Rules:

- requested names are case-insensitive
- replacement paths are Helen-controlled paths relative to the game root or the runtime layout, not arbitrary user input
- duplicate requested names are invalid
- an empty replacement path is invalid

The redirect model exists so the loader hook can evolve from log-only to real path substitution without changing the hook boundary again.

## Integration With HelenGameHook

Keep `HelenGameHook.dll` as the orchestrator, not the place where the Bink stack lives.

The new routing layer should be wired beside the existing services:

- pack selection still happens first
- the active pack set is still loaded and validated normally
- the module-routing hook installs after the runtime initializes enough state to log cleanly
- the service can read pack-driven routing metadata once the future redirect phase exists

This preserves the current bootstrap model and avoids dragging the Bink implementation into the core runtime DLL.

## Failure Handling

Fail fast when:

- the loader hook cannot be installed
- the routing metadata is malformed
- a requested redirect target is invalid

Do not hide hook installation failure with a silent pass-through. If the hook is not in place, Batman should not pretend that Bink routing is active.

The log should distinguish these cases:

- hook installed, request matched, redirect absent, original load continued
- hook installed, request matched, redirect present, alternate path used
- hook installed, request not matched, original load continued
- hook install failed, routing unavailable

## Testing

### Name Matching Tests

- `binkw32.dll` matches the Bink alias set
- `BINKW32.DLL` matches case-insensitively
- `bink2w32.dll` matches the Bink alias set
- unrelated DLL names do not match
- module paths normalize to their final DLL name before matching

### Routing Tests

- log-only mode preserves the original load request
- a configured replacement path is parsed and validated
- duplicate redirect entries are rejected
- empty replacement paths are rejected

### Hook Tests

- the loader hook installs and uninstalls cleanly
- each targeted `LoadLibrary*` import is redirected through the runtime hook
- log emission records the API name and normalized module name when a Bink alias is seen

### Batman Contract Tests

- the Batman pack can enable the module-routing feature without introducing Bink DLL payloads into the game directory
- the runtime can detect Bink load requests when Batman loads through the hooked loader path
- the runtime log captures the Bink alias hit with enough information to support the next redirect step

## Risks

Primary risk:

- Batman may load Bink through a path that does not hit the four hooked loader APIs, which would make the log-only slice appear inert even though the hook is installed correctly

Mitigation:

- log every routing decision clearly
- treat an absence of logged hits as evidence for a deeper loader boundary, not as a reason to guess at a redirect implementation

Secondary risk:

- the eventual H265 bridge may tempt the routing layer to accrete decode-specific behavior

Mitigation:

- keep routing and decode responsibilities separate so the runtime DLL stays small and the Bink implementation can evolve independently
