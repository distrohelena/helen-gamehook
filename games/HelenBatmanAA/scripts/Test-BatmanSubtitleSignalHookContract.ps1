param(
    [string]$BatmanRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
)

$ErrorActionPreference = 'Stop'

$ProgramPath = Join-Path $BatmanRoot 'builder\tools\NativeSubtitleExePatcher\Program.cs'
$ReadmePath = Join-Path $BatmanRoot 'builder\tools\NativeSubtitleExePatcher\README.md'
$ProgramText = Get-Content -LiteralPath $ProgramPath -Raw
$ReadmeText = Get-Content -LiteralPath $ReadmePath -Raw

$RequiredConstants = @(
    'private const int SubtitleSizeSmallCode = 4101;',
    'private const int SubtitleSizeMediumCode = 4102;',
    'private const int SubtitleSizeLargeCode = 4103;',
    'private const int SubtitleSizeVeryLargeCode = 4104;',
    'private const int SubtitleSizeHugeCode = 4105;',
    'private const int SubtitleSizeMassiveCode = 4106;'
)

foreach ($RequiredConstant in $RequiredConstants) {
    if (-not $ProgramText.Contains($RequiredConstant)) {
        throw "Missing subtitle signal constant: $RequiredConstant"
    }
}

if (-not $ProgramText.Contains('"4101,4102,4103,4104,4105,4106"')) {
    throw 'snapshot-live-subtitle-candidates still defaults to fewer than six values.'
}

if (-not $ReadmeText.Contains('4101/4102/4103/4104/4105/4106')) {
    throw 'README still documents the old three-state subtitle signal contract.'
}

Write-Output 'PASS'
