param()

$ErrorActionPreference = 'Stop'

$HelperScriptPath = Join-Path $PSScriptRoot 'BatmanWindowHelpers.ps1'
. $HelperScriptPath

$OutputDir = 'C:\dev\helenhook\artifacts\ocr-config-preference'
if (-not (Test-Path $OutputDir)) {
    New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null
}

$ConfigPath = Join-Path $OutputDir 'temp-ocr-config.json'

try {
    Write-BatmanRecognitionOcrConfig -OutputPath $ConfigPath

    if (-not (Test-Path -LiteralPath $ConfigPath)) {
        throw "OCR config was not written to '$ConfigPath'."
    }

    $Config = Get-Content -LiteralPath $ConfigPath -Raw | ConvertFrom-Json
    $Engines = @($Config.ocr.engines)
    if ($Engines.Count -lt 1) {
        throw 'OCR config did not contain any engines.'
    }

    if ($Engines[0].type -ne 'tesseract') {
        throw "Expected the first OCR engine to be 'tesseract' but found '$($Engines[0].type)'."
    }

    if ($Engines[0].exePath -ne 'C:\Program Files\Tesseract-OCR\tesseract.exe') {
        throw "Expected the tesseract exePath to be 'C:\Program Files\Tesseract-OCR\tesseract.exe' but found '$($Engines[0].exePath)'."
    }

    if ($Engines.Count -gt 1) {
        throw "Expected Batman OCR config to use only the preferred Tesseract engine, but found $($Engines.Count) engines."
    }

    Write-Output 'PASS'
}
finally {
    if (Test-Path -LiteralPath $ConfigPath) {
        Remove-Item -LiteralPath $ConfigPath -Force -ErrorAction SilentlyContinue
    }
}
