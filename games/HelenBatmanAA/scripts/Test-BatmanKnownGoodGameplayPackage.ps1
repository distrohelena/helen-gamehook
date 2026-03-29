param(
    [string]$BatmanRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
)

$ErrorActionPreference = 'Stop'

$ExpectedHash = '429D66DA5404E381BC1CB2E7CBAF6A04CA9819A52197A5F87F85E4D975734252'
$GameplayPackagePath = Join-Path $BatmanRoot 'helengamehook\packs\batman-aa-subtitles\builds\steam-goty-1.0\assets\packages\BmGame-subtitle-signal.u'

if (-not (Test-Path $GameplayPackagePath)) {
    throw "Batman gameplay package not found: $GameplayPackagePath"
}

$ActualHash = (Get-FileHash $GameplayPackagePath -Algorithm SHA256).Hash
if (-not [string]::Equals($ActualHash, $ExpectedHash, [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "Batman gameplay package hash mismatch. Expected $ExpectedHash but found $ActualHash."
}

Write-Output 'PASS'
