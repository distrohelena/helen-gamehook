param(
    [string]$BatmanRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
)

$ErrorActionPreference = 'Stop'

$ProgramPath = Join-Path $BatmanRoot 'builder\tools\NativeSubtitleExePatcher\Program.cs'
if (-not (Test-Path -LiteralPath $ProgramPath)) {
    throw "NativeSubtitleExePatcher Program.cs not found: $ProgramPath"
}

$ProgramText = Get-Content -LiteralPath $ProgramPath -Raw

$RequiredTokens = @(
    'private const int SubtitleUiStateMaxRegionsPerPass = 32;',
    'code.EmitUInt32(SubtitleUiStateMaxRegionsPerPass); // mov ebp,maxRegionsPerPass',
    'EmitReadableCommittedRegionCheck(code, mbiStateVa, mbiProtectVa, "region_readable", "next_region");',
    'code.EmitJccNear(0x87, "next_region"); // ja',
    'code.Label("next_region");',
    'code.Emit(0x4D); // dec ebp',
    'code.EmitJccNear(0x85, "query_region"); // jne'
)

foreach ($RequiredToken in $RequiredTokens) {
    if ($ProgramText.IndexOf($RequiredToken, [System.StringComparison]::Ordinal) -lt 0) {
        throw "UI-state live scanner is missing expected multi-region scan token: $RequiredToken"
    }
}

Write-Output 'PASS'
