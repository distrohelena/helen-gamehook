<#
.SYNOPSIS
    Navega automaticamente do inicio do Batman ate o menu Graphics Options.

.DESCRIPTION
    Este script usa inputs de teclado (ENTER, DOWN, ESC) para navegar nos menus
    do Batman ate chegar no menu Graphics Options.

    Fluxo de navegacao:
    1. Pressiona ENTER para passar da tela de titulo/save
    2. No menu principal, navega ate "Options" 
    3. Entra no menu Options
    4. Navega ate "Graphics"
    5. Entra no menu Graphics Options

    Captura screenshots a cada passo para verificacao.

.PARAMETER GamePath
    Caminho para o executavel ShippingPC-BmGame.exe

.PARAMETER NoLaunch
    Se especificado, nao inicia o jogo (assume que ja esta rodando)

.PARAMETER MaxSteps
    Numero maximo de passos de navegacao antes de desistir (padrao: 30)

.PARAMETER StepDelayMs
    Delay entre inputs de teclado em milissegundos (padrao: 600)

.PARAMETER MenuDelayMs
    Delay apos entrar em um submenu para carregar (padrao: 2000)

.EXAMPLE
    .\Test-BatmanNavigateToGraphicsSimple.ps1

.EXAMPLE
    .\Test-BatmanNavigateToGraphicsSimple.ps1 -NoLaunch -StepDelayMs 800
#>

param(
    [Parameter(Mandatory=$false)]
    [string]$GamePath = "D:\steam\steamapps\common\Batman Arkham Asylum GOTY\Binaries\ShippingPC-BmGame.exe",

    [Parameter(Mandatory=$false)]
    [switch]$NoLaunch,

    [Parameter(Mandatory=$false)]
    [int]$MaxSteps = 30,

    [Parameter(Mandatory=$false)]
    [int]$StepDelayMs = 600,

    [Parameter(Mandatory=$false)]
    [int]$MenuDelayMs = 2000
)

$OutputDir = "C:\dev\helenhook\artifacts\navigation-simple"

# Create output directory
if (-not (Test-Path $OutputDir)) {
    New-Item -ItemType Directory -Path $OutputDir -Force | Out-Null
}

Write-Host "=== Batman Navigation to Graphics Options (Simple) ===" -ForegroundColor Cyan
Write-Host "Game: $GamePath" -ForegroundColor Yellow
Write-Host "NoLaunch: $NoLaunch" -ForegroundColor Yellow
Write-Host "Max steps: $MaxSteps" -ForegroundColor Yellow
Write-Host "Output: $OutputDir" -ForegroundColor Yellow
Write-Host ""

# Check if game exists
if (-not $NoLaunch -and -not (Test-Path $GamePath)) {
    Write-Host "ERROR: Game executable not found at $GamePath" -ForegroundColor Red
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

Add-Type -AssemblyName System.Windows.Forms

# Start the game
$gameProcess = $null
if (-not $NoLaunch) {
    Write-Host "Launching Batman Arkham Asylum..." -ForegroundColor Green
    $gameProcess = Start-Process -FilePath $GamePath -PassThru
    Write-Host "Game launched (PID: $($gameProcess.Id))" -ForegroundColor Green
    Write-Host "Waiting 20 seconds for game to initialize..." -ForegroundColor Yellow
    Start-Sleep -Seconds 20
} else {
    Write-Host "NoLaunch specified - assuming game is already running" -ForegroundColor Yellow
    Start-Sleep -Seconds 2
}

# Focus game window
function Focus-GameWindow {
    # Try multiple window title patterns
    $possibleTitles = @(
        "BATMAN: ARKHAM ASYLUM",
        "Batman: Arkham Asylum",
        "Batman"
    )
    
    foreach ($title in $possibleTitles) {
        $hwnd = [Win32Input]::FindWindow($null, $title)
        if ($hwnd -ne [IntPtr]::Zero) {
            [Win32Input]::SetForegroundWindow($hwnd)
            Start-Sleep -Milliseconds 100
            return $true
        }
    }
    
    # Fallback: find by process
    $processes = Get-Process -Name "ShippingPC-BmGame" -ErrorAction SilentlyContinue
    if ($processes -and $processes.Count -gt 0) {
        $hwnd = $processes[0].MainWindowHandle
        if ($hwnd -ne [IntPtr]::Zero) {
            [Win32Input]::SetForegroundWindow($hwnd)
            Start-Sleep -Milliseconds 100
            return $true
        }
    }
    
    return $false
}

# Send key input
function Send-GameKey {
    param([string]$key, [string]$description)

    Focus-GameWindow
    Start-Sleep -Milliseconds 200
    Write-Host "  -> $description" -ForegroundColor White
    [System.Windows.Forms.SendKeys]::SendWait($key)
    Start-Sleep -Milliseconds $StepDelayMs
}

# Capture screenshot - only the game window
function Capture-Screenshot {
    param([string]$filename)

    $screenshotPath = Join-Path $OutputDir $filename
    
    $processes = Get-Process -Name "ShippingPC-BmGame" -ErrorAction SilentlyContinue
    if ($processes -and $processes.Count -gt 0) {
        $hwnd = $processes[0].MainWindowHandle
        if ($hwnd -ne [IntPtr]::Zero) {
            # Get window rectangle
            Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;

public class Win32Window {
    [DllImport("user32.dll")]
    public static extern bool GetWindowRect(IntPtr hWnd, out RECT lpRect);
    
    [StructLayout(LayoutKind.Sequential)]
    public struct RECT {
        public int Left;
        public int Top;
        public int Right;
        public int Bottom;
    }
}
"@
            
            $rect = New-Object Win32Window+RECT
            [Win32Window]::GetWindowRect($hwnd, [ref]$rect) | Out-Null
            
            $width = $rect.Right - $rect.Left
            $height = $rect.Bottom - $rect.Top
            
            $bitmap = New-Object System.Drawing.Bitmap $width, $height
            $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
            $graphics.CopyFromScreen($rect.Left, $rect.Top, 0, 0, (New-Object System.Drawing.Size($width, $height)))
            $bitmap.Save($screenshotPath, [System.Drawing.Imaging.ImageFormat]::Png)
            $graphics.Dispose()
            $bitmap.Dispose()
        } else {
            # Fallback to full screen
            $bounds = [System.Windows.Forms.Screen]::PrimaryScreen.Bounds
            $bitmap = New-Object System.Drawing.Bitmap $bounds.Width, $bounds.Height
            $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
            $graphics.CopyFromScreen($bounds.Location, [System.Drawing.Point]::Empty, $bounds.Size)
            $bitmap.Save($screenshotPath, [System.Drawing.Imaging.ImageFormat]::Png)
            $graphics.Dispose()
            $bitmap.Dispose()
        }
    }
    
    return $screenshotPath
}

# Main navigation sequence
$stepCount = 0

Write-Host ""
Write-Host "=== Starting Navigation ===" -ForegroundColor Cyan
Write-Host ""

# Phase 0: Pause the game first (if in gameplay)
Write-Host "Phase 0: Pausing game (if running)..." -ForegroundColor Yellow
Capture-Screenshot -filename "step_00_before.png" | Out-Null
Send-GameKey "{ESC}" "Pressing ESC to pause"
Capture-Screenshot -filename "step_00_after.png" | Out-Null
Start-Sleep -Milliseconds $MenuDelayMs

# Phase 1: Navigate from pause menu to Options (steps 1-4)
Write-Host ""
Write-Host "Phase 1: Navigating pause menu to Options..." -ForegroundColor Yellow
# In pause menu: Resume, Options, Quit (or similar)
# Press DOWN once to reach Options
$stepCount++
Capture-Screenshot -filename "step_$(('{0:D2}' -f $stepCount))_before.png" | Out-Null
Send-GameKey "{DOWN}" "Pressing DOWN to reach Options"
Capture-Screenshot -filename "step_$(('{0:D2}' -f $stepCount))_after.png" | Out-Null
Start-Sleep -Milliseconds $StepDelayMs

# Enter Options
$stepCount++
Capture-Screenshot -filename "step_$(('{0:D2}' -f $stepCount))_before.png" | Out-Null
Send-GameKey "{ENTER}" "Pressing ENTER to enter Options"
Capture-Screenshot -filename "step_$(('{0:D2}' -f $stepCount))_after.png" | Out-Null
Start-Sleep -Milliseconds $MenuDelayMs

# Phase 2: Navigate to Graphics in Options menu
Write-Host ""
Write-Host "Phase 2: Navigating to Graphics in Options..." -ForegroundColor Yellow
# Options menu typically has: Game Options, Graphics, Audio, Controls
# Graphics is usually the second item
$stepCount++
Capture-Screenshot -filename "step_$(('{0:D2}' -f $stepCount))_before.png" | Out-Null
Send-GameKey "{DOWN}" "Pressing DOWN to reach Graphics"
Capture-Screenshot -filename "step_$(('{0:D2}' -f $stepCount))_after.png" | Out-Null
Start-Sleep -Milliseconds $StepDelayMs

# Enter Graphics
$stepCount++
Capture-Screenshot -filename "step_$(('{0:D2}' -f $stepCount))_before.png" | Out-Null
Send-GameKey "{ENTER}" "Pressing ENTER to enter Graphics Options"
Capture-Screenshot -filename "step_$(('{0:D2}' -f $stepCount))_after.png" | Out-Null
Start-Sleep -Milliseconds $MenuDelayMs

# Summary
Write-Host ""
Write-Host "=== Navigation Complete ===" -ForegroundColor Cyan
Write-Host "Total steps: $stepCount" -ForegroundColor White
Write-Host "Screenshots saved to: $OutputDir" -ForegroundColor White
Write-Host ""
Write-Host "Check the screenshots to verify if we reached Graphics Options!" -ForegroundColor Yellow
Write-Host "Look at the last images in: $OutputDir" -ForegroundColor Cyan

# Note about game process
if ($gameProcess -and -not $gameProcess.HasExited) {
    Write-Host ""
    Write-Host "NOTE: Game is still running (PID: $($gameProcess.Id))" -ForegroundColor Yellow
}

Write-Host ""
Write-Host "Done!" -ForegroundColor Green
