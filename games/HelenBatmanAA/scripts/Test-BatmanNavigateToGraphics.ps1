<#
.SYNOPSIS
    Navega automaticamente do inicio do Batman ate o menu Graphics Options usando recognition feedback.

.DESCRIPTION
    Este script:
    1. Detecta qual tela o Batman esta exibindo via recognition CLI
    2. Toma decisoes de navegacao baseadas na tela atual
    3. Envia inputs de teclado/mouse para navegar nos menus
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
    [Parameter(Mandatory=$false)]
    [string]$GamePath = "C:\Program Files (x86)\Steam\steamapps\common\Batman Arkham Asylum GOTY\Binaries\ShippingPC-BmGame.exe",

    [Parameter(Mandatory=$false)]
    [switch]$NoLaunch,

    [Parameter(Mandatory=$false)]
    [int]$MaxSteps = 50,

    [Parameter(Mandatory=$false)]
    [int]$StepDelayMs = 800,

    [Parameter(Mandatory=$false)]
    [int]$RecognitionDelayMs = 2000
)

# Paths
$RecognitionCliProject = "C:\dev\helenui\plugins\recognition-cli"
$BatmanAaProject = "C:\dev\helenui\batman-aa.json"
$OutputDir = "$PSScriptRoot\..\..\artifacts\navigation-screenshots"
$TempOcrConfigPath = Join-Path $OutputDir "temp-ocr-config.json"

# Create output directory
if (-not (Test-Path $OutputDir)) {
    New-Item -ItemType Directory -Path $OutputDir -Force | Out-Null
}

# Generate OCR config
$ocrConfig = @{
    ocr = @{
        engines = @(
            @{ type = "windows_native" }
        )
    }
}
$ocrConfig | ConvertTo-Json -Depth 4 | Set-Content -Path $TempOcrConfigPath

Write-Host "=== Batman Navigation to Graphics Options ===" -ForegroundColor Cyan
Write-Host "Game: $GamePath" -ForegroundColor Yellow
Write-Host "NoLaunch: $NoLaunch" -ForegroundColor Yellow
Write-Host "Max steps: $MaxSteps" -ForegroundColor Yellow
Write-Host ""

# Check prerequisites
if (-not $NoLaunch -and -not (Test-Path $GamePath)) {
    Write-Host "ERROR: Game executable not found at $GamePath" -ForegroundColor Red
    exit 1
}

if (-not (Test-Path "$RecognitionCliProject\src\RecognitionCli\RecognitionCli.csproj")) {
    Write-Host "ERROR: Recognition CLI project not found" -ForegroundColor Red
    exit 1
}

if (-not (Test-Path $BatmanAaProject)) {
    Write-Host "ERROR: Batman AA project JSON not found" -ForegroundColor Red
    exit 1
}

# Win32 P/Invoke for input simulation
Add-Type @"
using System;
using System.Runtime.InteropServices;

public class Win32Input {
    [DllImport("user32.dll")]
    public static extern bool SetForegroundWindow(IntPtr hWnd);

    [DllImport("user32.dll")]
    public static extern IntPtr FindWindow(string lpClassName, string lpWindowName);

    [DllImport("user32.dll")]
    public static extern void mouse_event(uint dwFlags, uint dx, uint dy, uint dwData, IntPtr dwExtraInfo);

    public const uint MOUSEEVENTF_LEFTDOWN = 0x0002;
    public const uint MOUSEEVENTF_LEFTUP = 0x0004;
}
"@

# Start the game
$gameProcess = $null
if (-not $NoLaunch) {
    Write-Host "Launching Batman Arkham Asylum..." -ForegroundColor Green
    $gameProcess = Start-Process -FilePath $GamePath -PassThru
    Write-Host "Game launched (PID: $($gameProcess.Id))" -ForegroundColor Green
    Write-Host "Waiting 15 seconds for game to initialize..." -ForegroundColor Yellow
    Start-Sleep -Seconds 15
} else {
    Write-Host "NoLaunch specified - assuming game is already running" -ForegroundColor Yellow
    Start-Sleep -Seconds 2
}

# Focus game window
function Focus-GameWindow {
    $windowTitle = "Batman"
    $hwnd = [Win32Input]::FindWindow($null, $windowTitle)

    if ($hwnd -eq [IntPtr]::Zero) {
        # Try partial match
        $processes = Get-Process -Name "ShippingPC-BmGame" -ErrorAction SilentlyContinue
        if ($processes -and $processes.Count -gt 0) {
            $hwnd = $processes[0].MainWindowHandle
        }
    }

    if ($hwnd -ne [IntPtr]::Zero) {
        [Win32Input]::SetForegroundWindow($hwnd)
        return $true
    }

    return $false
}

# Send key input
function Send-GameKey {
    param([string]$key)

    Focus-GameWindow
    Start-Sleep -Milliseconds 100
    [System.Windows.Forms.SendKeys]::SendWait($key)
}

# Send mouse click at position
function Send-Click {
    param([int]$x, [int]$y)

    Focus-GameWindow
    Start-Sleep -Milliseconds 100

    # Get window bounds
    $processes = Get-Process -Name "ShippingPC-BmGame" -ErrorAction SilentlyContinue
    if ($processes -and $processes.Count -gt 0) {
        $hwnd = $processes[0].MainWindowHandle
        if ($hwnd -ne [IntPtr]::Zero) {
            Add-Type -AssemblyName System.Windows.Forms
            $rect = New-Object System.Drawing.Rectangle
            # Use absolute positioning
            [System.Windows.Forms.Cursor]::Position = New-Object System.Drawing.Point($x, $y)
            Start-Sleep -Milliseconds 50
            [Win32Input]::mouse_event([Win32Input]::MOUSEEVENTF_LEFTDOWN, 0, 0, 0, [IntPtr]::Zero)
            Start-Sleep -Milliseconds 50
            [Win32Input]::mouse_event([Win32Input]::MOUSEEVENTF_LEFTUP, 0, 0, 0, [IntPtr]::Zero)
            return
        }
    }

    # Fallback: just send Enter
    [System.Windows.Forms.SendKeys]::SendWait("{ENTER}")
}

# Capture screenshot
function Capture-Screenshot {
    param([string]$filename)

    $screenshotPath = Join-Path $OutputDir $filename
    Add-Type -AssemblyName System.Windows.Forms
    $bounds = [System.Windows.Forms.Screen]::PrimaryScreen.Bounds
    $bitmap = New-Object System.Drawing.Bitmap $bounds.Width, $bounds.Height
    $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
    $graphics.CopyFromScreen($bounds.Location, [System.Drawing.Point]::Empty, $bounds.Size)
    $bitmap.Save($screenshotPath, [System.Drawing.Imaging.ImageFormat]::Png)
    $graphics.Dispose()
    $bitmap.Dispose()
    return $screenshotPath
}

# Run recognition
function Run-Recognition {
    param([string]$screenshotPath)

    $output = dotnet run --project "$RecognitionCliProject\src\RecognitionCli\RecognitionCli.csproj" `
        -- analyze `
        --project $BatmanAaProject `
        --image $screenshotPath `
        --config $TempOcrConfigPath 2>&1

    if ($LASTEXITCODE -ne 0) {
        return $null
    }

    return ($output | ConvertFrom-Json)
}

# Navigation logic
function Get-NavigationAction {
    param(
        [string]$screenName,
        [string]$highlightedItem
    )

    switch ($screenName) {
        "Title" {
            # Click to start or press Enter
            return @{
                Action = "PressEnter"
                Description = "Pressing Enter to start from title screen"
            }
        }
        "Saves" {
            # Select first save or press Enter
            return @{
                Action = "PressEnter"
                Description = "Selecting save to reach main menu"
            }
        }
        "Loading" {
            # Wait for loading to complete
            return @{
                Action = "Wait"
                Description = "Waiting for loading to complete"
            }
        }
        "Main" {
            # Navigate to Options - need to press DOWN multiple times
            # Typical main menu: Continue/Challenge/DLC/Bios/Trophies/Options/Exit
            # Options is usually near the bottom
            if ($highlightedItem -and $highlightedItem -ne "Options") {
                return @{
                    Action = "PressDown"
                    Description = "Navigating down from '$highlightedItem' towards 'Options'"
                }
            } else {
                return @{
                    Action = "PressEnter"
                    Description = "Entering Options menu"
                }
            }
        }
        "Options" {
            # Navigate to Graphics
            if ($highlightedItem -and $highlightedItem -ne "Graphics") {
                return @{
                    Action = "PressDown"
                    Description = "Navigating down from '$highlightedItem' towards 'Graphics'"
                }
            } else {
                return @{
                    Action = "PressEnter"
                    Description = "Entering Graphics menu"
                }
            }
        }
        "GraphicsOptions" {
            return @{
                Action = "Success"
                Description = "Graphics Options menu detected!"
            }
        }
        "AudioOptions" {
            # Went too far, go back up
            return @{
                Action = "PressUp"
                Description = "Went past Graphics, going back up to Options"
            }
        }
        "PauseMenu" {
            # Need to resume first
            return @{
                Action = "PressEscape"
                Description = "Exiting pause menu"
            }
        }
        "Game" {
            # Pause the game first
            return @{
                Action = "PressEscape"
                Description = "Pausing the game"
            }
        }
        "CutScene" {
            return @{
                Action = "Wait"
                Description = "Waiting for cutscene to end"
            }
        }
        default {
            # Unknown screen, try pressing Enter
            return @{
                Action = "PressEnter"
                Description = "Unknown screen '$screenName', pressing Enter"
            }
        }
    }
}

# Main navigation loop
$stepCount = 0
$success = $false
$lastScreenName = ""
$consecutiveWaits = 0
$consecutiveSameScreen = 0

Write-Host ""
Write-Host "=== Starting Navigation Loop ===" -ForegroundColor Cyan
Write-Host ""

while ($stepCount -lt $MaxSteps -and -not $success) {
    $stepCount++

    # Capture screenshot
    $screenshotPath = Capture-Screenshot -filename "nav_step_$(('{0:D3}' -f $stepCount)).png"
    Write-Host "[$stepCount/$MaxSteps] " -NoNewline

    # Run recognition
    $result = Run-Recognition -screenshotPath $screenshotPath

    if (-not $result) {
        Write-Host "Recognition failed, waiting..." -ForegroundColor Red
        Start-Sleep -Milliseconds ($RecognitionDelayMs * 2)
        $consecutiveWaits++
        continue
    }

    $screenMatch = $result.screen_match
    $currentScreen = if ($screenMatch -and $screenMatch.screen_name) { $screenMatch.screen_name } else { "Unknown" }

    # Get highlighted item
    $highlightedItem = $null
    $mainMenuItem = $result.variable_states | Where-Object { $_.variable_name -eq "MainMenuItem" }
    if ($mainMenuItem -and $mainMenuItem.value -and $mainMenuItem.matched) {
        $highlightedItem = $mainMenuItem.value
    }

    # Display state
    if ($currentScreen -ne $lastScreenName) {
        Write-Host "Screen: $currentScreen" -ForegroundColor Green
        if ($highlightedItem) {
            Write-Host "           Highlighted: $highlightedItem" -ForegroundColor Yellow
        }
    } else {
        Write-Host "Still: $currentScreen" -ForegroundColor DarkGray
        $consecutiveSameScreen++
    }

    $lastScreenName = $currentScreen

    # Get navigation action
    $navAction = Get-NavigationAction -screenName $currentScreen -highlightedItem $highlightedItem

    switch ($navAction.Action) {
        "Success" {
            Write-Host "           >> $($navAction.Description)" -ForegroundColor Cyan
            $success = $true
            break
        }
        "Wait" {
            Write-Host "           >> $($navAction.Description)" -ForegroundColor Yellow
            Start-Sleep -Milliseconds ($RecognitionDelayMs * 2)
            $consecutiveWaits++
            if ($consecutiveWaits -gt 10) {
                Write-Host "           >> Too many waits, pressing Enter to progress" -ForegroundColor Yellow
                Send-GameKey "{ENTER}"
                Start-Sleep -Milliseconds $RecognitionDelayMs
                $consecutiveWaits = 0
            }
        }
        "PressEnter" {
            Write-Host "           >> $($navAction.Description)" -ForegroundColor White
            Send-GameKey "{ENTER}"
            Start-Sleep -Milliseconds $RecognitionDelayMs
            $consecutiveWaits = 0
            $consecutiveSameScreen = 0
        }
        "PressDown" {
            Write-Host "           >> $($navAction.Description)" -ForegroundColor White
            Send-GameKey "{DOWN}"
            Start-Sleep -Milliseconds $StepDelayMs
            Send-GameKey "{DOWN}"
            Start-Sleep -Milliseconds $RecognitionDelayMs
            $consecutiveWaits = 0
            $consecutiveSameScreen = 0
        }
        "PressUp" {
            Write-Host "           >> $($navAction.Description)" -ForegroundColor White
            Send-GameKey "{UP}"
            Start-Sleep -Milliseconds $RecognitionDelayMs
            $consecutiveWaits = 0
            $consecutiveSameScreen = 0
        }
        "PressEscape" {
            Write-Host "           >> $($navAction.Description)" -ForegroundColor White
            Send-GameKey "{ESC}"
            Start-Sleep -Milliseconds $RecognitionDelayMs
            $consecutiveWaits = 0
            $consecutiveSameScreen = 0
        }
    }
}

# Summary
Write-Host ""
Write-Host "=== Navigation Summary ===" -ForegroundColor Cyan
Write-Host "Total steps: $stepCount" -ForegroundColor White
Write-Host "Reached Graphics Options: $success" -ForegroundColor White
Write-Host "Screenshots saved to: $OutputDir" -ForegroundColor White

if ($success) {
    Write-Host ""
    Write-Host "========================================" -ForegroundColor Cyan
    Write-Host "  NAVIGATION SUCCESSFUL!" -ForegroundColor Cyan
    Write-Host "  Graphics Options menu reached!" -ForegroundColor Cyan
    Write-Host "========================================" -ForegroundColor Cyan
} else {
    Write-Host ""
    Write-Host "WARNING: Did not reach Graphics Options in $MaxSteps steps" -ForegroundColor Red
    Write-Host "Check screenshots in $OutputDir to debug" -ForegroundColor Yellow
}

# Clean up
if (Test-Path $TempOcrConfigPath) {
    Remove-Item $TempOcrConfigPath -Force -ErrorAction SilentlyContinue
}

if ($gameProcess -and -not $gameProcess.HasExited) {
    Write-Host ""
    Write-Host "NOTE: Game is still running (PID: $($gameProcess.Id))" -ForegroundColor Yellow
}

Write-Host ""
Write-Host "Done!" -ForegroundColor Green
