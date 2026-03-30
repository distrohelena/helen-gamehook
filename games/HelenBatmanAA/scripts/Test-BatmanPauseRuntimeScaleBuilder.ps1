param(
    [string]$BatmanRoot
)

$ErrorActionPreference = 'Stop'

if ([string]::IsNullOrWhiteSpace($BatmanRoot)) {
    $BatmanRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
} else {
    $BatmanRoot = (Resolve-Path $BatmanRoot).Path
}

$GeneratedRoot = Join-Path $BatmanRoot 'builder\generated'
$ListItemPath = Join-Path $GeneratedRoot 'pause-runtime-scale\_build\pause-scripts\__Packages\rs\ui\ListItem.as'

if (-not (Test-Path $ListItemPath)) {
    throw "Generated pause runtime scale script not found: $ListItemPath"
}

$ListItemContents = Get-Content $ListItemPath -Raw

if ($ListItemContents.IndexOf('Helen_GetInt', [System.StringComparison]::Ordinal) -ge 0) {
    throw "Pause runtime scale builder still emits Helen_GetInt."
}

if ($ListItemContents.IndexOf('Helen_SetInt', [System.StringComparison]::Ordinal) -ge 0) {
    throw "Pause runtime scale builder still emits Helen_SetInt."
}

if ($ListItemContents.IndexOf('FE_GetControlType', [System.StringComparison]::Ordinal) -lt 0) {
    throw "Pause runtime scale builder does not emit FE_GetControlType."
}

if ($ListItemContents.IndexOf('FE_SetControlType', [System.StringComparison]::Ordinal) -lt 0) {
    throw "Pause runtime scale builder does not emit FE_SetControlType."
}

if ($ListItemContents.IndexOf('4101', [System.StringComparison]::Ordinal) -lt 0) {
    throw "Pause runtime scale builder does not emit the Small raw code 4101."
}

if ($ListItemContents.IndexOf('4102', [System.StringComparison]::Ordinal) -lt 0) {
    throw "Pause runtime scale builder does not emit the Normal raw code 4102."
}

if ($ListItemContents.IndexOf('4103', [System.StringComparison]::Ordinal) -lt 0) {
    throw "Pause runtime scale builder does not emit the Large raw code 4103."
}

Write-Output 'PASS'
