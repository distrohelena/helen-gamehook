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
$SourceFrontendBasePackagePath = Join-Path $BatmanRoot 'builder\extracted\frontend-retail\Frontend.umap'
$FrontendBasePackagePath = Join-Path $BatmanRoot 'builder\extracted\frontend-retail\Frontend.umap'
$FfdecCliPath = Join-Path $BatmanRoot 'builder\extracted\ffdec\ffdec-cli.exe'
$ExpectedBaseSha256 = '621A5C8D99C9F7C7283531D05A4A6D56BDF15AD93EDE0D5BF2F5D3E45117FF36'
$ExpectedFrontendGfxSha256 = '9589D663DD76A5DC530378115A8F8CFA6CEE9550276C853319E1DCD9B6E8FCEC'
$ExpectedPauseGfxSha256 = '0426443F03642194D888199D7BB190DE48E3F7C7EB589FB9E8D732728330A630'
$ExpectedHudGfxSha256 = 'EF62EB89EB090E607B45AAF4AE46922CB2A78B678452F652F042480F4670D770'
$TemporaryRoot = Join-Path ([System.IO.Path]::GetTempPath()) ("BatmanBuilderPrep-" + [System.Guid]::NewGuid().ToString('N'))
$BuilderRoot = Join-Path $TemporaryRoot 'builder'
$PreparedBasePackagePath = Join-Path $BuilderRoot 'extracted\bmgame-unpacked\BmGame.u'
$PreparedFrontendBasePackagePath = Join-Path $BuilderRoot 'extracted\frontend\frontend-umap-unpacked\Frontend.umap'
$PreparedFrontendGfxPath = Join-Path $BuilderRoot 'extracted\frontend\mainv2\frontend-mainv2.gfx'
$PreparedFrontendXmlPath = Join-Path $BuilderRoot 'extracted\frontend\mainv2\frontend-mainv2.xml'
$PreparedFrontendScriptsRoot = Join-Path $BuilderRoot 'extracted\frontend\mainv2\frontend-mainv2-export\scripts'
$PreparedPauseGfxPath = Join-Path $BuilderRoot 'extracted\pause\Pause-extracted.gfx'
$PreparedPauseXmlPath = Join-Path $BuilderRoot 'extracted\pause\Pause.xml'
$PreparedPauseScriptsRoot = Join-Path $BuilderRoot 'extracted\pause\pause-ffdec-export\scripts'
$PreparedHudGfxPath = Join-Path $BuilderRoot 'extracted\hud\HUD-extracted.gfx'
$PreparedHudXmlPath = Join-Path $BuilderRoot 'extracted\hud\HUD.xml'
$PreparedHudScriptsRoot = Join-Path $BuilderRoot 'extracted\hud\hud-ffdec-scripts\scripts'
$PreparedFfdecCliPath = Join-Path $BuilderRoot 'extracted\ffdec\ffdec-cli.exe'
$FrontendAudioActionPath = Join-Path $PreparedFrontendScriptsRoot 'DefineSprite_359_ScreenOptionsAudio\frame_1\DoAction.as'
$PauseScreenActionPath = Join-Path $PreparedPauseScriptsRoot 'DefineSprite_394_ScreenOptionsAudio\frame_1\DoAction.as'
$HudFrameActionPath = Join-Path $PreparedHudScriptsRoot 'DefineSprite_987\frame_1\DoAction.as'

try {
    & powershell -ExecutionPolicy Bypass -File $PrepareScriptPath `
        -BatmanRoot $BatmanRoot `
        -BuilderRoot $BuilderRoot `
        -BasePackagePath $BasePackagePath `
        -FrontendBasePackagePath $FrontendBasePackagePath `
        -FfdecCliPath $FfdecCliPath `
        -Configuration $Configuration
    if ($LASTEXITCODE -ne 0) {
        throw "Prepare-BatmanBuilderWorkspace.ps1 failed with exit code $LASTEXITCODE."
    }

    $requiredPaths = @(
        $PreparedBasePackagePath,
        $PreparedFrontendBasePackagePath,
        $PreparedFrontendGfxPath,
        $PreparedFrontendXmlPath,
        $PreparedFrontendScriptsRoot,
        $FrontendAudioActionPath,
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

    if (-not (Test-Path -LiteralPath $SourceFrontendBasePackagePath)) {
        throw "Trusted frontend base package was not found: $SourceFrontendBasePackagePath"
    }

    $sourceFrontendBaseSha256 = (Get-FileHash -LiteralPath $SourceFrontendBasePackagePath -Algorithm SHA256).Hash
    $preparedFrontendBaseSha256 = (Get-FileHash -LiteralPath $PreparedFrontendBasePackagePath -Algorithm SHA256).Hash
    if ($preparedFrontendBaseSha256 -ne $sourceFrontendBaseSha256) {
        throw "Prepared frontend base package hash mismatch. Expected $sourceFrontendBaseSha256 from $SourceFrontendBasePackagePath, found $preparedFrontendBaseSha256."
    }

    $preparedFrontendGfxSha256 = (Get-FileHash -LiteralPath $PreparedFrontendGfxPath -Algorithm SHA256).Hash
    if ($preparedFrontendGfxSha256 -ne $ExpectedFrontendGfxSha256) {
        throw "Prepared frontend GFX hash mismatch. Expected $ExpectedFrontendGfxSha256, found $preparedFrontendGfxSha256."
    }

    $frontendXmlContents = Get-Content -LiteralPath $PreparedFrontendXmlPath -Raw
    if ($frontendXmlContents.IndexOf('swfName="MainV2"', [System.StringComparison]::Ordinal) -lt 0) {
        throw 'Prepared frontend XML is missing the MainV2 movie marker.'
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

    $frontendActionContents = Get-Content -LiteralPath $FrontendAudioActionPath -Raw
    if ($frontendActionContents.IndexOf('Options Audio', [System.StringComparison]::Ordinal) -lt 0) {
        throw 'Prepared frontend FFDec export does not contain the MainV2 audio action script.'
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
