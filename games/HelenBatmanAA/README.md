# HelenBatmanAA

This folder now contains the Batman-specific runtime pack experiments, the Batman-specific build inputs, and Batman-local helper scripts.

Important warning:

- The current shipped Batman pack and the legacy FFDec rebuild workflow are not retail-vanilla safe.
- Most Batman package rebuild paths still consume unpacked package bases, not the compressed retail files from a clean Steam install.
- Do not treat the current shipped Batman pack as "drop in Helen DLLs + pack on untouched Batman" until those shipped deltas are rebuilt from true retail files.
- The direct retail frontend experiments documented below are the current exception: `patch-mainv2-version-label` and `patch-mainv2-audio-subtitle-size` patch the compressed retail `Frontend.umap` directly.

Current layout:

- `helengamehook\packs\batman-aa-subtitles`
  - current shipped delta-backed runtime pack used by `HelenGameHook.dll`
  - ships `files.json` with `mode = "delta-on-read"`, `assets\deltas\BmGame-subtitle-signal.hgdelta`, `assets\deltas\Frontend-main-menu-subtitle-size.hgdelta`, and the native blob assets under `assets\native`
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
        DeltaSha256 = '421665A0A955276A156800C0FA2F26B8193F71601648118902D8EE044E885397'
    },
    @{
        Id = 'frontendMapPackage'
        Path = 'BmGame/CookedPC/Maps/Frontend/Frontend.umap'
        DeltaPath = 'assets/deltas/Frontend-main-menu-subtitle-size.hgdelta'
        DeltaFilePath = Join-Path $PackBuildRoot 'assets\deltas\Frontend-main-menu-subtitle-size.hgdelta'
        DeltaSha256 = 'C23B8251D33010CA790B7C657183ACEA63734CFC9354E46784D84338F2627395'
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
    if ($DeployedDeltaHash -ne $ExpectedVirtualFile.DeltaSha256) {
        throw "Unexpected deployed delta hash for $($ExpectedVirtualFile.Id): $DeployedDeltaHash"
    }
}

if (Test-Path -LiteralPath $PackagesPath) {
    throw "Batman deployment should not contain legacy packages: $PackagesPath"
}
```

Important notes:

- The deploy script now validates the shipped delta-backed gameplay package before it copies anything into the game directory, stages the pack before replacing the live install, and verifies the deployed manifest and delta hash after copy.
- The current repo still contains the shipped delta-backed `BmGame.u` gameplay package, the shipped delta-backed `Frontend.umap` front-end package, their checked-in `.hgdelta` containers, and the native text-scale blob, but those deltas are built from unpacked package bases and are not retail-vanilla safe.
- `Deploy-Batman.ps1` now runs `Test-BatmanInstalledBaseCompatibility.ps1` before copying anything into the game directory and fails fast when the installed Batman package files do not match the exact base hashes in `files.json`.
- The prep script recreates the ignored local builder prerequisites under `builder\extracted`, while generated build outputs stay under ignored `builder\generated`.
- Live front-end verification requires the installed `BmGame\CookedPC\Maps\Frontend\Frontend.umap` to match the trusted base hash used to build `assets\deltas\Frontend-main-menu-subtitle-size.hgdelta`. The scripts do not hide a base-package mismatch with fallbacks.
- Legacy investigation-only frontend assets, when kept locally, now live under `builder\extracted\frontend\...` instead of the old flat `builder\working` layout.
- Legacy debug scripts from the old `artifacts` folder were copied into `scripts` for reference, but the recommended entry points are the PascalCase scripts above.

## Main Menu Version Label Experiment

This experiment uses a direct patch of the existing compressed retail `Frontend.umap` package under `<BatmanBuilderRoot>\extracted\frontend-retail\Frontend.umap`. It does not rebuild `MainV2` through FFDec `-importScript`, and the experiment rebuild path consumes only that retail frontend copy plus the direct `patch-mainv2-version-label` command. Deployment does not replace the checked-in gameplay pack source; it temporarily swaps the live copied `batman-aa-subtitles` pack under the game directory so `PackRepository` sees the experiment pack first.

Full automated flow:

1. Prepare the builder workspace with `Prepare-BatmanBuilderWorkspace.ps1`.
2. Rebuild the experiment pack.
3. Verify the generated package.
4. Deploy the experiment pack copy.
5. Run `Launch-Check-Batman.ps1`.

Prepare the shared builder workspace first. This seeds the trusted retail frontend copy under `<BatmanBuilderRoot>\extracted\frontend-retail\Frontend.umap` for the experiment. The prep script may also refresh unpacked FFDec outputs for other Batman builder workflows, but this version-label proof does not consume the unpacked frontend outputs in its patch path:

```powershell
powershell -ExecutionPolicy Bypass -File .\games\HelenBatmanAA\scripts\Prepare-BatmanBuilderWorkspace.ps1 `
  -BuilderRoot <BatmanBuilderRoot> `
  -BasePackagePath <TrustedBmGamePath> `
  -FrontendBasePackagePath <TrustedFrontendPath> `
  -FfdecCliPath <FfdecCliPath>
```

Rebuild the experiment pack:

```powershell
powershell -ExecutionPolicy Bypass -File .\games\HelenBatmanAA\scripts\Rebuild-BatmanMainMenuVersionLabelExperiment.ps1 `
  -Configuration Debug `
  -BuilderRoot <BatmanBuilderRoot>
```

Verify the generated experiment pack:

```powershell
powershell -ExecutionPolicy Bypass -File .\games\HelenBatmanAA\scripts\Test-BatmanMainMenuVersionLabelPackage.ps1 `
  -BatmanRoot <BatmanRoot> `
  -BuilderRoot <BatmanBuilderRoot>
```

Deploy the experiment pack copy into the live game directory:

```powershell
powershell -ExecutionPolicy Bypass -File .\games\HelenBatmanAA\scripts\Deploy-BatmanMainMenuVersionLabelExperiment.ps1 `
  -BuilderRoot <BatmanBuilderRoot>
```

Run the launch check:

```powershell
powershell -ExecutionPolicy Bypass -File .\games\HelenBatmanAA\scripts\Launch-Check-Batman.ps1
```

Manual smoke gate:

1. Launch Batman.
2. Press start.
3. Reach profile/save selection.
4. Reach the main menu.
5. Confirm the label reads `v1.1 HELEN FIX`.

Rollback if the experiment needs to be removed:

1. Remove the live deployed experiment pack copy under `Binaries\helengamehook\packs\batman-aa-subtitles`, or move the backup folder created by the deploy script back into place.
2. Confirm the normal live `batman-aa-subtitles` pack is back in place under the game directory.

## Graphics Options Experiment

Build:

1. Run `Rebuild-BatmanGraphicsOptionsExperiment.ps1`.
2. Run `Test-BatmanGraphicsOptionsPackage.ps1`.
3. Run `Test-BatmanRetailGraphicsOptionsPatch.ps1`.

Deploy:

1. Run `Deploy-BatmanGraphicsOptionsExperiment.ps1`.

Live smoke:

1. Launch Batman.
2. Press start.
3. Reach profile/save selection.
4. Reach the main menu.
5. Open `Options -> Graphics Options`.
6. Confirm the options stack entries exist.
7. Confirm the graphics list scrolls and shows all requested rows.
8. Confirm `Fullscreen` and `Resolution` display saved values but do not expose arrows.
9. Change `VSync` or `Bloom` and confirm `Apply Changes` becomes enabled.
10. Back out without applying and confirm the three-choice unsaved-changes prompt appears.
11. Choose `Cancel` and confirm the draft is preserved.
12. Choose `Apply Changes` and confirm the restart warning appears after restart-required edits.
13. Reopen the screen and confirm the applied values persisted from `BmEngine.ini`.

## Main Menu Subtitle Size Experiment

This experiment also uses a direct patch of the existing compressed retail `Frontend.umap` package under `<BatmanBuilderRoot>\extracted\frontend-retail\Frontend.umap`, but it needs one generated prototype `MainV2-subtitle-size.gfx` as the transplant source for the `ScreenOptionsAudio` sprite and the subtitle-size-aware `rs.ui.ListItem` class-definition tag. It does not ship the old unpacked `Frontend.umap` delta path, and deployment still swaps only the live copied `batman-aa-subtitles` pack under the game directory.

Full automated flow:

1. Prepare the shared builder workspace.
2. Rebuild the subtitle-size experiment pack.
3. Verify the generated package and layout.
4. Deploy the experiment pack copy.
5. Run `Launch-Check-Batman.ps1`.

Prepare the shared builder workspace first. This seeds the trusted retail frontend copy under `<BatmanBuilderRoot>\extracted\frontend-retail\Frontend.umap`, the FFDec tool under `<BatmanBuilderRoot>\extracted\ffdec\ffdec-cli.exe`, and the generated prototype dependencies used by `build-main-menu-audio`:

```powershell
powershell -ExecutionPolicy Bypass -File .\games\HelenBatmanAA\scripts\Prepare-BatmanBuilderWorkspace.ps1 `
  -BuilderRoot <BatmanBuilderRoot> `
  -BasePackagePath <TrustedBmGamePath> `
  -FrontendBasePackagePath <TrustedFrontendPath> `
  -FfdecCliPath <FfdecCliPath>
```

Rebuild the subtitle-size experiment pack:

```powershell
powershell -ExecutionPolicy Bypass -File .\games\HelenBatmanAA\scripts\Rebuild-BatmanMainMenuSubtitleSizeExperiment.ps1 `
  -Configuration Debug `
  -BatmanRoot <BatmanRoot> `
  -BuilderRoot <BatmanBuilderRoot>
```

Verify the generated retail package contract and the patched `ScreenOptionsAudio` layout:

```powershell
powershell -ExecutionPolicy Bypass -File .\games\HelenBatmanAA\scripts\Test-BatmanMainMenuSubtitleSizePackage.ps1 `
  -BatmanRoot <BatmanRoot> `
  -BuilderRoot <BatmanBuilderRoot>

powershell -ExecutionPolicy Bypass -File .\games\HelenBatmanAA\scripts\Test-BatmanMainMenuAudioLayout.ps1 `
  -BatmanRoot <BatmanRoot>
```

Deploy the experiment pack copy into the live game directory:

```powershell
powershell -ExecutionPolicy Bypass -File .\games\HelenBatmanAA\scripts\Deploy-BatmanMainMenuSubtitleSizeExperiment.ps1 `
  -BuilderRoot <BatmanBuilderRoot>
```

Run the launch check:

```powershell
powershell -ExecutionPolicy Bypass -File .\games\HelenBatmanAA\scripts\Launch-Check-Batman.ps1
```

Manual smoke gate:

1. Launch Batman.
2. Press start.
3. Reach profile/save selection.
4. Reach the main menu.
5. Open `Options -> Audio`.
6. Confirm all five rows sit inside the square panel.
7. Confirm the visible order is `Subtitles`, `Subtitle Size`, `Volume SFX`, `Volume Music`, `Volume Dialogue`.
8. Confirm up/down focus order matches that visible order.
9. Turn `Subtitles` off and confirm `Subtitle Size` stays visible but greyed or inert.
10. Turn `Subtitles` on and confirm `Subtitle Size` becomes active again.
11. Change the `Subtitle Size` value, back out, and confirm the row persists on reopen.

Rollback if the experiment needs to be removed:

1. Remove the live deployed experiment pack copy under `Binaries\helengamehook\packs\batman-aa-subtitles`, or move the backup folder created by the deploy script back into place.
2. Confirm the normal live `batman-aa-subtitles` pack is back in place under the game directory.
