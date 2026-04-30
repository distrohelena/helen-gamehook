param(
    [string]$BatmanRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
)

$ErrorActionPreference = 'Stop'

$ProgramPath = Join-Path $BatmanRoot 'builder\tools\NativeSubtitleExePatcher\Program.cs'
if (-not (Test-Path -LiteralPath $ProgramPath)) {
    throw "NativeSubtitleExePatcher Program.cs not found: $ProgramPath"
}

$ProgramText = Get-Content -LiteralPath $ProgramPath -Raw
$StartPattern = 'private const uint SubtitleUiStateScanStartVa = 0x'
$EndPattern = 'private const uint SubtitleUiStateScanEndVa = 0x'

$StartIndex = $ProgramText.IndexOf($StartPattern, [System.StringComparison]::Ordinal)
$EndIndex = $ProgramText.IndexOf($EndPattern, [System.StringComparison]::Ordinal)
if ($StartIndex -lt 0 -or $EndIndex -lt 0) {
    throw 'Could not find SubtitleUiStateScanStartVa/SubtitleUiStateScanEndVa constants.'
}

$StartLine = ($ProgramText.Substring($StartIndex).Split([Environment]::NewLine)[0]).Trim()
$EndLine = ($ProgramText.Substring($EndIndex).Split([Environment]::NewLine)[0]).Trim()
$StartHex = ($StartLine -replace '.*0x', '') -replace ';', ''
$EndHex = ($EndLine -replace '.*0x', '') -replace ';', ''
$StartValue = [Convert]::ToUInt32($StartHex.Replace('_', ''), 16)
$EndValue = [Convert]::ToUInt32($EndHex.Replace('_', ''), 16)
$ObservedLiveBlockAddresses = @(
    [Convert]::ToUInt32('10266EA8', 16),
    [Convert]::ToUInt32('10F16EA8', 16)
)

foreach ($ObservedLiveBlockAddress in $ObservedLiveBlockAddresses) {
    if ($StartValue -gt $ObservedLiveBlockAddress -or $EndValue -le $ObservedLiveBlockAddress) {
        throw "Subtitle UI-state scan window 0x$($StartValue.ToString('X8'))..0x$($EndValue.ToString('X8')) does not include the observed live block address 0x$($ObservedLiveBlockAddress.ToString('X8'))."
    }
}

Write-Output 'PASS'
