param(
    [string]$GameBin = 'D:\steam\steamapps\common\Batman Arkham Asylum GOTY\Binaries',
    [string]$Configuration = 'Debug'
)

$ErrorActionPreference = 'Stop'

$BatmanRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$RepoRoot = (Resolve-Path (Join-Path $BatmanRoot '..\..')).Path
$GameRoot = [System.IO.Path]::GetFullPath((Join-Path $GameBin '..'))
$PackSource = Join-Path $BatmanRoot 'helengamehook\packs\batman-aa-subtitles'
$PackDestination = Join-Path $GameBin 'helengamehook\packs\batman-aa-subtitles'
$PackParent = Split-Path -Path $PackDestination -Parent
$PackLeaf = Split-Path -Path $PackDestination -Leaf
$PackStagingDestination = Join-Path $PackParent "$PackLeaf.staging.$([System.Guid]::NewGuid().ToString('N'))"
$PackBackupDestination = Join-Path $PackParent "$PackLeaf.backup.$([System.Guid]::NewGuid().ToString('N'))"
$PackBuildRoot = Join-Path $PackDestination 'builds\steam-goty-1.0'
$DeployedFilesJsonPath = Join-Path $PackBuildRoot 'files.json'
$DeployedPackagesPath = Join-Path $PackBuildRoot 'assets\packages'
$StagedBuildRoot = Join-Path $PackStagingDestination 'builds\steam-goty-1.0'
$StagedFilesJsonPath = Join-Path $StagedBuildRoot 'files.json'
$StagedPackagesPath = Join-Path $StagedBuildRoot 'assets\packages'
$SourceGameplayDeltaPath = Join-Path $PackSource 'builds\steam-goty-1.0\assets\deltas\BmGame-subtitle-signal.hgdelta'
$SourceFrontendDeltaPath = Join-Path $PackSource 'builds\steam-goty-1.0\assets\deltas\Frontend-main-menu-subtitle-size.hgdelta'
$SourcePackagesPath = Join-Path $PackSource 'assets\packages'
$VerifierPath = Join-Path $PSScriptRoot 'Test-BatmanKnownGoodGameplayPackage.ps1'
$InstalledBaseVerifierPath = Join-Path $PSScriptRoot 'Test-BatmanInstalledBaseCompatibility.ps1'
$HelenGameHookPath = Join-Path $RepoRoot "bin\Win32\$Configuration\HelenGameHook.dll"
$ProxyPath = Join-Path $RepoRoot "bin\Win32\$Configuration\dinput8.dll"
$ExpectedVirtualFiles = @(
    @{
        Id = 'bmgameGameplayPackage'
        Path = 'BmGame/CookedPC/BmGame.u'
        Mode = 'delta-on-read'
        Kind = 'delta-file'
        DeltaPath = 'assets/deltas/BmGame-subtitle-signal.hgdelta'
        SourceDeltaPath = $SourceGameplayDeltaPath
        DeltaHash = (Get-FileHash -LiteralPath $SourceGameplayDeltaPath -Algorithm SHA256).Hash
    },
    @{
        Id = 'frontendMapPackage'
        Path = 'BmGame/CookedPC/Maps/Frontend/Frontend.umap'
        Mode = 'delta-on-read'
        Kind = 'delta-file'
        DeltaPath = 'assets/deltas/Frontend-main-menu-subtitle-size.hgdelta'
        SourceDeltaPath = $SourceFrontendDeltaPath
        DeltaHash = (Get-FileHash -LiteralPath $SourceFrontendDeltaPath -Algorithm SHA256).Hash
    }
)

function Test-ExpectedVirtualFiles {
    param(
        [string]$ManifestPath,
        [string]$BuildRoot,
        [string]$Label
    )

    if (-not (Test-Path -LiteralPath $ManifestPath)) {
        throw "Batman $Label manifest not found: $ManifestPath"
    }

    $Manifest = Get-Content -LiteralPath $ManifestPath -Raw | ConvertFrom-Json
    $VirtualFiles = @($Manifest.virtualFiles)
    if ($VirtualFiles.Count -ne $ExpectedVirtualFiles.Count) {
        throw "Batman $Label manifest expected exactly $($ExpectedVirtualFiles.Count) virtual files, found $($VirtualFiles.Count)."
    }

    foreach ($ExpectedVirtualFile in $ExpectedVirtualFiles) {
        $VirtualFile = @($VirtualFiles | Where-Object { $_.id -eq $ExpectedVirtualFile.Id })[0]
        if ($null -eq $VirtualFile) {
            throw "Batman $Label manifest is missing virtual file id $($ExpectedVirtualFile.Id)."
        }

        if ($VirtualFile.path -ne $ExpectedVirtualFile.Path) {
            throw "Batman $Label virtual file path mismatch for $($ExpectedVirtualFile.Id). Expected $($ExpectedVirtualFile.Path) but found $($VirtualFile.path)."
        }

        if ($VirtualFile.mode -ne $ExpectedVirtualFile.Mode) {
            throw "Batman $Label virtual file mode mismatch for $($ExpectedVirtualFile.Id). Expected $($ExpectedVirtualFile.Mode) but found $($VirtualFile.mode)."
        }

        if ($VirtualFile.source.kind -ne $ExpectedVirtualFile.Kind) {
            throw "Batman $Label source kind mismatch for $($ExpectedVirtualFile.Id). Expected $($ExpectedVirtualFile.Kind) but found $($VirtualFile.source.kind)."
        }

        if ($VirtualFile.source.path -ne $ExpectedVirtualFile.DeltaPath) {
            throw "Batman $Label delta path mismatch for $($ExpectedVirtualFile.Id). Expected $($ExpectedVirtualFile.DeltaPath) but found $($VirtualFile.source.path)."
        }

        $ActualDeltaPath = Join-Path $BuildRoot ($ExpectedVirtualFile.DeltaPath -replace '/', '\')
        if (-not (Test-Path -LiteralPath $ActualDeltaPath)) {
            throw "Batman $Label delta not found: $ActualDeltaPath"
        }

        $ActualDeltaHash = (Get-FileHash -LiteralPath $ActualDeltaPath -Algorithm SHA256).Hash
        if (-not [string]::Equals($ActualDeltaHash, $ExpectedVirtualFile.DeltaHash, [System.StringComparison]::OrdinalIgnoreCase)) {
            throw "Batman $Label delta hash mismatch for $($ExpectedVirtualFile.Id). Expected $($ExpectedVirtualFile.DeltaHash) but found $ActualDeltaHash."
        }
    }
}

foreach ($RequiredPath in @($PackSource, $SourceGameplayDeltaPath, $SourceFrontendDeltaPath, $HelenGameHookPath, $ProxyPath, $InstalledBaseVerifierPath)) {
    if (-not (Test-Path -LiteralPath $RequiredPath)) {
        throw "Batman deployment input not found: $RequiredPath"
    }
}

if (Test-Path -LiteralPath $SourcePackagesPath) {
    $LegacySourcePackages = @(Get-ChildItem -LiteralPath $SourcePackagesPath -Force -ErrorAction SilentlyContinue)
    if ($LegacySourcePackages.Count -gt 0) {
        throw "Batman source pack still contains legacy package payloads: $SourcePackagesPath"
    }
}

& $VerifierPath -BatmanRoot $BatmanRoot
& $InstalledBaseVerifierPath -GameRoot $GameRoot -PackBuildRoot (Join-Path $PackSource 'builds\steam-goty-1.0')

Get-Process ShippingPC-BmGame -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Seconds 2

New-Item -ItemType Directory -Force -Path $PackParent | Out-Null

try {
    New-Item -ItemType Directory -Force -Path $PackStagingDestination | Out-Null
    Copy-Item -Path (Join-Path $PackSource '*') -Destination $PackStagingDestination -Recurse -Force

    if (Test-Path -LiteralPath $StagedPackagesPath) {
        $StagedLegacyPackages = @(Get-ChildItem -LiteralPath $StagedPackagesPath -Force -ErrorAction SilentlyContinue)
        if ($StagedLegacyPackages.Count -gt 0) {
            throw "Batman staged pack still contains legacy package payloads: $StagedPackagesPath"
        }

        Remove-Item -LiteralPath $StagedPackagesPath -Recurse -Force
    }

    Test-ExpectedVirtualFiles -ManifestPath $StagedFilesJsonPath -BuildRoot $StagedBuildRoot -Label 'staged deployment'

    Copy-Item -LiteralPath $HelenGameHookPath -Destination (Join-Path $GameBin 'HelenGameHook.dll') -Force
    Copy-Item -LiteralPath $ProxyPath -Destination (Join-Path $GameBin 'dinput8.dll') -Force

    $BackupExists = $false
    if (Test-Path -LiteralPath $PackDestination) {
        Move-Item -LiteralPath $PackDestination -Destination $PackBackupDestination
        $BackupExists = $true
    }

    Move-Item -LiteralPath $PackStagingDestination -Destination $PackDestination

    if ($BackupExists -and (Test-Path -LiteralPath $PackBackupDestination)) {
        Remove-Item -LiteralPath $PackBackupDestination -Recurse -Force
    }
} catch {
    $DeploymentFailure = $_

    if (Test-Path -LiteralPath $PackStagingDestination) {
        Remove-Item -LiteralPath $PackStagingDestination -Recurse -Force
    }

    if ((Test-Path -LiteralPath $PackBackupDestination) -and -not (Test-Path -LiteralPath $PackDestination)) {
        try {
            Move-Item -LiteralPath $PackBackupDestination -Destination $PackDestination
        } catch {
            throw "Batman deployment failed and the previous pack could not be restored from $PackBackupDestination. Original error: $($DeploymentFailure.Exception.Message). Restore error: $($_.Exception.Message)"
        }
    }

    throw $DeploymentFailure
}

Get-ChildItem (Join-Path $GameBin 'helengamehook\logs') -ErrorAction SilentlyContinue | Remove-Item -Force

if (Test-Path -LiteralPath $DeployedPackagesPath) {
    throw "Batman deployment should not contain legacy packages after copy: $DeployedPackagesPath"
}

Test-ExpectedVirtualFiles -ManifestPath $DeployedFilesJsonPath -BuildRoot $PackBuildRoot -Label 'deployment'

Write-Output 'DEPLOYED'
