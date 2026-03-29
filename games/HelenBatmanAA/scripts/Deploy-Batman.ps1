param(
    [string]$GameBin = 'D:\steam\steamapps\common\Batman Arkham Asylum GOTY\Binaries',
    [string]$Configuration = 'Debug'
)

$ErrorActionPreference = 'Stop'

$BatmanRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$RepoRoot = (Resolve-Path (Join-Path $BatmanRoot '..\..')).Path
$PackSource = Join-Path $BatmanRoot 'helengamehook\packs\batman-aa-subtitles'
$PackDestination = Join-Path $GameBin 'helengamehook\packs\batman-aa-subtitles'
$PackParent = Split-Path -Path $PackDestination -Parent
$PackLeaf = Split-Path -Path $PackDestination -Leaf
$PackStagingDestination = Join-Path $PackParent "$PackLeaf.staging.$([System.Guid]::NewGuid().ToString('N'))"
$PackBackupDestination = Join-Path $PackParent "$PackLeaf.backup.$([System.Guid]::NewGuid().ToString('N'))"
$PackBuildRoot = Join-Path $PackDestination 'builds\steam-goty-1.0'
$DeployedFilesJsonPath = Join-Path $PackBuildRoot 'files.json'
$DeployedDeltaPath = Join-Path $PackBuildRoot 'assets\deltas\BmGame-subtitle-signal.hgdelta'
$DeployedPackagesPath = Join-Path $PackBuildRoot 'assets\packages'
$StagedBuildRoot = Join-Path $PackStagingDestination 'builds\steam-goty-1.0'
$StagedFilesJsonPath = Join-Path $StagedBuildRoot 'files.json'
$StagedDeltaPath = Join-Path $StagedBuildRoot 'assets\deltas\BmGame-subtitle-signal.hgdelta'
$StagedPackagesPath = Join-Path $StagedBuildRoot 'assets\packages'
$SourceDeltaPath = Join-Path $PackSource 'builds\steam-goty-1.0\assets\deltas\BmGame-subtitle-signal.hgdelta'
$SourcePackagesPath = Join-Path $PackSource 'assets\packages'
$VerifierPath = Join-Path $PSScriptRoot 'Test-BatmanKnownGoodGameplayPackage.ps1'
$HelenGameHookPath = Join-Path $RepoRoot "bin\Win32\$Configuration\HelenGameHook.dll"
$ProxyPath = Join-Path $RepoRoot "bin\Win32\$Configuration\dinput8.dll"
$ExpectedVirtualFileId = 'bmgameGameplayPackage'
$ExpectedVirtualFilePath = 'BmGame/CookedPC/BmGame.u'
$ExpectedVirtualFileMode = 'delta-on-read'
$ExpectedVirtualFileKind = 'delta-file'
$ExpectedDeltaPath = 'assets/deltas/BmGame-subtitle-signal.hgdelta'
$ExpectedGameplayDeltaHash = (Get-FileHash -LiteralPath $SourceDeltaPath -Algorithm SHA256).Hash

foreach ($RequiredPath in @($PackSource, $SourceDeltaPath, $HelenGameHookPath, $ProxyPath)) {
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

    if (-not (Test-Path -LiteralPath $StagedFilesJsonPath)) {
        throw "Batman staged deployment manifest not found: $StagedFilesJsonPath"
    }

    if (-not (Test-Path -LiteralPath $StagedDeltaPath)) {
        throw "Batman staged deployment delta not found: $StagedDeltaPath"
    }

    $StagedManifest = Get-Content -LiteralPath $StagedFilesJsonPath -Raw | ConvertFrom-Json
    $VirtualFiles = @($StagedManifest.virtualFiles)
    if ($VirtualFiles.Count -ne 1) {
        throw "Batman staged deployment manifest expected exactly one virtual file, found $($VirtualFiles.Count)."
    }

    $GameplayFile = $VirtualFiles[0]
    if ($GameplayFile.id -ne $ExpectedVirtualFileId) {
        throw "Batman staged deployment virtual file id mismatch. Expected $ExpectedVirtualFileId but found $($GameplayFile.id)."
    }

    if ($GameplayFile.path -ne $ExpectedVirtualFilePath) {
        throw "Batman staged deployment virtual file path mismatch. Expected $ExpectedVirtualFilePath but found $($GameplayFile.path)."
    }

    if ($GameplayFile.mode -ne $ExpectedVirtualFileMode) {
        throw "Batman staged deployment virtual file mode mismatch. Expected $ExpectedVirtualFileMode but found $($GameplayFile.mode)."
    }

    if ($GameplayFile.source.kind -ne $ExpectedVirtualFileKind) {
        throw "Batman staged deployment source kind mismatch. Expected $ExpectedVirtualFileKind but found $($GameplayFile.source.kind)."
    }

    if ($GameplayFile.source.path -ne $ExpectedDeltaPath) {
        throw "Batman staged deployment delta path mismatch. Expected $ExpectedDeltaPath but found $($GameplayFile.source.path)."
    }

    $StagedGameplayDeltaHash = (Get-FileHash -LiteralPath $StagedDeltaPath -Algorithm SHA256).Hash
    if (-not [string]::Equals($StagedGameplayDeltaHash, $ExpectedGameplayDeltaHash, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "Batman staged deployment delta hash mismatch. Expected $ExpectedGameplayDeltaHash but found $StagedGameplayDeltaHash."
    }

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

if (-not (Test-Path -LiteralPath $DeployedFilesJsonPath)) {
    throw "Batman deployment manifest not found: $DeployedFilesJsonPath"
}

if (-not (Test-Path -LiteralPath $DeployedDeltaPath)) {
    throw "Batman deployment delta not found: $DeployedDeltaPath"
}

$DeployedManifest = Get-Content -LiteralPath $DeployedFilesJsonPath -Raw | ConvertFrom-Json
$VirtualFiles = @($DeployedManifest.virtualFiles)
if ($VirtualFiles.Count -ne 1) {
    throw "Batman deployment manifest expected exactly one virtual file, found $($VirtualFiles.Count)."
}

$GameplayFile = $VirtualFiles[0]
if ($GameplayFile.id -ne $ExpectedVirtualFileId) {
    throw "Batman deployment virtual file id mismatch. Expected $ExpectedVirtualFileId but found $($GameplayFile.id)."
}

if ($GameplayFile.path -ne $ExpectedVirtualFilePath) {
    throw "Batman deployment virtual file path mismatch. Expected $ExpectedVirtualFilePath but found $($GameplayFile.path)."
}

if ($GameplayFile.mode -ne $ExpectedVirtualFileMode) {
    throw "Batman deployment virtual file mode mismatch. Expected $ExpectedVirtualFileMode but found $($GameplayFile.mode)."
}

if ($GameplayFile.source.kind -ne $ExpectedVirtualFileKind) {
    throw "Batman deployment source kind mismatch. Expected $ExpectedVirtualFileKind but found $($GameplayFile.source.kind)."
}

if ($GameplayFile.source.path -ne $ExpectedDeltaPath) {
    throw "Batman deployment delta path mismatch. Expected $ExpectedDeltaPath but found $($GameplayFile.source.path)."
}

$ActualGameplayDeltaHash = (Get-FileHash -LiteralPath $DeployedDeltaPath -Algorithm SHA256).Hash
if (-not [string]::Equals($ActualGameplayDeltaHash, $ExpectedGameplayDeltaHash, [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "Batman deployment delta hash mismatch. Expected $ExpectedGameplayDeltaHash but found $ActualGameplayDeltaHash."
}

Write-Output 'DEPLOYED'
