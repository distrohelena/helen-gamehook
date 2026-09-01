param([string]$BatmanRoot)

$ErrorActionPreference = 'Stop'

if ([string]::IsNullOrWhiteSpace($BatmanRoot)) {
    $BatmanRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
} else {
    $BatmanRoot = (Resolve-Path $BatmanRoot).Path
}

$PackRoot = Join-Path $BatmanRoot 'helengamehook\packs\batman-aa-skip-videos'
$PackJsonPath = Join-Path $PackRoot 'pack.json'
$BuildJsonPath = Join-Path $PackRoot 'builds\steam-goty-1.0\build.json'
$ConfigJsonPath = Join-Path $BatmanRoot 'helengamehook\config\packs.json'

foreach ($RequiredPath in @($PackJsonPath, $BuildJsonPath, $ConfigJsonPath)) {
    if (-not (Test-Path -LiteralPath $RequiredPath)) {
        throw "Required Batman skip-videos file not found: $RequiredPath"
    }
}

$PackJson = Get-Content -LiteralPath $PackJsonPath -Raw | ConvertFrom-Json
$BuildJson = Get-Content -LiteralPath $BuildJsonPath -Raw | ConvertFrom-Json
$ConfigJson = Get-Content -LiteralPath $ConfigJsonPath -Raw | ConvertFrom-Json

if ($PackJson.id -ne 'batman-aa-skip-videos') { throw 'Pack id mismatch.' }
if ($BuildJson.id -ne 'steam-goty-1.0') { throw 'Build id mismatch.' }
$ExpectedMissingPaths = @(
    'BmGame/Movies/baa_logo_run_v5_h264.bik',
    'BmGame/Movies/Legal.bik',
    'BmGame/Movies/Legalus.bik',
    'BmGame/Movies/nvidia.bik',
    'BmGame/Movies/utlogo.bik'
)
$ActualMissingPaths = @($BuildJson.missingPaths)
if ($ActualMissingPaths.Count -ne $ExpectedMissingPaths.Count) { throw 'Expected exactly five hidden startup videos.' }
for ($Index = 0; $Index -lt $ExpectedMissingPaths.Count; $Index++) {
    if ($ActualMissingPaths[$Index] -ne $ExpectedMissingPaths[$Index]) {
        throw "Unexpected hidden startup video at index ${Index}: $($ActualMissingPaths[$Index])"
    }
}
if ($BuildJson.match.fileSize -ne 38758728) { throw 'Executable match file size mismatch.' }
if ($BuildJson.match.sha256 -ne '4DAC1F5E2AC6710B7378FDCE74601F616F4753E3756CB5FDA63C7519CC2EB028') { throw 'Executable match hash mismatch.' }

$EnabledPacks = @($ConfigJson.enabledPacksByExecutable.'ShippingPC-BmGame.exe')
if ($EnabledPacks.Count -ne 2) { throw 'Expected 2 enabled Batman packs in packs.json.' }
if ($EnabledPacks[0] -ne 'batman-aa-subtitles') { throw 'Primary Batman pack order mismatch.' }
if ($EnabledPacks[1] -ne 'batman-aa-graphics-options') { throw 'Graphics-options Batman pack order mismatch.' }
if ($EnabledPacks -contains 'batman-aa-skip-videos') { throw 'Skip-videos pack must remain inactive for the graphics shell checkpoint.' }

Write-Output 'PASS'
