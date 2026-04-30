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
    'private const int SubtitleUiStateRescanIntervalMs = 50;',
    'private const int SubtitleUiStateMaxRegionsPerPass = 32;',
    'uint getTickCountIatVa = ResolveImportIatVa(bytes, sections, "KERNEL32.dll", "GetTickCount");',
    'uint lastScanTickVa = ImageBase + stateBlock.Rva + StateBytesReadOffset;',
    'code.EmitUInt32(lastScanTickVa); // mov ecx,[last_scan_tick]',
    'code.EmitInt32(SubtitleUiStateRescanIntervalMs); // cmp edx,rescanIntervalMs'
)

foreach ($RequiredToken in $RequiredTokens) {
    if ($ProgramText.IndexOf($RequiredToken, [System.StringComparison]::Ordinal) -lt 0) {
        throw "UI-state live scanner is missing expected throttle token: $RequiredToken"
    }
}

Write-Output 'PASS'
