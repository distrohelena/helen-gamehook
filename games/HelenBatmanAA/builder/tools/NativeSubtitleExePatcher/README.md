# NativeSubtitleExePatcher

Small C# CLI for patching `ShippingPC-BmGame.exe` in Batman: Arkham Asylum GOTY.

Current supported patches:

- `patch-bink-subtitles`
  - legacy brightness-style Bink subtitle scale patch
- `patch-bink-text-scale`
  - patches the native text helper used by Bink subtitles and other HUD text
  - supports `--global`
  - supports `--subtitle-size-signal` to drive live scale updates from the game's FE setter path
  - also supports `--internal-ini-live` for the older in-process INI polling path
  - current working mode for larger centered subtitles/tips with in-game size changes is `--global --subtitle-size-signal --scale-multiplier 1.5`
- `set-live-text-scale`
  - rewrites the current live scale constant in the running game process
- `watch-live-text-scale`
  - polls `BmEngine.ini` and updates the running game process from the subtitle-size signal bits
  - prints explicit `waiting`, `attached`, and `apply` lines while running
- `verify-bink-subtitles`
- `verify-bink-text-scale`

## Build

```powershell
dotnet build .\NativeSubtitleExePatcher.csproj -c Release
```

## Usage

Patch a clean executable:

```powershell
dotnet .\bin\Release\net8.0\NativeSubtitleExePatcher.dll patch-bink-text-scale `
  --exe "D:\steam\steamapps\common\Batman Arkham Asylum GOTY\Binaries\ShippingPC-BmGame.exe" `
  --scale-multiplier 1.5 `
  --global
```

Patch a clean executable with live subtitle-size updates coming from the in-game FE setter path:

```powershell
dotnet .\bin\Release\net8.0\NativeSubtitleExePatcher.dll patch-bink-text-scale `
  --exe "D:\steam\steamapps\common\Batman Arkham Asylum GOTY\Binaries\ShippingPC-BmGame.exe" `
  --scale-multiplier 1.5 `
  --global `
  --subtitle-size-signal `
  --small-scale 1.3 `
  --normal-scale 1.5 `
  --large-scale 1.8
```

Verify the patch:

```powershell
dotnet .\bin\Release\net8.0\NativeSubtitleExePatcher.dll verify-bink-text-scale `
  --exe "D:\steam\steamapps\common\Batman Arkham Asylum GOTY\Binaries\ShippingPC-BmGame.exe"
```

Watch a live game and mirror subtitle scale from the user config:

```powershell
dotnet .\bin\Release\net8.0\NativeSubtitleExePatcher.dll watch-live-text-scale `
  --ini "C:\Users\Helena\Documents\Square Enix\Batman Arkham Asylum GOTY\BmGame\Config\BmEngine.ini"
```

## Notes

- The project assumes the retail Windows executable layout used during investigation.
- Hook/cave RVAs are hardcoded in [Program.cs](./Program.cs).
- The fixed global-only patch stores its scale float at `CaveRva + 85`.
- The `--subtitle-size-signal` mode also allocates a writable state block and one worker cave; the current implementation hooks the executable's one-argument FE wrapper stubs and updates scale for the raw subtitle-size codes `4101/4102/4103`.
- `--internal-ini-live` is kept for investigation history, but the FE signal path is the current runtime route.
- Always keep an untouched backup of the original executable before patching.
