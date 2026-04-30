param(
    [string]$GameRoot = 'D:\steam\steamapps\common\Batman Arkham Asylum GOTY',
    [string]$PackBuildRoot
)

$ErrorActionPreference = 'Stop'

if ([string]::IsNullOrWhiteSpace($GameRoot)) {
    throw 'GameRoot is required.'
}

$GameRoot = (Resolve-Path $GameRoot).Path

if ([string]::IsNullOrWhiteSpace($PackBuildRoot)) {
    $BatmanRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
    $PackBuildRoot = Join-Path $BatmanRoot 'helengamehook\packs\batman-aa-subtitles\builds\steam-goty-1.0'
} else {
    $PackBuildRoot = (Resolve-Path $PackBuildRoot).Path
}

$ManifestPath = Join-Path $PackBuildRoot 'files.json'
if (-not (Test-Path -LiteralPath $ManifestPath)) {
    throw "Batman pack manifest not found: $ManifestPath"
}

$BuildManifestPath = Join-Path $PackBuildRoot 'build.json'
if (-not (Test-Path -LiteralPath $BuildManifestPath)) {
    throw "Batman build manifest not found: $BuildManifestPath"
}

$BuildManifest = Get-Content -LiteralPath $BuildManifestPath -Raw | ConvertFrom-Json
$ExecutablePath = Join-Path (Join-Path $GameRoot 'Binaries') $BuildManifest.executable
if (-not (Test-Path -LiteralPath $ExecutablePath)) {
    throw "Installed Batman executable was not found for build validation: $ExecutablePath"
}

$ExpectedExecutableSize = [UInt64]$BuildManifest.match.fileSize
$ExpectedExecutableSha256 = [string]$BuildManifest.match.sha256
$ActualExecutableItem = Get-Item -LiteralPath $ExecutablePath
$ActualExecutableSha256 = (Get-FileHash -LiteralPath $ExecutablePath -Algorithm SHA256).Hash.ToLowerInvariant()

if ($ActualExecutableItem.Length -ne $ExpectedExecutableSize) {
    throw "Installed Batman executable size mismatch. Path=$ExecutablePath Expected=$ExpectedExecutableSize Actual=$($ActualExecutableItem.Length)"
}

if (-not [string]::Equals($ActualExecutableSha256, $ExpectedExecutableSha256, [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "Installed Batman executable hash mismatch. Path=$ExecutablePath Expected=$ExpectedExecutableSha256 Actual=$ActualExecutableSha256"
}

$Manifest = Get-Content -LiteralPath $ManifestPath -Raw | ConvertFrom-Json
$VirtualFiles = @($Manifest.virtualFiles)
if ($VirtualFiles.Count -eq 0) {
    throw "Batman pack manifest did not contain any virtual files: $ManifestPath"
}

foreach ($VirtualFile in $VirtualFiles) {
    $ActualBasePath = Join-Path $GameRoot ($VirtualFile.path -replace '/', '\')
    if (-not (Test-Path -LiteralPath $ActualBasePath)) {
        throw "Installed Batman base file was not found for $($VirtualFile.id): $ActualBasePath"
    }

    $ExpectedBaseSize = [UInt64]$VirtualFile.source.base.size
    $ExpectedBaseSha256 = [string]$VirtualFile.source.base.sha256
    $ActualBaseItem = Get-Item -LiteralPath $ActualBasePath
    $ActualBaseSha256 = (Get-FileHash -LiteralPath $ActualBasePath -Algorithm SHA256).Hash.ToLowerInvariant()

    if ($ActualBaseItem.Length -ne $ExpectedBaseSize) {
        throw "Installed Batman base size mismatch for $($VirtualFile.id). Path=$ActualBasePath Expected=$ExpectedBaseSize Actual=$($ActualBaseItem.Length)"
    }

    if (-not [string]::Equals($ActualBaseSha256, $ExpectedBaseSha256, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "Installed Batman base hash mismatch for $($VirtualFile.id). Path=$ActualBasePath Expected=$ExpectedBaseSha256 Actual=$ActualBaseSha256"
    }
}

Write-Output 'PASS'
