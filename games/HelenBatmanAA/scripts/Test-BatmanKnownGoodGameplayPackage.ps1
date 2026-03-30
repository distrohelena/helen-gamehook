param(
    [string]$BatmanRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
)

$ErrorActionPreference = 'Stop'

$PackBuildRoot = Join-Path $BatmanRoot 'helengamehook\packs\batman-aa-subtitles\builds\steam-goty-1.0'
$FilesJsonPath = Join-Path $PackBuildRoot 'files.json'
$GameplayDeltaPath = Join-Path $PackBuildRoot 'assets\deltas\BmGame-subtitle-signal.hgdelta'
$FrontendDeltaPath = Join-Path $PackBuildRoot 'assets\deltas\Frontend-main-menu-subtitle-size.hgdelta'
$TrustedGameplayBasePath = Join-Path $BatmanRoot 'builder\extracted\bmgame-unpacked\BmGame.u'
$GeneratedGameplayPackagePath = Join-Path $BatmanRoot 'builder\generated\pause-runtime-scale\BmGame-subtitle-signal.u'
$TrustedFrontendBasePath = Join-Path $BatmanRoot 'builder\extracted\frontend\frontend-umap-unpacked\Frontend.umap'
$GeneratedFrontendPackagePath = Join-Path $BatmanRoot 'builder\generated\main-menu-audio\Frontend-main-menu-subtitle-size.umap'
$ExpectedVirtualFiles = @(
    @{
        Id = 'bmgameGameplayPackage'
        Path = 'BmGame/CookedPC/BmGame.u'
        Mode = 'delta-on-read'
        Kind = 'delta-file'
        DeltaPath = 'assets/deltas/BmGame-subtitle-signal.hgdelta'
        BasePath = $TrustedGameplayBasePath
        TargetPath = $GeneratedGameplayPackagePath
        DeltaFilePath = $GameplayDeltaPath
        ChunkSize = 65536
    },
    @{
        Id = 'frontendMapPackage'
        Path = 'BmGame/CookedPC/Maps/Frontend/Frontend.umap'
        Mode = 'delta-on-read'
        Kind = 'delta-file'
        DeltaPath = 'assets/deltas/Frontend-main-menu-subtitle-size.hgdelta'
        BasePath = $TrustedFrontendBasePath
        TargetPath = $GeneratedFrontendPackagePath
        DeltaFilePath = $FrontendDeltaPath
        ChunkSize = 65536
    }
)

if (-not (Test-Path $FilesJsonPath)) {
    throw "Batman gameplay package manifest not found: $FilesJsonPath"
}

foreach ($ExpectedVirtualFile in $ExpectedVirtualFiles) {
    foreach ($RequiredPath in @($ExpectedVirtualFile.BasePath, $ExpectedVirtualFile.TargetPath, $ExpectedVirtualFile.DeltaFilePath)) {
        if (-not (Test-Path -LiteralPath $RequiredPath)) {
            throw "Batman pack verification input not found: $RequiredPath"
        }
    }
}

$FilesManifest = Get-Content -LiteralPath $FilesJsonPath -Raw | ConvertFrom-Json
$VirtualFiles = @($FilesManifest.virtualFiles)
if ($VirtualFiles.Count -ne $ExpectedVirtualFiles.Count) {
    throw "Batman gameplay package manifest expected exactly $($ExpectedVirtualFiles.Count) virtual files, found $($VirtualFiles.Count)."
}

foreach ($ExpectedVirtualFile in $ExpectedVirtualFiles) {
    $VirtualFile = @($VirtualFiles | Where-Object { $_.id -eq $ExpectedVirtualFile.Id })[0]
    if ($null -eq $VirtualFile) {
        throw "Batman gameplay package manifest is missing virtual file id $($ExpectedVirtualFile.Id)."
    }

    if ($VirtualFile.path -ne $ExpectedVirtualFile.Path) {
        throw "Batman gameplay package virtual file path mismatch for $($ExpectedVirtualFile.Id). Expected $($ExpectedVirtualFile.Path) but found $($VirtualFile.path)."
    }

    if ($VirtualFile.mode -ne $ExpectedVirtualFile.Mode) {
        throw "Batman gameplay package virtual file mode mismatch for $($ExpectedVirtualFile.Id). Expected $($ExpectedVirtualFile.Mode) but found $($VirtualFile.mode)."
    }

    if ($VirtualFile.source.kind -ne $ExpectedVirtualFile.Kind) {
        throw "Batman gameplay package source kind mismatch for $($ExpectedVirtualFile.Id). Expected $($ExpectedVirtualFile.Kind) but found $($VirtualFile.source.kind)."
    }

    if ($VirtualFile.source.path -ne $ExpectedVirtualFile.DeltaPath) {
        throw "Batman gameplay package delta path mismatch for $($ExpectedVirtualFile.Id). Expected $($ExpectedVirtualFile.DeltaPath) but found $($VirtualFile.source.path)."
    }

    $ExpectedBaseSize = (Get-Item -LiteralPath $ExpectedVirtualFile.BasePath).Length
    $ExpectedBaseSha256 = (Get-FileHash -LiteralPath $ExpectedVirtualFile.BasePath -Algorithm SHA256).Hash.ToLowerInvariant()
    $ExpectedTargetSize = (Get-Item -LiteralPath $ExpectedVirtualFile.TargetPath).Length
    $ExpectedTargetSha256 = (Get-FileHash -LiteralPath $ExpectedVirtualFile.TargetPath -Algorithm SHA256).Hash.ToLowerInvariant()

    if ([int64]$VirtualFile.source.base.size -ne $ExpectedBaseSize) {
        throw "Batman gameplay package base size mismatch for $($ExpectedVirtualFile.Id). Expected $ExpectedBaseSize but found $($VirtualFile.source.base.size)."
    }

    if (-not [string]::Equals($VirtualFile.source.base.sha256, $ExpectedBaseSha256, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "Batman gameplay package base hash mismatch for $($ExpectedVirtualFile.Id). Expected $ExpectedBaseSha256 but found $($VirtualFile.source.base.sha256)."
    }

    if ([int64]$VirtualFile.source.target.size -ne $ExpectedTargetSize) {
        throw "Batman gameplay package target size mismatch for $($ExpectedVirtualFile.Id). Expected $ExpectedTargetSize but found $($VirtualFile.source.target.size)."
    }

    if (-not [string]::Equals($VirtualFile.source.target.sha256, $ExpectedTargetSha256, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "Batman gameplay package target hash mismatch for $($ExpectedVirtualFile.Id). Expected $ExpectedTargetSha256 but found $($VirtualFile.source.target.sha256)."
    }

    if ([int64]$VirtualFile.source.chunkSize -ne $ExpectedVirtualFile.ChunkSize) {
        throw "Batman gameplay package chunk size mismatch for $($ExpectedVirtualFile.Id). Expected $($ExpectedVirtualFile.ChunkSize) but found $($VirtualFile.source.chunkSize)."
    }
}

Write-Output 'PASS'
