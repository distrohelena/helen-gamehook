param(
    [string]$BatmanRoot,
    [string]$Configuration = 'Debug'
)

$ErrorActionPreference = 'Stop'

if ([string]::IsNullOrWhiteSpace($BatmanRoot)) {
    $BatmanRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
} else {
    $BatmanRoot = (Resolve-Path $BatmanRoot).Path
}
$PrepareScriptPath = Join-Path $BatmanRoot 'scripts\Prepare-BatmanBuilderWorkspace.ps1'
$BasePackagePath = Join-Path $BatmanRoot 'builder\extracted\bmgame-unpacked\BmGame.u'
$FfdecCliPath = Join-Path $BatmanRoot 'builder\extracted\ffdec\ffdec-cli.exe'
$ExpectedBaseSha256 = '621A5C8D99C9F7C7283531D05A4A6D56BDF15AD93EDE0D5BF2F5D3E45117FF36'
$ExpectedPauseGfxSha256 = '0426443F03642194D888199D7BB190DE48E3F7C7EB589FB9E8D732728330A630'
$ExpectedHudGfxSha256 = 'EF62EB89EB090E607B45AAF4AE46922CB2A78B678452F652F042480F4670D770'
$TemporaryRoot = Join-Path ([System.IO.Path]::GetTempPath()) ("BatmanBuilderPrep-" + [System.Guid]::NewGuid().ToString('N'))
$BuilderRoot = Join-Path $TemporaryRoot 'builder'
$PreparedBasePackagePath = Join-Path $BuilderRoot 'extracted\bmgame-unpacked\BmGame.u'
$PreparedPauseGfxPath = Join-Path $BuilderRoot 'extracted\pause\Pause-extracted.gfx'
$PreparedPauseXmlPath = Join-Path $BuilderRoot 'extracted\pause\Pause.xml'
$PreparedPauseScriptsRoot = Join-Path $BuilderRoot 'extracted\pause\pause-ffdec-export\scripts'
$PreparedHudGfxPath = Join-Path $BuilderRoot 'extracted\hud\HUD-extracted.gfx'
$PreparedHudXmlPath = Join-Path $BuilderRoot 'extracted\hud\HUD.xml'
$PreparedHudScriptsRoot = Join-Path $BuilderRoot 'extracted\hud\hud-ffdec-scripts\scripts'
$PreparedFfdecCliPath = Join-Path $BuilderRoot 'extracted\ffdec\ffdec-cli.exe'
$PauseScreenActionPath = Join-Path $PreparedPauseScriptsRoot 'DefineSprite_394_ScreenOptionsAudio\frame_1\DoAction.as'
$HudFrameActionPath = Join-Path $PreparedHudScriptsRoot 'DefineSprite_987\frame_1\DoAction.as'

try {
    & powershell -ExecutionPolicy Bypass -File $PrepareScriptPath `
        -BatmanRoot $BatmanRoot `
        -BuilderRoot $BuilderRoot `
        -BasePackagePath $BasePackagePath `
        -FfdecCliPath $FfdecCliPath `
        -Configuration $Configuration
    if ($LASTEXITCODE -ne 0) {
        throw "Prepare-BatmanBuilderWorkspace.ps1 failed with exit code $LASTEXITCODE."
    }

    $requiredPaths = @(
        $PreparedBasePackagePath,
        $PreparedPauseGfxPath,
        $PreparedPauseXmlPath,
        $PreparedPauseScriptsRoot,
        $PreparedHudGfxPath,
        $PreparedHudXmlPath,
        $PreparedHudScriptsRoot,
        $PreparedFfdecCliPath,
        $PauseScreenActionPath,
        $HudFrameActionPath
    )

    foreach ($requiredPath in $requiredPaths) {
        if (-not (Test-Path -LiteralPath $requiredPath)) {
            throw "Prepared builder workspace is missing expected path: $requiredPath"
        }
    }

    $preparedBaseSha256 = (Get-FileHash -LiteralPath $PreparedBasePackagePath -Algorithm SHA256).Hash
    if ($preparedBaseSha256 -ne $ExpectedBaseSha256) {
        throw "Prepared base package hash mismatch. Expected $ExpectedBaseSha256, found $preparedBaseSha256."
    }

    $preparedPauseGfxSha256 = (Get-FileHash -LiteralPath $PreparedPauseGfxPath -Algorithm SHA256).Hash
    if ($preparedPauseGfxSha256 -ne $ExpectedPauseGfxSha256) {
        throw "Prepared Pause GFX hash mismatch. Expected $ExpectedPauseGfxSha256, found $preparedPauseGfxSha256."
    }

    $preparedHudGfxSha256 = (Get-FileHash -LiteralPath $PreparedHudGfxPath -Algorithm SHA256).Hash
    if ($preparedHudGfxSha256 -ne $ExpectedHudGfxSha256) {
        throw "Prepared HUD GFX hash mismatch. Expected $ExpectedHudGfxSha256, found $preparedHudGfxSha256."
    }

    $hudXmlContents = Get-Content -LiteralPath $PreparedHudXmlPath -Raw
    if ($hudXmlContents.IndexOf('swfName="HUD"', [System.StringComparison]::Ordinal) -lt 0) {
        throw 'Prepared HUD.xml is missing the HUD movie marker.'
    }

    $pauseXmlContents = Get-Content -LiteralPath $PreparedPauseXmlPath -Raw
    if ($pauseXmlContents.IndexOf('ScreenOptionsAudio', [System.StringComparison]::Ordinal) -lt 0) {
        throw "Prepared Pause.xml is missing ScreenOptionsAudio."
    }

    $pauseActionContents = Get-Content -LiteralPath $PauseScreenActionPath -Raw
    if ($pauseActionContents.IndexOf('Subtitles', [System.StringComparison]::Ordinal) -lt 0) {
        throw "Prepared pause FFDec export does not contain the audio screen action script."
    }

    $hudActionContents = Get-Content -LiteralPath $HudFrameActionPath -Raw
    if ($hudActionContents.IndexOf('PromptManager', [System.StringComparison]::Ordinal) -lt 0) {
        throw "Prepared HUD FFDec export does not contain the HUD frame action script."
    }

    Write-Output 'PASS'
}
finally {
    if (Test-Path -LiteralPath $TemporaryRoot) {
        Remove-Item -LiteralPath $TemporaryRoot -Recurse -Force
    }
}
