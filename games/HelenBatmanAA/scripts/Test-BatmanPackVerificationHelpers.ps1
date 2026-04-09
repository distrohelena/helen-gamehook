param(
    [string]$BatmanRoot,
    [string]$BuilderRoot
)

$ErrorActionPreference = 'Stop'

if ([string]::IsNullOrWhiteSpace($BatmanRoot)) {
    $BatmanRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
} else {
    $BatmanRoot = (Resolve-Path $BatmanRoot).Path
}

if ([string]::IsNullOrWhiteSpace($BuilderRoot)) {
    $BuilderRoot = Join-Path $BatmanRoot 'builder'
} elseif (-not [System.IO.Path]::IsPathRooted($BuilderRoot)) {
    $BuilderRoot = Join-Path $BatmanRoot $BuilderRoot
}

$BuilderRoot = [System.IO.Path]::GetFullPath($BuilderRoot)
$HelperScriptPath = Join-Path $PSScriptRoot 'BatmanPackVerificationHelpers.ps1'
. $HelperScriptPath

$ExperimentRoot = Join-Path $BuilderRoot 'generated\main-menu-version-label-experiment'
$PackBuildRoot = Join-Path $ExperimentRoot 'pack\batman-aa-subtitles\builds\steam-goty-1.0'
$FilesJsonPath = Join-Path $PackBuildRoot 'files.json'
$DeltaPath = Join-Path $PackBuildRoot 'assets\deltas\Frontend-version-label.hgdelta'
$TrustedFrontendBasePath = Join-Path $BuilderRoot 'extracted\frontend-retail\Frontend.umap'
$GeneratedFrontendPackagePath = Join-Path $ExperimentRoot 'frontend\Frontend-version-label.umap'

$Manifest = Get-Content -LiteralPath $FilesJsonPath -Raw | ConvertFrom-Json
$VirtualFiles = @($Manifest.virtualFiles)
if ($VirtualFiles.Count -ne 1) {
    throw "Expected exactly 1 frontend experiment virtual file, found $($VirtualFiles.Count)."
}

Assert-HgdeltaVirtualFileContract `
    -Context 'Frontend experiment helper smoke test' `
    -VirtualFile $VirtualFiles[0] `
    -ExpectedId 'frontendVersionLabelPackage' `
    -ExpectedPath 'BmGame/CookedPC/Maps/Frontend/Frontend.umap' `
    -ExpectedMode 'delta-on-read' `
    -ExpectedKind 'delta-file' `
    -ExpectedDeltaRelativePath 'assets/deltas/Frontend-version-label.hgdelta' `
    -BasePath $TrustedFrontendBasePath `
    -TargetPath $GeneratedFrontendPackagePath `
    -DeltaFilePath $DeltaPath `
    -ChunkSize 65536 `
    -ChunkTableOffset 116

Write-Output 'PASS'
