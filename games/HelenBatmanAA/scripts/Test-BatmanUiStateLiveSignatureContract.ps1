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
    'EmitCompareDwordPtrEdiDisp32(code, unchecked((sbyte)0xF0), 50, "invalid_candidate");',
    'EmitCompareDwordPtrEdiDisp32(code, unchecked((sbyte)0xF4), 100, "invalid_candidate");',
    'EmitCompareDwordPtrEdiDisp32(code, unchecked((sbyte)0xF8), 100, "invalid_candidate");',
    'EmitCompareDwordPtrEdiDisp32(code, unchecked((sbyte)0xFC), 100, "invalid_candidate");',
    'EmitCompareDwordPtrEdiDisp32(code, 0x00, SubtitleSizeMediumCode, "invalid_candidate");',
    'EmitCompareDwordPtrEdiDisp32(code, 0x04, 1, "invalid_candidate");',
    'EmitCompareDwordPtrEdiDisp32(code, 0x08, 0, "invalid_candidate");',
    'code.Emit(0x8B, 0x47, 0x0C); // mov eax,[edi+0Ch]',
    'EmitCompareDwordPtrEdiDisp32(code, 0x10, SubtitleSizeMediumCode, "invalid_candidate");',
    'EmitCompareDwordPtrEdiDisp32(code, 0x14, 2, "invalid_candidate");',
    'EmitCompareDwordPtrEdiDisp32(code, 0x1C, 3, "invalid_candidate");',
    'EmitCompareDwordPtrEdiDisp32(code, 0x20, 3, "invalid_candidate");'
)

foreach ($RequiredToken in $RequiredTokens) {
    if ($ProgramText.IndexOf($RequiredToken, [System.StringComparison]::Ordinal) -lt 0) {
        throw "UI-state live scanner is missing expected signature token: $RequiredToken"
    }
}

if ($ProgramText.IndexOf('code.Emit(0x8B, 0x07); // mov eax,[edi]', [System.StringComparison]::Ordinal) -ge 0) {
    throw 'UI-state live scanner still reads the active subtitle code from [edi] instead of [edi+0Ch].'
}

Write-Output 'PASS'
