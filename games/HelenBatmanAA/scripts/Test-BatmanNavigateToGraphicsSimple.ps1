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

$OutputDir = "C:\dev\helenhook\artifacts\nav-graphics"
if (-not (Test-Path $OutputDir)) { New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null }

Add-Type -AssemblyName System.Windows.Forms
Add-Type @"
using System;
using System.Runtime.InteropServices;
public class Win32Input {
    [DllImport("user32.dll")]
    public static extern bool SetForegroundWindow(IntPtr hWnd);
    [DllImport("user32.dll")]
    public static extern IntPtr FindWindow(string lpClassName, string lpWindowName);
    [DllImport("user32.dll")]
    public static extern bool GetWindowRect(IntPtr hWnd, out RECT r);
    [StructLayout(LayoutKind.Sequential)]
    public struct RECT { public int Left; public int Top; public int Right; public int Bottom; }
}
"@

function Focus-Game {
    $p = Get-Process -Name "ShippingPC-BmGame" -ErrorAction SilentlyContinue
    if ($p -and $p.Count -gt 0 -and $p[0].MainWindowHandle -ne [IntPtr]::Zero) {
        [Win32Input]::SetForegroundWindow($p[0].MainWindowHandle)
        Start-Sleep -Milliseconds 300
        return $true
    }
    return $false
}

function Press {
    param([string]$key, [string]$label)
    Focus-Game
    Start-Sleep -Milliseconds 200
    Write-Host "  -> $label" -ForegroundColor White
    [System.Windows.Forms.SendKeys]::SendWait($key)
    Start-Sleep -Milliseconds $StepDelayMs
}

function Snap {
    param([string]$name)
    $path = Join-Path $OutputDir "$name.png"
    # Full-screen capture to ensure we see all menus
    $bounds = [System.Windows.Forms.Screen]::PrimaryScreen.Bounds
    $bmp = New-Object System.Drawing.Bitmap $bounds.Width, $bounds.Height
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.CopyFromScreen($bounds.Location, [System.Drawing.Point]::Empty, $bounds.Size)
    $bmp.Save($path, [System.Drawing.Imaging.ImageFormat]::Png)
    $g.Dispose(); $bmp.Dispose()

    # Also copy to C:\dev\batma\prints for user review
    $batmaDir = "C:\dev\batma\prints"
    if (-not (Test-Path $batmaDir)) { New-Item -ItemType Directory -Force -Path $batmaDir | Out-Null }
    $batmaPath = Join-Path $batmaDir "$name.png"
    Copy-Item -Path $path -Destination $batmaPath -Force

    return $path
}

Write-Host "=== Navigate to Graphics Options ===" -ForegroundColor Cyan

if (-not $NoLaunch) {
    Write-Host "Launching Batman..." -ForegroundColor Green
    Start-Process "D:\steam\steamapps\common\Batman Arkham Asylum GOTY\Binaries\ShippingPC-BmGame.exe"
    Start-Sleep -Seconds 20
} else {
    Start-Sleep -Seconds 2
}

# Step 1: Title -> Saved Game Select
Snap "01_title" | Out-Null
Press "{ENTER}" "Title: Press ENTER"
Snap "02_after_title" | Out-Null
Start-Sleep -Milliseconds $MenuDelayMs

# Step 2: Saved Game Select -> Main Menu
Snap "03_save_select" | Out-Null
Press "{ENTER}" "Save Select: Press ENTER to reach Main Menu"
Snap "04_after_save" | Out-Null
Start-Sleep -Milliseconds $MenuDelayMs

# Step 3: Main Menu -> navigate to Options (5 DOWNs)
Snap "05_main_menu" | Out-Null
for ($i = 0; $i -lt 5; $i++) {
    Press "{DOWN}" "Main Menu: DOWN ($($i+1)/5)"
    Snap "06_main_down_$($i+1)" | Out-Null
    Start-Sleep -Milliseconds $StepDelayMs
}

# Step 4: Enter Options
Press "{ENTER}" "Main Menu: ENTER into Options"
Snap "07_options_menu" | Out-Null
Start-Sleep -Milliseconds $MenuDelayMs

# Step 5: Navigate to Graphics (1 DOWN)
Snap "08_in_options" | Out-Null
Press "{DOWN}" "Options: DOWN to Graphics"
Snap "09_options_down" | Out-Null
Start-Sleep -Milliseconds $StepDelayMs

# Step 6: Enter Graphics
Press "{ENTER}" "Options: ENTER into Graphics Options"
Snap "10_graphics_options" | Out-Null
Start-Sleep -Milliseconds $MenuDelayMs

Write-Host ""
Write-Host "=== Done! Screenshots saved to: $OutputDir ===" -ForegroundColor Green
Write-Host "Check 10_graphics_options.png to verify!" -ForegroundColor Yellow
