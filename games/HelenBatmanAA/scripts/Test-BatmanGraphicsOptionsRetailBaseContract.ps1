param(
    [string]$BatmanRoot,
    [string]$BuilderRoot
)

$ErrorActionPreference = 'Stop'

$HelperScriptPath = Join-Path $PSScriptRoot 'BatmanPackVerificationHelpers.ps1'
. $HelperScriptPath

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
$PackBuildRoot = Join-Path $BatmanRoot 'helengamehook\packs\batman-aa-graphics-options\builds\steam-goty-1.0'
$FilesJsonPath = Join-Path $PackBuildRoot 'files.json'
$DeltaPath = Join-Path $PackBuildRoot 'assets\deltas\Frontend-graphics-options.hgdelta'
$RetailFrontendBasePath = Join-Path $BuilderRoot 'extracted\frontend-retail\Frontend.umap'
$GeneratedFrontendPackagePath = Join-Path $BuilderRoot 'generated\graphics-options-experiment\Frontend-graphics-options.umap'

if (-not (Test-Path -LiteralPath $FilesJsonPath)) {
    throw "Batman graphics-options manifest not found: $FilesJsonPath"
}

$Manifest = Get-Content -LiteralPath $FilesJsonPath -Raw | ConvertFrom-Json
$VirtualFiles = @($Manifest.virtualFiles)
if ($VirtualFiles.Count -ne 1) {
    throw "Expected exactly 1 graphics-options virtual file, found $($VirtualFiles.Count)."
}

Assert-HgdeltaVirtualFileContract `
    -Context 'Batman graphics-options retail base contract' `
    -VirtualFile $VirtualFiles[0] `
    -ExpectedId 'frontendGraphicsOptionsPackage' `
    -ExpectedPath 'BmGame/CookedPC/Maps/Frontend/Frontend.umap' `
    -ExpectedMode 'delta-on-read' `
    -ExpectedKind 'delta-file' `
    -ExpectedDeltaRelativePath 'assets/deltas/Frontend-graphics-options.hgdelta' `
    -BasePath $RetailFrontendBasePath `
    -TargetPath $GeneratedFrontendPackagePath `
    -DeltaFilePath $DeltaPath `
    -ChunkSize 65536 `
    -ChunkTableOffset 116

Write-Output 'PASS'
