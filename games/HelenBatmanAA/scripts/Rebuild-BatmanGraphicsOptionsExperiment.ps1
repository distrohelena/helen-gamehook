param(
    [string]$Configuration = 'Release',
    [string]$BatmanRoot,
    [string]$BuilderRoot,
    [string]$BatmanUserIniPath,
    [switch]$FunctionsOnly
)

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'BatmanBuilderWorkspaceHelpers.ps1')

function Write-Utf8TextFile {
    param([string]$Path, [string]$Contents)
    $directory = Split-Path -Parent $Path
    if (-not [string]::IsNullOrWhiteSpace($directory)) { New-Item -ItemType Directory -Force -Path $directory | Out-Null }
    [IO.File]::WriteAllText($Path, $Contents, [Text.UTF8Encoding]::new($false))
}

function New-GraphicsCarrierChecks {
    <#
    Return the ordered structural checks for the shared Batman graphics state carrier.
    The offsets and constants deliberately identify the verified retail structure;
    both observers reuse this list so a raw state code controls only emission and
    never changes address resolution or causes observers to scan different shapes.
    #>
    return @(
        [ordered]@{
            comparison = 'equals-constant'
            offset = [int]-16
            expectedValue = [int]50
        },
        [ordered]@{
            comparison = 'equals-constant'
            offset = [int]-12
            expectedValue = [int]100
        },
        [ordered]@{
            comparison = 'equals-constant'
            offset = [int]-8
            expectedValue = [int]100
        },
        [ordered]@{
            comparison = 'equals-constant'
            offset = [int]-4
            expectedValue = [int]100
        },
        [ordered]@{
            comparison = 'equals-constant'
            offset = [int]4
            expectedValue = [int]1
        },
        [ordered]@{
            comparison = 'equals-constant'
            offset = [int]8
            expectedValue = [int]0
        },
        [ordered]@{
            comparison = 'equals-constant'
            offset = [int]12
            expectedValue = [int]1
        },
        [ordered]@{
            comparison = 'equals-value-at-offset'
            offset = [int]16
            compareOffset = [int]0
        },
        [ordered]@{
            comparison = 'equals-constant'
            offset = [int]20
            expectedValue = [int]2
        },
        [ordered]@{
            comparison = 'equals-constant'
            offset = [int]28
            expectedValue = [int]3
        },
        [ordered]@{
            comparison = 'equals-constant'
            offset = [int]32
            expectedValue = [int]3
        }
    )
}

function New-GraphicsCarrierObserver {
    <#
    Build one declarative observer for a graphics state code carried by the shared
    retail structure. Geometry and checks are fixed for collision-free discovery,
    mappings remain observer-specific, and the optional command is omitted entirely
    for the passive VSync observer so the generated schema stays exact.
    #>
    param(
        [Parameter(Mandatory = $true)] [string]$Id,
        [Parameter(Mandatory = $true)] [string]$TargetConfigKey,
        [Parameter(Mandatory = $true)] [object[]]$Mappings,
        [AllowEmptyString()] [string]$Command = ''
    )

    $observer = [ordered]@{
        id = $Id
        scanStartAddress = '0x2B000000'
        scanEndAddress = '0x30000000'
        scanStride = [int]4
        valueOffset = [int]0
        pollIntervalMs = [int]50
        targetConfigKey = $TargetConfigKey
        addressMatchValues = @([int]4101, [int]4102, [int]4103, [int]4104, [int]4105, [int]4106, [int]4210, [int]4211, [int]4990, [int]4991)
        checks = @(New-GraphicsCarrierChecks)
        mappings = $Mappings
    }
    if (-not [string]::IsNullOrWhiteSpace($Command)) { $observer.command = $Command }
    return $observer
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

function Ensure-SafeDirectoryPath {
    <# Create a missing destination directory and return only the directory paths created by this invocation. #>
    param([Parameter(Mandatory = $true)][string]$Path)
    $fullPath = Assert-SafeMutationTarget -Path $Path -AllowedExactPaths @($Path)
    if (Test-Path -LiteralPath $fullPath -PathType Container) { return @() }
    if (Test-Path -LiteralPath $fullPath) { throw "Graphics-shell destination parent is not a directory: $fullPath" }

    $missing = [Collections.Generic.List[string]]::new()
    $cursor = $fullPath
    while (-not (Test-Path -LiteralPath $cursor)) {
        $missing.Insert(0, $cursor)
        $parent = Split-Path -Parent $cursor
        if ([string]::IsNullOrWhiteSpace($parent) -or [String]::Equals($parent, $cursor, [StringComparison]::OrdinalIgnoreCase)) {
            throw "Graphics-shell destination parent has no existing ancestor: $fullPath"
        }
        $cursor = $parent
    }
    foreach ($directory in $missing) {
        New-Item -ItemType Directory -Path $directory -Force | Out-Null
    }
    return @($missing)
}

function Assert-GraphicsPublicationSameVolume {
    <# Require staging, recovery, and both live graphics outputs to share a volume before any publication move. #>
    param(
        [Parameter(Mandatory = $true)] [string]$TempRoot,
        [Parameter(Mandatory = $true)] [string]$BackupRoot,
        [Parameter(Mandatory = $true)] [string]$LivePackRoot,
        [Parameter(Mandatory = $true)] [string]$LiveTargetPath
    )

    $paths = @(
        [pscustomobject]@{ Label = 'staging'; Path = $TempRoot },
        [pscustomobject]@{ Label = 'activation backup'; Path = $BackupRoot },
        [pscustomobject]@{ Label = 'live pack'; Path = $LivePackRoot },
        [pscustomobject]@{ Label = 'stable target'; Path = $LiveTargetPath }
    )
    $expectedVolume = [IO.Path]::GetPathRoot((Get-SafeFullPath $TempRoot))
    foreach ($entry in $paths) {
        $actualVolume = [IO.Path]::GetPathRoot((Get-SafeFullPath $entry.Path))
        if (-not [String]::Equals($actualVolume, $expectedVolume, [StringComparison]::OrdinalIgnoreCase)) {
            throw "Rollback-safe graphics publication requires all paths on one Windows volume; $($entry.Label) '$($entry.Path)' is on '$actualVolume' while staging is on '$expectedVolume'."
        }
    }
}

function Restore-AtomicRebuild {
    <# Restore both live artifacts from the activation backup, tolerating absent originals. #>
    param(
        [Parameter(Mandatory = $true)][string]$LivePackRoot,
        [Parameter(Mandatory = $true)][string]$LiveTargetPath,
        [Parameter(Mandatory = $true)][string]$PackBackupRoot,
        [Parameter(Mandatory = $true)][string]$TargetBackupPath,
        [Parameter(Mandatory = $true)][string]$ActivationBackupRoot,
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
        Move-SafeMutationTarget -Source $PackBackupRoot -Destination $LivePackRoot -SourceDescendantRoots @($ActivationBackupRoot) -DestinationExactPaths @($LivePackRoot)
    }
    if ($TargetWasInstalled -or $TargetWasBackedUp -or $targetBackupExists) {
        if (Test-Path -LiteralPath $LiveTargetPath) { Remove-SafeMutationTarget -Path $LiveTargetPath -AllowedExactPaths @($LiveTargetPath) }
    }
    if (($TargetWasBackedUp -or $targetBackupExists) -and $targetBackupExists) {
        Move-SafeMutationTarget -Source $TargetBackupPath -Destination $LiveTargetPath -SourceDescendantRoots @($ActivationBackupRoot) -DestinationExactPaths @($LiveTargetPath)
    }
}

function Invoke-AtomicGraphicsPublication {
    <#
    Publish a fully verified pack and target through a rollback-safe transaction.
    The caller supplies a verifier and optional injected transition failure so
    tests exercise real file moves, restoration, and post-commit cleanup. Backups
    live in a unique sibling of the staging root and are never removed in finally.
    #>
    param(
        [Parameter(Mandatory = $true)][string]$LivePackRoot,
        [Parameter(Mandatory = $true)][string]$LiveTargetPath,
        [Parameter(Mandatory = $true)][string]$StagedPackRoot,
        [Parameter(Mandatory = $true)][string]$StagedTargetPath,
        [Parameter(Mandatory = $true)][string]$BackupRoot,
        [Parameter(Mandatory = $true)][string]$TempRoot,
        [Parameter(Mandatory = $true)][scriptblock]$VerifyPublication,
        [ValidateSet('', 'AfterPackBackup', 'AfterTargetBackup', 'AfterPackActivation', 'AfterTargetActivation', 'AfterTargetVerification', 'AfterBackupDeletion')]
        [string]$FailureInjection = ''
    )
    $livePack = Get-SafeFullPath $LivePackRoot
    $liveTarget = Get-SafeFullPath $LiveTargetPath
    $tempFull = Get-SafeFullPath $TempRoot
    $backupFull = Get-SafeFullPath $BackupRoot
    Assert-GraphicsPublicationSameVolume -TempRoot $tempFull -BackupRoot $backupFull -LivePackRoot $livePack -LiveTargetPath $liveTarget
    $stagedPack = Assert-SafeMutationTarget -Path $StagedPackRoot -AllowedDescendantRoots @($TempRoot)
    $stagedTarget = Assert-SafeMutationTarget -Path $StagedTargetPath -AllowedDescendantRoots @($TempRoot)
    if ([String]::Equals($backupFull, $tempFull, [StringComparison]::OrdinalIgnoreCase) -or $backupFull.StartsWith($tempFull.TrimEnd('\') + '\', [StringComparison]::OrdinalIgnoreCase)) {
        throw 'Rollback-safe publication backups must be outside the staging temp tree.'
    }
    if (Test-Path -LiteralPath $backupFull) { throw "Rollback-safe publication backup root already exists: $backupFull" }
    $packBackupRoot = Join-Path $backupFull 'pack\batman-aa-graphics-options'
    $targetBackupPath = Join-Path $backupFull 'target\Frontend-graphics-options.umap'
    $packBackedUp = $false
    $targetBackedUp = $false
    $packInstalled = $false
    $targetInstalled = $false
    $committed = $false
    $createdDestinationParents = [Collections.Generic.List[string]]::new()
    try {
        Assert-SafeMutationTarget -Path $backupFull -AllowedDescendantRoots @([IO.Path]::GetTempPath()) | Out-Null
        New-Item -ItemType Directory -Force -Path (Join-Path $backupFull 'pack'), (Join-Path $backupFull 'target') | Out-Null
        foreach ($destinationParent in @((Split-Path -Parent $livePack), (Split-Path -Parent $liveTarget))) {
            foreach ($createdParent in @(Ensure-SafeDirectoryPath -Path $destinationParent)) {
                if (-not $createdDestinationParents.Contains($createdParent)) { $createdDestinationParents.Add($createdParent) }
            }
        }
        if (-not (Test-Path -LiteralPath $stagedPack) -or -not (Test-Path -LiteralPath $stagedTarget)) { throw 'Rollback-safe publication staged inputs disappeared before activation.' }
        if (Test-Path -LiteralPath $livePack) {
            Move-SafeMutationTarget -Source $livePack -Destination $packBackupRoot -SourceExactPaths @($livePack) -DestinationDescendantRoots @($backupFull)
            $packBackedUp = $true
        }
        if ($FailureInjection -eq 'AfterPackBackup') { throw 'Injected rollback-safe publication failure after pack backup.' }
        if (Test-Path -LiteralPath $liveTarget) {
            Move-SafeMutationTarget -Source $liveTarget -Destination $targetBackupPath -SourceExactPaths @($liveTarget) -DestinationDescendantRoots @($backupFull)
            $targetBackedUp = $true
        }
        if ($FailureInjection -eq 'AfterTargetBackup') { throw 'Injected rollback-safe publication failure after target backup.' }
        Move-SafeMutationTarget -Source $stagedPack -Destination $livePack -SourceDescendantRoots @($TempRoot) -DestinationExactPaths @($livePack)
        $packInstalled = $true
        if ($FailureInjection -eq 'AfterPackActivation') { throw 'Injected rollback-safe publication failure after pack activation.' }
        Move-SafeMutationTarget -Source $stagedTarget -Destination $liveTarget -SourceDescendantRoots @($TempRoot) -DestinationExactPaths @($liveTarget)
        $targetInstalled = $true
        if ($FailureInjection -eq 'AfterTargetActivation') { throw 'Injected rollback-safe publication failure after target activation.' }
        & $VerifyPublication $livePack $liveTarget
        if (-not $?) { throw 'Rollback-safe publication live-output verification failed.' }
        if ($FailureInjection -eq 'AfterTargetVerification') { throw 'Injected rollback-safe publication failure after target verification.' }
        $committed = $true

        if (Test-Path -LiteralPath $packBackupRoot) { Remove-SafeMutationTarget -Path $packBackupRoot -AllowedDescendantRoots @($backupFull) }
        if ($FailureInjection -eq 'AfterBackupDeletion') { throw "Injected rollback-safe publication backup cleanup failure while deleting '$targetBackupPath'." }
        if (Test-Path -LiteralPath $targetBackupPath) { Remove-SafeMutationTarget -Path $targetBackupPath -AllowedDescendantRoots @($backupFull) }
        if (Test-Path -LiteralPath $backupFull) { Remove-SafeMutationTarget -Path $backupFull -AllowedDescendantRoots @([IO.Path]::GetTempPath()) }
    }
    catch {
        $failure = $_
        if ($committed) {
            throw "Rollback-safe graphics-shell publication committed, but backup cleanup failed: $($failure.Exception.Message). Recovery copies are retained at $backupFull."
        }
        try {
            Restore-AtomicRebuild -LivePackRoot $livePack -LiveTargetPath $liveTarget -PackBackupRoot $packBackupRoot -TargetBackupPath $targetBackupPath -ActivationBackupRoot $backupFull -PackWasBackedUp $packBackedUp -TargetWasBackedUp $targetBackedUp -PackWasInstalled $packInstalled -TargetWasInstalled $targetInstalled
            if (Test-Path -LiteralPath $backupFull) { Remove-SafeMutationTarget -Path $backupFull -AllowedDescendantRoots @([IO.Path]::GetTempPath()) }
            for ($index = $createdDestinationParents.Count - 1; $index -ge 0; $index--) {
                $createdParent = $createdDestinationParents[$index]
                if (Test-Path -LiteralPath $createdParent -PathType Container -and @(Get-ChildItem -LiteralPath $createdParent -Force).Count -eq 0) {
                    Remove-SafeMutationTarget -Path $createdParent -AllowedExactPaths @($createdParent)
                }
            }
        }
        catch {
            throw "Rollback-safe graphics-shell publication failed: $($failure.Exception.Message); rollback failed: $($_.Exception.Message)"
        }
        throw $failure
    }
}

if ($FunctionsOnly) { return }

if ([string]::IsNullOrWhiteSpace($BatmanRoot)) { $BatmanRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path } else { $BatmanRoot = (Resolve-Path $BatmanRoot).Path }
if ([string]::IsNullOrWhiteSpace($BuilderRoot)) { $BuilderRoot = Join-Path $BatmanRoot 'builder' } elseif ([IO.Path]::IsPathRooted($BuilderRoot)) { $BuilderRoot = [IO.Path]::GetFullPath($BuilderRoot) } elseif (Test-Path -LiteralPath $BuilderRoot) { $BuilderRoot = (Resolve-Path $BuilderRoot).Path } else { $BuilderRoot = [IO.Path]::GetFullPath((Join-Path $BatmanRoot $BuilderRoot)) }
if ([string]::IsNullOrWhiteSpace($BatmanUserIniPath)) {
    $documentsPath = [Environment]::GetFolderPath([Environment+SpecialFolder]::MyDocuments)
    $BatmanUserIniPath = Join-Path $documentsPath 'Square Enix\Batman Arkham Asylum GOTY\BmGame\Config\BmEngine.ini'
} else {
    $BatmanUserIniPath = [IO.Path]::GetFullPath($BatmanUserIniPath)
}
if (-not (Test-Path -LiteralPath $BatmanUserIniPath -PathType Leaf)) { throw "Batman user INI was not found as a file: $BatmanUserIniPath" }

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
$activationBackupRoot = Join-Path $systemTempRoot ('HelenBatmanGraphicsShellBackup-' + [Guid]::NewGuid().ToString('N'))
$stagedPackJsonPath = Join-Path $stagedPackRoot 'pack.json'
$stagedBuildJsonPath = Join-Path $stagedPackBuildRoot 'build.json'
$stagedBindingsJsonPath = Join-Path $stagedPackBuildRoot 'bindings.json'
$stagedCommandsJsonPath = Join-Path $stagedPackBuildRoot 'commands.json'
$stagedFilesJsonPath = Join-Path $stagedPackBuildRoot 'files.json'
$stagedHooksJsonPath = Join-Path $stagedPackBuildRoot 'hooks.json'
Assert-GraphicsPublicationSameVolume -TempRoot $tempRoot -BackupRoot $activationBackupRoot -LivePackRoot $packRoot -LiveTargetPath $stableTargetPath
$primaryFailure = $null
try {
    New-Item -ItemType Directory -Force -Path $tempRoot | Out-Null
    Assert-SafeMutationTarget -Path $tempRoot -AllowedDescendantRoots @($systemTempRoot) | Out-Null
    Invoke-RequiredProcess -FilePath 'dotnet' -Arguments @('build', $builderProjectPath, '-c', $Configuration) -FailureMessage 'SubtitleSizeModBuilder build failed.'
    Invoke-RequiredProcess -FilePath 'dotnet' -Arguments @('build', $patcherProjectPath, '-c', $Configuration) -FailureMessage 'BmGameGfxPatcher build failed.'
    Invoke-RequiredProcess -FilePath 'dotnet' -Arguments @('run', '--project', $builderProjectPath, '-c', $Configuration, '--', 'build-main-menu-graphics-shell', '--root', $BuilderRoot, '--output-dir', $prototypeOutputRoot, '--ffdec', $ffdecPath, '--ini', $BatmanUserIniPath) -FailureMessage 'build-main-menu-graphics-shell failed.'
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
    $configKeys = @(
        'fullscreen',
        'resolutionWidth',
        'resolutionHeight',
        'vsync',
        'msaa',
        'detailLevel',
        'bloom',
        'dynamicShadows',
        'motionBlur',
        'distortion',
        'fogVolumes',
        'sphericalHarmonicLighting',
        'ambientOcclusion',
        'physx',
        'stereo',
        'applySignal'
    )
    $config = @(
        foreach ($configKey in $configKeys) {
            [ordered]@{
                key = $configKey
                type = 'int'
                defaultValue = [int]0
            }
        }
    )
    $loadGraphicsDraftCommand = [ordered]@{
        id = 'loadBatmanGraphicsDraftIntoConfig'
        name = 'Load Batman Graphics Draft Into Config'
        steps = @(
            [ordered]@{
                kind = 'load-batman-graphics-draft-into-config'
            }
        )
    }
    $applyGraphicsDraftCommand = [ordered]@{
        id = 'applyBatmanGraphicsDraft'
        name = 'Apply Batman Graphics Draft'
        steps = @(
            [ordered]@{
                kind = 'apply-batman-graphics-config'
            },
            [ordered]@{
                kind = 'load-batman-graphics-draft-into-config'
            }
        )
    }
    $commands = [ordered]@{
        commands = @($loadGraphicsDraftCommand, $applyGraphicsDraftCommand)
    }
    $hooks = [ordered]@{
        runtimeSlots = @()
        stateObservers = @(
            (New-GraphicsCarrierObserver -Id 'graphicsObserverVsync' -TargetConfigKey 'vsync' -Mappings @(
                    [ordered]@{ match = [int]4210; value = [int]0 },
                    [ordered]@{ match = [int]4211; value = [int]1 }
                )),
            (New-GraphicsCarrierObserver -Id 'graphicsObserverApplySignal' -TargetConfigKey 'applySignal' -Command 'applyBatmanGraphicsDraft' -Mappings @(
                    [ordered]@{ match = [int]4990; value = [int]0 },
                    [ordered]@{ match = [int]4991; value = [int]1 }
                ))
        )
        hooks = @()
    }
    $pack = [ordered]@{
        schemaVersion = 1
        id = 'batman-aa-graphics-options'
        name = 'Batman Graphics Options VSync'
        targets = @([ordered]@{ gameId = 'batman-arkham-asylum'; executables = @($buildMatch.Executable) })
        config = $config
        builds = @($buildMatch.BuildId)
    }
    $build = [ordered]@{
        id = $buildMatch.BuildId
        executable = $buildMatch.Executable
        match = [ordered]@{ fileSize = $buildMatch.FileSize; sha256 = $buildMatch.Sha256 }
        startupCommands = @('loadBatmanGraphicsDraftIntoConfig')
    }
    Write-Utf8TextFile -Path $stagedFilesJsonPath -Contents ($files | ConvertTo-Json -Depth 7)
    Write-Utf8TextFile -Path $stagedPackJsonPath -Contents ($pack | ConvertTo-Json -Depth 5)
    Write-Utf8TextFile -Path $stagedBuildJsonPath -Contents ($build | ConvertTo-Json -Depth 5)
    Write-Utf8TextFile -Path $stagedBindingsJsonPath -Contents (([ordered]@{ bindings = @() }) | ConvertTo-Json -Depth 3)
    Write-Utf8TextFile -Path $stagedCommandsJsonPath -Contents ($commands | ConvertTo-Json -Depth 6)
    Write-Utf8TextFile -Path $stagedHooksJsonPath -Contents ($hooks | ConvertTo-Json -Depth 8)

    # The verifier reopens/export-checks the staged target and reconstructs its delta before activation.
    & $packageVerifierPath -BatmanRoot $BatmanRoot -BuilderRoot $BuilderRoot -Configuration $Configuration -PackRootOverride $stagedPackRoot -TargetPathOverride $stagedTargetPath -StagedPackageValidation
    if (-not $?) { throw 'Staged graphics-options pack verification failed.' }

    $verifyPublication = {
        param($LivePackRootForVerification, $LiveTargetPathForVerification)
        & $packageVerifierPath -BatmanRoot $BatmanRoot -BuilderRoot $BuilderRoot -Configuration $Configuration
        if (-not $?) { throw 'Post-activation graphics-options pack verification failed.' }
    }
    Invoke-AtomicGraphicsPublication -LivePackRoot $packRoot -LiveTargetPath $stableTargetPath -StagedPackRoot $stagedPackRoot -StagedTargetPath $stagedTargetPath -BackupRoot $activationBackupRoot -TempRoot $tempRoot -VerifyPublication $verifyPublication
}
catch {
    $primaryFailure = $_
    throw $primaryFailure
}
finally {
    if (Test-Path -LiteralPath $tempRoot) {
        try {
            Remove-SafeMutationTarget -Path $tempRoot -AllowedDescendantRoots @($systemTempRoot)
        }
        catch {
            if ($null -ne $primaryFailure) {
                Write-Warning "Graphics-shell staging cleanup failed after the primary failure '$($primaryFailure.Exception.Message)': $($_.Exception.Message)"
            } else {
                throw "Graphics-shell staging cleanup failed: $($_.Exception.Message)"
            }
        }
    }
}

$finalInfo = Get-Item -LiteralPath $stableTargetPath
$finalHash = (Get-FileHash -LiteralPath $stableTargetPath -Algorithm SHA256).Hash
$finalDeltaPath = Join-Path $packRoot 'builds\steam-goty-1.0\assets\deltas\Frontend-graphics-options.hgdelta'
Write-Output 'Rebuilt Batman graphics-options shell outputs with rollback-safe publication:'
Write-Output "  Retail base:     $retailBasePath ($($retailInfo.Length) bytes, $retailHash)"
Write-Output "  Frontend target: $stableTargetPath ($($finalInfo.Length) bytes, $finalHash)"
Write-Output "  Frontend delta:  $finalDeltaPath ($((Get-Item -LiteralPath $finalDeltaPath).Length) bytes, $((Get-FileHash -LiteralPath $finalDeltaPath -Algorithm SHA256).Hash))"
