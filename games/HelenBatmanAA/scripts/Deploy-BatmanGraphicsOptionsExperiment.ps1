param(
    [string]$GameBin = 'D:\steam\steamapps\common\Batman Arkham Asylum GOTY\Binaries',
    [string]$BuilderRoot,
    [string]$Configuration = 'Release',
    [switch]$FunctionsOnly
)

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'BatmanBuilderWorkspaceHelpers.ps1')

$BatmanRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$RepoRoot = (Resolve-Path (Join-Path $BatmanRoot '..\..')).Path
$BuilderRoot = Resolve-OptionalBuilderRoot -BatmanRootPath $BatmanRoot -BuilderRootPath $BuilderRoot
$GameRoot = [IO.Path]::GetFullPath((Join-Path $GameBin '..'))

$PackSource = Join-Path $BatmanRoot 'helengamehook\packs\batman-aa-graphics-options'
$PackDestination = Join-Path $GameBin 'helengamehook\packs\batman-aa-graphics-options'
$PackParent = Split-Path -Path $PackDestination -Parent
$ConfigSourcePath = Join-Path $BatmanRoot 'helengamehook\config\packs.json'
$ConfigDestinationPath = Join-Path $GameBin 'helengamehook\config\packs.json'
$ConfigParent = Split-Path -Path $ConfigDestinationPath -Parent
$SubtitlePackDestination = Join-Path $GameBin 'helengamehook\packs\batman-aa-subtitles'
$RebuildScriptPath = Join-Path $PSScriptRoot 'Rebuild-BatmanGraphicsOptionsExperiment.ps1'
$VerifierPath = Join-Path $PSScriptRoot 'Test-BatmanGraphicsOptionsPackage.ps1'
$InstalledBaseVerifierPath = Join-Path $PSScriptRoot 'Test-BatmanInstalledBaseCompatibility.ps1'
$HelenGameHookPath = Join-Path $RepoRoot "bin\Win32\$Configuration\HelenGameHook.dll"
$ProxyPath = Join-Path $RepoRoot "bin\Win32\$Configuration\dinput8.dll"
$DeploymentId = [Guid]::NewGuid().ToString('N')
$SystemTempRoot = [IO.Path]::GetTempPath()
$DeploymentTempRoot = Join-Path $SystemTempRoot "HelenBatmanGraphicsDeployment-$DeploymentId"
$PackStagingDestination = Join-Path $DeploymentTempRoot 'pack'
$ConfigStagingPath = Join-Path $DeploymentTempRoot 'config\packs.json'
$ConfigStagingParent = Split-Path -Path $ConfigStagingPath -Parent
$BackupRoot = Join-Path $SystemTempRoot "HelenBatmanGraphicsDeploymentBackup-$DeploymentId"
$PackBackupRoot = Join-Path $BackupRoot 'pack'
$ConfigBackupPath = Join-Path $BackupRoot 'packs.json'

function Assert-BatmanGraphicsPackConfig {
    <# Validate the only supported checkpoint configuration and its pack order. #>
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path,
        [Parameter(Mandatory = $true)]
        [string]$Context
    )

    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw "$Context pack configuration was not found: $Path"
    }

    $config = Get-Content -LiteralPath $Path -Raw | ConvertFrom-Json
    $enabledPacks = @($config.enabledPacksByExecutable.'ShippingPC-BmGame.exe')
    if ($enabledPacks.Count -ne 2) {
        throw "$Context pack configuration must contain exactly two enabled packs, found $($enabledPacks.Count)."
    }
    if ($enabledPacks[0] -ne 'batman-aa-subtitles' -or $enabledPacks[1] -ne 'batman-aa-graphics-options') {
        throw "$Context pack configuration must enable batman-aa-subtitles before batman-aa-graphics-options."
    }
}

function Test-ExpectedGraphicsVirtualFile {
    <# Verify that one staged or activated graphics shell manifest names its delta file. #>
    param(
        [Parameter(Mandatory = $true)]
        [string]$PackRoot,
        [Parameter(Mandatory = $true)]
        [string]$Context
    )

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
    if ($virtualFile.id -ne 'frontendGraphicsOptionsPackage' -or
        $virtualFile.path -ne 'BmGame/CookedPC/Maps/Frontend/Frontend.umap' -or
        $virtualFile.mode -ne 'delta-on-read' -or
        $virtualFile.source.kind -ne 'delta-file' -or
        $virtualFile.source.path -ne 'assets/deltas/Frontend-graphics-options.hgdelta') {
        throw "$Context graphics shell virtual-file contract drifted."
    }

    $deltaPath = Join-Path $buildRoot 'assets\deltas\Frontend-graphics-options.hgdelta'
    if (-not (Test-Path -LiteralPath $deltaPath -PathType Leaf)) {
        throw "$Context graphics shell delta was not found: $deltaPath"
    }
}

function Invoke-DeploymentFailureInjection {
    <# Inject a deterministic transition failure for the isolated rollback tests. #>
    param(
        [Parameter(Mandatory = $true)]
        [AllowEmptyString()]
        [string]$FailureInjection,
        [Parameter(Mandatory = $true)]
        [string]$Point
    )

    if ($FailureInjection -eq $Point) {
        throw "Injected graphics deployment failure at $Point."
    }
}

function Restore-AtomicGraphicsDeployment {
    <# Restore the previous graphics pack and repository config after a pre-commit failure. #>
    param(
        [Parameter(Mandatory = $true)] [string]$LivePackRoot,
        [Parameter(Mandatory = $true)] [string]$LiveConfigPath,
        [Parameter(Mandatory = $true)] [string]$PackBackupRoot,
        [Parameter(Mandatory = $true)] [string]$ConfigBackupPath,
        [bool]$PackWasBackedUp,
        [bool]$ConfigWasBackedUp,
        [bool]$PackWasInstalled,
        [bool]$ConfigWasInstalled
    )

    if ($PackWasInstalled -and (Test-Path -LiteralPath $LivePackRoot)) {
        Remove-Item -LiteralPath $LivePackRoot -Recurse -Force
    }
    if ($PackWasBackedUp -and (Test-Path -LiteralPath $PackBackupRoot)) {
        Move-Item -LiteralPath $PackBackupRoot -Destination $LivePackRoot
    }

    if ($ConfigWasInstalled -and (Test-Path -LiteralPath $LiveConfigPath)) {
        Remove-Item -LiteralPath $LiveConfigPath -Force
    }
    if ($ConfigWasBackedUp -and (Test-Path -LiteralPath $ConfigBackupPath)) {
        Move-Item -LiteralPath $ConfigBackupPath -Destination $LiveConfigPath
    }
}

function Invoke-AtomicGraphicsDeployment {
    <# Publish the pack and config with rollback until both are verified live. #>
    param(
        [Parameter(Mandatory = $true)] [string]$LivePackRoot,
        [Parameter(Mandatory = $true)] [string]$LiveConfigPath,
        [Parameter(Mandatory = $true)] [string]$StagedPackRoot,
        [Parameter(Mandatory = $true)] [string]$StagedConfigPath,
        [Parameter(Mandatory = $true)] [string]$PackBackupRoot,
        [Parameter(Mandatory = $true)] [string]$ConfigBackupPath,
        [Parameter(Mandatory = $true)] [string]$BackupRoot,
        [Parameter(Mandatory = $true)] [string]$TempRoot,
        [Parameter(Mandatory = $true)] [scriptblock]$VerifyPublication,
        [ValidateSet('', 'AfterPackBackup', 'AfterConfigBackup', 'AfterPackActivation', 'AfterConfigActivation', 'AfterVerification', 'AfterBackupDeletion')]
        [string]$FailureInjection = ''
    )

    $fullTempRoot = [IO.Path]::GetFullPath($TempRoot).TrimEnd('\') + '\'
    $fullBackupRoot = [IO.Path]::GetFullPath($BackupRoot).TrimEnd('\') + '\'
    if ($fullBackupRoot.StartsWith($fullTempRoot, [StringComparison]::OrdinalIgnoreCase)) {
        throw 'Graphics deployment recovery backups must live outside the staging tree.'
    }
    if (-not (Test-Path -LiteralPath $StagedPackRoot -PathType Container)) {
        throw "Staged graphics pack was not found: $StagedPackRoot"
    }
    if (-not (Test-Path -LiteralPath $StagedConfigPath -PathType Leaf)) {
        throw "Staged graphics pack config was not found: $StagedConfigPath"
    }
    if (Test-Path -LiteralPath $BackupRoot) {
        $existingRecoveryEntries = @(Get-ChildItem -LiteralPath $BackupRoot -Force)
        if ($existingRecoveryEntries.Count -ne 0) {
            throw "Graphics deployment recovery directory is not empty: $BackupRoot"
        }
    } else {
        New-Item -ItemType Directory -Force -Path $BackupRoot | Out-Null
    }
    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $PackBackupRoot) | Out-Null

    $packWasBackedUp = $false
    $configWasBackedUp = $false
    $packWasInstalled = $false
    $configWasInstalled = $false
    $publicationCommitted = $false

    try {
        if (Test-Path -LiteralPath $LivePackRoot) {
            Move-Item -LiteralPath $LivePackRoot -Destination $PackBackupRoot
            $packWasBackedUp = $true
        }
        Invoke-DeploymentFailureInjection -FailureInjection $FailureInjection -Point 'AfterPackBackup'

        if (Test-Path -LiteralPath $LiveConfigPath) {
            Move-Item -LiteralPath $LiveConfigPath -Destination $ConfigBackupPath
            $configWasBackedUp = $true
        }
        Invoke-DeploymentFailureInjection -FailureInjection $FailureInjection -Point 'AfterConfigBackup'

        Move-Item -LiteralPath $StagedPackRoot -Destination $LivePackRoot
        $packWasInstalled = $true
        Invoke-DeploymentFailureInjection -FailureInjection $FailureInjection -Point 'AfterPackActivation'

        Move-Item -LiteralPath $StagedConfigPath -Destination $LiveConfigPath
        $configWasInstalled = $true
        Invoke-DeploymentFailureInjection -FailureInjection $FailureInjection -Point 'AfterConfigActivation'

        & $VerifyPublication $LivePackRoot $LiveConfigPath
        Invoke-DeploymentFailureInjection -FailureInjection $FailureInjection -Point 'AfterVerification'

        # Verification establishes the commit point. Recovery copies remain outside the
        # staging tree so a cleanup failure cannot erase the only rollback state.
        $publicationCommitted = $true
        if ($packWasBackedUp -and (Test-Path -LiteralPath $PackBackupRoot)) {
            Remove-Item -LiteralPath $PackBackupRoot -Recurse -Force
            Invoke-DeploymentFailureInjection -FailureInjection $FailureInjection -Point 'AfterBackupDeletion'
        } elseif ($configWasBackedUp -and (Test-Path -LiteralPath $ConfigBackupPath)) {
            Remove-Item -LiteralPath $ConfigBackupPath -Force
            Invoke-DeploymentFailureInjection -FailureInjection $FailureInjection -Point 'AfterBackupDeletion'
        }
        if ($configWasBackedUp -and (Test-Path -LiteralPath $ConfigBackupPath)) {
            Remove-Item -LiteralPath $ConfigBackupPath -Force
        }
        if (Test-Path -LiteralPath $BackupRoot) {
            $remainingRecoveryEntries = @(Get-ChildItem -LiteralPath $BackupRoot -Force)
            if ($remainingRecoveryEntries.Count -eq 0) {
                Remove-Item -LiteralPath $BackupRoot -Force
            }
        }
    } catch {
        $deploymentFailure = $_
        if (-not $publicationCommitted) {
            try {
                Restore-AtomicGraphicsDeployment -LivePackRoot $LivePackRoot -LiveConfigPath $LiveConfigPath -PackBackupRoot $PackBackupRoot -ConfigBackupPath $ConfigBackupPath -PackWasBackedUp $packWasBackedUp -ConfigWasBackedUp $configWasBackedUp -PackWasInstalled $packWasInstalled -ConfigWasInstalled $configWasInstalled
            } catch {
                throw "Graphics deployment failed and rollback failed. Original error: $($deploymentFailure.Exception.Message). Rollback error: $($_.Exception.Message)"
            }
        }
        throw $deploymentFailure
    }
}

if ($FunctionsOnly) {
    return
}

foreach ($requiredPath in @($RebuildScriptPath, $VerifierPath, $InstalledBaseVerifierPath, $ConfigSourcePath)) {
    if (-not (Test-Path -LiteralPath $requiredPath)) {
        throw "Batman graphics deployment input not found: $requiredPath"
    }
}

& $RebuildScriptPath -BatmanRoot $BatmanRoot -BuilderRoot $BuilderRoot -Configuration $Configuration
if ($LASTEXITCODE -ne 0) {
    throw "Batman graphics shell rebuild failed with exit code $LASTEXITCODE."
}

Assert-BatmanGraphicsPackConfig -Path $ConfigSourcePath -Context 'Repository'

try {
    & $VerifierPath -BatmanRoot $BatmanRoot -BuilderRoot $BuilderRoot -Configuration $Configuration
    if ($LASTEXITCODE -ne 0) {
        throw "Batman graphics-options package verification failed with exit code $LASTEXITCODE."
    }
} catch {
    throw "Batman graphics-options package verification failed before deployment. $($_.Exception.Message)"
}

try {
    & $InstalledBaseVerifierPath -GameRoot $GameRoot -PackBuildRoot (Join-Path $PackSource 'builds\steam-goty-1.0')
    if ($LASTEXITCODE -ne 0) {
        throw "Installed Batman base compatibility verification failed with exit code $LASTEXITCODE."
    }
} catch {
    throw "Batman graphics-options deployment refused to target an incompatible installed base. Installed Batman base hash mismatch or missing retail input: $($_.Exception.Message)"
}

foreach ($runtimePath in @($HelenGameHookPath, $ProxyPath)) {
    if (-not (Test-Path -LiteralPath $runtimePath -PathType Leaf)) {
        throw "Batman graphics deployment runtime input not found: $runtimePath"
    }
}

Get-Process ShippingPC-BmGame -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Seconds 2

$primaryFailure = $null
try {
    New-Item -ItemType Directory -Force -Path $DeploymentTempRoot, $PackStagingDestination, $ConfigStagingParent, $PackParent, $ConfigParent | Out-Null
    foreach ($sourceChild in @(Get-ChildItem -LiteralPath $PackSource -Force)) {
        Copy-Item -LiteralPath $sourceChild.FullName -Destination $PackStagingDestination -Recurse -Force
    }
    Copy-Item -LiteralPath $ConfigSourcePath -Destination $ConfigStagingPath -Force

    Test-ExpectedGraphicsVirtualFile -PackRoot $PackStagingDestination -Context 'Staged deployment'
    Assert-BatmanGraphicsPackConfig -Path $ConfigStagingPath -Context 'Staged'

    $stagedHooksJsonPath = Join-Path $PackStagingDestination 'builds\steam-goty-1.0\hooks.json'
    if (Test-Path -LiteralPath $stagedHooksJsonPath) {
        throw "Batman graphics-options staged pack should not declare hooks.json: $stagedHooksJsonPath"
    }

    Copy-Item -LiteralPath $HelenGameHookPath -Destination (Join-Path $GameBin 'HelenGameHook.dll') -Force
    Copy-Item -LiteralPath $ProxyPath -Destination (Join-Path $GameBin 'dinput8.dll') -Force

    $verifyPublication = {
        param($LivePackRootForVerification, $LiveConfigPathForVerification)
        Test-ExpectedGraphicsVirtualFile -PackRoot $LivePackRootForVerification -Context 'Activated deployment'
        Assert-BatmanGraphicsPackConfig -Path $LiveConfigPathForVerification -Context 'Activated'
        if (-not (Test-Path -LiteralPath $SubtitlePackDestination -PathType Container)) {
            throw "Activated deployment removed the working subtitle pack: $SubtitlePackDestination"
        }
    }.GetNewClosure()

    Invoke-AtomicGraphicsDeployment -LivePackRoot $PackDestination -LiveConfigPath $ConfigDestinationPath -StagedPackRoot $PackStagingDestination -StagedConfigPath $ConfigStagingPath -PackBackupRoot $PackBackupRoot -ConfigBackupPath $ConfigBackupPath -BackupRoot $BackupRoot -TempRoot $DeploymentTempRoot -VerifyPublication $verifyPublication

    $logRoot = Join-Path $GameBin 'helengamehook\logs'
    if (Test-Path -LiteralPath $logRoot -PathType Container) {
        foreach ($logFile in @(Get-ChildItem -LiteralPath $logRoot -Force -File)) {
            Remove-Item -LiteralPath $logFile.FullName -Force
        }
    }

    Write-Output 'DEPLOYED'
} catch {
    $primaryFailure = $_
    throw $primaryFailure
} finally {
    if (Test-Path -LiteralPath $DeploymentTempRoot) {
        try {
            Remove-Item -LiteralPath $DeploymentTempRoot -Recurse -Force
        } catch {
            if ($null -ne $primaryFailure) {
                Write-Warning "Graphics deployment staging cleanup failed after the primary failure '$($primaryFailure.Exception.Message)': $($_.Exception.Message)"
            } else {
                throw "Graphics deployment staging cleanup failed: $($_.Exception.Message)"
            }
        }
    }
}
