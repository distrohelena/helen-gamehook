# HelenBatmanAA

This folder now contains the Batman-specific delta-backed runtime pack, the Batman-specific build inputs, and Batman-local helper scripts.

Current layout:

- `helengamehook\packs\batman-aa-subtitles`
  - current shipped delta-backed runtime pack used by `HelenGameHook.dll`
  - ships `files.json` with `mode = "delta-on-read"`, `assets\deltas\BmGame-subtitle-signal.hgdelta`, and the native blob assets under `assets\native`
- `builder`
  - maintained Batman build tool source under `tools\NativeSubtitleExePatcher`
  - local ignored builder prerequisites under `extracted`
  - local ignored builder outputs under `generated`
  - the ignored local workspace is recreated by `scripts\Prepare-BatmanBuilderWorkspace.ps1` from a trusted `BmGame.u` plus an FFDec install
- `scripts`
  - Batman-local rebuild, deploy, and launch helpers
- `notes`
  - preserved investigation notes from the original Batman subtitle work

Recommended Batman workflow:

1. Prepare the ignored local builder workspace from a trusted Batman base package and FFDec:

```powershell
powershell -ExecutionPolicy Bypass -File .\games\HelenBatmanAA\scripts\Prepare-BatmanBuilderWorkspace.ps1 `
  -BasePackagePath D:\trusted\BmGame.u `
  -FrontendBasePackagePath D:\trusted\Startup_INT.upk `
  -FfdecCliPath C:\tools\ffdec\ffdec-cli.exe
```

This populates the local ignored builder inputs:

- `builder\extracted\bmgame-unpacked\BmGame.u`
- `builder\extracted\frontend\startup-int-unpacked\Startup_INT.upk`
- `builder\extracted\frontend\mainv2\frontend-mainv2.gfx`
- `builder\extracted\frontend\mainv2\frontend-mainv2.xml`
- `builder\extracted\frontend\mainv2\frontend-mainv2-export\scripts`
- `builder\extracted\ffdec\...`
- `builder\extracted\pause\Pause-extracted.gfx`
- `builder\extracted\pause\Pause.xml`
- `builder\extracted\pause\pause-ffdec-export\scripts`
- `builder\extracted\hud\HUD-extracted.gfx`
- `builder\extracted\hud\HUD.xml`
- `builder\extracted\hud\hud-ffdec-scripts\scripts`

2. Rebuild the Batman pack assets:

```powershell
powershell -ExecutionPolicy Bypass -File .\games\HelenBatmanAA\scripts\Rebuild-BatmanPack.ps1
```

`Rebuild-BatmanPack.ps1` now fails fast with a precise error if the local ignored builder workspace has not been prepared yet.

3. Verify the shipped gameplay package before deployment:

```powershell
powershell -ExecutionPolicy Bypass -File .\games\HelenBatmanAA\scripts\Test-BatmanKnownGoodGameplayPackage.ps1
```

4. Deploy the validated runtime and pack to the game:

```powershell
powershell -ExecutionPolicy Bypass -File .\games\HelenBatmanAA\scripts\Deploy-Batman.ps1
```

5. Launch Batman and complete a quick in-game verification:

```powershell
powershell -ExecutionPolicy Bypass -File .\games\HelenBatmanAA\scripts\Launch-Check-Batman.ps1
```

After the process starts, open the pause menu and confirm it still shows `Subtitle Size`. Change the value and verify the subtitle size updates live, then confirm `D:\steam\steamapps\common\Batman Arkham Asylum GOTY\Binaries\helengamehook\config\runtime.json` is updated while `D:\steam\steamapps\common\Batman Arkham Asylum GOTY\BmGame\CookedPC\BmGame.u` remains unchanged.

6. Verify the deployed pack copy under the game directory is still the shipped delta-backed package:

```powershell
$PackBuildRoot = 'D:\steam\steamapps\common\Batman Arkham Asylum GOTY\Binaries\helengamehook\packs\batman-aa-subtitles\builds\steam-goty-1.0'
$FilesJsonPath = Join-Path $PackBuildRoot 'files.json'
$DeployedDeltaPath = Join-Path $PackBuildRoot 'assets\deltas\BmGame-subtitle-signal.hgdelta'
$PackagesPath = Join-Path $PackBuildRoot 'assets\packages'

$Manifest = Get-Content -LiteralPath $FilesJsonPath -Raw | ConvertFrom-Json
$GameplayFile = @($Manifest.virtualFiles)[0]
if ($GameplayFile.id -ne 'bmgameGameplayPackage') { throw "Unexpected deployed virtual file id: $($GameplayFile.id)" }
if ($GameplayFile.path -ne 'BmGame/CookedPC/BmGame.u') { throw "Unexpected deployed virtual file path: $($GameplayFile.path)" }
if ($GameplayFile.mode -ne 'delta-on-read') { throw "Unexpected deployed virtual file mode: $($GameplayFile.mode)" }
if ($GameplayFile.source.kind -ne 'delta-file') { throw "Unexpected deployed source kind: $($GameplayFile.source.kind)" }
if ($GameplayFile.source.path -ne 'assets/deltas/BmGame-subtitle-signal.hgdelta') { throw "Unexpected deployed delta path: $($GameplayFile.source.path)" }

$DeployedDeltaHash = (Get-FileHash -LiteralPath $DeployedDeltaPath -Algorithm SHA256).Hash
if ($DeployedDeltaHash -ne '421665A0A955276A156800C0FA2F26B8193F71601648118902D8EE044E885397') {
    throw "Batman deployment delta hash mismatch: $DeployedDeltaHash"
}

if (Test-Path -LiteralPath $PackagesPath) {
    throw "Batman deployment should not contain legacy packages: $PackagesPath"
}
```

Important notes:

- The deploy script now validates the shipped delta-backed gameplay package before it copies anything into the game directory, stages the pack before replacing the live install, and verifies the deployed manifest and delta hash after copy.
- The current supported runtime pack is the shipped delta-backed `BmGame.u` package plus the native text-scale and subtitle-signal blobs.
- The prep script recreates the ignored local builder prerequisites under `builder\extracted`, while generated build outputs stay under ignored `builder\generated`.
- Legacy investigation-only frontend assets, when kept locally, now live under `builder\extracted\frontend\...` instead of the old flat `builder\working` layout.
- Legacy debug scripts from the old `artifacts` folder were copied into `scripts` for reference, but the recommended entry points are the PascalCase scripts above.
