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
  - the ignored local workspace is recreated by `scripts\Prepare-BatmanBuilderWorkspace.ps1` from a trusted `BmGame.u`, a trusted decompressed `Frontend.umap`, and an FFDec install
- `scripts`
  - Batman-local rebuild, deploy, and launch helpers
- `notes`
  - preserved investigation notes from the original Batman subtitle work

Recommended Batman workflow:

1. Prepare the ignored local builder workspace from trusted Batman base packages plus FFDec:

```powershell
powershell -ExecutionPolicy Bypass -File .\games\HelenBatmanAA\scripts\Prepare-BatmanBuilderWorkspace.ps1 `
  -BasePackagePath D:\trusted\BmGame.u `
  -FrontendBasePackagePath D:\trusted\Frontend.umap `
  -FfdecCliPath C:\tools\ffdec\ffdec-cli.exe
```

This populates the local ignored builder inputs:

- `builder\extracted\bmgame-unpacked\BmGame.u`
- `builder\extracted\frontend\frontend-umap-unpacked\Frontend.umap`
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

3. Verify the shipped Batman pack before deployment:

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

After the process starts, verify both menu paths:

1. Pause `Options -> Audio` still shows `Subtitle Size`.
2. Main menu `Options -> Audio` shows `Subtitle Size` as the fifth row.
3. Turn `Subtitles` off and confirm `Subtitle Size` is greyed or inert.
4. Turn `Subtitles` on and confirm `Subtitle Size` becomes active again.
5. Pass press-start and save/profile selection without front-end breakage.

Then confirm `D:\steam\steamapps\common\Batman Arkham Asylum GOTY\Binaries\helengamehook\config\runtime.json` is updated while `D:\steam\steamapps\common\Batman Arkham Asylum GOTY\BmGame\CookedPC\BmGame.u` and `D:\steam\steamapps\common\Batman Arkham Asylum GOTY\BmGame\CookedPC\Maps\Frontend\Frontend.umap` remain unchanged.

6. Verify the deployed pack copy under the game directory is still the shipped delta-backed package:

```powershell
$PackBuildRoot = 'D:\steam\steamapps\common\Batman Arkham Asylum GOTY\Binaries\helengamehook\packs\batman-aa-subtitles\builds\steam-goty-1.0'
$FilesJsonPath = Join-Path $PackBuildRoot 'files.json'
$ExpectedVirtualFiles = @(
    @{
        Id = 'bmgameGameplayPackage'
        Path = 'BmGame/CookedPC/BmGame.u'
        DeltaPath = 'assets/deltas/BmGame-subtitle-signal.hgdelta'
        DeltaFilePath = Join-Path $PackBuildRoot 'assets\deltas\BmGame-subtitle-signal.hgdelta'
    },
    @{
        Id = 'frontendMapPackage'
        Path = 'BmGame/CookedPC/Maps/Frontend/Frontend.umap'
        DeltaPath = 'assets/deltas/Frontend-main-menu-subtitle-size.hgdelta'
        DeltaFilePath = Join-Path $PackBuildRoot 'assets\deltas\Frontend-main-menu-subtitle-size.hgdelta'
    }
)
$PackagesPath = Join-Path $PackBuildRoot 'assets\packages'

$Manifest = Get-Content -LiteralPath $FilesJsonPath -Raw | ConvertFrom-Json
if (@($Manifest.virtualFiles).Count -ne $ExpectedVirtualFiles.Count) {
    throw "Unexpected deployed virtual file count: $(@($Manifest.virtualFiles).Count)"
}

foreach ($ExpectedVirtualFile in $ExpectedVirtualFiles) {
    $VirtualFile = @($Manifest.virtualFiles | Where-Object { $_.id -eq $ExpectedVirtualFile.Id })[0]
    if ($null -eq $VirtualFile) { throw "Unexpected deployed virtual file id set." }
    if ($VirtualFile.path -ne $ExpectedVirtualFile.Path) { throw "Unexpected deployed virtual file path for $($ExpectedVirtualFile.Id): $($VirtualFile.path)" }
    if ($VirtualFile.mode -ne 'delta-on-read') { throw "Unexpected deployed virtual file mode for $($ExpectedVirtualFile.Id): $($VirtualFile.mode)" }
    if ($VirtualFile.source.kind -ne 'delta-file') { throw "Unexpected deployed source kind for $($ExpectedVirtualFile.Id): $($VirtualFile.source.kind)" }
    if ($VirtualFile.source.path -ne $ExpectedVirtualFile.DeltaPath) { throw "Unexpected deployed delta path for $($ExpectedVirtualFile.Id): $($VirtualFile.source.path)" }

    $DeployedDeltaHash = (Get-FileHash -LiteralPath $ExpectedVirtualFile.DeltaFilePath -Algorithm SHA256).Hash
    Write-Output "$($ExpectedVirtualFile.Id): $DeployedDeltaHash"
}

if (Test-Path -LiteralPath $PackagesPath) {
    throw "Batman deployment should not contain legacy packages: $PackagesPath"
}
```

Important notes:

- The deploy script now validates the shipped delta-backed gameplay package before it copies anything into the game directory, stages the pack before replacing the live install, and verifies the deployed manifest and delta hash after copy.
- The current supported runtime pack is the shipped delta-backed `BmGame.u` gameplay package, the shipped delta-backed `Frontend.umap` front-end package, and the native text-scale blob.
- The prep script recreates the ignored local builder prerequisites under `builder\extracted`, while generated build outputs stay under ignored `builder\generated`.
- Live front-end verification requires the installed `BmGame\CookedPC\Maps\Frontend\Frontend.umap` to match the trusted base hash used to build `assets\deltas\Frontend-main-menu-subtitle-size.hgdelta`. The scripts do not hide a base-package mismatch with fallbacks.
- Legacy investigation-only frontend assets, when kept locally, now live under `builder\extracted\frontend\...` instead of the old flat `builder\working` layout.
- Legacy debug scripts from the old `artifacts` folder were copied into `scripts` for reference, but the recommended entry points are the PascalCase scripts above.
