<#
.SYNOPSIS
    Navega automaticamente do inicio do Batman ate o menu Graphics Options usando recognition feedback.

.DESCRIPTION
    Este script:
    1. Detecta qual tela o Batman esta exibindo via recognition CLI
    2. Toma decisoes de navegacao baseadas na tela atual
    3. Envia inputs de teclado para navegar nos menus
    4. Continua ate detectar o menu Graphics Options

    Fluxo de navegacao esperado:
    Title Screen -> Click to Start
    Saved Game Select -> Select save -> Main Menu
    Main Menu -> Navigate to "Options" -> Enter -> Options Screen
    Options Screen -> Navigate to "Graphics" -> Enter -> Graphics Options

.PARAMETER GamePath
    Caminho para o executavel ShippingPC-BmGame.exe

.PARAMETER NoLaunch
    Se especificado, nao inicia o jogo (assume que ja esta rodando)

.PARAMETER MaxSteps
    Numero maximo de passos de navegacao antes de desistir (padrao: 50)

.PARAMETER StepDelayMs
    Delay entre steps de navegacao em milissegundos (padrao: 800)

.PARAMETER RecognitionDelayMs
    Delay apos cada input antes de capturar para recognition (padrao: 2000)

.EXAMPLE
    .\Test-BatmanNavigateToGraphics.ps1

.EXAMPLE
    .\Test-BatmanNavigateToGraphics.ps1 -NoLaunch -MaxSteps 30
#>

param(
    [Parameter(Mandatory = $false)]
    [string]$GamePath = 'D:\steam\steamapps\common\Batman Arkham Asylum GOTY\Binaries\ShippingPC-BmGame.exe',

    [Parameter(Mandatory = $false)]
    [switch]$NoLaunch,

    [Parameter(Mandatory = $false)]
    [int]$MaxSteps = 50,

    [Parameter(Mandatory = $false)]
    [int]$StepDelayMs = 800,

    [Parameter(Mandatory = $false)]
    [int]$RecognitionDelayMs = 2000
)

$ErrorActionPreference = 'Stop'

$RecognitionCliProject = 'C:\dev\helenui\plugins\recognition-cli'
$ScreenshotCliProject = 'C:\dev\helenui\plugins\screenshot-cli'
$BatmanAaProject = 'C:\dev\helenui\batman-aa.json'
$OutputDir = Join-Path $PSScriptRoot '..\..\artifacts\navigation-screenshots'
$TempOcrConfigPath = Join-Path $OutputDir 'temp-ocr-config.json'
$HelperScriptPath = Join-Path $PSScriptRoot 'BatmanWindowHelpers.ps1'

if (-not (Test-Path $OutputDir)) {
    New-Item -ItemType Directory -Path $OutputDir -Force | Out-Null
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

function Focus-GameWindow {
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

    if (-not (Focus-GameWindow)) {
        throw "Batman main window not found while handling '$Label'."
    }
}

function Send-GameKey {
    param(
        [string]$Key,
        [string]$Label
    )

    Assert-BatmanReady -Label "before-$Label"
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

function Capture-Screenshot {
    param(
        [string]$Filename
    )

    Assert-BatmanReady -Label "capture-$Filename"
    $ScreenshotPath = Join-Path $OutputDir $Filename
    Capture-BatmanMainWindowImage -OutputPath $ScreenshotPath -ScreenshotCliProject $ScreenshotCliProject | Out-Null
    return $ScreenshotPath
}

function Run-Recognition {
    param(
        [string]$ScreenshotPath
    )

    try {
        return Invoke-RecognitionCliAnalyzeImage `
            -ImagePath $ScreenshotPath `
            -RecognitionCliProject $RecognitionCliProject `
            -BatmanProjectPath $BatmanAaProject `
            -OcrConfigPath $TempOcrConfigPath
    } catch {
        Write-Host "Recognition failed for '$ScreenshotPath': $($_.Exception.Message)" -ForegroundColor Yellow
        return $null
    }
}

function Get-VariableStateValue {
    param(
        [Parameter(Mandatory = $true)]
        [object]$Recognition,
        [Parameter(Mandatory = $true)]
        [string]$VariableName
    )

    $Variable = @($Recognition.variable_states | Where-Object { $_.variable_name -eq $VariableName }) | Select-Object -First 1
    if ($null -eq $Variable -or -not $Variable.matched -or [string]::IsNullOrWhiteSpace($Variable.value)) {
        return $null
    }

    return $Variable.value
}

function Get-ScreenScore {
    param(
        [Parameter(Mandatory = $true)]
        [object]$Recognition,
        [Parameter(Mandatory = $true)]
        [string]$ScreenName
    )

    $Screen = @($Recognition.diagnostics.candidate_screen_scores | Where-Object { $_.screen_name -eq $ScreenName }) | Select-Object -First 1
    if ($null -eq $Screen) {
        return 0
    }

    return [double]$Screen.score
}

function Resolve-DetectedScreenName {
    param(
        [Parameter(Mandatory = $true)]
        [object]$Recognition
    )

    $MatchedScreen = if ($Recognition.screen_match -and $Recognition.screen_match.screen_name) {
        $Recognition.screen_match.screen_name
    } else {
        'Unknown'
    }

    $GraphicsScore = Get-ScreenScore -Recognition $Recognition -ScreenName 'GraphicsOptions'
    $AudioScore = Get-ScreenScore -Recognition $Recognition -ScreenName 'AudioOptions'
    $OptionsScore = Get-ScreenScore -Recognition $Recognition -ScreenName 'Options'

    if ($GraphicsScore -ge 1 -and $GraphicsScore -ge $OptionsScore) {
        return 'GraphicsOptions'
    }

    if ($AudioScore -ge 1 -and $AudioScore -ge $OptionsScore) {
        return 'AudioOptions'
    }

    return $MatchedScreen
}

function Get-NavigationAction {
    param(
        [string]$ScreenName,
        [string]$HighlightedItem,
        [int]$StepCount,
        [int]$ConsecutiveWaits
    )

    if ([string]::IsNullOrEmpty($ScreenName) -or $ScreenName -eq 'Unknown') {
        if ($StepCount -le 2) {
            return @{
                Action = 'PressEnter'
                Description = 'Starting: pressing Enter to get past title/save screens'
            }
        }

        if ($StepCount -le 6) {
            return @{
                Action = 'PressEnter'
                Description = 'Navigating main menu: pressing Enter to reach Options'
            }
        }

        if ($StepCount -le 10) {
            return @{
                Action = 'PressDown'
                Description = 'Looking for Options: pressing Down'
            }
        }

        if ($StepCount -le 15) {
            return @{
                Action = 'PressEnter'
                Description = 'Trying to enter Options menu'
            }
        }

        return @{
            Action = 'PressDown'
            Description = 'Searching for Graphics option'
        }
    }

    switch ($ScreenName) {
        'Title' {
            return @{
                Action = 'PressEnter'
                Description = 'Pressing Enter to start from title screen'
            }
        }
        'Saves' {
            return @{
                Action = 'PressEnter'
                Description = 'Selecting save to reach main menu'
            }
        }
        'Loading' {
            return @{
                Action = 'Wait'
                Description = 'Waiting for loading to complete'
            }
        }
        'Main' {
            if ($HighlightedItem -and $HighlightedItem -ne 'Options') {
                return @{
                    Action = 'PressDown'
                    Description = "Navigating down from '$HighlightedItem' towards 'Options'"
                }
            }

            return @{
                Action = 'PressEnter'
                Description = 'Entering Options menu'
            }
        }
        'Options' {
            if ($HighlightedItem -eq 'Audio Options') {
                return @{
                    Action = 'Failure'
                    Description = 'Graphics option is absent. Options menu advanced from Game Options to Audio Options instead.'
                }
            }

            if ($HighlightedItem -and $HighlightedItem -ne 'Graphics') {
                return @{
                    Action = 'PressDown'
                    Description = "Navigating down from '$HighlightedItem' towards 'Graphics'"
                }
            }

            return @{
                Action = 'PressEnter'
                Description = 'Entering Graphics menu'
            }
        }
        'GraphicsOptions' {
            return @{
                Action = 'Success'
                Description = 'Graphics Options menu detected!'
            }
        }
        'CrashDialog' {
            return @{
                Action = 'Failure'
                Description = 'Crash dialog detected'
            }
        }
        'AudioOptions' {
            return @{
                Action = 'Failure'
                Description = 'Expected Graphics Options but reached Audio Options instead.'
            }
        }
        'PauseMenu' {
            return @{
                Action = 'PressEscape'
                Description = 'Exiting pause menu'
            }
        }
        'Game' {
            return @{
                Action = 'PressEscape'
                Description = 'Pausing the game'
            }
        }
        'CutScene' {
            return @{
                Action = 'Wait'
                Description = 'Waiting for cutscene to end'
            }
        }
        default {
            return @{
                Action = 'PressEnter'
                Description = "Unknown screen '$ScreenName', pressing Enter"
            }
        }
    }
}

$GameProcess = $null
$StepCount = 0
$Success = $false
$LastScreenName = ''
$ConsecutiveWaits = 0
$ConsecutiveSameScreen = 0

try {
    Write-Host '=== Batman Navigation to Graphics Options ===' -ForegroundColor Cyan
    Write-Host "Game: $GamePath" -ForegroundColor Yellow
    Write-Host "NoLaunch: $NoLaunch" -ForegroundColor Yellow
    Write-Host "Max steps: $MaxSteps" -ForegroundColor Yellow
    Write-Host ''

    if (-not $NoLaunch -and -not (Test-Path $GamePath)) {
        Write-Host "ERROR: Game executable not found at $GamePath" -ForegroundColor Red
        exit 1
    }

    if (-not $NoLaunch) {
        Write-Host 'Launching Batman Arkham Asylum...' -ForegroundColor Green
        $GameProcess = Start-Process -FilePath $GamePath -PassThru
        Write-Host "Game launched (PID: $($GameProcess.Id))" -ForegroundColor Green
        Write-Host 'Waiting 15 seconds for game to initialize...' -ForegroundColor Yellow
        Start-Sleep -Seconds 15
    } else {
        Write-Host 'NoLaunch specified - assuming game is already running' -ForegroundColor Yellow
        Start-Sleep -Seconds 2
    }

    Assert-BatmanReady -Label 'post-launch'

    Write-Host ''
    Write-Host '=== Starting Navigation Loop ===' -ForegroundColor Cyan
    Write-Host ''

    while ($StepCount -lt $MaxSteps -and -not $Success) {
        $StepCount++
        $ScreenshotPath = Capture-Screenshot -Filename "nav_step_$(('{0:D3}' -f $StepCount)).png"
        Write-Host "[$StepCount/$MaxSteps] " -NoNewline

        $Result = Run-Recognition -ScreenshotPath $ScreenshotPath
        if ($null -eq $Result) {
            Write-Host 'Recognition failed, waiting...' -ForegroundColor Red
            Start-Sleep -Milliseconds ($RecognitionDelayMs * 2)
            $ConsecutiveWaits++
            continue
        }

        $CurrentScreen = Resolve-DetectedScreenName -Recognition $Result

        $HighlightedItem = $null
        if ($CurrentScreen -eq 'Main') {
            $HighlightedItem = Get-VariableStateValue -Recognition $Result -VariableName 'MainMenuItem'
        } elseif ($CurrentScreen -eq 'Options') {
            $HighlightedItem = Get-VariableStateValue -Recognition $Result -VariableName 'OptionsMenuItem'
        }

        if ($CurrentScreen -ne $LastScreenName) {
            Write-Host "Screen: $CurrentScreen" -ForegroundColor Green
            if ($HighlightedItem) {
                Write-Host "           Highlighted: $HighlightedItem" -ForegroundColor Yellow
            }
        } else {
            Write-Host "Still: $CurrentScreen" -ForegroundColor DarkGray
            $ConsecutiveSameScreen++
        }

        $LastScreenName = $CurrentScreen
        $NavigationAction = Get-NavigationAction `
            -ScreenName $CurrentScreen `
            -HighlightedItem $HighlightedItem `
            -StepCount $StepCount `
            -ConsecutiveWaits $ConsecutiveWaits

        switch ($NavigationAction.Action) {
            'Success' {
                Write-Host "           >> $($NavigationAction.Description)" -ForegroundColor Cyan
                $Success = $true
                break
            }
            'Failure' {
                throw $NavigationAction.Description
            }
            'Wait' {
                Write-Host "           >> $($NavigationAction.Description)" -ForegroundColor Yellow
                Start-Sleep -Milliseconds ($RecognitionDelayMs * 2)
                $ConsecutiveWaits++
                if ($ConsecutiveWaits -gt 10) {
                    Write-Host '           >> Too many waits, pressing Enter to progress' -ForegroundColor Yellow
                    Send-GameKey -Key '{ENTER}' -Label 'auto-progress-enter'
                    Start-Sleep -Milliseconds $RecognitionDelayMs
                    $ConsecutiveWaits = 0
                }
            }
            'PressSpace' {
                Write-Host "           >> $($NavigationAction.Description)" -ForegroundColor White
                Send-GameKey -Key ' ' -Label 'recognition-space'
                Start-Sleep -Milliseconds $RecognitionDelayMs
                $ConsecutiveWaits = 0
                $ConsecutiveSameScreen = 0
            }
            'PressEnter' {
                Write-Host "           >> $($NavigationAction.Description)" -ForegroundColor White
                Send-GameKey -Key '{ENTER}' -Label 'recognition-enter'
                Start-Sleep -Milliseconds $RecognitionDelayMs
                $ConsecutiveWaits = 0
                $ConsecutiveSameScreen = 0
            }
            'PressDown' {
                Write-Host "           >> $($NavigationAction.Description)" -ForegroundColor White
                Send-GameKey -Key '{DOWN}' -Label 'recognition-down-1'
                Start-Sleep -Milliseconds $StepDelayMs
                Send-GameKey -Key '{DOWN}' -Label 'recognition-down-2'
                Start-Sleep -Milliseconds $RecognitionDelayMs
                $ConsecutiveWaits = 0
                $ConsecutiveSameScreen = 0
            }
            'PressUp' {
                Write-Host "           >> $($NavigationAction.Description)" -ForegroundColor White
                Send-GameKey -Key '{UP}' -Label 'recognition-up'
                Start-Sleep -Milliseconds $RecognitionDelayMs
                $ConsecutiveWaits = 0
                $ConsecutiveSameScreen = 0
            }
            'PressEscape' {
                Write-Host "           >> $($NavigationAction.Description)" -ForegroundColor White
                Send-GameKey -Key '{ESC}' -Label 'recognition-escape'
                Start-Sleep -Milliseconds $RecognitionDelayMs
                $ConsecutiveWaits = 0
                $ConsecutiveSameScreen = 0
            }
        }
    }

    Write-Host ''
    Write-Host '=== Navigation Summary ===' -ForegroundColor Cyan
    Write-Host "Total steps: $StepCount" -ForegroundColor White
    Write-Host "Reached Graphics Options: $Success" -ForegroundColor White
    Write-Host "Screenshots saved to: $OutputDir" -ForegroundColor White

    if ($Success) {
        Write-Host ''
        Write-Host '========================================' -ForegroundColor Cyan
        Write-Host '  NAVIGATION SUCCESSFUL!' -ForegroundColor Cyan
        Write-Host '  Graphics Options menu reached!' -ForegroundColor Cyan
        Write-Host '========================================' -ForegroundColor Cyan
    } else {
        Write-Host ''
        Write-Host "WARNING: Did not reach Graphics Options in $MaxSteps steps" -ForegroundColor Red
        Write-Host "Check screenshots in $OutputDir to debug" -ForegroundColor Yellow
    }

    if ($GameProcess -and -not $GameProcess.HasExited) {
        Write-Host ''
        Write-Host "NOTE: Game is still running (PID: $($GameProcess.Id))" -ForegroundColor Yellow
    }

    Write-Host ''
    Write-Host 'Done!' -ForegroundColor Green
}
finally {
    if (Test-Path $TempOcrConfigPath) {
        Remove-Item $TempOcrConfigPath -Force -ErrorAction SilentlyContinue
    }
}
