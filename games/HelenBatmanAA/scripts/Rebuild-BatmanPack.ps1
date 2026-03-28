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
$ManifestPath = Join-Path $BuildAssetsRoot 'pause-runtime-scale.manifest.jsonc'
$GameplayPackagePath = Join-Path $PackBuildRoot 'assets\packages\BmGame-subtitle-signal.u'
$GlobalBlobPath = Join-Path $PackBuildRoot 'assets\native\batman-global-text-scale.bin'

New-Item -ItemType Directory -Force -Path (Split-Path -Parent $GameplayPackagePath) | Out-Null
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

& dotnet run --project $BmGameGfxPatcherProjectPath -c $Configuration -- `
    patch `
    --package $BasePackagePath `
    --manifest $ManifestPath `
    --output $GameplayPackagePath
if ($LASTEXITCODE -ne 0) {
    throw "BmGame package patch build failed."
}

& dotnet run --project $NativeSubtitleExePatcherProjectPath -c $Configuration -- `
    export-global-text-scale-blob `
    --output $GlobalBlobPath `
    --scale-multiplier 1.5
if ($LASTEXITCODE -ne 0) {
    throw "Global text scale blob export failed."
}

Write-Output "Rebuilt Batman pack outputs:"
Write-Output "  Gameplay package: $GameplayPackagePath"
Write-Output "  Native blob:      $GlobalBlobPath"
