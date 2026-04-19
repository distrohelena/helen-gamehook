<#
.SYNOPSIS
    Navega ate o menu Graphics Options a partir do jogo rodando.

.DESCRIPTION
    Fluxo conhecido (confirmado por screenshots):
    1. Title screen -> ENTER (passa "Click to Start")
    2. Saved Game Select -> ENTER (seleciona save, vai para Main Menu)
    3. Main Menu -> 5x DOWN (Options e o 6o item)
    4. Options menu -> ENTER (entra em Options)
    5. Options -> DOWN (Graphics e o 2o item)
    6. ENTER (entra em Graphics Options)

.PARAMETER NoLaunch
    Se o jogo ja esta rodando.

.PARAMETER StepDelayMs
    Delay entre teclas (default 600ms).

.PARAMETER MenuDelayMs
    Delay apos entrar em submenu (default 2500ms).
#>

param(
    [switch]$NoLaunch,
    [int]$StepDelayMs = 600,
    [int]$MenuDelayMs = 2500
)

$ErrorActionPreference = 'Stop'

$OutputDir = 'C:\dev\helenhook\artifacts\nav-graphics'
$RecognitionCliProject = 'C:\dev\helenui\plugins\recognition-cli'
$ScreenshotCliProject = 'C:\dev\helenui\plugins\screenshot-cli'
$BatmanAaProject = 'C:\dev\helenui\batman-aa.json'
$TempOcrConfigPath = Join-Path $OutputDir 'temp-ocr-config.json'
$BatmanPrintsDir = 'C:\dev\batma\prints'
$HelperScriptPath = Join-Path $PSScriptRoot 'BatmanWindowHelpers.ps1'

if (-not (Test-Path $OutputDir)) {
    New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null
}

if (-not (Test-Path $BatmanPrintsDir)) {
    New-Item -ItemType Directory -Force -Path $BatmanPrintsDir | Out-Null
}

. $HelperScriptPath
Add-Type -AssemblyName System.Windows.Forms

Write-BatmanRecognitionOcrConfig -OutputPath $TempOcrConfigPath

function Get-ArtifactLabel {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Value
    )

    $Sanitized = $Value -replace '[^A-Za-z0-9_-]+', '-'
    return $Sanitized.Trim('-')
}

function Focus-Game {
    $MainWindow = Get-BatmanMainWindowSnapshot
    if ($null -eq $MainWindow) {
        return $false
    }

    [BatmanWindowNative]::SetForegroundWindow($MainWindow.HandleValue) | Out-Null
    Start-Sleep -Milliseconds 300
    return $true
}

function Assert-BatmanReady {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Label
    )

    $ArtifactLabel = Get-ArtifactLabel -Value $Label
    Assert-NoBatmanDialog `
        -ArtifactsRoot $OutputDir `
        -Label $ArtifactLabel `
        -ScreenshotCliProject $ScreenshotCliProject `
        -RecognitionCliProject $RecognitionCliProject `
        -BatmanProjectPath $BatmanAaProject `
        -OcrConfigPath $TempOcrConfigPath

    if (-not (Focus-Game)) {
        throw "Batman main window not found while handling '$Label'."
    }
}

function Press {
    param(
        [string]$Key,
        [string]$Label
    )

    Assert-BatmanReady -Label "before-$Label"
    Write-Host "  -> $Label" -ForegroundColor White
    [System.Windows.Forms.SendKeys]::SendWait($Key)
    Start-Sleep -Milliseconds $StepDelayMs

    Assert-NoBatmanDialog `
        -ArtifactsRoot $OutputDir `
        -Label (Get-ArtifactLabel -Value "after-$Label") `
        -ScreenshotCliProject $ScreenshotCliProject `
        -RecognitionCliProject $RecognitionCliProject `
        -BatmanProjectPath $BatmanAaProject `
        -OcrConfigPath $TempOcrConfigPath
}

function Snap {
    param(
        [string]$Name
    )

    Assert-NoBatmanDialog `
        -ArtifactsRoot $OutputDir `
        -Label (Get-ArtifactLabel -Value "snapshot-$Name") `
        -ScreenshotCliProject $ScreenshotCliProject `
        -RecognitionCliProject $RecognitionCliProject `
        -BatmanProjectPath $BatmanAaProject `
        -OcrConfigPath $TempOcrConfigPath

    $Path = Join-Path $OutputDir "$Name.png"
    Capture-BatmanMainWindowImage -OutputPath $Path -ScreenshotCliProject $ScreenshotCliProject | Out-Null

    $BatmanPrintPath = Join-Path $BatmanPrintsDir "$Name.png"
    Copy-Item -Path $Path -Destination $BatmanPrintPath -Force
    return $Path
}

function Run-Recognition {
    param(
        [Parameter(Mandatory = $true)]
        [string]$ScreenshotPath
    )

    return Invoke-RecognitionCliAnalyzeImage `
        -ImagePath $ScreenshotPath `
        -RecognitionCliProject $RecognitionCliProject `
        -BatmanProjectPath $BatmanAaProject `
        -OcrConfigPath $TempOcrConfigPath
}

function Wait-ForCheckpointScreen {
    param(
        [Parameter(Mandatory = $true)]
        [string]$ExpectedScreenName,
        [Parameter(Mandatory = $true)]
        [string]$Context,
        [Parameter(Mandatory = $true)]
        [string]$Label,
        [int]$TimeoutMs = 10000
    )

    return Wait-ForBatmanMainWindowExpectedScreen `
        -ExpectedScreenName $ExpectedScreenName `
        -Context $Context `
        -ArtifactsRoot $OutputDir `
        -Label $Label `
        -ScreenshotCliProject $ScreenshotCliProject `
        -RecognitionCliProject $RecognitionCliProject `
        -BatmanProjectPath $BatmanAaProject `
        -OcrConfigPath $TempOcrConfigPath `
        -TimeoutMilliseconds $TimeoutMs `
        -PollMilliseconds 500
}

function Assert-GraphicsOptionsScreen {
    param(
        [Parameter(Mandatory = $true)]
        [string]$ScreenshotPath
    )

    $Recognition = Run-Recognition -ScreenshotPath $ScreenshotPath
    $GraphicsScore = Get-BatmanRecognitionScreenScore -Recognition $Recognition -ScreenName 'GraphicsOptions'
    $AudioScore = Get-BatmanRecognitionScreenScore -Recognition $Recognition -ScreenName 'AudioOptions'
    $MatchedScreen = Get-BatmanRecognitionMatchedScreenName -Recognition $Recognition
    $TopCandidates = Get-BatmanRecognitionTopCandidateSummary -Recognition $Recognition

    if ($GraphicsScore -ge 1) {
        return
    }

    if ($AudioScore -ge 1) {
        throw "Graphics option is absent. Navigation reached Audio Options instead. Top candidates: $TopCandidates. Screenshot: $ScreenshotPath"
    }

    throw "Expected GraphicsOptions screen but found '$MatchedScreen'. Top candidates: $TopCandidates. Screenshot: $ScreenshotPath"
}

try {
    Write-Host '=== Navigate to Graphics Options ===' -ForegroundColor Cyan

    if (-not $NoLaunch) {
        Write-Host 'Launching Batman...' -ForegroundColor Green
        Start-Process 'D:\steam\steamapps\common\Batman Arkham Asylum GOTY\Binaries\ShippingPC-BmGame.exe'
        Start-Sleep -Seconds 20
    } else {
        Start-Sleep -Seconds 2
    }

    Assert-BatmanReady -Label 'post-launch'

    Snap '01_title' | Out-Null
    Press '{ENTER}' 'Title: Press ENTER'
    Snap '02_after_title' | Out-Null

    Wait-ForCheckpointScreen `
        -ExpectedScreenName 'Saves' `
        -Context 'Saved Game Select did not load after leaving title' `
        -Label '03_save_select' `
        -TimeoutMs ([Math]::Max($MenuDelayMs * 3, 6000)) | Out-Null

    Snap '03_save_select' | Out-Null
    Press '{ENTER}' 'Post-title: Press ENTER to reach Main Menu'
    Wait-ForCheckpointScreen `
        -ExpectedScreenName 'Main' `
        -Context 'Main menu did not load after leaving title' `
        -Label '04_after_save' `
        -TimeoutMs ([Math]::Max($MenuDelayMs * 4, 10000)) | Out-Null

    Snap '05_main_menu' | Out-Null
    for ($Index = 0; $Index -lt 5; $Index++) {
        Press '{DOWN}' "Main Menu: DOWN ($($Index + 1)/5)"
        Snap "06_main_down_$($Index + 1)" | Out-Null
        Start-Sleep -Milliseconds $StepDelayMs
    }

    Press '{ENTER}' 'Main Menu: ENTER into Options'
    Wait-ForCheckpointScreen `
        -ExpectedScreenName 'Options' `
        -Context 'Options menu did not load from the main menu' `
        -Label '07_options_menu' `
        -TimeoutMs ([Math]::Max($MenuDelayMs * 3, 6000)) | Out-Null

    Snap '08_in_options' | Out-Null
    Press '{DOWN}' 'Options: DOWN to Graphics'
    Snap '09_options_down' | Out-Null
    Start-Sleep -Milliseconds $StepDelayMs

    Press '{ENTER}' 'Options: ENTER into Graphics Options'
    $GraphicsOptionsPath = Snap '10_graphics_options'
    Start-Sleep -Milliseconds $MenuDelayMs
    Assert-GraphicsOptionsScreen -ScreenshotPath $GraphicsOptionsPath

    Write-Host ''
    Write-Host "=== Done! Screenshots saved to: $OutputDir ===" -ForegroundColor Green
    Write-Host 'Check 10_graphics_options.png to verify!' -ForegroundColor Yellow
}
finally {
    if (Test-Path $TempOcrConfigPath) {
        Remove-Item $TempOcrConfigPath -Force -ErrorAction SilentlyContinue
    }
}
