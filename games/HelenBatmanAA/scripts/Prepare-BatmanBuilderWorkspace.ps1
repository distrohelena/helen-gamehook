param(
    [string]$BatmanRoot,
    [string]$BuilderRoot,
    [Parameter(Mandatory = $true)]
    [string]$BasePackagePath,
    [Parameter(Mandatory = $true)]
    [string]$FrontendBasePackagePath,
    [Parameter(Mandatory = $true)]
    [string]$FfdecCliPath,
    [string]$Configuration = 'Release'
)

$ErrorActionPreference = 'Stop'

function Resolve-OptionalBuilderRoot {
    param(
        [string]$BatmanRootPath,
        [string]$BuilderRootPath
    )

    if ([string]::IsNullOrWhiteSpace($BuilderRootPath)) {
        return [System.IO.Path]::GetFullPath((Join-Path $BatmanRootPath 'builder'))
    }

    if ([System.IO.Path]::IsPathRooted($BuilderRootPath)) {
        return [System.IO.Path]::GetFullPath($BuilderRootPath)
    }

    return [System.IO.Path]::GetFullPath((Join-Path $BatmanRootPath $BuilderRootPath))
}

function Invoke-CheckedCommand {
    param(
        [string]$FilePath,
        [string[]]$Arguments,
        [string]$FailureMessage
    )

    & $FilePath @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw $FailureMessage
    }
}

function Reset-Directory {
    param(
        [string]$Path
    )

    if (Test-Path -LiteralPath $Path) {
        Remove-Item -LiteralPath $Path -Recurse -Force
    }

    New-Item -ItemType Directory -Force -Path $Path | Out-Null
}

function Copy-DirectoryContents {
    param(
        [string]$SourceDirectory,
        [string]$DestinationDirectory
    )

    Reset-Directory -Path $DestinationDirectory
    Copy-Item -Path (Join-Path $SourceDirectory '*') -Destination $DestinationDirectory -Recurse -Force
}

if ([string]::IsNullOrWhiteSpace($BatmanRoot)) {
    $BatmanRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
} else {
    $BatmanRoot = (Resolve-Path $BatmanRoot).Path
}
$BuilderRoot = Resolve-OptionalBuilderRoot -BatmanRootPath $BatmanRoot -BuilderRootPath $BuilderRoot
$BasePackagePath = (Resolve-Path $BasePackagePath).Path
$FrontendBasePackagePath = (Resolve-Path $FrontendBasePackagePath).Path
$FfdecCliPath = (Resolve-Path $FfdecCliPath).Path
$SourceBuilderRoot = Join-Path $BatmanRoot 'builder'
$ExtractedRoot = Join-Path $BuilderRoot 'extracted'
$ToolRoot = Join-Path $SourceBuilderRoot 'tools\NativeSubtitleExePatcher'
$BmGameGfxPatcherProjectPath = Join-Path $ToolRoot 'BmGameGfxPatcher\BmGameGfxPatcher.csproj'
$PreparedBasePackagePath = Join-Path $ExtractedRoot 'bmgame-unpacked\BmGame.u'
$PreparedFrontendBasePackagePath = Join-Path $ExtractedRoot 'frontend\startup-int-unpacked\Startup_INT.upk'
$PreparedFrontendRoot = Join-Path $ExtractedRoot 'frontend\mainv2'
$PreparedFrontendGfxPath = Join-Path $PreparedFrontendRoot 'frontend-mainv2.gfx'
$PreparedFrontendXmlPath = Join-Path $PreparedFrontendRoot 'frontend-mainv2.xml'
$PreparedFrontendExportRoot = Join-Path $PreparedFrontendRoot 'frontend-mainv2-export'
$PreparedFfdecRoot = Join-Path $ExtractedRoot 'ffdec'
$PreparedFfdecCliPath = Join-Path $PreparedFfdecRoot 'ffdec-cli.exe'
$PreparedPauseGfxPath = Join-Path $ExtractedRoot 'pause\Pause-extracted.gfx'
$PreparedPauseXmlPath = Join-Path $ExtractedRoot 'pause\Pause.xml'
$PreparedPauseExportRoot = Join-Path $ExtractedRoot 'pause\pause-ffdec-export'
$PreparedHudGfxPath = Join-Path $ExtractedRoot 'hud\HUD-extracted.gfx'
$PreparedHudXmlPath = Join-Path $ExtractedRoot 'hud\HUD.xml'
$PreparedHudExportRoot = Join-Path $ExtractedRoot 'hud\hud-ffdec-scripts'
$SourceFfdecRoot = Split-Path -Parent $FfdecCliPath

New-Item -ItemType Directory -Force -Path (Split-Path -Parent $PreparedBasePackagePath) | Out-Null
New-Item -ItemType Directory -Force -Path (Split-Path -Parent $PreparedFrontendBasePackagePath) | Out-Null
New-Item -ItemType Directory -Force -Path $PreparedFrontendRoot | Out-Null
New-Item -ItemType Directory -Force -Path (Split-Path -Parent $PreparedPauseGfxPath) | Out-Null
New-Item -ItemType Directory -Force -Path (Split-Path -Parent $PreparedHudGfxPath) | Out-Null

if (-not (Test-Path -LiteralPath $BmGameGfxPatcherProjectPath)) {
    throw "BmGameGfxPatcher project was not found: $BmGameGfxPatcherProjectPath"
}

if (-not [System.StringComparer]::OrdinalIgnoreCase.Equals($BasePackagePath, $PreparedBasePackagePath)) {
    Copy-Item -LiteralPath $BasePackagePath -Destination $PreparedBasePackagePath -Force
}

if (-not [System.StringComparer]::OrdinalIgnoreCase.Equals($FrontendBasePackagePath, $PreparedFrontendBasePackagePath)) {
    Copy-Item -LiteralPath $FrontendBasePackagePath -Destination $PreparedFrontendBasePackagePath -Force
}

if (-not [System.StringComparer]::OrdinalIgnoreCase.Equals($SourceFfdecRoot, $PreparedFfdecRoot)) {
    Copy-DirectoryContents -SourceDirectory $SourceFfdecRoot -DestinationDirectory $PreparedFfdecRoot
}

Invoke-CheckedCommand `
    -FilePath 'dotnet' `
    -Arguments @('build', $BmGameGfxPatcherProjectPath, '-c', $Configuration) `
    -FailureMessage 'dotnet build failed for BmGameGfxPatcher.csproj'

Invoke-CheckedCommand `
    -FilePath 'dotnet' `
    -Arguments @(
        'run',
        '--project', $BmGameGfxPatcherProjectPath,
        '-c', $Configuration,
        '--',
        'extract-gfx',
        '--package', $PreparedBasePackagePath,
        '--owner', 'PauseMenu',
        '--name', 'Pause',
        '--output', $PreparedPauseGfxPath
    ) `
    -FailureMessage 'Failed to extract PauseMenu.Pause from the trusted Batman package.'

Invoke-CheckedCommand `
    -FilePath 'dotnet' `
    -Arguments @(
        'run',
        '--project', $BmGameGfxPatcherProjectPath,
        '-c', $Configuration,
        '--',
        'extract-gfx',
        '--package', $PreparedBasePackagePath,
        '--owner', 'GameHUD',
        '--name', 'HUD',
        '--output', $PreparedHudGfxPath
    ) `
    -FailureMessage 'Failed to extract GameHUD.HUD from the trusted Batman package.'

Reset-Directory -Path $PreparedFrontendExportRoot

Invoke-CheckedCommand `
    -FilePath 'dotnet' `
    -Arguments @(
        'run',
        '--project', $BmGameGfxPatcherProjectPath,
        '-c', $Configuration,
        '--',
        'extract-gfx',
        '--package', $PreparedFrontendBasePackagePath,
        '--owner', 'MainMenu',
        '--name', 'MainV2',
        '--output', $PreparedFrontendGfxPath
    ) `
    -FailureMessage 'Failed to extract MainMenu.MainV2 from the trusted frontend package.'

Invoke-CheckedCommand `
    -FilePath $PreparedFfdecCliPath `
    -Arguments @('-swf2xml', $PreparedFrontendGfxPath, $PreparedFrontendXmlPath) `
    -FailureMessage 'FFDec failed to convert frontend-mainv2.gfx into frontend-mainv2.xml.'

Invoke-CheckedCommand `
    -FilePath $PreparedFfdecCliPath `
    -Arguments @('-export', 'script', $PreparedFrontendExportRoot, $PreparedFrontendGfxPath) `
    -FailureMessage 'FFDec failed to export the MainV2 ActionScript tree.'

Reset-Directory -Path $PreparedPauseExportRoot
Reset-Directory -Path $PreparedHudExportRoot

Invoke-CheckedCommand `
    -FilePath $PreparedFfdecCliPath `
    -Arguments @('-swf2xml', $PreparedPauseGfxPath, $PreparedPauseXmlPath) `
    -FailureMessage 'FFDec failed to convert Pause-extracted.gfx into Pause.xml.'

Invoke-CheckedCommand `
    -FilePath $PreparedFfdecCliPath `
    -Arguments @('-swf2xml', $PreparedHudGfxPath, $PreparedHudXmlPath) `
    -FailureMessage 'FFDec failed to convert HUD-extracted.gfx into HUD.xml.'

Invoke-CheckedCommand `
    -FilePath $PreparedFfdecCliPath `
    -Arguments @('-export', 'script', $PreparedPauseExportRoot, $PreparedPauseGfxPath) `
    -FailureMessage 'FFDec failed to export the Pause ActionScript tree.'

Invoke-CheckedCommand `
    -FilePath $PreparedFfdecCliPath `
    -Arguments @('-export', 'script', $PreparedHudExportRoot, $PreparedHudGfxPath) `
    -FailureMessage 'FFDec failed to export the HUD ActionScript tree.'

Write-Output 'Prepared Batman builder workspace:'
Write-Output "  Builder root:    $BuilderRoot"
Write-Output "  Base package:    $PreparedBasePackagePath"
Write-Output "  Frontend base:   $PreparedFrontendBasePackagePath"
Write-Output "  Frontend GFX:    $PreparedFrontendGfxPath"
Write-Output "  Frontend XML:    $PreparedFrontendXmlPath"
Write-Output "  Frontend scripts: $(Join-Path $PreparedFrontendExportRoot 'scripts')"
Write-Output "  Pause GFX:       $PreparedPauseGfxPath"
Write-Output "  Pause XML:       $PreparedPauseXmlPath"
Write-Output "  Pause scripts:   $(Join-Path $PreparedPauseExportRoot 'scripts')"
Write-Output "  HUD GFX:         $PreparedHudGfxPath"
Write-Output "  HUD XML:         $PreparedHudXmlPath"
Write-Output "  HUD scripts:     $(Join-Path $PreparedHudExportRoot 'scripts')"
Write-Output "  FFDec root:      $PreparedFfdecRoot"
