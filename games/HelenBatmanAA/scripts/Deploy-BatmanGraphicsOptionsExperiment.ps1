param(
    [string]$GameBin = 'D:\steam\steamapps\common\Batman Arkham Asylum GOTY\Binaries',
    [string]$Configuration = 'Debug'
)

$ErrorActionPreference = 'Stop'

$BatmanRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$RepoRoot = (Resolve-Path (Join-Path $BatmanRoot '..\..')).Path
$GameRoot = [System.IO.Path]::GetFullPath((Join-Path $GameBin '..'))
$PackSource = Join-Path $BatmanRoot 'helengamehook\packs\batman-aa-graphics-options'
$PackDestination = Join-Path $GameBin 'helengamehook\packs\batman-aa-graphics-options'
$PackParent = Split-Path -Path $PackDestination -Parent
$PackLeaf = Split-Path -Path $PackDestination -Leaf
$PackStagingDestination = Join-Path $PackParent "$PackLeaf.staging.$([System.Guid]::NewGuid().ToString('N'))"
$PackBackupDestination = Join-Path $PackParent "$PackLeaf.backup.$([System.Guid]::NewGuid().ToString('N'))"
$PackBuildRoot = Join-Path $PackDestination 'builds\steam-goty-1.0'
$DeployedFilesJsonPath = Join-Path $PackBuildRoot 'files.json'
$SourceDeltaPath = Join-Path $PackSource 'builds\steam-goty-1.0\assets\deltas\Frontend-graphics-options.hgdelta'
$HelenGameHookPath = Join-Path $RepoRoot "bin\Win32\$Configuration\HelenGameHook.dll"
$ProxyPath = Join-Path $RepoRoot "bin\Win32\$Configuration\dinput8.dll"
$VerifierPath = Join-Path $PSScriptRoot 'Test-BatmanGraphicsOptionsPackage.ps1'

function Test-ExpectedVirtualFiles {
    param(
        [Parameter(Mandatory = $true)]
        [string]$ManifestPath,
        [Parameter(Mandatory = $true)]
        [string]$BuildRoot,
        [Parameter(Mandatory = $true)]
        [string]$Label
    )

    if (-not (Test-Path -LiteralPath $ManifestPath)) {
        throw "Batman $Label manifest not found: $ManifestPath"
    }

    $Manifest = Get-Content -LiteralPath $ManifestPath -Raw | ConvertFrom-Json
    $VirtualFiles = @($Manifest.virtualFiles)
    if ($VirtualFiles.Count -ne 1) {
        throw "Batman $Label manifest expected exactly 1 virtual file, found $($VirtualFiles.Count)."
    }

    $VirtualFile = $VirtualFiles[0]
    if ($VirtualFile.id -ne 'frontendGraphicsOptionsPackage') {
        throw "Batman $Label manifest virtual file id mismatch."
    }

    if ($VirtualFile.path -ne 'BmGame/CookedPC/Maps/Frontend/Frontend.umap') {
        throw "Batman $Label manifest virtual file path mismatch."
    }

    if ($VirtualFile.mode -ne 'delta-on-read') {
        throw "Batman $Label manifest virtual file mode mismatch."
    }

    if ($VirtualFile.source.kind -ne 'delta-file') {
        throw "Batman $Label manifest source kind mismatch."
    }

    if ($VirtualFile.source.path -ne 'assets/deltas/Frontend-graphics-options.hgdelta') {
        throw "Batman $Label manifest delta path mismatch."
    }

    $DeltaPath = Join-Path $BuildRoot 'assets\deltas\Frontend-graphics-options.hgdelta'
    if (-not (Test-Path -LiteralPath $DeltaPath)) {
        throw "Batman $Label delta not found: $DeltaPath"
    }
}

foreach ($RequiredPath in @($PackSource, $SourceDeltaPath, $HelenGameHookPath, $ProxyPath, $VerifierPath)) {
    if (-not (Test-Path -LiteralPath $RequiredPath)) {
        throw "Batman deployment input not found: $RequiredPath"
    }
}

try {
    & $VerifierPath -BatmanRoot $BatmanRoot -BuilderRoot (Join-Path $BatmanRoot 'builder')
} catch {
    throw "Batman graphics-options package verification failed before deployment. $($_.Exception.Message)"
}

Get-Process ShippingPC-BmGame -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Seconds 2

New-Item -ItemType Directory -Force -Path $PackParent | Out-Null

$HadExistingPack = $false
$ActivatedNewPack = $false

try {
    New-Item -ItemType Directory -Force -Path $PackStagingDestination | Out-Null
    Copy-Item -Path (Join-Path $PackSource '*') -Destination $PackStagingDestination -Recurse -Force

    if (Test-Path -LiteralPath $PackDestination) {
        Move-Item -LiteralPath $PackDestination -Destination $PackBackupDestination
        $HadExistingPack = $true
    }

    Move-Item -LiteralPath $PackStagingDestination -Destination $PackDestination
    $ActivatedNewPack = $true

    Copy-Item -LiteralPath $HelenGameHookPath -Destination (Join-Path $GameBin 'HelenGameHook.dll') -Force
    Copy-Item -LiteralPath $ProxyPath -Destination (Join-Path $GameBin 'dinput8.dll') -Force

    if ($HadExistingPack -and (Test-Path -LiteralPath $PackBackupDestination)) {
        Remove-Item -LiteralPath $PackBackupDestination -Recurse -Force
    }
} catch {
    $DeploymentFailure = $_

    if (Test-Path -LiteralPath $PackStagingDestination) {
        Remove-Item -LiteralPath $PackStagingDestination -Recurse -Force
    }

    if ($ActivatedNewPack -and (Test-Path -LiteralPath $PackDestination)) {
        try {
            Remove-Item -LiteralPath $PackDestination -Recurse -Force
        } catch {
            throw "Batman deployment failed while removing the activated graphics-options pack at $PackDestination. Original error: $($DeploymentFailure.Exception.Message). Remove error: $($_.Exception.Message)"
        }
    }

    if ($HadExistingPack) {
        if (-not (Test-Path -LiteralPath $PackBackupDestination)) {
            throw "Batman deployment failed and the previous graphics-options pack backup was missing at $PackBackupDestination. Original error: $($DeploymentFailure.Exception.Message)."
        }

        try {
            Move-Item -LiteralPath $PackBackupDestination -Destination $PackDestination
        } catch {
            throw "Batman deployment failed and the previous graphics-options pack could not be restored from $PackBackupDestination. Original error: $($DeploymentFailure.Exception.Message). Restore error: $($_.Exception.Message)"
        }
    }

    throw $DeploymentFailure
}

if (Test-Path -LiteralPath $DeployedFilesJsonPath) {
    Test-ExpectedVirtualFiles -ManifestPath $DeployedFilesJsonPath -BuildRoot $PackBuildRoot -Label 'deployment'
}

Write-Output 'DEPLOYED'
