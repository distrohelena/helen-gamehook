# HelenBatmanAA

This folder now contains the Batman-specific runtime pack, the Batman-specific build inputs, and Batman-local helper scripts.

Current layout:

- `helengamehook\packs\batman-aa-subtitles`
  - current runtime pack used by `HelenGameHook.dll`
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

2. Deploy the rebuilt runtime and pack to the game:

```powershell
powershell -ExecutionPolicy Bypass -File .\games\HelenBatmanAA\scripts\Deploy-Batman.ps1
```

3. Launch the game:

```powershell
powershell -ExecutionPolicy Bypass -File .\games\HelenBatmanAA\scripts\Launch-Batman.ps1
```

Important notes:

- The current supported runtime pack is the gameplay `BmGame.u` slice plus the native text-scale blob.
- The builder mirror keeps the original relative path assumptions intact so the old C# toolchain can run without being rewritten first.
- Legacy debug scripts from the old `artifacts` folder were copied into `scripts` for reference, but the recommended entry points are the PascalCase scripts above.
