# HelenBatmanAA

This folder now contains the Batman-specific delta-backed runtime pack, the Batman-specific build inputs, and Batman-local helper scripts.

Current layout:

- `helengamehook\packs\batman-aa-subtitles`
  - current shipped delta-backed runtime pack used by `HelenGameHook.dll`
  - ships `files.json` with `mode = "delta-on-read"`, `assets\deltas\BmGame-subtitle-signal.hgdelta`, and the native blob assets under `assets\native`
- `builder`
  - self-contained mirror of the Batman subtitle build workspace
  - includes `tools\NativeSubtitleExePatcher`, `ffdec`, `bmgame-unpacked\BmGame.u`, and the source GFx/XML/script inputs needed by the current builders
- `scripts`
  - Batman-local rebuild, deploy, and launch helpers
- `notes`
  - preserved investigation notes from the original Batman subtitle work

Recommended Batman workflow:

1. Rebuild the Batman pack assets:

```powershell
powershell -ExecutionPolicy Bypass -File .\games\HelenBatmanAA\scripts\Rebuild-BatmanPack.ps1
```

2. Verify the shipped gameplay package before deployment:

```powershell
powershell -ExecutionPolicy Bypass -File .\games\HelenBatmanAA\scripts\Test-BatmanKnownGoodGameplayPackage.ps1
```

3. Deploy the validated runtime and pack to the game:

```powershell
powershell -ExecutionPolicy Bypass -File .\games\HelenBatmanAA\scripts\Deploy-Batman.ps1
```

4. Launch Batman and confirm the process starts:

```powershell
powershell -ExecutionPolicy Bypass -File .\games\HelenBatmanAA\scripts\Launch-Check-Batman.ps1
```

5. Verify the deployed pack is still the shipped delta-backed package:

```powershell
$FilesJson = 'D:\steam\steamapps\common\Batman Arkham Asylum GOTY\Binaries\helengamehook\packs\batman-aa-subtitles\builds\steam-goty-1.0\files.json'
$DeltaPath = 'D:\steam\steamapps\common\Batman Arkham Asylum GOTY\Binaries\helengamehook\packs\batman-aa-subtitles\builds\steam-goty-1.0\assets\deltas\BmGame-subtitle-signal.hgdelta'
$Manifest = Get-Content -LiteralPath $FilesJson -Raw | ConvertFrom-Json
$Manifest.virtualFiles[0].mode
$Manifest.virtualFiles[0].source.kind
$Manifest.virtualFiles[0].source.path
Get-FileHash -LiteralPath $DeltaPath -Algorithm SHA256
```

Important notes:

- The deploy script now validates the shipped delta-backed gameplay package before it copies anything into the game directory and verifies the deployed manifest and delta hash after copy.
- The current supported runtime pack is the shipped delta-backed `BmGame.u` package plus the native text-scale and subtitle-signal blobs.
- The builder mirror keeps the original relative path assumptions intact so the old C# toolchain can run without being rewritten first.
- Legacy debug scripts from the old `artifacts` folder were copied into `scripts` for reference, but the recommended entry points are the PascalCase scripts above.
