param(
    [string]$BatmanRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
)

$ErrorActionPreference = 'Stop'

$DeployScriptPath = Join-Path $BatmanRoot 'scripts\Deploy-Batman.ps1'
$PauseListItemPath = Join-Path $BatmanRoot 'patch-source\PauseRuntimeScaleListItem.as'

if (-not (Test-Path -LiteralPath $DeployScriptPath)) {
    throw "Deploy script not found: $DeployScriptPath"
}

if (-not (Test-Path -LiteralPath $PauseListItemPath)) {
    throw "Pause subtitle list item override not found: $PauseListItemPath"
}

$DeployScriptText = Get-Content -LiteralPath $DeployScriptPath -Raw
$PauseListItemText = Get-Content -LiteralPath $PauseListItemPath -Raw
$UsesControlTypeBackend = $PauseListItemText.IndexOf('FE_SetControlType', [System.StringComparison]::Ordinal) -ge 0

if ($UsesControlTypeBackend) {
    if ($DeployScriptText.IndexOf("'--ui-state-live'", [System.StringComparison]::Ordinal) -lt 0) {
        throw "Deploy-Batman must patch the executable with --ui-state-live when the pause subtitle row uses FE_SetControlType."
    }

    if ($DeployScriptText.IndexOf("'--subtitle-size-signal'", [System.StringComparison]::Ordinal) -ge 0) {
        throw "Deploy-Batman must not patch the executable with --subtitle-size-signal while the pause subtitle row uses FE_SetControlType."
    }

    if ($DeployScriptText.IndexOf("'--subtitle-tail-debug-signal'", [System.StringComparison]::Ordinal) -ge 0) {
        throw "Deploy-Batman must not patch the executable with --subtitle-tail-debug-signal while the pause subtitle row uses FE_SetControlType."
    }
}

Write-Output 'PASS'
