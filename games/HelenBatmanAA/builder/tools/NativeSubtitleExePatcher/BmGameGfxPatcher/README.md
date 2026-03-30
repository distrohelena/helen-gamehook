# BmGameGfxPatcher

Reusable CLI for patching one or more exports inside an unpacked Unreal package.

This replaces the old one-off PowerShell flow with:

- export discovery
- export hashing / inspection
- manifest-driven multi-export patching
- output verification

## Scope

This tool currently supports:

- Arkham-style `.u` packages where the name/import/export tables are directly readable
- replacing embedded `GFX` payloads inside `GFxMovieInfo` exports
- replacing whole export objects from raw extracted bytes
- patching multiple exports into one output package
- export validation by length and SHA-256

This tool does not currently support:

- encrypted packages
- rebuilding chunk compression
- bytecode editing or native Unreal function patching

## Commands

```powershell
dotnet run --project .\tools\BmGameGfxPatcher -- list-exports --package .\extracted\bmgame-unpacked\BmGame.u --type GFxMovieInfo
```

```powershell
dotnet run --project .\tools\BmGameGfxPatcher -- describe-export --package .\extracted\bmgame-unpacked\BmGame.u --owner GameHUD --name HUD --type GFxMovieInfo
```

```powershell
dotnet run --project .\tools\BmGameGfxPatcher -- extract-gfx --package .\extracted\bmgame-unpacked\BmGame.u --owner PauseMenu --name Pause --output .\extracted\pause\Pause-extracted.gfx
```

```powershell
dotnet run --project .\tools\BmGameGfxPatcher -- patch --package .\extracted\bmgame-unpacked\BmGame.u --manifest .\tools\BmGameGfxPatcher\examples\hud-only.manifest.jsonc --output .\generated\BmGame-HUD-patched-cs.u
```

## Manifest

The manifest format is JSON with optional comments and trailing commas.

```jsonc
{
  "name": "HUD only example",
  "patches": [
    {
      "patchMode": "gfx",
      "owner": "GameHUD",
      "exportName": "HUD",
      "exportType": "GFxMovieInfo",
      "replacementPath": "..\\..\\extracted\\hud\\HUD-patched.gfx",
      "payloadMagic": "GFX",
      "expectedOriginalLength": 453578,
      "expectedOriginalSha256": "9d934e9cbb503dc0a868e01fc15b52235f9034dcd96e21e92bf214355909270b",
      "outputObjectPath": "..\\..\\generated\\HUD-patched.GFxMovieInfo"
    }
  ]
}
```

For raw object replacement, set `"patchMode": "raw"` and point `replacementPath` at an extracted object blob such as `MediumFont.Font`. `payloadMagic` is ignored for raw patches.

## Distribution Notes

For a distributable mod, ship:

- the replacement `.gfx` files
- the manifest
- this CLI, published with `dotnet publish`

Then patch both `PauseMenu.Pause` and `GameHUD.HUD` into one output package from a clean unpacked `BmGame.u`.

Example publish command:

```powershell
dotnet publish .\tools\BmGameGfxPatcher\BmGameGfxPatcher.csproj -c Release -r win-x64 --self-contained true
```

## Subtitle Size Path

For the eventual subtitle-size mod, the manifest should contain two entries:

- `PauseMenu.Pause`
- `GameHUD.HUD`

That keeps the build process reusable even when the Flash assets change.
