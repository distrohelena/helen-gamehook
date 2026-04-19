param(
    [string]$BatmanRoot,
    [string]$BuilderRoot
)

$ErrorActionPreference = 'Stop'
$HelperScriptPath = Join-Path $PSScriptRoot 'BatmanBuilderWorkspaceHelpers.ps1'
. $HelperScriptPath

if ([string]::IsNullOrWhiteSpace($BatmanRoot)) {
    $BatmanRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
} else {
    $BatmanRoot = (Resolve-Path $BatmanRoot).Path
}

$BuilderRoot = Resolve-OptionalBuilderRoot -BatmanRootPath $BatmanRoot -BuilderRootPath $BuilderRoot
$BasePackagePath = Join-Path $BuilderRoot 'extracted\bmgame-unpacked\BmGame.u'
$FrontendPackagePath = Join-Path $BuilderRoot 'extracted\frontend\frontend-umap-unpacked\Frontend.umap'

foreach ($Path in @($BasePackagePath, $FrontendPackagePath)) {
    if (-not (Test-Path -LiteralPath $Path)) {
        throw "Expected builder workspace package was not found: $Path"
    }
}

Assert-UnrealPackageIsUnpacked -Path $BasePackagePath -Context 'Builder workspace gameplay base contract' | Out-Null
Assert-UnrealPackageIsUnpacked -Path $FrontendPackagePath -Context 'Builder workspace frontend base contract' | Out-Null

Write-Output 'PASS'
