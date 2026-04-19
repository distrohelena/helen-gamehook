param(
    [string]$AudioOptionsPath = 'C:\dev\helenhook\artifacts\graphics-rollback-nav\07_graphics_options.png'
)

$ErrorActionPreference = 'Stop'

$HelperScriptPath = Join-Path $PSScriptRoot 'BatmanWindowHelpers.ps1'
. $HelperScriptPath

$OutputDir = 'C:\dev\helenhook\artifacts\graphics-absence-recognition'
if (-not (Test-Path $OutputDir)) {
    New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null
}

$TempOcrConfigPath = Join-Path $OutputDir 'temp-ocr-config.json'
Write-BatmanRecognitionOcrConfig -OutputPath $TempOcrConfigPath

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

try {
    if (-not (Test-Path -LiteralPath $AudioOptionsPath)) {
        throw "Required screenshot not found: $AudioOptionsPath"
    }

    $Recognition = Invoke-RecognitionCliAnalyzeImage `
        -ImagePath $AudioOptionsPath `
        -RecognitionCliProject 'C:\dev\helenui\plugins\recognition-cli' `
        -BatmanProjectPath 'C:\dev\helenui\batman-aa.json' `
        -OcrConfigPath $TempOcrConfigPath

    $AudioScore = Get-ScreenScore -Recognition $Recognition -ScreenName 'AudioOptions'
    $GraphicsScore = Get-ScreenScore -Recognition $Recognition -ScreenName 'GraphicsOptions'
    if ($AudioScore -lt 1) {
        throw "Expected AudioOptions score to prove missing graphics, but found '$AudioScore'."
    }

    if ($GraphicsScore -ne 0) {
        throw "Expected GraphicsOptions score to stay at 0 on the Audio Options screen, but found '$GraphicsScore'."
    }

    Write-Output 'PASS'
}
finally {
    if (Test-Path $TempOcrConfigPath) {
        Remove-Item -LiteralPath $TempOcrConfigPath -Force -ErrorAction SilentlyContinue
    }
}
