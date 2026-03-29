param(
    [string]$Configuration = 'Release',
    [string]$BatmanRoot,
    [string]$BuilderRoot
)

$ErrorActionPreference = 'Stop'

if ([string]::IsNullOrWhiteSpace($BatmanRoot)) {
    $BatmanRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
} else {
    $BatmanRoot = (Resolve-Path $BatmanRoot).Path
}
if ([string]::IsNullOrWhiteSpace($BuilderRoot)) {
    $BuilderRoot = Join-Path $BatmanRoot 'builder'
} elseif (-not [System.IO.Path]::IsPathRooted($BuilderRoot)) {
    $BuilderRoot = Join-Path $BatmanRoot $BuilderRoot
}
$BuilderRoot = [System.IO.Path]::GetFullPath($BuilderRoot)
$SourceBuilderRoot = Join-Path $BatmanRoot 'builder'
$ExtractedRoot = Join-Path $BuilderRoot 'extracted'
$GeneratedRoot = Join-Path $BuilderRoot 'generated'
$ToolRoot = Join-Path $SourceBuilderRoot 'tools\NativeSubtitleExePatcher'
$NativeSubtitleExePatcherProjectPath = Join-Path $ToolRoot 'NativeSubtitleExePatcher.csproj'
$SubtitleSizeModBuilderProjectPath = Join-Path $ToolRoot 'SubtitleSizeModBuilder\SubtitleSizeModBuilder.csproj'
$BmGameGfxPatcherProjectPath = Join-Path $ToolRoot 'BmGameGfxPatcher\BmGameGfxPatcher.csproj'
$PackBuildRoot = Join-Path $BatmanRoot 'helengamehook\packs\batman-aa-subtitles\builds\steam-goty-1.0'
$BuildAssetsRoot = Join-Path $GeneratedRoot 'pause-runtime-scale'
$FfdecPath = Join-Path $ExtractedRoot 'ffdec\ffdec-cli.exe'
$BasePackagePath = Join-Path $ExtractedRoot 'bmgame-unpacked\BmGame.u'
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
$PauseAudioLayoutVerifierPath = Join-Path $PSScriptRoot 'Test-BatmanPauseAudioLayout.ps1'
$PrepareBuilderWorkspaceScriptPath = Join-Path $PSScriptRoot 'Prepare-BatmanBuilderWorkspace.ps1'

$builderWorkspacePrerequisites = @(
    $FfdecPath,
    $BasePackagePath,
    (Join-Path $ExtractedRoot 'pause\Pause-extracted.gfx'),
    (Join-Path $ExtractedRoot 'pause\Pause.xml'),
    (Join-Path $ExtractedRoot 'pause\pause-ffdec-export\scripts'),
    (Join-Path $ExtractedRoot 'hud\HUD-extracted.gfx'),
    (Join-Path $ExtractedRoot 'hud\hud-ffdec-scripts\scripts')
)

$missingBuilderWorkspacePaths = @(
    $builderWorkspacePrerequisites |
    Where-Object { -not (Test-Path -LiteralPath $_) }
)

if ($missingBuilderWorkspacePaths.Count -gt 0) {
    $missingBuilderWorkspaceText = ($missingBuilderWorkspacePaths | ForEach-Object { "  - $_" }) -join [Environment]::NewLine
    throw "Batman builder workspace is incomplete. Run `"$PrepareBuilderWorkspaceScriptPath`" with trusted BmGame.u, trusted Startup_INT.upk, and FFDec before rebuilding.$([Environment]::NewLine)Missing paths:$([Environment]::NewLine)$missingBuilderWorkspaceText"
}

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

& powershell -ExecutionPolicy Bypass -File $PauseAudioLayoutVerifierPath -BatmanRoot $BatmanRoot
if ($LASTEXITCODE -ne 0) {
    throw "Pause audio layout verification failed."
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
