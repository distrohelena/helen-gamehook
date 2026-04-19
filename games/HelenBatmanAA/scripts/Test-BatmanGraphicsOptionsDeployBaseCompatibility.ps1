param(
    [string]$BatmanRoot,
    [string]$Configuration = 'Debug'
)

$ErrorActionPreference = 'Stop'

if ([string]::IsNullOrWhiteSpace($BatmanRoot)) {
    $BatmanRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
} else {
    $BatmanRoot = (Resolve-Path $BatmanRoot).Path
}

$DeployScriptPath = Join-Path $PSScriptRoot 'Deploy-BatmanGraphicsOptionsExperiment.ps1'
$TempRoot = Join-Path $BatmanRoot 'artifacts\graphics-options-deploy-base-compat-test'
$GameRoot = Join-Path $TempRoot 'game-root'
$GameBin = Join-Path $GameRoot 'Binaries'
$FrontendPath = Join-Path $GameRoot 'BmGame\CookedPC\Maps\Frontend\Frontend.umap'

if (Test-Path -LiteralPath $TempRoot) {
    Remove-Item -LiteralPath $TempRoot -Recurse -Force
}

New-Item -ItemType Directory -Force -Path (Split-Path -Parent $FrontendPath) | Out-Null
New-Item -ItemType Directory -Force -Path $GameBin | Out-Null
[System.IO.File]::WriteAllText($FrontendPath, 'wrong frontend base')

try {
    $OutputLines = @()
    $Succeeded = $true

    try {
        $OutputLines = & $DeployScriptPath -GameBin $GameBin -Configuration $Configuration 2>&1
    } catch {
        $Succeeded = $false
        $OutputLines = @($_.Exception.Message)
    }

    if ($Succeeded) {
        throw 'Deploy-BatmanGraphicsOptionsExperiment unexpectedly succeeded against an incompatible installed frontend base.'
    }

    $OutputText = @($OutputLines) -join [Environment]::NewLine
    if ($OutputText -notmatch 'Installed Batman base (size|hash) mismatch') {
        throw "Expected installed base compatibility failure, but got: $OutputText"
    }

    Write-Output 'PASS'
}
finally {
    if (Test-Path -LiteralPath $TempRoot) {
        Remove-Item -LiteralPath $TempRoot -Recurse -Force
    }
}
