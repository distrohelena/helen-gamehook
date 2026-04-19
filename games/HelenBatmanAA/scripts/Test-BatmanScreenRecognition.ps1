<#
.SYNOPSIS
    Captura uma única screenshot e executa o recognition CLI para testar detecção de telas.

.DESCRIPTION
    Este script captura uma screenshot do desktop atual e executa o recognition CLI
    para identificar qual tela do Batman está sendo exibida.

    Útil para:
    - Testar se o OCR está funcionando
    - Debugar detecção de telas específicas
    - Validar configuração do recognition

.PARAMETER OutputPath
    Caminho para salvar a screenshot (padrão: artifacts\screenshot-recognition-test.png)

.PARAMETER OcrEngine
    Motor OCR a usar: 'tesseract' (padrão) ou 'windows_native'

.PARAMETER TesseractPath
    Caminho para tesseract.exe se usar OcrEngine='tesseract'

.EXAMPLE
    .\Test-BatmanScreenRecognition.ps1

.EXAMPLE
    .\Test-BatmanScreenRecognition.ps1 -OcrEngine tesseract -TesseractPath "C:\Program Files\Tesseract-OCR\tesseract.exe"
#>

param(
    [Parameter(Mandatory=$false)]
    [string]$OutputPath = "$PSScriptRoot\..\..\artifacts\screenshot-recognition-test.png",

    [Parameter(Mandatory=$false)]
    [ValidateSet('windows_native', 'tesseract')]
    [string]$OcrEngine = 'tesseract',

    [Parameter(Mandatory=$false)]
    [string]$TesseractPath = "C:\Program Files\Tesseract-OCR\tesseract.exe"
)

# Paths
$RecognitionCliProject = "C:\dev\helenui\plugins\recognition-cli"
$BatmanAaProject = "C:\dev\helenui\batman-aa.json"
$HelperScriptPath = Join-Path $PSScriptRoot 'BatmanWindowHelpers.ps1'
. $HelperScriptPath

Write-Host "=== Batman Screen Recognition Test ===" -ForegroundColor Cyan
Write-Host ""

# Check prerequisites
if (-not (Test-Path "$RecognitionCliProject\src\RecognitionCli\RecognitionCli.csproj")) {
    Write-Host "ERROR: Recognition CLI project not found" -ForegroundColor Red
    exit 1
}

if (-not (Test-Path $BatmanAaProject)) {
    Write-Host "ERROR: Batman AA project JSON not found at $BatmanAaProject" -ForegroundColor Red
    exit 1
}

# Create output directory
$outputDir = Split-Path $OutputPath -Parent
if (-not (Test-Path $outputDir)) {
    New-Item -ItemType Directory -Path $outputDir -Force | Out-Null
}

# Generate OCR config
$TempOcrConfigPath = Join-Path $outputDir "temp-ocr-config.json"
if ($OcrEngine -eq 'tesseract' -and $TesseractPath -ne (Get-BatmanTesseractExecutablePath)) {
    if (-not (Test-Path -LiteralPath $TesseractPath)) {
        Write-Host "ERROR: Tesseract not found at $TesseractPath" -ForegroundColor Red
        exit 1
    }

    @{
        ocr = @{
            engines = @(
                @{ type = 'tesseract'; exePath = $TesseractPath }
            )
        }
    } | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath $TempOcrConfigPath
} else {
    Write-BatmanRecognitionOcrConfig -OutputPath $TempOcrConfigPath -PreferredEngine $OcrEngine
}

if ($OcrEngine -eq 'tesseract') {
    Write-Host "Using Tesseract OCR: $TesseractPath" -ForegroundColor Yellow
} else {
    Write-Host "Using Windows Native OCR" -ForegroundColor Yellow
}

# Capture screenshot
Write-Host "Capturing screenshot..." -ForegroundColor Green
Add-Type -AssemblyName System.Windows.Forms
$bounds = [System.Windows.Forms.Screen]::PrimaryScreen.Bounds
$bitmap = New-Object System.Drawing.Bitmap $bounds.Width, $bounds.Height
$graphics = [System.Drawing.Graphics]::FromImage($bitmap)
$graphics.CopyFromScreen($bounds.Location, [System.Drawing.Point]::Empty, $bounds.Size)
$bitmap.Save($OutputPath, [System.Drawing.Imaging.ImageFormat]::Png)
$graphics.Dispose()
$bitmap.Dispose()

Write-Host "Screenshot saved to: $OutputPath" -ForegroundColor Green
Write-Host ""

# Run recognition
Write-Host "Running recognition..." -ForegroundColor Green
Write-Host ""

$output = dotnet run --project "$RecognitionCliProject\src\RecognitionCli\RecognitionCli.csproj" `
    -- analyze `
    --project $BatmanAaProject `
    --image $OutputPath `
    --config $TempOcrConfigPath 2>&1

$recognitionExitCode = $LASTEXITCODE

# Clean up temp config
if (Test-Path $TempOcrConfigPath) {
    Remove-Item $TempOcrConfigPath -Force -ErrorAction SilentlyContinue
}

# Display results
if ($recognitionExitCode -ne 0) {
    Write-Host "ERROR: Recognition CLI failed" -ForegroundColor Red
    Write-Host "Output:" -ForegroundColor Yellow
    $output | ForEach-Object { Write-Host "  $_" -ForegroundColor Gray }
    exit 1
}

try {
    $result = $output | ConvertFrom-Json
} catch {
    Write-Host "ERROR: Failed to parse recognition result" -ForegroundColor Red
    Write-Host "Output:" -ForegroundColor Yellow
    $output | ForEach-Object { Write-Host "  $_" -ForegroundColor Gray }
    exit 1
}

# Display screen match
Write-Host "=== Recognition Result ===" -ForegroundColor Cyan
Write-Host ""

if ($result.screen_match -and $result.screen_match.screen_name) {
    Write-Host "Screen: $($result.screen_match.screen_name)" -ForegroundColor Green
    Write-Host "Confidence: $($result.screen_match.confidence)" -ForegroundColor White
    
    if ($result.screen_match.matched_clues) {
        Write-Host ""
        Write-Host "Matched clues:" -ForegroundColor White
        $result.screen_match.matched_clues | ForEach-Object {
            Write-Host "  ✓ $($_.clue_id) (weight: $($_.weight))" -ForegroundColor Gray
        }
    }
} else {
    Write-Host "No screen matched!" -ForegroundColor Yellow
    Write-Host "This means none of the expected screens were detected." -ForegroundColor Gray
    Write-Host "Make sure Batman is showing a recognizable menu screen." -ForegroundColor Gray
}

# Show variable states
if ($result.variable_states) {
    Write-Host ""
    Write-Host "Variable states:" -ForegroundColor White
    $result.variable_states | ForEach-Object {
        $valueStr = if ($_.value) { $_.value } else { "unknown" }
        $statusColor = if ($_.matched) { "Green" } else { "Gray" }
        Write-Host "  $($_.variable_name) = " -NoNewline
        Write-Host "$valueStr" -ForegroundColor $statusColor
    }
    
    # Highlight the MainMenuItem if detected
    $mainMenuItem = $result.variable_states | Where-Object { $_.variable_name -eq "MainMenuItem" }
    if ($mainMenuItem -and $mainMenuItem.value) {
        Write-Host ""
        Write-Host "==> Currently highlighted menu item: $($mainMenuItem.value)" -ForegroundColor Yellow -BackgroundColor DarkGreen
    }
}

# Show evidence
if ($result.evidence -and $result.evidence.Count -gt 0) {
    Write-Host ""
    Write-Host "Evidence:" -ForegroundColor White
    $result.evidence | ForEach-Object {
        Write-Host "  - $($_.clue_type): $($_.detail)" -ForegroundColor Gray
    }
}

# Show OCR diagnostics
if ($result.diagnostics -and $result.diagnostics.ocr_results) {
    Write-Host ""
    Write-Host "OCR Diagnostics:" -ForegroundColor White
    $result.diagnostics.ocr_results | ForEach-Object {
        $status = if ($_.success) { "✓" } else { "✗" }
        Write-Host "  $status $($_.engine_type): $($_.text_count) texts detected" -ForegroundColor Gray
    }
}

Write-Host ""
Write-Host "Done!" -ForegroundColor Green
