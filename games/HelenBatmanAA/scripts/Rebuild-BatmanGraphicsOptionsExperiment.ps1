param(
    [string]$Configuration = 'Release',
    [string]$BatmanRoot,
    [string]$BuilderRoot
)

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'BatmanBuilderWorkspaceHelpers.ps1')

function Write-Utf8TextFile {
    param([string]$Path, [string]$Contents)
    $directory = Split-Path -Parent $Path
    if (-not [string]::IsNullOrWhiteSpace($directory)) { New-Item -ItemType Directory -Force -Path $directory | Out-Null }
    [IO.File]::WriteAllText($Path, $Contents, [Text.UTF8Encoding]::new($false))
}

function Invoke-RequiredProcess {
    param([string]$FilePath, [string[]]$Arguments, [string]$FailureMessage)
    & $FilePath @Arguments
    if ($LASTEXITCODE -ne 0) { throw $FailureMessage }
}

if ([string]::IsNullOrWhiteSpace($BatmanRoot)) { $BatmanRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path } else { $BatmanRoot = (Resolve-Path $BatmanRoot).Path }
if ([string]::IsNullOrWhiteSpace($BuilderRoot)) { $BuilderRoot = Join-Path $BatmanRoot 'builder' } elseif ([IO.Path]::IsPathRooted($BuilderRoot)) { $BuilderRoot = [IO.Path]::GetFullPath($BuilderRoot) } elseif (Test-Path -LiteralPath $BuilderRoot) { $BuilderRoot = (Resolve-Path $BuilderRoot).Path } else { $BuilderRoot = [IO.Path]::GetFullPath((Join-Path $BatmanRoot $BuilderRoot)) }

$retailBasePath = Join-Path $BuilderRoot 'extracted\frontend-retail\Frontend.umap'
$expectedRetailSize = 2988548
$expectedRetailHash = '271916B888F83374122AF0FCCC5C685804F4C8286A92A772CD71E4F48A00F2CC'
if (-not (Test-Path -LiteralPath $retailBasePath)) { throw "Verified retail Frontend.umap was not found: $retailBasePath" }
$retailInfo = Get-Item -LiteralPath $retailBasePath
$retailHash = (Get-FileHash -LiteralPath $retailBasePath -Algorithm SHA256).Hash
if ($retailInfo.Length -ne $expectedRetailSize -or $retailHash -cne $expectedRetailHash) { throw "Refusing to rebuild against an unverified retail Frontend.umap. Expected $expectedRetailSize bytes/$expectedRetailHash, found $($retailInfo.Length) bytes/$retailHash." }

$builderProjectPath = Join-Path $BuilderRoot 'tools\NativeSubtitleExePatcher\SubtitleSizeModBuilder\SubtitleSizeModBuilder.csproj'
$patcherProjectPath = Join-Path $BuilderRoot 'tools\NativeSubtitleExePatcher\BmGameGfxPatcher\BmGameGfxPatcher.csproj'
$ffdecPath = Join-Path $BuilderRoot 'extracted\ffdec\ffdec-cli.exe'
$frontendXmlPath = Join-Path $BuilderRoot 'extracted\frontend\mainv2\frontend-mainv2.xml'
$frontendGfxPath = Join-Path $BuilderRoot 'extracted\frontend\mainv2\frontend-mainv2.gfx'
$frontendScriptsPath = Join-Path $BuilderRoot 'extracted\frontend\mainv2\frontend-mainv2-export\scripts'
$buildHgdeltaPath = Join-Path $PSScriptRoot 'Build-Hgdelta.ps1'
$buildMatchPath = Join-Path $PSScriptRoot 'Get-BatmanSteamBuildMatch.ps1'
foreach ($path in @($builderProjectPath, $patcherProjectPath, $ffdecPath, $frontendXmlPath, $frontendGfxPath, $frontendScriptsPath, $buildHgdeltaPath, $buildMatchPath)) { if (-not (Test-Path -LiteralPath $path)) { throw "Graphics shell build input was not found: $path" } }

$stableExperimentRoot = Join-Path $BuilderRoot 'generated\graphics-options-experiment'
$packRoot = Join-Path $BatmanRoot 'helengamehook\packs\batman-aa-graphics-options'
$packBuildRoot = Join-Path $packRoot 'builds\steam-goty-1.0'
$stableTargetPath = Join-Path $stableExperimentRoot 'Frontend-graphics-options.umap'
$filesJsonPath = Join-Path $packBuildRoot 'files.json'
$deltaPath = Join-Path $packBuildRoot 'assets\deltas\Frontend-graphics-options.hgdelta'
$packJsonPath = Join-Path $packRoot 'pack.json'
$buildJsonPath = Join-Path $packBuildRoot 'build.json'
$bindingsJsonPath = Join-Path $packBuildRoot 'bindings.json'
$commandsJsonPath = Join-Path $packBuildRoot 'commands.json'

$tempRoot = Join-Path ([IO.Path]::GetTempPath()) ('HelenBatmanGraphicsShell-' + [Guid]::NewGuid().ToString('N'))
$prototypeOutputRoot = Join-Path $tempRoot 'prototype'
$prototypeGfxPath = Join-Path $prototypeOutputRoot 'MainV2-graphics-options.gfx'
$patchManifestPath = Join-Path $tempRoot 'MainV2-graphics-options.manifest.json'
$tempTargetPath = Join-Path $tempRoot 'Frontend-graphics-options.umap'
New-Item -ItemType Directory -Force -Path $tempRoot | Out-Null

try {
    if (Test-Path -LiteralPath $stableExperimentRoot) { Remove-Item -LiteralPath $stableExperimentRoot -Recurse -Force }
    if (Test-Path -LiteralPath $packBuildRoot) { Remove-Item -LiteralPath $packBuildRoot -Recurse -Force }
    New-Item -ItemType Directory -Force -Path $packBuildRoot | Out-Null

    Invoke-RequiredProcess -FilePath 'dotnet' -Arguments @('build', $builderProjectPath, '-c', $Configuration) -FailureMessage 'SubtitleSizeModBuilder build failed.'
    Invoke-RequiredProcess -FilePath 'dotnet' -Arguments @('build', $patcherProjectPath, '-c', $Configuration) -FailureMessage 'BmGameGfxPatcher build failed.'
    Invoke-RequiredProcess -FilePath 'dotnet' -Arguments @('run', '--project', $builderProjectPath, '-c', $Configuration, '--', 'build-main-menu-graphics-shell', '--root', $BuilderRoot, '--output-dir', $prototypeOutputRoot, '--ffdec', $ffdecPath) -FailureMessage 'build-main-menu-graphics-shell failed.'
    if (-not (Test-Path -LiteralPath $prototypeGfxPath)) { throw "Shell prototype GFX was not generated: $prototypeGfxPath" }

    $manifest = [ordered]@{
        name = 'MainMenu MainV2 graphics-options shell patch'
        patches = @([ordered]@{
            owner = 'MainMenu'
            exportName = 'MainV2'
            exportType = 'GFxMovieInfo'
            replacementPath = $prototypeGfxPath
            payloadMagic = 'GFX'
        })
    }
    Write-Utf8TextFile -Path $patchManifestPath -Contents ($manifest | ConvertTo-Json -Depth 5)
    Invoke-RequiredProcess -FilePath 'dotnet' -Arguments @('run', '--project', $patcherProjectPath, '-c', $Configuration, '--', 'patch', '--package', $retailBasePath, '--manifest', $patchManifestPath, '--output', $tempTargetPath) -FailureMessage 'Patching the verified retail Frontend.umap failed.'
    if (-not (Test-Path -LiteralPath $tempTargetPath)) { throw "Current-run shell target was not generated: $tempTargetPath" }

    $targetStorage = Get-UnrealPackageStorageInfo -Path $tempTargetPath
    if ($targetStorage.CompressionChunkCount -le 0) { throw 'Current-run shell target is not chunk-compressed.' }
    $deltaInfo = & $buildHgdeltaPath -BaseFile $retailBasePath -TargetFile $tempTargetPath -OutputFile $deltaPath -ChunkSize 65536
    if ($LASTEXITCODE -ne 0 -or -not (Test-Path -LiteralPath $deltaPath)) { throw 'Building the graphics shell hgdelta failed.' }

    New-Item -ItemType Directory -Force -Path $stableExperimentRoot | Out-Null
    Copy-Item -LiteralPath $tempTargetPath -Destination $stableTargetPath -Force
    $buildMatch = & $buildMatchPath
    $files = [ordered]@{
        virtualFiles = @([ordered]@{
            id = 'frontendGraphicsOptionsPackage'
            path = 'BmGame/CookedPC/Maps/Frontend/Frontend.umap'
            mode = 'delta-on-read'
            source = [ordered]@{
                kind = 'delta-file'
                path = 'assets/deltas/Frontend-graphics-options.hgdelta'
                base = [ordered]@{ size = $deltaInfo.BaseSize; sha256 = $deltaInfo.BaseSha256 }
                target = [ordered]@{ size = $deltaInfo.TargetSize; sha256 = $deltaInfo.TargetSha256 }
                chunkSize = $deltaInfo.ChunkSize
            }
        })
    }
    $pack = [ordered]@{
        schemaVersion = 1
        id = 'batman-aa-graphics-options'
        name = 'Batman Graphics Options Shell'
        targets = @([ordered]@{ gameId = 'batman-arkham-asylum'; executables = @($buildMatch.Executable) })
        builds = @($buildMatch.BuildId)
    }
    $build = [ordered]@{
        id = $buildMatch.BuildId
        executable = $buildMatch.Executable
        match = [ordered]@{ fileSize = $buildMatch.FileSize; sha256 = $buildMatch.Sha256 }
    }
    $bindings = [ordered]@{ bindings = @() }
    $commands = [ordered]@{ commands = @() }
    Write-Utf8TextFile -Path $filesJsonPath -Contents ($files | ConvertTo-Json -Depth 7)
    Write-Utf8TextFile -Path $packJsonPath -Contents ($pack | ConvertTo-Json -Depth 5)
    Write-Utf8TextFile -Path $buildJsonPath -Contents ($build | ConvertTo-Json -Depth 5)
    Write-Utf8TextFile -Path $bindingsJsonPath -Contents ($bindings | ConvertTo-Json -Depth 3)
    Write-Utf8TextFile -Path $commandsJsonPath -Contents ($commands | ConvertTo-Json -Depth 3)

    Write-Output 'Rebuilt Batman graphics-options shell outputs:'
    Write-Output "  Retail base:     $retailBasePath ($($retailInfo.Length) bytes, $retailHash)"
    Write-Output "  Frontend target: $stableTargetPath ($($deltaInfo.TargetSize) bytes, $($deltaInfo.TargetSha256))"
    Write-Output "  Frontend delta:  $deltaPath ($((Get-Item -LiteralPath $deltaPath).Length) bytes)"
}
finally {
    if (Test-Path -LiteralPath $tempRoot) { Remove-Item -LiteralPath $tempRoot -Recurse -Force }
}
