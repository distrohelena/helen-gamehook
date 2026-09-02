param(
    [string]$GameBin = 'D:\steam\steamapps\common\Batman Arkham Asylum GOTY\Binaries',
    [string]$BuilderRoot,
    [string]$Configuration = 'Release',
    [switch]$FunctionsOnly
)

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'BatmanBuilderWorkspaceHelpers.ps1')

function Get-SafeFullPath {
    <# Resolve a filesystem path while rejecting Windows device namespaces. #>
    param(
        [Parameter(Mandatory = $true)] [string]$Path
    )

    if ([string]::IsNullOrWhiteSpace($Path)) {
        throw 'Deployment paths must not be empty.'
    }
    if ($Path.StartsWith('\\?\', [StringComparison]::Ordinal) -or
        $Path.StartsWith('\\.\', [StringComparison]::Ordinal) -or
        $Path.StartsWith('\??\', [StringComparison]::Ordinal)) {
        throw "Deployment device-namespace paths are not allowed: $Path"
    }
    return [IO.Path]::GetFullPath($Path)
}

function Test-PathWithinRoot {
    <# Return whether a path is the allowed root or a strict descendant of it. #>
    param(
        [Parameter(Mandatory = $true)] [string]$Path,
        [Parameter(Mandatory = $true)] [string]$Root
    )

    $fullPath = Get-SafeFullPath $Path
    $fullRoot = Get-SafeFullPath $Root
    return [String]::Equals($fullPath, $fullRoot, [StringComparison]::OrdinalIgnoreCase) -or
        $fullPath.StartsWith($fullRoot.TrimEnd('\') + '\', [StringComparison]::OrdinalIgnoreCase)
}

function Get-ExistingDeploymentItem {
    <# Read an exact directory entry, including a dangling link that Test-Path would hide. #>
    param(
        [Parameter(Mandatory = $true)] [string]$Path
    )

    try {
        return Get-Item -LiteralPath (Get-SafeFullPath $Path) -Force -ErrorAction Stop
    } catch {
        if ($_.CategoryInfo.Category -eq [System.Management.Automation.ErrorCategory]::ObjectNotFound) {
            return $null
        }
        throw
    }
}

function Assert-SafeDeploymentPath {
    <# Validate path scope and reject reparse points on every existing path component. #>
    param(
        [Parameter(Mandatory = $true)] [string]$Path,
        [Parameter(Mandatory = $true)] [string[]]$AllowedRoots,
        [switch]$RequireExisting
    )

    $fullPath = Get-SafeFullPath $Path
    $matchingRoot = $null
    foreach ($root in $AllowedRoots) {
        if (Test-PathWithinRoot -Path $fullPath -Root $root) {
            $matchingRoot = Get-SafeFullPath $root
            break
        }
    }
    if ($null -eq $matchingRoot) {
        throw "Deployment path is outside its allowed root: $fullPath"
    }
    if ($RequireExisting -and $null -eq (Get-ExistingDeploymentItem -Path $fullPath)) {
        throw "Required deployment path was not found: $fullPath"
    }

    $current = $fullPath
    while ($true) {
        $item = Get-ExistingDeploymentItem -Path $current
        if ($null -ne $item) {
            if (($item.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
                throw "Deployment path contains a reparse point: $current"
            }
        }
        if ([String]::Equals($current, $matchingRoot, [StringComparison]::OrdinalIgnoreCase)) {
            break
        }
        $parent = Split-Path -Parent $current
        if ([string]::IsNullOrWhiteSpace($parent) -or [String]::Equals($parent, $current, [StringComparison]::OrdinalIgnoreCase)) {
            throw "Deployment path could not reach its allowed root: $fullPath"
        }
        $current = $parent
    }
    return $fullPath
}

function Assert-SafeDeploymentTree {
    <# Validate a recursive mutation or copy target without traversing links. #>
    param(
        [Parameter(Mandatory = $true)] [string]$Root,
        [Parameter(Mandatory = $true)] [string[]]$AllowedRoots,
        [switch]$RequireExisting
    )

    $fullRoot = Assert-SafeDeploymentPath -Path $Root -AllowedRoots $AllowedRoots -RequireExisting:$RequireExisting
    if (-not (Test-Path -LiteralPath $fullRoot -PathType Container)) {
        throw "Deployment tree is not a directory: $fullRoot"
    }
    $pendingDirectories = [Collections.Generic.Stack[string]]::new()
    $pendingDirectories.Push($fullRoot)
    while ($pendingDirectories.Count -gt 0) {
        $currentDirectory = $pendingDirectories.Pop()
        foreach ($child in @(Get-ChildItem -LiteralPath $currentDirectory -Force)) {
            $childPath = Assert-SafeDeploymentPath -Path $child.FullName -AllowedRoots $AllowedRoots -RequireExisting
            if ($child.PSIsContainer) {
                $pendingDirectories.Push($childPath)
            }
        }
    }
    return $fullRoot
}

function Get-SafeDeploymentItems {
    <# Enumerate a validated tree without descending through a reparse point. #>
    param(
        [Parameter(Mandatory = $true)] [string]$Root,
        [Parameter(Mandatory = $true)] [string[]]$AllowedRoots
    )

    $fullRoot = Assert-SafeDeploymentPath -Path $Root -AllowedRoots $AllowedRoots -RequireExisting
    if (-not (Test-Path -LiteralPath $fullRoot -PathType Container)) {
        throw "Deployment tree is not a directory: $fullRoot"
    }
    $pendingDirectories = [Collections.Generic.Stack[string]]::new()
    $pendingDirectories.Push($fullRoot)
    while ($pendingDirectories.Count -gt 0) {
        $currentDirectory = $pendingDirectories.Pop()
        foreach ($child in @(Get-ChildItem -LiteralPath $currentDirectory -Force)) {
            $childPath = Assert-SafeDeploymentPath -Path $child.FullName -AllowedRoots $AllowedRoots -RequireExisting
            Write-Output $child
            if ($child.PSIsContainer) {
                $pendingDirectories.Push($childPath)
            }
        }
    }
}

function Assert-DeploymentArtifactKind {
    <# Require the expected file or directory type before moving a deployment artifact. #>
    param(
        [Parameter(Mandatory = $true)] [string]$Path,
        [Parameter(Mandatory = $true)] [ValidateSet('File', 'Directory')] [string]$Kind,
        [Parameter(Mandatory = $true)] [string]$Context
    )

    $existingItem = Get-ExistingDeploymentItem -Path $Path
    if ($null -eq $existingItem) {
        return
    }
    $isDirectory = $existingItem.PSIsContainer
    if (($Kind -eq 'Directory' -and -not $isDirectory) -or ($Kind -eq 'File' -and $isDirectory)) {
        throw "$Context has the wrong filesystem type: $Path"
    }
}

function Get-DirectorySnapshot {
    <# Capture every directory and file, including size and SHA-256, for exact preservation checks. #>
    param(
        [Parameter(Mandatory = $true)] [string]$Root
    )

    Assert-SafeDeploymentTree -Root $Root -AllowedRoots @($Root) -RequireExisting | Out-Null
    $fullRoot = (Get-SafeFullPath $Root).TrimEnd('\') + '\'
    return @(
        Get-SafeDeploymentItems -Root $Root -AllowedRoots @($Root) | ForEach-Object {
            $relativePath = $_.FullName.Substring($fullRoot.Length).Replace('/', '\')
            if ($_.PSIsContainer) {
                [pscustomobject]@{ RelativePath = $relativePath; Kind = 'Directory'; Length = [int64]0; Sha256 = '' }
            } else {
                [pscustomobject]@{ RelativePath = $relativePath; Kind = 'File'; Length = [int64]$_.Length; Sha256 = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash }
            }
        } | Sort-Object Kind, RelativePath
    )
}

function Assert-DirectorySnapshotEqual {
    <# Fail if an installed directory differs in entries, lengths, or hashes from its baseline. #>
    param(
        [Parameter(Mandatory = $true)] [object[]]$Expected,
        [Parameter(Mandatory = $true)] [string]$ActualRoot,
        [Parameter(Mandatory = $true)] [string]$Context
    )

    $actual = @(Get-DirectorySnapshot -Root $ActualRoot)
    if ($Expected.Count -ne $actual.Count) {
        throw "$Context directory entry count changed. Expected $($Expected.Count), found $($actual.Count)."
    }
    for ($index = 0; $index -lt $Expected.Count; $index++) {
        foreach ($property in @('RelativePath', 'Kind', 'Length', 'Sha256')) {
            if ($Expected[$index].$property -cne $actual[$index].$property) {
                throw "$Context directory entry changed at index $index property $property."
            }
        }
    }
}

function Assert-BatmanGraphicsPackConfig {
    <# Validate the only supported checkpoint configuration and its pack order. #>
    param(
        [Parameter(Mandatory = $true)] [string]$Path,
        [Parameter(Mandatory = $true)] [string]$Context
    )

    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw "$Context pack configuration was not found: $Path"
    }
    $config = Get-Content -LiteralPath $Path -Raw | ConvertFrom-Json
    $enabledPacks = @($config.enabledPacksByExecutable.'ShippingPC-BmGame.exe')
    if ($enabledPacks.Count -ne 2 -or $enabledPacks[0] -ne 'batman-aa-subtitles' -or $enabledPacks[1] -ne 'batman-aa-graphics-options') {
        throw "$Context pack configuration must enable exactly batman-aa-subtitles then batman-aa-graphics-options."
    }
}

function Assert-BatmanGraphicsVsyncExactProperties {
    <# Require the exact property set for one VSync carrier manifest object. #>
    param(
        [Parameter(Mandatory = $true)] [psobject]$Object,
        [Parameter(Mandatory = $true)] [string[]]$Names,
        [Parameter(Mandatory = $true)] [string]$Context
    )

    $actual = @($Object.PSObject.Properties.Name | Sort-Object)
    $expected = @($Names | Sort-Object)
    if (($actual -join '|') -cne ($expected -join '|')) {
        throw "$Context properties drifted. Expected '$($expected -join ', ')' but found '$($actual -join ', ')'."
    }
}

function Assert-BatmanGraphicsVsyncChecks {
    <# Validate the shared stock graphics carrier structure without accepting unrelated option records. #>
    param(
        [Parameter(Mandatory = $true)] [psobject]$Observer,
        [Parameter(Mandatory = $true)] [string]$Context
    )

    $expectedChecks = @(
        [pscustomobject]@{ Comparison = 'equals-constant'; Offset = -16; ExpectedValue = 50 },
        [pscustomobject]@{ Comparison = 'equals-constant'; Offset = -12; ExpectedValue = 100 },
        [pscustomobject]@{ Comparison = 'equals-constant'; Offset = -8; ExpectedValue = 100 },
        [pscustomobject]@{ Comparison = 'equals-constant'; Offset = -4; ExpectedValue = 100 },
        [pscustomobject]@{ Comparison = 'equals-constant'; Offset = 4; ExpectedValue = 1 },
        [pscustomobject]@{ Comparison = 'equals-constant'; Offset = 8; ExpectedValue = 0 },
        [pscustomobject]@{ Comparison = 'equals-constant'; Offset = 12; ExpectedValue = 1 },
        [pscustomobject]@{ Comparison = 'equals-value-at-offset'; Offset = 16; CompareOffset = 0 },
        [pscustomobject]@{ Comparison = 'equals-constant'; Offset = 20; ExpectedValue = 2 },
        [pscustomobject]@{ Comparison = 'equals-constant'; Offset = 28; ExpectedValue = 3 },
        [pscustomobject]@{ Comparison = 'equals-constant'; Offset = 32; ExpectedValue = 3 }
    )
    if (@($Observer.checks).Count -ne $expectedChecks.Count) {
        throw "$Context must contain exactly $($expectedChecks.Count) structural checks."
    }

    for ($index = 0; $index -lt $expectedChecks.Count; $index++) {
        $expected = $expectedChecks[$index]
        $actual = $Observer.checks[$index]
        if ($expected.Comparison -eq 'equals-value-at-offset') {
            Assert-BatmanGraphicsVsyncExactProperties -Object $actual -Names @('comparison', 'offset', 'compareOffset') -Context "$Context check $($index + 1)"
            if ($actual.comparison -cne $expected.Comparison -or $actual.offset -ne $expected.Offset -or $actual.compareOffset -ne $expected.CompareOffset) {
                throw "$Context check $($index + 1) drifted."
            }
        } else {
            Assert-BatmanGraphicsVsyncExactProperties -Object $actual -Names @('comparison', 'offset', 'expectedValue') -Context "$Context check $($index + 1)"
            if ($actual.comparison -cne $expected.Comparison -or $actual.offset -ne $expected.Offset -or $actual.expectedValue -ne $expected.ExpectedValue) {
                throw "$Context check $($index + 1) drifted."
            }
        }
    }
}

function Assert-BatmanGraphicsVsyncMappings {
    <# Validate the two ordered raw control-code mappings owned by one graphics observer. #>
    param(
        [Parameter(Mandatory = $true)] [psobject]$Observer,
        [Parameter(Mandatory = $true)] [int]$FirstMatch,
        [Parameter(Mandatory = $true)] [string]$Context
    )

    if (@($Observer.mappings).Count -ne 2) { throw "$Context must contain exactly two mappings." }
    for ($index = 0; $index -lt 2; $index++) {
        $mapping = $Observer.mappings[$index]
        Assert-BatmanGraphicsVsyncExactProperties -Object $mapping -Names @('match', 'value') -Context "$Context mapping $($index + 1)"
        if ($mapping.match -ne ($FirstMatch + $index) -or $mapping.value -ne $index) {
            throw "$Context mapping $($index + 1) drifted."
        }
    }
}

function Assert-BatmanGraphicsVsyncHooks {
    <# Validate the minimal two-observer graphics carrier used by deployment without allowing executable hooks or runtime slots. #>
    param(
        [Parameter(Mandatory = $true)] [string]$PackRoot,
        [Parameter(Mandatory = $true)] [string]$Context
    )

    $hooksPath = Join-Path $PackRoot 'builds\steam-goty-1.0\hooks.json'
    if (-not (Test-Path -LiteralPath $hooksPath -PathType Leaf)) { throw "$Context hooks.json was not found: $hooksPath" }
    $hooks = Get-Content -LiteralPath $hooksPath -Raw | ConvertFrom-Json
    Assert-BatmanGraphicsVsyncExactProperties -Object $hooks -Names @('runtimeSlots', 'stateObservers', 'hooks') -Context "$Context hooks.json"
    if (@($hooks.runtimeSlots).Count -ne 0 -or @($hooks.hooks).Count -ne 0 -or @($hooks.stateObservers).Count -ne 2) {
        throw "$Context hooks.json must contain empty runtimeSlots/hooks and exactly two observers."
    }

    $vsyncObserver = $hooks.stateObservers[0]
    $applyObserver = $hooks.stateObservers[1]
    Assert-BatmanGraphicsVsyncExactProperties -Object $vsyncObserver -Names @('id', 'scanStartAddress', 'scanEndAddress', 'scanStride', 'valueOffset', 'pollIntervalMs', 'targetConfigKey', 'addressMatchValues', 'checks', 'mappings') -Context "$Context graphicsObserverVsync"
    Assert-BatmanGraphicsVsyncExactProperties -Object $applyObserver -Names @('id', 'scanStartAddress', 'scanEndAddress', 'scanStride', 'valueOffset', 'pollIntervalMs', 'targetConfigKey', 'addressMatchValues', 'checks', 'mappings', 'command') -Context "$Context graphicsObserverApplySignal"
    if ($vsyncObserver.id -cne 'graphicsObserverVsync' -or $vsyncObserver.targetConfigKey -cne 'vsync') { throw "$Context VSync observer identity drifted." }
    if ($applyObserver.id -cne 'graphicsObserverApplySignal' -or $applyObserver.targetConfigKey -cne 'applySignal' -or $applyObserver.command -cne 'applyBatmanGraphicsDraft') { throw "$Context apply observer identity drifted." }

    foreach ($observer in @($vsyncObserver, $applyObserver)) {
        if ($observer.scanStartAddress -cne '0x2B000000' -or $observer.scanEndAddress -cne '0x30000000' -or $observer.scanStride -ne 4 -or $observer.valueOffset -ne 0 -or $observer.pollIntervalMs -ne 50) {
            throw "$Context $($observer.id) scan geometry drifted."
        }
        $expectedAddressMatchValues = @(4101, 4102, 4103, 4104, 4105, 4106, 4210, 4211, 4990, 4991)
        if (@($observer.addressMatchValues).Count -ne $expectedAddressMatchValues.Count -or (@($observer.addressMatchValues) -join ',') -cne ($expectedAddressMatchValues -join ',')) {
            throw "$Context $($observer.id) addressMatchValues drifted."
        }
        Assert-BatmanGraphicsVsyncChecks -Observer $observer -Context "$Context $($observer.id)"
    }
    Assert-BatmanGraphicsVsyncMappings -Observer $vsyncObserver -FirstMatch 4210 -Context "$Context graphicsObserverVsync"
    Assert-BatmanGraphicsVsyncMappings -Observer $applyObserver -FirstMatch 4990 -Context "$Context graphicsObserverApplySignal"
}

function Test-ExpectedGraphicsVirtualFile {
    <# Verify that one staged or activated graphics shell manifest names its delta file. #>
    param(
        [Parameter(Mandatory = $true)] [string]$PackRoot,
        [Parameter(Mandatory = $true)] [string]$Context
    )

    Assert-SafeDeploymentTree -Root $PackRoot -AllowedRoots @($PackRoot) -RequireExisting | Out-Null
    $buildRoot = Join-Path $PackRoot 'builds\steam-goty-1.0'
    $filesJsonPath = Join-Path $buildRoot 'files.json'
    if (-not (Test-Path -LiteralPath $filesJsonPath -PathType Leaf)) {
        throw "$Context graphics shell files.json was not found: $filesJsonPath"
    }
    $files = Get-Content -LiteralPath $filesJsonPath -Raw | ConvertFrom-Json
    $virtualFiles = @($files.virtualFiles)
    if ($virtualFiles.Count -ne 1) {
        throw "$Context graphics shell must contain exactly one virtual file, found $($virtualFiles.Count)."
    }
    $virtualFile = $virtualFiles[0]
    if ($virtualFile.id -ne 'frontendGraphicsOptionsPackage' -or $virtualFile.path -ne 'BmGame/CookedPC/Maps/Frontend/Frontend.umap' -or $virtualFile.mode -ne 'delta-on-read' -or $virtualFile.source.kind -ne 'delta-file' -or $virtualFile.source.path -ne 'assets/deltas/Frontend-graphics-options.hgdelta') {
        throw "$Context graphics shell virtual-file contract drifted."
    }
    $deltaPath = Join-Path $buildRoot 'assets\deltas\Frontend-graphics-options.hgdelta'
    if (-not (Test-Path -LiteralPath $deltaPath -PathType Leaf)) {
        throw "$Context graphics shell delta was not found: $deltaPath"
    }
}

function Get-RecoverySurvivorPaths {
    <# Enumerate only actual recovery entries so post-commit errors never claim deleted backups survive. #>
    param(
        [Parameter(Mandatory = $true)] [string]$RecoveryRoot,
        [Parameter(Mandatory = $true)] [string]$GameBin
    )

    if ($null -eq (Get-ExistingDeploymentItem -Path $RecoveryRoot)) {
        return @()
    }
    Assert-SafeDeploymentTree -Root $RecoveryRoot -AllowedRoots @($GameBin) -RequireExisting | Out-Null
    return @(Get-SafeDeploymentItems -Root $RecoveryRoot -AllowedRoots @($GameBin) | Select-Object -ExpandProperty FullName)
}

function Remove-EmptyDeploymentRecoveryRoot {
    <# Remove a validated empty recovery directory after rollback has moved originals back. #>
    param(
        [Parameter(Mandatory = $true)] [string]$RecoveryRoot,
        [Parameter(Mandatory = $true)] [string]$GameBin
    )

    if ($null -eq (Get-ExistingDeploymentItem -Path $RecoveryRoot)) {
        return
    }
    Assert-SafeDeploymentTree -Root $RecoveryRoot -AllowedRoots @($GameBin) -RequireExisting | Out-Null
    if (@(Get-ChildItem -LiteralPath $RecoveryRoot -Force).Count -eq 0) {
        Remove-Item -LiteralPath $RecoveryRoot -Force
    }
}

function Remove-DeploymentStagingRoot {
    <# Remove only the validated outer staging root, leaving recovery copies untouched. #>
    param(
        [Parameter(Mandatory = $true)] [string]$StagingRoot,
        [Parameter(Mandatory = $true)] [string]$GameBin
    )

    if ($null -eq (Get-ExistingDeploymentItem -Path $StagingRoot)) {
        return
    }
    Assert-SafeDeploymentTree -Root $StagingRoot -AllowedRoots @($GameBin) -RequireExisting | Out-Null
    Remove-Item -LiteralPath $StagingRoot -Recurse -Force
}

function Assert-DeploymentStagingRootAvailable {
    <# Reject any pre-existing unique staging root before the deployment can mutate live files. #>
    param(
        [Parameter(Mandatory = $true)] [string]$StagingRoot,
        [Parameter(Mandatory = $true)] [string]$GameBin
    )

    Assert-SafeDeploymentPath -Path $StagingRoot -AllowedRoots @($GameBin) | Out-Null
    $existingItem = Get-ExistingDeploymentItem -Path $StagingRoot
    if ($null -ne $existingItem) {
        throw "Deployment staging root already exists and will not be reused: $StagingRoot"
    }
}

function Initialize-DeploymentRoots {
    <# Create validated same-volume staging and recovery roots, cleaning empty roots on setup failure. #>
    param(
        [Parameter(Mandatory = $true)] [string]$GameBin,
        [Parameter(Mandatory = $true)] [string]$StagingRoot,
        [Parameter(Mandatory = $true)] [string]$RecoveryRoot,
        [Parameter(Mandatory = $true)] [string]$PackStagingDestination,
        [Parameter(Mandatory = $true)] [string]$ConfigStagingPath,
        [Parameter(Mandatory = $true)] [string]$PackParent,
        [Parameter(Mandatory = $true)] [string]$ConfigParent,
        [AllowEmptyString()] [string]$FailureInjection = ''
    )

    $stagingRootCreated = $false
    $recoveryRootCreated = $false
    try {
        Assert-DeploymentStagingRootAvailable -StagingRoot $StagingRoot -GameBin $GameBin
        foreach ($path in @($RecoveryRoot, $PackStagingDestination, $ConfigStagingPath, $PackParent, $ConfigParent)) {
            Assert-SafeDeploymentPath -Path $path -AllowedRoots @($GameBin) | Out-Null
        }
        New-Item -ItemType Directory -Path $StagingRoot | Out-Null
        $stagingRootCreated = $true
        $existingRecoveryItem = Get-ExistingDeploymentItem -Path $RecoveryRoot
        if ($null -eq $existingRecoveryItem) {
            New-Item -ItemType Directory -Path $RecoveryRoot | Out-Null
            $recoveryRootCreated = $true
        } else {
            Assert-SafeDeploymentTree -Root $RecoveryRoot -AllowedRoots @($GameBin) -RequireExisting | Out-Null
            if (@(Get-ChildItem -LiteralPath $RecoveryRoot -Force).Count -ne 0) {
                throw "Deployment recovery root is not empty: $RecoveryRoot"
            }
        }
        Invoke-DeploymentFailureInjection -FailureInjection $FailureInjection -Point 'AfterRecoveryRootCreation'
        New-Item -ItemType Directory -Force -Path $PackStagingDestination, (Split-Path -Parent $ConfigStagingPath), $PackParent, $ConfigParent | Out-Null
        Assert-SafeDeploymentTree -Root $StagingRoot -AllowedRoots @($GameBin) -RequireExisting | Out-Null
        Assert-SafeDeploymentPath -Path $RecoveryRoot -AllowedRoots @($GameBin) -RequireExisting | Out-Null
        return [pscustomobject]@{
            StagingRootCreated = $stagingRootCreated
            RecoveryRootCreated = $recoveryRootCreated
        }
    } catch {
        $setupFailure = $_
        if ($recoveryRootCreated) {
            try {
                Remove-EmptyDeploymentRecoveryRoot -RecoveryRoot $RecoveryRoot -GameBin $GameBin
            } catch {
                Write-Warning "Deployment setup cleanup could not remove recovery root after '$($setupFailure.Exception.Message)': $($_.Exception.Message)"
            }
        }
        if ($stagingRootCreated) {
            try {
                Remove-DeploymentStagingRoot -StagingRoot $StagingRoot -GameBin $GameBin
            } catch {
                Write-Warning "Deployment setup cleanup could not remove staging root after '$($setupFailure.Exception.Message)': $($_.Exception.Message)"
            }
        }
        throw $setupFailure
    }
}

function Invoke-DeploymentFailureInjection {
    <# Inject a deterministic transition failure for isolated rollback tests. #>
    param(
        [Parameter(Mandatory = $true)] [AllowEmptyString()] [string]$FailureInjection,
        [Parameter(Mandatory = $true)] [string]$Point
    )

    if ($FailureInjection -eq $Point) {
        throw "Injected graphics deployment failure at $Point."
    }
}

function Restore-AtomicGraphicsDeployment {
    <# Restore every replaced artifact after a failure before publication is committed. #>
    param(
        [Parameter(Mandatory = $true)] [string]$GameBin,
        [Parameter(Mandatory = $true)] [string]$LivePackRoot,
        [Parameter(Mandatory = $true)] [string]$LiveConfigPath,
        [Parameter(Mandatory = $true)] [string]$LiveHelenGameHookPath,
        [Parameter(Mandatory = $true)] [string]$LiveProxyPath,
        [Parameter(Mandatory = $true)] [string]$PackBackupRoot,
        [Parameter(Mandatory = $true)] [string]$ConfigBackupPath,
        [Parameter(Mandatory = $true)] [string]$HelenGameHookBackupPath,
        [Parameter(Mandatory = $true)] [string]$ProxyBackupPath,
        [Parameter(Mandatory = $true)] [string]$RecoveryRoot,
        [bool]$PackWasBackedUp,
        [bool]$ConfigWasBackedUp,
        [bool]$HelenGameHookWasBackedUp,
        [bool]$ProxyWasBackedUp,
        [bool]$PackWasInstalled,
        [bool]$ConfigWasInstalled,
        [bool]$HelenGameHookWasInstalled,
        [bool]$ProxyWasInstalled
    )

    foreach ($path in @($LivePackRoot, $LiveConfigPath, $LiveHelenGameHookPath, $LiveProxyPath, $PackBackupRoot, $ConfigBackupPath, $HelenGameHookBackupPath, $ProxyBackupPath)) {
        Assert-SafeDeploymentPath -Path $path -AllowedRoots @($GameBin) | Out-Null
    }

    if ($PackWasInstalled -and $null -ne (Get-ExistingDeploymentItem -Path $LivePackRoot)) { Remove-Item -LiteralPath $LivePackRoot -Recurse -Force }
    if ($PackWasBackedUp -and $null -ne (Get-ExistingDeploymentItem -Path $PackBackupRoot)) { Move-Item -LiteralPath $PackBackupRoot -Destination $LivePackRoot }
    if ($ConfigWasInstalled -and $null -ne (Get-ExistingDeploymentItem -Path $LiveConfigPath)) { Remove-Item -LiteralPath $LiveConfigPath -Force }
    if ($ConfigWasBackedUp -and $null -ne (Get-ExistingDeploymentItem -Path $ConfigBackupPath)) { Move-Item -LiteralPath $ConfigBackupPath -Destination $LiveConfigPath }
    if ($HelenGameHookWasInstalled -and $null -ne (Get-ExistingDeploymentItem -Path $LiveHelenGameHookPath)) { Remove-Item -LiteralPath $LiveHelenGameHookPath -Force }
    if ($HelenGameHookWasBackedUp -and $null -ne (Get-ExistingDeploymentItem -Path $HelenGameHookBackupPath)) { Move-Item -LiteralPath $HelenGameHookBackupPath -Destination $LiveHelenGameHookPath }
    if ($ProxyWasInstalled -and $null -ne (Get-ExistingDeploymentItem -Path $LiveProxyPath)) { Remove-Item -LiteralPath $LiveProxyPath -Force }
    if ($ProxyWasBackedUp -and $null -ne (Get-ExistingDeploymentItem -Path $ProxyBackupPath)) { Move-Item -LiteralPath $ProxyBackupPath -Destination $LiveProxyPath }
    Remove-EmptyDeploymentRecoveryRoot -RecoveryRoot $RecoveryRoot -GameBin $GameBin
}

function Invoke-AtomicGraphicsDeployment {
    <# Stage, publish, verify, and clean four artifacts with rollback before the commit point. #>
    param(
        [Parameter(Mandatory = $true)] [string]$GameBin,
        [Parameter(Mandatory = $true)] [string]$LivePackRoot,
        [Parameter(Mandatory = $true)] [string]$LiveConfigPath,
        [Parameter(Mandatory = $true)] [string]$LiveHelenGameHookPath,
        [Parameter(Mandatory = $true)] [string]$LiveProxyPath,
        [Parameter(Mandatory = $true)] [string]$StagedPackRoot,
        [Parameter(Mandatory = $true)] [string]$StagedConfigPath,
        [Parameter(Mandatory = $true)] [string]$StagedHelenGameHookPath,
        [Parameter(Mandatory = $true)] [string]$StagedProxyPath,
        [Parameter(Mandatory = $true)] [string]$PackBackupRoot,
        [Parameter(Mandatory = $true)] [string]$ConfigBackupPath,
        [Parameter(Mandatory = $true)] [string]$HelenGameHookBackupPath,
        [Parameter(Mandatory = $true)] [string]$ProxyBackupPath,
        [Parameter(Mandatory = $true)] [string]$RecoveryRoot,
        [Parameter(Mandatory = $true)] [string]$StagingRoot,
        [Parameter(Mandatory = $true)] [scriptblock]$VerifyPublication,
        [ValidateSet('', 'AfterPackBackup', 'AfterConfigBackup', 'AfterHelenGameHookBackup', 'AfterProxyBackup', 'AfterPackActivation', 'AfterConfigActivation', 'AfterHelenGameHookActivation', 'AfterProxyActivation', 'AfterVerification', 'AfterPackBackupDeletion', 'AfterConfigBackupDeletion', 'AfterHelenGameHookBackupDeletion', 'AfterProxyBackupDeletion')]
        [string]$FailureInjection = ''
    )

    $gameBinPath = Assert-SafeDeploymentPath -Path $GameBin -AllowedRoots @($GameBin) -RequireExisting
    $gameVolume = [IO.Path]::GetPathRoot($gameBinPath)
    foreach ($volumePath in @($StagingRoot, $RecoveryRoot)) {
        if (-not [String]::Equals([IO.Path]::GetPathRoot((Get-SafeFullPath $volumePath)), $gameVolume, [StringComparison]::OrdinalIgnoreCase)) {
            throw "Deployment staging and recovery paths must share the GameBin volume: $volumePath"
        }
    }
    foreach ($path in @($LivePackRoot, $LiveConfigPath, $LiveHelenGameHookPath, $LiveProxyPath, $StagedPackRoot, $StagedConfigPath, $StagedHelenGameHookPath, $StagedProxyPath, $PackBackupRoot, $ConfigBackupPath, $HelenGameHookBackupPath, $ProxyBackupPath, $RecoveryRoot, $StagingRoot)) {
        Assert-SafeDeploymentPath -Path $path -AllowedRoots @($gameBinPath) | Out-Null
    }
    Assert-SafeDeploymentTree -Root $StagingRoot -AllowedRoots @($gameBinPath) -RequireExisting | Out-Null
    Assert-SafeDeploymentTree -Root $StagedPackRoot -AllowedRoots @($gameBinPath) -RequireExisting | Out-Null
    Assert-DeploymentArtifactKind -Path $StagedConfigPath -Kind File -Context 'Staged packs.json'
    Assert-DeploymentArtifactKind -Path $StagedHelenGameHookPath -Kind File -Context 'Staged HelenGameHook.dll'
    Assert-DeploymentArtifactKind -Path $StagedProxyPath -Kind File -Context 'Staged dinput8.dll'

    if ($null -ne (Get-ExistingDeploymentItem -Path $RecoveryRoot)) {
        Assert-SafeDeploymentTree -Root $RecoveryRoot -AllowedRoots @($gameBinPath) -RequireExisting | Out-Null
        if (@(Get-ChildItem -LiteralPath $RecoveryRoot -Force).Count -ne 0) { throw "Graphics deployment recovery root is not empty: $RecoveryRoot" }
    } else {
        New-Item -ItemType Directory -Force -Path $RecoveryRoot | Out-Null
    }
    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $PackBackupRoot) | Out-Null

    foreach ($path in @($LivePackRoot, $LiveConfigPath, $LiveHelenGameHookPath, $LiveProxyPath)) {
        Assert-SafeDeploymentPath -Path $path -AllowedRoots @($gameBinPath) | Out-Null
    }
    if ($null -ne (Get-ExistingDeploymentItem -Path $LivePackRoot)) { Assert-SafeDeploymentTree -Root $LivePackRoot -AllowedRoots @($gameBinPath) -RequireExisting | Out-Null }
    Assert-DeploymentArtifactKind -Path $LivePackRoot -Kind Directory -Context 'Installed graphics pack'
    Assert-DeploymentArtifactKind -Path $LiveConfigPath -Kind File -Context 'Installed packs.json'
    Assert-DeploymentArtifactKind -Path $LiveHelenGameHookPath -Kind File -Context 'Installed HelenGameHook.dll'
    Assert-DeploymentArtifactKind -Path $LiveProxyPath -Kind File -Context 'Installed dinput8.dll'

    $packWasBackedUp = $false
    $configWasBackedUp = $false
    $helenGameHookWasBackedUp = $false
    $proxyWasBackedUp = $false
    $packWasInstalled = $false
    $configWasInstalled = $false
    $helenGameHookWasInstalled = $false
    $proxyWasInstalled = $false
    $publicationCommitted = $false

    try {
        if ($null -ne (Get-ExistingDeploymentItem -Path $LivePackRoot)) { Move-Item -LiteralPath $LivePackRoot -Destination $PackBackupRoot; $packWasBackedUp = $true }
        Invoke-DeploymentFailureInjection -FailureInjection $FailureInjection -Point 'AfterPackBackup'
        if ($null -ne (Get-ExistingDeploymentItem -Path $LiveConfigPath)) { Move-Item -LiteralPath $LiveConfigPath -Destination $ConfigBackupPath; $configWasBackedUp = $true }
        Invoke-DeploymentFailureInjection -FailureInjection $FailureInjection -Point 'AfterConfigBackup'
        if ($null -ne (Get-ExistingDeploymentItem -Path $LiveHelenGameHookPath)) { Move-Item -LiteralPath $LiveHelenGameHookPath -Destination $HelenGameHookBackupPath; $helenGameHookWasBackedUp = $true }
        Invoke-DeploymentFailureInjection -FailureInjection $FailureInjection -Point 'AfterHelenGameHookBackup'
        if ($null -ne (Get-ExistingDeploymentItem -Path $LiveProxyPath)) { Move-Item -LiteralPath $LiveProxyPath -Destination $ProxyBackupPath; $proxyWasBackedUp = $true }
        Invoke-DeploymentFailureInjection -FailureInjection $FailureInjection -Point 'AfterProxyBackup'

        Move-Item -LiteralPath $StagedPackRoot -Destination $LivePackRoot; $packWasInstalled = $true
        Invoke-DeploymentFailureInjection -FailureInjection $FailureInjection -Point 'AfterPackActivation'
        Move-Item -LiteralPath $StagedConfigPath -Destination $LiveConfigPath; $configWasInstalled = $true
        Invoke-DeploymentFailureInjection -FailureInjection $FailureInjection -Point 'AfterConfigActivation'
        Move-Item -LiteralPath $StagedHelenGameHookPath -Destination $LiveHelenGameHookPath; $helenGameHookWasInstalled = $true
        Invoke-DeploymentFailureInjection -FailureInjection $FailureInjection -Point 'AfterHelenGameHookActivation'
        Move-Item -LiteralPath $StagedProxyPath -Destination $LiveProxyPath; $proxyWasInstalled = $true
        Invoke-DeploymentFailureInjection -FailureInjection $FailureInjection -Point 'AfterProxyActivation'

        & $VerifyPublication $LivePackRoot $LiveConfigPath $LiveHelenGameHookPath $LiveProxyPath
        Invoke-DeploymentFailureInjection -FailureInjection $FailureInjection -Point 'AfterVerification'
        $publicationCommitted = $true

        $cleanupOperations = @(
            [pscustomobject]@{ Path = $PackBackupRoot; WasBackedUp = $packWasBackedUp; Point = 'AfterPackBackupDeletion'; Recursive = $true },
            [pscustomobject]@{ Path = $ConfigBackupPath; WasBackedUp = $configWasBackedUp; Point = 'AfterConfigBackupDeletion'; Recursive = $false },
            [pscustomobject]@{ Path = $HelenGameHookBackupPath; WasBackedUp = $helenGameHookWasBackedUp; Point = 'AfterHelenGameHookBackupDeletion'; Recursive = $false },
            [pscustomobject]@{ Path = $ProxyBackupPath; WasBackedUp = $proxyWasBackedUp; Point = 'AfterProxyBackupDeletion'; Recursive = $false }
        )
        foreach ($operation in $cleanupOperations) {
            if (-not $operation.WasBackedUp) { continue }
            if ($null -eq (Get-ExistingDeploymentItem -Path $operation.Path)) { throw "Recovery backup disappeared before cleanup: $($operation.Path)" }
            Assert-SafeDeploymentPath -Path $operation.Path -AllowedRoots @($gameBinPath) -RequireExisting | Out-Null
            if ($operation.Recursive) { Assert-SafeDeploymentTree -Root $operation.Path -AllowedRoots @($gameBinPath) -RequireExisting | Out-Null; Remove-Item -LiteralPath $operation.Path -Recurse -Force } else { Remove-Item -LiteralPath $operation.Path -Force }
            Invoke-DeploymentFailureInjection -FailureInjection $FailureInjection -Point $operation.Point
        }
        Remove-EmptyDeploymentRecoveryRoot -RecoveryRoot $RecoveryRoot -GameBin $gameBinPath
    } catch {
        $deploymentFailure = $_
        if (-not $publicationCommitted) {
            try {
                Restore-AtomicGraphicsDeployment -GameBin $gameBinPath -LivePackRoot $LivePackRoot -LiveConfigPath $LiveConfigPath -LiveHelenGameHookPath $LiveHelenGameHookPath -LiveProxyPath $LiveProxyPath -PackBackupRoot $PackBackupRoot -ConfigBackupPath $ConfigBackupPath -HelenGameHookBackupPath $HelenGameHookBackupPath -ProxyBackupPath $ProxyBackupPath -RecoveryRoot $RecoveryRoot -PackWasBackedUp $packWasBackedUp -ConfigWasBackedUp $configWasBackedUp -HelenGameHookWasBackedUp $helenGameHookWasBackedUp -ProxyWasBackedUp $proxyWasBackedUp -PackWasInstalled $packWasInstalled -ConfigWasInstalled $configWasInstalled -HelenGameHookWasInstalled $helenGameHookWasInstalled -ProxyWasInstalled $proxyWasInstalled
            } catch {
                throw "Graphics deployment failed and rollback failed. Original error: $($deploymentFailure.Exception.Message). Rollback error: $($_.Exception.Message)"
            }
        } else {
            $survivors = @()
            try { $survivors = @(Get-RecoverySurvivorPaths -RecoveryRoot $RecoveryRoot -GameBin $gameBinPath) } catch { $survivors = @("inspection failed: $($_.Exception.Message)") }
            $survivorText = if ($survivors.Count -eq 0) { 'none' } else { $survivors -join '; ' }
            throw "Graphics deployment committed, but recovery cleanup failed. Remaining recovery paths: $survivorText. Original error: $($deploymentFailure.Exception.Message)"
        }
        throw $deploymentFailure
    }
}

$BatmanRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$RepoRoot = (Resolve-Path (Join-Path $BatmanRoot '..\..')).Path
$BuilderRoot = Resolve-OptionalBuilderRoot -BatmanRootPath $BatmanRoot -BuilderRootPath $BuilderRoot
$GameBin = Get-SafeFullPath $GameBin
Assert-SafeDeploymentPath -Path $GameBin -AllowedRoots @($GameBin) -RequireExisting | Out-Null
Assert-SafeDeploymentPath -Path $BatmanRoot -AllowedRoots @($BatmanRoot) -RequireExisting | Out-Null
Assert-SafeDeploymentPath -Path $RepoRoot -AllowedRoots @($RepoRoot) -RequireExisting | Out-Null

$PackSource = Join-Path $BatmanRoot 'helengamehook\packs\batman-aa-graphics-options'
$PackDestination = Join-Path $GameBin 'helengamehook\packs\batman-aa-graphics-options'
$PackParent = Split-Path -Parent $PackDestination
$ConfigSourcePath = Join-Path $BatmanRoot 'helengamehook\config\packs.json'
$ConfigDestinationPath = Join-Path $GameBin 'helengamehook\config\packs.json'
$ConfigParent = Split-Path -Parent $ConfigDestinationPath
$SubtitlePackDestination = Join-Path $GameBin 'helengamehook\packs\batman-aa-subtitles'
$RebuildScriptPath = Join-Path $PSScriptRoot 'Rebuild-BatmanGraphicsOptionsExperiment.ps1'
$VerifierPath = Join-Path $PSScriptRoot 'Test-BatmanGraphicsOptionsPackage.ps1'
$InstalledBaseVerifierPath = Join-Path $PSScriptRoot 'Test-BatmanInstalledBaseCompatibility.ps1'
$HelenGameHookPath = Join-Path $RepoRoot "bin\Win32\$Configuration\HelenGameHook.dll"
$ProxyPath = Join-Path $RepoRoot "bin\Win32\$Configuration\dinput8.dll"
$DeploymentId = [Guid]::NewGuid().ToString('N')
$DeploymentStagingRoot = Join-Path $GameBin ".helengamehook-staging-$DeploymentId"
$DeploymentRecoveryRoot = Join-Path $GameBin ".helengamehook-recovery-$DeploymentId"
$PackStagingDestination = Join-Path $DeploymentStagingRoot 'pack'
$ConfigStagingPath = Join-Path $DeploymentStagingRoot 'config\packs.json'
$HelenGameHookStagingPath = Join-Path $DeploymentStagingRoot 'HelenGameHook.dll'
$ProxyStagingPath = Join-Path $DeploymentStagingRoot 'dinput8.dll'
$PackBackupRoot = Join-Path $DeploymentRecoveryRoot 'graphics-options'
$ConfigBackupPath = Join-Path $DeploymentRecoveryRoot 'packs.json'
$HelenGameHookBackupPath = Join-Path $DeploymentRecoveryRoot 'HelenGameHook.dll'
$ProxyBackupPath = Join-Path $DeploymentRecoveryRoot 'dinput8.dll'

if ($FunctionsOnly) {
    return
}

foreach ($requiredPath in @($RebuildScriptPath, $VerifierPath, $InstalledBaseVerifierPath, $ConfigSourcePath)) {
    Assert-SafeDeploymentPath -Path $requiredPath -AllowedRoots @($BatmanRoot) -RequireExisting | Out-Null
}
Assert-SafeDeploymentPath -Path $HelenGameHookPath -AllowedRoots @($RepoRoot) | Out-Null
Assert-SafeDeploymentPath -Path $ProxyPath -AllowedRoots @($RepoRoot) | Out-Null

& $RebuildScriptPath -BatmanRoot $BatmanRoot -BuilderRoot $BuilderRoot -Configuration $Configuration
if ($LASTEXITCODE -ne 0) { throw "Batman graphics shell rebuild failed with exit code $LASTEXITCODE." }

Assert-BatmanGraphicsPackConfig -Path $ConfigSourcePath -Context 'Repository'
Assert-SafeDeploymentTree -Root $PackSource -AllowedRoots @($BatmanRoot) -RequireExisting | Out-Null

try {
    & $VerifierPath -BatmanRoot $BatmanRoot -BuilderRoot $BuilderRoot -Configuration $Configuration
    if ($LASTEXITCODE -ne 0) { throw "Batman graphics-options package verification failed with exit code $LASTEXITCODE." }
} catch {
    throw "Batman graphics-options package verification failed before deployment. $($_.Exception.Message)"
}

try {
    & $InstalledBaseVerifierPath -GameRoot ([IO.Path]::GetFullPath((Join-Path $GameBin '..'))) -PackBuildRoot (Join-Path $PackSource 'builds\steam-goty-1.0')
    if ($LASTEXITCODE -ne 0) { throw "Installed Batman base compatibility verification failed with exit code $LASTEXITCODE." }
} catch {
    throw "Batman graphics-options deployment refused to target an incompatible installed base. Installed Batman base hash mismatch or missing retail input: $($_.Exception.Message)"
}

foreach ($runtimePath in @($HelenGameHookPath, $ProxyPath)) {
    if (-not (Test-Path -LiteralPath $runtimePath -PathType Leaf)) { throw "Batman graphics deployment runtime input not found: $runtimePath" }
    Assert-SafeDeploymentPath -Path $runtimePath -AllowedRoots @($RepoRoot) -RequireExisting | Out-Null
}
Assert-SafeDeploymentPath -Path $SubtitlePackDestination -AllowedRoots @($GameBin) -RequireExisting | Out-Null
$SubtitleSnapshot = @(Get-DirectorySnapshot -Root $SubtitlePackDestination)

Get-Process ShippingPC-BmGame -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Seconds 2

$primaryFailure = $null
$stagingRootCreated = $false
$recoveryRootCreated = $false
try {
    $deploymentRoots = Initialize-DeploymentRoots -GameBin $GameBin -StagingRoot $DeploymentStagingRoot -RecoveryRoot $DeploymentRecoveryRoot -PackStagingDestination $PackStagingDestination -ConfigStagingPath $ConfigStagingPath -PackParent $PackParent -ConfigParent $ConfigParent
    $stagingRootCreated = $deploymentRoots.StagingRootCreated
    $recoveryRootCreated = $deploymentRoots.RecoveryRootCreated
    Assert-SafeDeploymentPath -Path $PackParent -AllowedRoots @($GameBin) -RequireExisting | Out-Null
    Assert-SafeDeploymentPath -Path $ConfigParent -AllowedRoots @($GameBin) -RequireExisting | Out-Null
    Assert-SafeDeploymentTree -Root $DeploymentStagingRoot -AllowedRoots @($GameBin) -RequireExisting | Out-Null
    foreach ($sourceChild in @(Get-ChildItem -LiteralPath $PackSource -Force)) {
        Assert-SafeDeploymentPath -Path $sourceChild.FullName -AllowedRoots @($BatmanRoot) -RequireExisting | Out-Null
        if ($sourceChild.PSIsContainer) { Assert-SafeDeploymentTree -Root $sourceChild.FullName -AllowedRoots @($BatmanRoot) -RequireExisting | Out-Null }
        Copy-Item -LiteralPath $sourceChild.FullName -Destination $PackStagingDestination -Recurse -Force
    }
    Copy-Item -LiteralPath $ConfigSourcePath -Destination $ConfigStagingPath -Force
    Copy-Item -LiteralPath $HelenGameHookPath -Destination $HelenGameHookStagingPath -Force
    Copy-Item -LiteralPath $ProxyPath -Destination $ProxyStagingPath -Force

    Test-ExpectedGraphicsVirtualFile -PackRoot $PackStagingDestination -Context 'Staged deployment'
    Assert-BatmanGraphicsVsyncHooks -PackRoot $PackStagingDestination -Context 'Staged deployment'
    Assert-BatmanGraphicsPackConfig -Path $ConfigStagingPath -Context 'Staged'
    foreach ($stagedPath in @($ConfigStagingPath, $HelenGameHookStagingPath, $ProxyStagingPath)) {
        Assert-SafeDeploymentPath -Path $stagedPath -AllowedRoots @($GameBin) -RequireExisting | Out-Null
    }
    $StagedHelenGameHookHash = (Get-FileHash -LiteralPath $HelenGameHookStagingPath -Algorithm SHA256).Hash
    $StagedProxyHash = (Get-FileHash -LiteralPath $ProxyStagingPath -Algorithm SHA256).Hash
    $verifyPublication = {
        param($LivePackRootForVerification, $LiveConfigPathForVerification, $LiveHelenGameHookPathForVerification, $LiveProxyPathForVerification)
        Test-ExpectedGraphicsVirtualFile -PackRoot $LivePackRootForVerification -Context 'Activated deployment'
        Assert-BatmanGraphicsVsyncHooks -PackRoot $LivePackRootForVerification -Context 'Activated deployment'
        Assert-BatmanGraphicsPackConfig -Path $LiveConfigPathForVerification -Context 'Activated'
        Assert-SafeDeploymentPath -Path $LiveHelenGameHookPathForVerification -AllowedRoots @($GameBin) -RequireExisting | Out-Null
        Assert-SafeDeploymentPath -Path $LiveProxyPathForVerification -AllowedRoots @($GameBin) -RequireExisting | Out-Null
        if (-not [String]::Equals((Get-FileHash -LiteralPath $LiveHelenGameHookPathForVerification -Algorithm SHA256).Hash, $StagedHelenGameHookHash, [StringComparison]::OrdinalIgnoreCase)) { throw 'Activated HelenGameHook.dll differs from the staged binary.' }
        if (-not [String]::Equals((Get-FileHash -LiteralPath $LiveProxyPathForVerification -Algorithm SHA256).Hash, $StagedProxyHash, [StringComparison]::OrdinalIgnoreCase)) { throw 'Activated dinput8.dll differs from the staged binary.' }
        Assert-DirectorySnapshotEqual -Expected $SubtitleSnapshot -ActualRoot $SubtitlePackDestination -Context 'Activated subtitle pack'
    }.GetNewClosure()

    Invoke-AtomicGraphicsDeployment -GameBin $GameBin -LivePackRoot $PackDestination -LiveConfigPath $ConfigDestinationPath -LiveHelenGameHookPath (Join-Path $GameBin 'HelenGameHook.dll') -LiveProxyPath (Join-Path $GameBin 'dinput8.dll') -StagedPackRoot $PackStagingDestination -StagedConfigPath $ConfigStagingPath -StagedHelenGameHookPath $HelenGameHookStagingPath -StagedProxyPath $ProxyStagingPath -PackBackupRoot $PackBackupRoot -ConfigBackupPath $ConfigBackupPath -HelenGameHookBackupPath $HelenGameHookBackupPath -ProxyBackupPath $ProxyBackupPath -RecoveryRoot $DeploymentRecoveryRoot -StagingRoot $DeploymentStagingRoot -VerifyPublication $verifyPublication

    $logRoot = Join-Path $GameBin 'helengamehook\logs'
    if (Test-Path -LiteralPath $logRoot -PathType Container) {
        Assert-SafeDeploymentTree -Root $logRoot -AllowedRoots @($GameBin) -RequireExisting | Out-Null
        foreach ($logFile in @(Get-ChildItem -LiteralPath $logRoot -Force -File)) {
            Assert-SafeDeploymentPath -Path $logFile.FullName -AllowedRoots @($GameBin) -RequireExisting | Out-Null
            Remove-Item -LiteralPath $logFile.FullName -Force
        }
    }
    Write-Output 'DEPLOYED'
} catch {
    $primaryFailure = $_
    throw $primaryFailure
} finally {
    if ($stagingRootCreated) {
        try {
            if ($null -ne (Get-ExistingDeploymentItem -Path $DeploymentStagingRoot)) {
                Remove-DeploymentStagingRoot -StagingRoot $DeploymentStagingRoot -GameBin $GameBin
            }
        } catch {
            if ($null -ne $primaryFailure) { Write-Warning "Graphics deployment staging cleanup failed after the primary failure '$($primaryFailure.Exception.Message)': $($_.Exception.Message)" } else { throw "Graphics deployment staging cleanup failed: $($_.Exception.Message)" }
        }
    }
    if ($recoveryRootCreated) {
        try {
            if ($null -ne (Get-ExistingDeploymentItem -Path $DeploymentRecoveryRoot)) {
                Remove-EmptyDeploymentRecoveryRoot -RecoveryRoot $DeploymentRecoveryRoot -GameBin $GameBin
            }
        } catch {
            if ($null -ne $primaryFailure) { Write-Warning "Graphics deployment recovery cleanup failed after the primary failure '$($primaryFailure.Exception.Message)': $($_.Exception.Message)" } else { throw "Graphics deployment recovery cleanup failed: $($_.Exception.Message)" }
        }
    }
}
