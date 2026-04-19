param(
    [string]$Configuration = 'Release',
    [string]$BatmanRoot,
    [string]$BuilderRoot
)

$ErrorActionPreference = 'Stop'
$HelperScriptPath = Join-Path $PSScriptRoot 'BatmanBuilderWorkspaceHelpers.ps1'
. $HelperScriptPath

function Write-Utf8TextFile {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path,
        [Parameter(Mandatory = $true)]
        [string]$Contents
    )

    $Directory = Split-Path -Parent $Path
    if (-not [string]::IsNullOrWhiteSpace($Directory)) {
        New-Item -ItemType Directory -Force -Path $Directory | Out-Null
    }

    [System.IO.File]::WriteAllText($Path, $Contents, (New-Object System.Text.UTF8Encoding($false)))
}

if ([string]::IsNullOrWhiteSpace($BatmanRoot)) {
    $BatmanRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
} else {
    $BatmanRoot = (Resolve-Path $BatmanRoot).Path
}

if ([string]::IsNullOrWhiteSpace($BuilderRoot)) {
    $BuilderRoot = Join-Path $BatmanRoot 'builder'
}

$BuilderRoot = Resolve-OptionalBuilderRoot -BatmanRootPath $BatmanRoot -BuilderRootPath $BuilderRoot
$SourceBuilderRoot = Join-Path $BatmanRoot 'builder'
$GeneratedRoot = Join-Path $BuilderRoot 'generated'
$ExperimentRoot = Join-Path $GeneratedRoot 'graphics-options-experiment'
$PrototypeBuildRoot = Join-Path $ExperimentRoot 'prototype'
$PrototypeGfxPath = Join-Path $PrototypeBuildRoot 'MainV2-graphics-options.gfx'
$GeneratedFrontendPackagePath = Join-Path $ExperimentRoot 'Frontend-graphics-options.umap'
$PackSourceRoot = Join-Path $BatmanRoot 'helengamehook\packs\batman-aa-graphics-options'
$PackBuildRoot = Join-Path $PackSourceRoot 'builds\steam-goty-1.0'
$BindingsJsonPath = Join-Path $PackBuildRoot 'bindings.json'
$CommandsJsonPath = Join-Path $PackBuildRoot 'commands.json'
$DeltaPath = Join-Path $PackBuildRoot 'assets\deltas\Frontend-graphics-options.hgdelta'
$FilesJsonPath = Join-Path $PackBuildRoot 'files.json'
$PackJsonPath = Join-Path $PackSourceRoot 'pack.json'
$BuildJsonPath = Join-Path $PackBuildRoot 'build.json'
$BuildMatchScriptPath = Join-Path $PSScriptRoot 'Get-BatmanSteamBuildMatch.ps1'
$FfdecPath = Join-Path $SourceBuilderRoot 'extracted\ffdec\ffdec-cli.exe'
$SubtitleSizeModBuilderProjectPath = Join-Path $SourceBuilderRoot 'tools\NativeSubtitleExePatcher\SubtitleSizeModBuilder\SubtitleSizeModBuilder.csproj'
$BmGameGfxPatcherProjectPath = Join-Path $SourceBuilderRoot 'tools\NativeSubtitleExePatcher\BmGameGfxPatcher\BmGameGfxPatcher.csproj'
$FrontendBasePackagePath = Join-Path $SourceBuilderRoot 'extracted\frontend\frontend-umap-unpacked\Frontend.umap'
$RetailFrontendBasePackagePath = Join-Path $BuilderRoot 'extracted\frontend-retail\Frontend.umap'
$BuildHgdeltaScriptPath = Join-Path $PSScriptRoot 'Build-Hgdelta.ps1'

foreach ($RequiredPath in @(
    $FfdecPath,
    $SubtitleSizeModBuilderProjectPath,
    $BmGameGfxPatcherProjectPath,
    $BuildMatchScriptPath,
    $FrontendBasePackagePath,
    $RetailFrontendBasePackagePath,
    (Join-Path $SourceBuilderRoot 'extracted\frontend\mainv2\frontend-mainv2.xml'),
    (Join-Path $SourceBuilderRoot 'extracted\frontend\mainv2\frontend-mainv2.gfx'),
    (Join-Path $SourceBuilderRoot 'extracted\frontend\mainv2\frontend-mainv2-export\scripts')
)) {
    if (-not (Test-Path -LiteralPath $RequiredPath)) {
        throw "Batman graphics-options build input was not found: $RequiredPath"
    }
}

Assert-UnrealPackageIsUnpacked -Path $FrontendBasePackagePath -Context 'Rebuild-BatmanGraphicsOptionsExperiment frontend base' | Out-Null

if (Test-Path -LiteralPath $ExperimentRoot) {
    Remove-Item -LiteralPath $ExperimentRoot -Recurse -Force
}

if (Test-Path -LiteralPath $PackBuildRoot) {
    Remove-Item -LiteralPath $PackBuildRoot -Recurse -Force
}

New-Item -ItemType Directory -Force -Path $PrototypeBuildRoot | Out-Null
New-Item -ItemType Directory -Force -Path $PackBuildRoot | Out-Null

& dotnet build $SubtitleSizeModBuilderProjectPath -c $Configuration
if ($LASTEXITCODE -ne 0) {
    throw 'dotnet build failed for SubtitleSizeModBuilder.csproj'
}

& dotnet build $BmGameGfxPatcherProjectPath -c $Configuration
if ($LASTEXITCODE -ne 0) {
    throw 'dotnet build failed for BmGameGfxPatcher.csproj'
}

& dotnet run --project $SubtitleSizeModBuilderProjectPath -c $Configuration -- `
    build-main-menu-graphics `
    --root $BuilderRoot `
    --output-dir $PrototypeBuildRoot `
    --ffdec $FfdecPath
if ($LASTEXITCODE -ne 0) {
    throw 'build-main-menu-graphics failed.'
}

& dotnet run --project $BmGameGfxPatcherProjectPath -c $Configuration -- `
    patch-mainv2-graphics-options `
    --package $FrontendBasePackagePath `
    --output $GeneratedFrontendPackagePath `
    --prototype-gfx $PrototypeGfxPath
if ($LASTEXITCODE -ne 0) {
    throw 'patch-mainv2-graphics-options failed.'
}

$DeltaInfo = & $BuildHgdeltaScriptPath `
    -BaseFile $RetailFrontendBasePackagePath `
    -TargetFile $GeneratedFrontendPackagePath `
    -OutputFile $DeltaPath `
    -ChunkSize 65536
if ($LASTEXITCODE -ne 0) {
    throw 'Batman graphics-options hgdelta build failed.'
}

$FilesManifest = @{
    virtualFiles = @(
        @{
            id = 'frontendGraphicsOptionsPackage'
            path = 'BmGame/CookedPC/Maps/Frontend/Frontend.umap'
            mode = 'delta-on-read'
            source = @{
                kind = 'delta-file'
                path = 'assets/deltas/Frontend-graphics-options.hgdelta'
                base = @{
                    size = $DeltaInfo.BaseSize
                    sha256 = $DeltaInfo.BaseSha256
                }
                target = @{
                    size = $DeltaInfo.TargetSize
                    sha256 = $DeltaInfo.TargetSha256
                }
                chunkSize = $DeltaInfo.ChunkSize
            }
        }
    )
}

Write-Utf8TextFile -Path $FilesJsonPath -Contents ($FilesManifest | ConvertTo-Json -Depth 6)

$BuildMatch = & $BuildMatchScriptPath
$ConfigEntries = @(
    [ordered]@{ key = 'fullscreen'; type = 'int'; defaultValue = 0 },
    [ordered]@{ key = 'resolutionWidth'; type = 'int'; defaultValue = 0 },
    [ordered]@{ key = 'resolutionHeight'; type = 'int'; defaultValue = 0 },
    [ordered]@{ key = 'vsync'; type = 'int'; defaultValue = 0 },
    [ordered]@{ key = 'msaa'; type = 'int'; defaultValue = 0 },
    [ordered]@{ key = 'detailLevel'; type = 'int'; defaultValue = 0 },
    [ordered]@{ key = 'bloom'; type = 'int'; defaultValue = 0 },
    [ordered]@{ key = 'dynamicShadows'; type = 'int'; defaultValue = 0 },
    [ordered]@{ key = 'motionBlur'; type = 'int'; defaultValue = 0 },
    [ordered]@{ key = 'distortion'; type = 'int'; defaultValue = 0 },
    [ordered]@{ key = 'fogVolumes'; type = 'int'; defaultValue = 0 },
    [ordered]@{ key = 'sphericalHarmonicLighting'; type = 'int'; defaultValue = 0 },
    [ordered]@{ key = 'ambientOcclusion'; type = 'int'; defaultValue = 0 },
    [ordered]@{ key = 'physx'; type = 'int'; defaultValue = 0 },
    [ordered]@{ key = 'stereo'; type = 'int'; defaultValue = 0 }
)

$PackJsonObject = [ordered]@{
    schemaVersion = 1
    id = 'batman-aa-graphics-options'
    name = 'Batman Graphics Options Experiment'
    targets = @(
        [ordered]@{
            gameId = 'batman-arkham-asylum'
            executables = @($BuildMatch.Executable)
        }
    )
    config = $ConfigEntries
    builds = @($BuildMatch.BuildId)
}

$BuildJsonObject = [ordered]@{
    id = $BuildMatch.BuildId
    executable = $BuildMatch.Executable
    match = [ordered]@{
        fileSize = $BuildMatch.FileSize
        sha256 = $BuildMatch.Sha256
    }
    startupCommands = @()
}

$BindingsObject = [ordered]@{
    bindings = @()
}

$CommandsObject = [ordered]@{
    commands = @()
}

Write-Utf8TextFile -Path $PackJsonPath -Contents ($PackJsonObject | ConvertTo-Json -Depth 5)
Write-Utf8TextFile -Path $BuildJsonPath -Contents ($BuildJsonObject | ConvertTo-Json -Depth 5)
Write-Utf8TextFile -Path $BindingsJsonPath -Contents ($BindingsObject | ConvertTo-Json -Depth 5)
Write-Utf8TextFile -Path $CommandsJsonPath -Contents ($CommandsObject | ConvertTo-Json -Depth 6)

Write-Output "Rebuilt Batman graphics-options experiment outputs:"
Write-Output "  Prototype gfx:   $PrototypeGfxPath"
Write-Output "  Frontend target: $GeneratedFrontendPackagePath"
Write-Output "  Frontend delta:  $DeltaPath"
Write-Output "  Manifest:        $FilesJsonPath"
