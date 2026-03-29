param(
    [string]$Configuration = 'Release'
)

$ErrorActionPreference = 'Stop'

$BatmanRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$BuilderRoot = Join-Path $BatmanRoot 'builder'
$ToolRoot = Join-Path $BuilderRoot 'tools\NativeSubtitleExePatcher'
$NativeSubtitleExePatcherProjectPath = Join-Path $ToolRoot 'NativeSubtitleExePatcher.csproj'
$SubtitleSizeModBuilderProjectPath = Join-Path $ToolRoot 'SubtitleSizeModBuilder\SubtitleSizeModBuilder.csproj'
$BmGameGfxPatcherProjectPath = Join-Path $ToolRoot 'BmGameGfxPatcher\BmGameGfxPatcher.csproj'
$PackBuildRoot = Join-Path $BatmanRoot 'helengamehook\packs\batman-aa-subtitles\builds\steam-goty-1.0'
$BuildAssetsRoot = Join-Path $BuilderRoot 'build-assets\pause-runtime-scale'
$FfdecPath = Join-Path $BuilderRoot 'ffdec\ffdec-cli.exe'
$BasePackagePath = Join-Path $BuilderRoot 'bmgame-unpacked\BmGame.u'
$FilesJsonPath = Join-Path $PackBuildRoot 'files.json'
$ManifestPath = Join-Path $BuildAssetsRoot 'pause-runtime-scale.manifest.jsonc'
$PauseListItemOverridePath = Join-Path $BatmanRoot 'patch-source\PauseRuntimeScaleListItem.as'
$GeneratedPauseScriptsRoot = Join-Path $BuildAssetsRoot '_build\pause-scripts'
$GeneratedPauseListItemPath = Join-Path $GeneratedPauseScriptsRoot '__Packages\rs\ui\ListItem.as'
$PauseStructuralGfxPath = Join-Path $BuildAssetsRoot '_build\Pause-runtime-scale-structural.gfx'
$PauseOutputGfxPath = Join-Path $BuildAssetsRoot 'Pause-runtime-scale.gfx'
$GeneratedGameplayPackagePath = Join-Path $BuildAssetsRoot 'BmGame-subtitle-signal.u'
$GameplayDeltaPath = Join-Path $PackBuildRoot 'assets\deltas\BmGame-subtitle-signal.hgdelta'
$GlobalBlobPath = Join-Path $PackBuildRoot 'assets\native\batman-global-text-scale.bin'
$BuildHgdeltaScriptPath = Join-Path $PSScriptRoot 'Build-Hgdelta.ps1'

New-Item -ItemType Directory -Force -Path (Split-Path -Parent $GeneratedGameplayPackagePath) | Out-Null
New-Item -ItemType Directory -Force -Path (Split-Path -Parent $GameplayDeltaPath) | Out-Null
New-Item -ItemType Directory -Force -Path (Split-Path -Parent $GlobalBlobPath) | Out-Null

& dotnet build $SubtitleSizeModBuilderProjectPath -c $Configuration
if ($LASTEXITCODE -ne 0) {
    throw "dotnet build failed for SubtitleSizeModBuilder.csproj"
}

& dotnet build $BmGameGfxPatcherProjectPath -c $Configuration
if ($LASTEXITCODE -ne 0) {
    throw "dotnet build failed for BmGameGfxPatcher.csproj"
}

& dotnet build $NativeSubtitleExePatcherProjectPath -c $Configuration
if ($LASTEXITCODE -ne 0) {
    throw "dotnet build failed for NativeSubtitleExePatcher.csproj"
}

& dotnet run --project $SubtitleSizeModBuilderProjectPath -c $Configuration -- `
    build-pause-runtime-scale `
    --root $BuilderRoot `
    --output-dir $BuildAssetsRoot `
    --ffdec $FfdecPath
if ($LASTEXITCODE -ne 0) {
    throw "Pause runtime scale asset build failed."
}

if (-not (Test-Path $PauseListItemOverridePath)) {
    throw "Pause runtime scale override file was not found: $PauseListItemOverridePath"
}

if (-not (Test-Path $GeneratedPauseListItemPath)) {
    throw "Generated pause runtime scale list item was not found: $GeneratedPauseListItemPath"
}

if (-not (Test-Path $PauseStructuralGfxPath)) {
    throw "Generated pause runtime scale structural GFX was not found: $PauseStructuralGfxPath"
}

Copy-Item $PauseListItemOverridePath $GeneratedPauseListItemPath -Force

& $FfdecPath -importScript $PauseStructuralGfxPath $PauseOutputGfxPath $GeneratedPauseScriptsRoot
if ($LASTEXITCODE -ne 0) {
    throw "Pause runtime scale script import failed after applying the Batman override."
}

& dotnet run --project $BmGameGfxPatcherProjectPath -c $Configuration -- `
    patch `
    --package $BasePackagePath `
    --manifest $ManifestPath `
    --output $GeneratedGameplayPackagePath
if ($LASTEXITCODE -ne 0) {
    throw "BmGame package patch build failed."
}

$deltaInfo = & $BuildHgdeltaScriptPath `
    -BaseFile $BasePackagePath `
    -TargetFile $GeneratedGameplayPackagePath `
    -OutputFile $GameplayDeltaPath `
    -ChunkSize 65536
if ($LASTEXITCODE -ne 0) {
    throw "Batman gameplay hgdelta build failed."
}

$filesManifest = @{
    virtualFiles = @(
        @{
            id = 'bmgameGameplayPackage'
            path = 'BmGame/CookedPC/BmGame.u'
            mode = 'delta-on-read'
            source = @{
                kind = 'delta-file'
                path = 'assets/deltas/BmGame-subtitle-signal.hgdelta'
                base = @{
                    size = $deltaInfo.BaseSize
                    sha256 = $deltaInfo.BaseSha256
                }
                target = @{
                    size = $deltaInfo.TargetSize
                    sha256 = $deltaInfo.TargetSha256
                }
                chunkSize = $deltaInfo.ChunkSize
            }
        }
    )
}

$filesManifest | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $FilesJsonPath

& dotnet run --project $NativeSubtitleExePatcherProjectPath -c $Configuration -- `
    export-global-text-scale-blob `
    --output $GlobalBlobPath `
    --scale-multiplier 1.5
if ($LASTEXITCODE -ne 0) {
    throw "Global text scale blob export failed."
}

& powershell -ExecutionPolicy Bypass -File (Join-Path $PSScriptRoot 'Test-BatmanPauseRuntimeScaleBuilder.ps1') -BatmanRoot $BatmanRoot
if ($LASTEXITCODE -ne 0) {
    throw "Pause runtime scale verification failed."
}

Write-Output "Rebuilt Batman pack outputs:"
Write-Output "  Gameplay delta:   $GameplayDeltaPath"
Write-Output "  Gameplay target:  $GeneratedGameplayPackagePath"
Write-Output "  Native blob:      $GlobalBlobPath"
