# Batman Bink Startup Video Hiding Design

## Goal

Make the `batman-aa-skip-videos` pack suppress only Batman Arkham Asylum's five startup movies without deleting, renaming, or modifying any `.bik` file on disk.

## Evidence

`ShippingPC-BmGame.exe` imports `binkw32.dll` directly and includes the `_BinkOpen@8` import. The existing skip pack contributes five `missingPaths`, but the current file API hooks only patch the main executable's Win32 file APIs. Bink performs its own movie-file opening after the game calls `_BinkOpen@8`, so the existing path hiding is not observed by Bink.

## Scope

The feature applies only to these normalized game-relative paths:

- `BmGame/Movies/baa_logo_run_v5_h264.bik`
- `BmGame/Movies/Legal.bik`
- `BmGame/Movies/Legalus.bik`
- `BmGame/Movies/nvidia.bik`
- `BmGame/Movies/utlogo.bik`

All other Bink movies, including gameplay cinematics, must call the original Bink API unchanged.

## Architecture

Add a dedicated `BinkMovieHookSet` to the runtime. It owns an IAT hook for the game's `_BinkOpen@8` import from `binkw32.dll`.

The hook set receives the existing merged `MissingPaths` list and a game-root path. A separate, testable matcher normalizes Bink's ANSI filename argument as a game-relative or absolute game path and compares it against the configured hidden paths. The `BinkOpen` detour logs one suppression event and returns `nullptr` when the requested movie matches. For every non-matching request, it calls the original Bink import with its original arguments.

The hook is installed only when the active pack set has hidden paths. Runtime initialization fails clearly if hidden movie paths are configured but the executable does not expose the required Bink import; it must not silently claim that videos are skipped.

## Error Handling

- Invalid or duplicate normalized hidden paths remain rejected during pack loading.
- The Bink hook is not installed when no hidden paths are enabled.
- A configured Bink-hiding pack fails initialization if `_BinkOpen@8` cannot be hooked.
- Suppression returns a normal Bink-open failure (`nullptr`); it does not manufacture a fake movie object.

## Tests

- Unit-test exact path matching for the five startup movies, including slash and case normalization.
- Unit-test that a gameplay cutscene path does not match.
- Test IAT installation/removal against a test import when available.
- Extend the Batman skip-pack contract test to assert the intended five startup paths.
- Build the Win32 Release runtime, deploy it, launch Batman, and verify a log entry for each suppressed Bink open.

## Success Criteria

- Batman advances past the startup logos/legal screens without input.
- The five real startup movie files remain present and unmodified.
- Gameplay and cutscene Bink videos remain available.
- The log records each suppressed Bink request with its normalized path.
