param(
    [string]$BatmanRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
)

$ErrorActionPreference = 'Stop'

$DeployScriptPath = Join-Path $BatmanRoot 'scripts\Deploy-Batman.ps1'
if (-not (Test-Path -LiteralPath $DeployScriptPath)) {
    throw "Deploy-Batman.ps1 not found: $DeployScriptPath"
}

$DeployScriptText = Get-Content -LiteralPath $DeployScriptPath -Raw
$RequiredTokens = @(
    '$RebuildPackScriptPath = Join-Path $PSScriptRoot ''Rebuild-BatmanPack.ps1''',
    '& $RebuildPackScriptPath -BuilderRoot $ResolvedBuilderRoot -Configuration $Configuration',
    'Batman pack rebuild failed with exit code $LASTEXITCODE.'
)

foreach ($RequiredToken in $RequiredTokens) {
    if ($DeployScriptText.IndexOf($RequiredToken, [System.StringComparison]::Ordinal) -lt 0) {
        throw "Deploy script is missing expected rebuild token: $RequiredToken"
    }
}

$RebuildIndex = $DeployScriptText.IndexOf('& $RebuildPackScriptPath -BuilderRoot $ResolvedBuilderRoot -Configuration $Configuration', [System.StringComparison]::Ordinal)
$VerifierIndex = $DeployScriptText.IndexOf('& $VerifierPath -BatmanRoot $BatmanRoot -BuilderRoot $ResolvedBuilderRoot', [System.StringComparison]::Ordinal)
if ($RebuildIndex -lt 0 -or $VerifierIndex -lt 0 -or $RebuildIndex -gt $VerifierIndex) {
    throw 'Deploy script must rebuild the Batman subtitle pack before running the known-good gameplay verifier.'
}

Write-Output 'PASS'
