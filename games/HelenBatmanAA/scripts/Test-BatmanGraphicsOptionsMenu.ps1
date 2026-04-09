<#
.SYNOPSIS
    Captura telas do Batman AA automaticamente até detectar o menu Graphics Options usando recognition CLI.

.DESCRIPTION
    Este script:
    1. Opcionalmente inicia o Batman Arkham Asylum
    2. Captura screenshots periodicamente
    3. Usa o recognition CLI do HelenUI para detectar qual tela está sendo exibida
    4. Continua capturando até detectar o menu Graphics Options
    5. Salva todas as capturas em um diretório de output

.PARAMETER GamePath
    Caminho para o executável ShippingPC-BmGame.exe

.PARAMETER OutputDir
    Diretório onde as screenshots serão salvas

.PARAMETER IntervalSeconds
    Intervalo entre capturas (padrão: 2 segundos)

.PARAMETER MaxScreenshots
    Número máximo de capturas antes de desistir (padrão: 100)

.PARAMETER NoLaunch
    Se especificado, não inicia o jogo (assume que já está rodando)

.PARAMETER OcrEngine
    Motor OCR a usar: 'windows_native' (padrão) ou 'tesseract'

.PARAMETER TesseractPath
    Caminho para tesseract.exe se usar OcrEngine='tesseract'

.EXAMPLE
    .\Test-BatmanGraphicsOptionsMenu.ps1 -GamePath "C:\Games\Batman\Binaries\ShippingPC-BmGame.exe"

.EXAMPLE
    .\Test-BatmanGraphicsOptionsMenu.ps1 -NoLaunch -IntervalSeconds 3

.EXAMPLE
    .\Test-BatmanGraphicsOptionsMenu.ps1 -OcrEngine tesseract -TesseractPath "C:\Program Files\Tesseract-OCR\tesseract.exe"
#>

param(
    [Parameter(Mandatory=$false)]
    [string]$GamePath = "C:\Program Files (x86)\Steam\steamapps\common\Batman Arkham Asylum GOTY\Binaries\ShippingPC-BmGame.exe",

    [Parameter(Mandatory=$false)]
    [string]$OutputDir = "$PSScriptRoot\..\..\artifacts\graphics-options-screenshots",

    [Parameter(Mandatory=$false)]
    [int]$IntervalSeconds = 2,

    [Parameter(Mandatory=$false)]
    [int]$MaxScreenshots = 100,

    [Parameter(Mandatory=$false)]
    [switch]$NoLaunch,

    [Parameter(Mandatory=$false)]
    [ValidateSet('windows_native', 'tesseract')]
    [string]$OcrEngine = 'windows_native',

    [Parameter(Mandatory=$false)]
    [string]$TesseractPath = "C:\Program Files\Tesseract-OCR\tesseract.exe"
)

# Paths
$RecognitionCliProject = "C:\dev\helenui\plugins\recognition-cli"
$BatmanAaProject = "C:\dev\helenui\batman-aa.json"

# Create output directory
if (-not (Test-Path $OutputDir)) {
    New-Item -ItemType Directory -Path $OutputDir -Force | Out-Null
}

# Generate OCR config based on parameters
$TempOcrConfigPath = Join-Path $OutputDir "temp-ocr-config.json"

if ($OcrEngine -eq 'tesseract') {
    if (-not (Test-Path $TesseractPath)) {
        Write-Host "ERROR: Tesseract not found at $TesseractPath" -ForegroundColor Red
        Write-Host "Please install Tesseract or use windows_native OCR engine" -ForegroundColor Red
        exit 1
    }
    
    $ocrConfig = @{
        ocr = @{
            engines = @(
                @{ type = "tesseract"; exePath = $TesseractPath },
                @{ type = "windows_native" }
            )
        }
    }
    Write-Host "Using Tesseract OCR: $TesseractPath" -ForegroundColor Yellow
} else {
    $ocrConfig = @{
        ocr = @{
            engines = @(
                @{ type = "windows_native" }
            )
        }
    }
    Write-Host "Using Windows Native OCR" -ForegroundColor Yellow
}

$ocrConfig | ConvertTo-Json -Depth 4 | Set-Content -Path $TempOcrConfigPath

Write-Host "=== Batman Graphics Options Screenshot Capture ===" -ForegroundColor Cyan
Write-Host "Game: $GamePath" -ForegroundColor Yellow
Write-Host "Output: $OutputDir" -ForegroundColor Yellow
Write-Host "Interval: ${IntervalSeconds}s" -ForegroundColor Yellow
Write-Host "Max screenshots: $MaxScreenshots" -ForegroundColor Yellow
Write-Host "NoLaunch: $NoLaunch" -ForegroundColor Yellow
Write-Host "OCR Engine: $OcrEngine" -ForegroundColor Yellow
Write-Host ""

# Check if recognition CLI project exists
if (-not (Test-Path "$RecognitionCliProject\src\RecognitionCli\RecognitionCli.csproj")) {
    Write-Host "ERROR: Recognition CLI project not found at $RecognitionCliProject" -ForegroundColor Red
    exit 1
}

# Check if batman-aa.json exists
if (-not (Test-Path $BatmanAaProject)) {
    Write-Host "ERROR: Batman AA project JSON not found at $BatmanAaProject" -ForegroundColor Red
    exit 1
}

# Check if game executable exists (only if we're going to launch it)
if (-not $NoLaunch -and -not (Test-Path $GamePath)) {
    Write-Host "ERROR: Game executable not found at $GamePath" -ForegroundColor Red
    Write-Host "Please provide correct path via -GamePath parameter" -ForegroundColor Red
    exit 1
}

# Start the game (if not using NoLaunch)
$gameProcess = $null
if (-not $NoLaunch) {
    Write-Host "Launching Batman Arkham Asylum..." -ForegroundColor Green
    $gameProcess = Start-Process -FilePath $GamePath -PassThru
    Write-Host "Game launched (PID: $($gameProcess.Id))" -ForegroundColor Green

    # Wait a bit for game to initialize
    Write-Host "Waiting 10 seconds for game to initialize..." -ForegroundColor Yellow
    Start-Sleep -Seconds 10
} else {
    Write-Host "NoLaunch specified - assuming game is already running" -ForegroundColor Yellow
    Write-Host "Make sure Batman is running and visible before continuing" -ForegroundColor Yellow
    Start-Sleep -Seconds 2
}

# Function to capture screenshot
function Capture-Screenshot {
    param([int]$index)

    $screenshotPath = Join-Path $OutputDir "screenshot_$($index.ToString('D4')).png"

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

# Function to run recognition CLI
function Run-Recognition {
    param([string]$screenshotPath)

    Write-Host "  Running recognition on: $(Split-Path $screenshotPath -Leaf)" -ForegroundColor DarkGray

    $output = dotnet run --project "$RecognitionCliProject\src\RecognitionCli\RecognitionCli.csproj" `
        -- analyze `
        --project $BatmanAaProject `
        --image $screenshotPath `
        --config $TempOcrConfigPath 2>&1

    if ($LASTEXITCODE -ne 0) {
        Write-Host "  WARNING: Recognition CLI failed" -ForegroundColor Yellow
        return $null
    }

    return ($output | ConvertFrom-Json)
}

# Main capture loop
$screenshotCount = 0
$graphicsOptionsDetected = $false
$lastScreenName = ""

Write-Host ""
Write-Host "Starting capture loop..." -ForegroundColor Cyan
Write-Host "Navigate to Options -> Graphics in the game when ready" -ForegroundColor Magenta
Write-Host ""

while ($screenshotCount -lt $MaxScreenshots -and -not $graphicsOptionsDetected) {
    $screenshotCount++

    # Capture screenshot
    $screenshotPath = Capture-Screenshot -index $screenshotCount
    Write-Host "[$screenshotCount/$MaxScreenshots] Captured: $(Split-Path $screenshotPath -Leaf)" -ForegroundColor White

    # Run recognition
    $result = Run-Recognition -screenshotPath $screenshotPath

    if ($result) {
        $screenMatch = $result.screen_match

        if ($screenMatch -and $screenMatch.screen_name) {
            $currentScreen = $screenMatch.screen_name

            if ($currentScreen -ne $lastScreenName) {
                Write-Host "  --> Screen detected: $currentScreen" -ForegroundColor Green
                $lastScreenName = $currentScreen
            } else {
                Write-Host "  --> Still on: $currentScreen" -ForegroundColor DarkGreen
            }

            # Check if we detected Graphics Options
            if ($currentScreen -eq "GraphicsOptions") {
                Write-Host ""
                Write-Host "========================================" -ForegroundColor Cyan
                Write-Host "  GRAPHICS OPTIONS MENU DETECTED! ✓" -ForegroundColor Cyan
                Write-Host "========================================" -ForegroundColor Cyan
                Write-Host "Screenshot saved: $screenshotPath" -ForegroundColor Yellow
                Write-Host ""

                $graphicsOptionsDetected = $true

                # Show evidence
                if ($result.evidence) {
                    Write-Host "Evidence:" -ForegroundColor White
                    $result.evidence | ForEach-Object {
                        Write-Host "  - $($_.clue_type): $($_.detail)" -ForegroundColor Gray
                    }
                }

                break
            }
        } else {
            Write-Host "  --> No screen matched" -ForegroundColor Yellow
        }
    } else {
        Write-Host "  --> Recognition failed" -ForegroundColor Red
    }

    # Wait before next capture
    if (-not $graphicsOptionsDetected -and $screenshotCount -lt $MaxScreenshots) {
        Start-Sleep -Seconds $IntervalSeconds
    }
}

# Summary
Write-Host ""
Write-Host "=== Capture Session Summary ===" -ForegroundColor Cyan
Write-Host "Total screenshots: $screenshotCount" -ForegroundColor White
Write-Host "Graphics Options detected: $graphicsOptionsDetected" -ForegroundColor White
Write-Host "Output directory: $OutputDir" -ForegroundColor White

if ($graphicsOptionsDetected) {
    Write-Host ""
    Write-Host "SUCCESS! Graphics Options menu was detected." -ForegroundColor Green
    Write-Host "Screenshots saved to: $OutputDir" -ForegroundColor Yellow
} else {
    Write-Host ""
    Write-Host "WARNING: Graphics Options menu not detected after $MaxScreenshots screenshots" -ForegroundColor Red
    Write-Host "Tips:" -ForegroundColor Yellow
    Write-Host "  1. Make sure to navigate to Options -> Graphics in the game" -ForegroundColor Yellow
    Write-Host "  2. Try increasing -IntervalSeconds if recognition is failing" -ForegroundColor Yellow
    Write-Host "  3. Ensure the game window is visible and not minimized" -ForegroundColor Yellow
    Write-Host "  4. Check screenshots to see what was captured" -ForegroundColor Yellow
}

# Clean up temp config
if (Test-Path $TempOcrConfigPath) {
    Remove-Item $TempOcrConfigPath -Force -ErrorAction SilentlyContinue
}

# Note about game process
if ($gameProcess -and -not $gameProcess.HasExited) {
    Write-Host ""
    Write-Host "NOTE: Game is still running (PID: $($gameProcess.Id))" -ForegroundColor Yellow
    Write-Host "To stop it: Stop-Process -Id $($gameProcess.Id)" -ForegroundColor Gray
}

Write-Host ""
Write-Host "Done!" -ForegroundColor Green
