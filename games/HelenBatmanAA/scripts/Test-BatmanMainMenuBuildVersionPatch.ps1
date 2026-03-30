param(
    [string]$BatmanRoot,
    [string]$Configuration = 'Debug',
    [string]$BuildVersion = 'tdd-main-menu-build-version'
)

$ErrorActionPreference = 'Stop'

if ([string]::IsNullOrWhiteSpace($BatmanRoot)) {
    $BatmanRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
} else {
    $BatmanRoot = (Resolve-Path $BatmanRoot).Path
}

$BuilderRoot = Join-Path $BatmanRoot 'builder'
$FfdecPath = Join-Path $BuilderRoot 'extracted\ffdec\ffdec-cli.exe'
$SubtitleSizeModBuilderProjectPath = Join-Path $BuilderRoot 'tools\NativeSubtitleExePatcher\SubtitleSizeModBuilder\SubtitleSizeModBuilder.csproj'
$OutputRoot = Join-Path $BuilderRoot 'generated\test-main-menu-build-version'
$RootScriptPath = Join-Path $OutputRoot '_build\frontend-scripts\frame_1\DoAction.as'

foreach ($RequiredPath in @($FfdecPath, $SubtitleSizeModBuilderProjectPath)) {
    if (-not (Test-Path -LiteralPath $RequiredPath)) {
        throw "Required builder input was not found: $RequiredPath"
    }
}

if (Test-Path -LiteralPath $OutputRoot) {
    Remove-Item -LiteralPath $OutputRoot -Recurse -Force
}

& dotnet run --project $SubtitleSizeModBuilderProjectPath -c $Configuration -- `
    build-main-menu-audio `
    --root $BuilderRoot `
    --output-dir $OutputRoot `
    --ffdec $FfdecPath `
    --build-version $BuildVersion
if ($LASTEXITCODE -ne 0) {
    throw "build-main-menu-audio failed for regression test."
}

if (-not (Test-Path -LiteralPath $RootScriptPath)) {
    throw "Generated frontend root script was not found: $RootScriptPath"
}

$RootScript = Get-Content -LiteralPath $RootScriptPath -Raw
$ExpectedToken = "var PCVersionString = ""$BuildVersion"";"
if ($RootScript.IndexOf($ExpectedToken, [System.StringComparison]::Ordinal) -lt 0) {
    throw "Expected frontend root script to contain '$ExpectedToken'."
}

Write-Output 'PASS'
