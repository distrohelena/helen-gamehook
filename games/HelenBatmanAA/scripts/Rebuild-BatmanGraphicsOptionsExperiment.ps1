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

function Get-SafeFullPath {
    param([Parameter(Mandatory = $true)][string]$Path)
    if ($Path.StartsWith('\\?\', [StringComparison]::Ordinal) -or $Path.StartsWith('\\.\', [StringComparison]::Ordinal) -or $Path.StartsWith('\??\', [StringComparison]::Ordinal)) {
        throw "Device namespace paths are not allowed for graphics-shell cleanup: $Path"
    }
    return [IO.Path]::GetFullPath($Path)
}

function Test-StrictDescendantPath {
    param([Parameter(Mandatory = $true)][string]$Path, [Parameter(Mandatory = $true)][string]$Root)
    $fullPath = Get-SafeFullPath $Path
    $fullRoot = (Get-SafeFullPath $Root).TrimEnd('\') + '\'
    return $fullPath.StartsWith($fullRoot, [StringComparison]::OrdinalIgnoreCase)
}

function Assert-SafeMutationTarget {
    <#
    Resolve and validate a path before cleanup or movement. Temporary paths must
    be strict descendants of the system temp directory; live paths are limited to
    the exact checked-in pack root or generated target. Existing reparse points
    and device namespaces are rejected so a typo cannot redirect recursive work.
    #>
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [string[]]$AllowedDescendantRoots = @(),
        [string[]]$AllowedExactPaths = @()
    )
    $fullPath = Get-SafeFullPath $Path
    $isAllowed = $false
    foreach ($root in $AllowedDescendantRoots) {
        if (Test-StrictDescendantPath -Path $fullPath -Root $root) { $isAllowed = $true; break }
    }
    if (-not $isAllowed) {
        foreach ($exact in $AllowedExactPaths) {
            if ([String]::Equals($fullPath, (Get-SafeFullPath $exact), [StringComparison]::OrdinalIgnoreCase)) { $isAllowed = $true; break }
        }
    }
    if (-not $isAllowed) { throw "Refusing unsafe graphics-shell mutation target: $fullPath" }

    $current = $fullPath
    while (-not [string]::IsNullOrWhiteSpace($current)) {
        if (Test-Path -LiteralPath $current) {
            $item = Get-Item -LiteralPath $current -Force
            if (($item.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) { throw "Reparse-point cleanup target is not allowed: $current" }
        }
        $parent = Split-Path -Parent $current
        if ([string]::IsNullOrWhiteSpace($parent) -or [String]::Equals($parent, $current, [StringComparison]::OrdinalIgnoreCase)) { break }
        $current = $parent
    }
    if (Test-Path -LiteralPath $fullPath -PathType Container) {
        foreach ($child in @(Get-ChildItem -LiteralPath $fullPath -Recurse -Force)) {
            if (($child.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) { throw "Reparse-point descendant is not allowed: $($child.FullName)" }
        }
    }
    return $fullPath
}

function Remove-SafeMutationTarget {
    <# Remove one validated file or directory using LiteralPath only. #>
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [string[]]$AllowedDescendantRoots = @(),
        [string[]]$AllowedExactPaths = @()
    )
    $fullPath = Assert-SafeMutationTarget -Path $Path -AllowedDescendantRoots $AllowedDescendantRoots -AllowedExactPaths $AllowedExactPaths
    if (Test-Path -LiteralPath $fullPath) { Remove-Item -LiteralPath $fullPath -Recurse -Force }
}

function Move-SafeMutationTarget {
    <# Move one validated path using LiteralPath only; both source and destination are constrained. #>
    param(
        [Parameter(Mandatory = $true)][string]$Source,
        [Parameter(Mandatory = $true)][string]$Destination,
        [string[]]$SourceDescendantRoots = @(),
        [string[]]$SourceExactPaths = @(),
        [string[]]$DestinationDescendantRoots = @(),
        [string[]]$DestinationExactPaths = @()
    )
    $sourcePath = Assert-SafeMutationTarget -Path $Source -AllowedDescendantRoots $SourceDescendantRoots -AllowedExactPaths $SourceExactPaths
    $destinationPath = Assert-SafeMutationTarget -Path $Destination -AllowedDescendantRoots $DestinationDescendantRoots -AllowedExactPaths $DestinationExactPaths
    Move-Item -LiteralPath $sourcePath -Destination $destinationPath -Force
}

function Restore-AtomicRebuild {
    <# Restore both live artifacts from the activation backup, tolerating absent originals. #>
    param(
        [Parameter(Mandatory = $true)][string]$LivePackRoot,
        [Parameter(Mandatory = $true)][string]$LiveTargetPath,
        [Parameter(Mandatory = $true)][string]$PackBackupRoot,
        [Parameter(Mandatory = $true)][string]$TargetBackupPath,
        [Parameter(Mandatory = $true)][string]$ActivationTempRoot,
        [bool]$PackWasBackedUp = $false,
        [bool]$TargetWasBackedUp = $false,
        [bool]$PackWasInstalled = $false,
        [bool]$TargetWasInstalled = $false
    )
    $packBackupExists = Test-Path -LiteralPath $PackBackupRoot
    $targetBackupExists = Test-Path -LiteralPath $TargetBackupPath
    if ($PackWasInstalled -or $PackWasBackedUp -or $packBackupExists) {
        if (Test-Path -LiteralPath $LivePackRoot) { Remove-SafeMutationTarget -Path $LivePackRoot -AllowedExactPaths @($LivePackRoot) }
    }
    if (($PackWasBackedUp -or $packBackupExists) -and $packBackupExists) {
        Move-SafeMutationTarget -Source $PackBackupRoot -Destination $LivePackRoot -SourceDescendantRoots @($ActivationTempRoot) -DestinationExactPaths @($LivePackRoot)
    }
    if ($TargetWasInstalled -or $TargetWasBackedUp -or $targetBackupExists) {
        if (Test-Path -LiteralPath $LiveTargetPath) { Remove-SafeMutationTarget -Path $LiveTargetPath -AllowedExactPaths @($LiveTargetPath) }
    }
    if (($TargetWasBackedUp -or $targetBackupExists) -and $targetBackupExists) {
        Move-SafeMutationTarget -Source $TargetBackupPath -Destination $LiveTargetPath -SourceDescendantRoots @($ActivationTempRoot) -DestinationExactPaths @($LiveTargetPath)
    }
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
$buildHgdeltaPath = Join-Path $PSScriptRoot 'Build-Hgdelta.ps1'
$buildMatchPath = Join-Path $PSScriptRoot 'Get-BatmanSteamBuildMatch.ps1'
$packageVerifierPath = Join-Path $PSScriptRoot 'Test-BatmanGraphicsOptionsPackage.ps1'
foreach ($path in @($builderProjectPath, $patcherProjectPath, $ffdecPath, $buildHgdeltaPath, $buildMatchPath, $packageVerifierPath)) { if (-not (Test-Path -LiteralPath $path)) { throw "Graphics shell build input was not found: $path" } }

$stableExperimentRoot = Join-Path $BuilderRoot 'generated\graphics-options-experiment'
$packRoot = Join-Path $BatmanRoot 'helengamehook\packs\batman-aa-graphics-options'
$stableTargetPath = Join-Path $stableExperimentRoot 'Frontend-graphics-options.umap'

$systemTempRoot = [IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd('\')
$tempRoot = Join-Path $systemTempRoot ('HelenBatmanGraphicsShell-' + [Guid]::NewGuid().ToString('N'))
$prototypeOutputRoot = Join-Path $tempRoot 'prototype'
$prototypeGfxPath = Join-Path $prototypeOutputRoot 'MainV2-graphics-options.gfx'
$patchManifestPath = Join-Path $tempRoot 'MainV2-graphics-options.manifest.json'
$tempTargetPath = Join-Path $tempRoot 'Frontend-graphics-options.umap'
$stagedTargetPath = Join-Path $tempRoot 'target\Frontend-graphics-options.umap'
$stagedPackRoot = Join-Path $tempRoot 'pack\batman-aa-graphics-options'
$stagedPackBuildRoot = Join-Path $stagedPackRoot 'builds\steam-goty-1.0'
$stagedDeltaPath = Join-Path $stagedPackBuildRoot 'assets\deltas\Frontend-graphics-options.hgdelta'
$packBackupRoot = Join-Path $tempRoot 'activation-backup\pack\batman-aa-graphics-options'
$targetBackupPath = Join-Path $tempRoot 'activation-backup\target\Frontend-graphics-options.umap'
$activationTempRoot = Join-Path $tempRoot 'activation-backup'
$stagedPackJsonPath = Join-Path $stagedPackRoot 'pack.json'
$stagedBuildJsonPath = Join-Path $stagedPackBuildRoot 'build.json'
$stagedBindingsJsonPath = Join-Path $stagedPackBuildRoot 'bindings.json'
$stagedCommandsJsonPath = Join-Path $stagedPackBuildRoot 'commands.json'
$stagedFilesJsonPath = Join-Path $stagedPackBuildRoot 'files.json'
$stagedPackCreated = $false
$stagedTargetCreated = $false
$packBackedUp = $false
$targetBackedUp = $false
$packInstalled = $false
$targetInstalled = $false
$activationStarted = $false
$activationCommitted = $false

New-Item -ItemType Directory -Force -Path $tempRoot | Out-Null
Assert-SafeMutationTarget -Path $tempRoot -AllowedDescendantRoots @($systemTempRoot) | Out-Null
try {
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
    New-Item -ItemType Directory -Force -Path $stagedPackBuildRoot | Out-Null
    $deltaInfo = & $buildHgdeltaPath -BaseFile $retailBasePath -TargetFile $tempTargetPath -OutputFile $stagedDeltaPath -ChunkSize 65536
    if ($LASTEXITCODE -ne 0 -or -not (Test-Path -LiteralPath $stagedDeltaPath)) { throw 'Building the graphics shell hgdelta failed.' }
    # Keep the verified current-run target beside the staged pack; this is the only target that may become stable.
    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $stagedTargetPath) | Out-Null
    Copy-Item -LiteralPath $tempTargetPath -Destination $stagedTargetPath -Force
    $stagedTargetCreated = $true

    $buildMatch = & $buildMatchPath
    if ($buildMatch.BuildId -cne 'steam-goty-1.0' -or $buildMatch.Executable -cne 'ShippingPC-BmGame.exe' -or [int64]$buildMatch.FileSize -ne 38758728 -or $buildMatch.Sha256 -cne '4DAC1F5E2AC6710B7378FDCE74601F616F4753E3756CB5FDA63C7519CC2EB028') { throw 'Steam build match is not the verified retail executable identity.' }
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
    Write-Utf8TextFile -Path $stagedFilesJsonPath -Contents ($files | ConvertTo-Json -Depth 7)
    Write-Utf8TextFile -Path $stagedPackJsonPath -Contents ($pack | ConvertTo-Json -Depth 5)
    Write-Utf8TextFile -Path $stagedBuildJsonPath -Contents ($build | ConvertTo-Json -Depth 5)
    Write-Utf8TextFile -Path $stagedBindingsJsonPath -Contents (([ordered]@{ bindings = @() }) | ConvertTo-Json -Depth 3)
    Write-Utf8TextFile -Path $stagedCommandsJsonPath -Contents (([ordered]@{ commands = @() }) | ConvertTo-Json -Depth 3)
    $stagedPackCreated = $true

    # The verifier reopens/export-checks the staged target and reconstructs its delta before activation.
    & $packageVerifierPath -BatmanRoot $BatmanRoot -BuilderRoot $BuilderRoot -Configuration $Configuration -PackRootOverride $stagedPackRoot -TargetPathOverride $stagedTargetPath
    if (-not $?) { throw 'Staged graphics-options pack verification failed.' }

    $stagedPackFullPath = Get-SafeFullPath $stagedPackRoot
    $stagedTargetFullPath = Get-SafeFullPath $stagedTargetPath
    $packFullPath = Get-SafeFullPath $packRoot
    $targetFullPath = Get-SafeFullPath $stableTargetPath
    if ([String]::Equals($stagedPackFullPath, $packFullPath, [StringComparison]::OrdinalIgnoreCase) -or [String]::Equals($stagedTargetFullPath, $targetFullPath, [StringComparison]::OrdinalIgnoreCase)) { throw 'Staged graphics-shell paths must differ from live paths.' }
    Assert-SafeMutationTarget -Path $stableExperimentRoot -AllowedExactPaths @($stableExperimentRoot) | Out-Null
    New-Item -ItemType Directory -Force -Path $stableExperimentRoot | Out-Null
    New-Item -ItemType Directory -Force -Path (Join-Path $activationTempRoot 'pack') | Out-Null
    New-Item -ItemType Directory -Force -Path (Join-Path $activationTempRoot 'target') | Out-Null
    $activationStarted = $true
    if (Test-Path -LiteralPath $packRoot) {
        Move-SafeMutationTarget -Source $packRoot -Destination $packBackupRoot -SourceExactPaths @($packRoot) -DestinationDescendantRoots @($tempRoot)
        $packBackedUp = $true
    }
    if (Test-Path -LiteralPath $stableTargetPath) {
        Move-SafeMutationTarget -Source $stableTargetPath -Destination $targetBackupPath -SourceExactPaths @($stableTargetPath) -DestinationDescendantRoots @($tempRoot)
        $targetBackedUp = $true
    }
    Move-SafeMutationTarget -Source $stagedPackRoot -Destination $packRoot -SourceDescendantRoots @($tempRoot) -DestinationExactPaths @($packRoot)
    $stagedPackCreated = $false
    $packInstalled = $true
    Move-SafeMutationTarget -Source $stagedTargetPath -Destination $stableTargetPath -SourceDescendantRoots @($tempRoot) -DestinationExactPaths @($stableTargetPath)
    $stagedTargetCreated = $false
    $targetInstalled = $true

    # Verify the activated paths before deleting their explicit rollback backups.
    & $packageVerifierPath -BatmanRoot $BatmanRoot -BuilderRoot $BuilderRoot -Configuration $Configuration
    if (-not $?) { throw 'Post-activation graphics-options pack verification failed.' }
    $activationCommitted = $true
    if (Test-Path -LiteralPath $packBackupRoot) { Remove-SafeMutationTarget -Path $packBackupRoot -AllowedDescendantRoots @($tempRoot) }
    if (Test-Path -LiteralPath $targetBackupPath) { Remove-SafeMutationTarget -Path $targetBackupPath -AllowedDescendantRoots @($tempRoot) }
}
catch {
    $failure = $_
    try {
        if ($activationStarted -and -not $activationCommitted) {
            Restore-AtomicRebuild -LivePackRoot $packRoot -LiveTargetPath $stableTargetPath -PackBackupRoot $packBackupRoot -TargetBackupPath $targetBackupPath -ActivationTempRoot $tempRoot -PackWasBackedUp $packBackedUp -TargetWasBackedUp $targetBackedUp -PackWasInstalled $packInstalled -TargetWasInstalled $targetInstalled
        }
    }
    catch {
        throw "Atomic graphics-shell rollback failed after '$($failure.Exception.Message)': $($_.Exception.Message)"
    }
    throw $failure
}
finally {
    if (Test-Path -LiteralPath $tempRoot) {
        Remove-SafeMutationTarget -Path $tempRoot -AllowedDescendantRoots @($systemTempRoot)
    }
}

$finalInfo = Get-Item -LiteralPath $stableTargetPath
$finalHash = (Get-FileHash -LiteralPath $stableTargetPath -Algorithm SHA256).Hash
$finalDeltaPath = Join-Path $packRoot 'builds\steam-goty-1.0\assets\deltas\Frontend-graphics-options.hgdelta'
Write-Output 'Rebuilt Batman graphics-options shell outputs atomically:'
Write-Output "  Retail base:     $retailBasePath ($($retailInfo.Length) bytes, $retailHash)"
Write-Output "  Frontend target: $stableTargetPath ($($finalInfo.Length) bytes, $finalHash)"
Write-Output "  Frontend delta:  $finalDeltaPath ($((Get-Item -LiteralPath $finalDeltaPath).Length) bytes, $((Get-FileHash -LiteralPath $finalDeltaPath -Algorithm SHA256).Hash))"
