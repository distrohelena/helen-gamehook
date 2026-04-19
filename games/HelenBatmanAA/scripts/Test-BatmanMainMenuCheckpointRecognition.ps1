param(
    [string]$MainMenuImagePath = 'C:\dev\helenhook\artifacts\live-main-menu-probe\01_main_before.png',
    [string]$SaveSelectImagePath = 'C:\dev\helenhook\artifacts\graphics-rollback-nav\02_save_select.png',
    [string]$StillTitleImagePath = 'C:\dev\helenhook\artifacts\nav-graphics\05_main_menu.png'
)

$ErrorActionPreference = 'Stop'

$HelperScriptPath = Join-Path $PSScriptRoot 'BatmanWindowHelpers.ps1'
. $HelperScriptPath

$OutputDir = 'C:\dev\helenhook\artifacts\main-menu-checkpoint-recognition'
if (-not (Test-Path $OutputDir)) {
    New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null
}

$TempOcrConfigPath = Join-Path $OutputDir 'temp-ocr-config.json'
Write-BatmanRecognitionOcrConfig -OutputPath $TempOcrConfigPath

try {
    foreach ($RequiredPath in @($MainMenuImagePath, $SaveSelectImagePath, $StillTitleImagePath)) {
        if (-not (Test-Path -LiteralPath $RequiredPath)) {
            throw "Required checkpoint screenshot not found: $RequiredPath"
        }
    }

    $SaveRecognition = Invoke-RecognitionCliAnalyzeImage `
        -ImagePath $SaveSelectImagePath `
        -RecognitionCliProject 'C:\dev\helenui\plugins\recognition-cli' `
        -BatmanProjectPath 'C:\dev\helenui\batman-aa.json' `
        -OcrConfigPath $TempOcrConfigPath

    Assert-BatmanRecognitionExpectedScreen `
        -Recognition $SaveRecognition `
        -ExpectedScreenName 'Saves' `
        -Context 'Recorded save-select checkpoint'

    $MainRecognition = Invoke-RecognitionCliAnalyzeImage `
        -ImagePath $MainMenuImagePath `
        -RecognitionCliProject 'C:\dev\helenui\plugins\recognition-cli' `
        -BatmanProjectPath 'C:\dev\helenui\batman-aa.json' `
        -OcrConfigPath $TempOcrConfigPath

    Assert-BatmanRecognitionExpectedScreen `
        -Recognition $MainRecognition `
        -ExpectedScreenName 'Main' `
        -Context 'Recorded main-menu checkpoint'

    $TitleRecognition = Invoke-RecognitionCliAnalyzeImage `
        -ImagePath $StillTitleImagePath `
        -RecognitionCliProject 'C:\dev\helenui\plugins\recognition-cli' `
        -BatmanProjectPath 'C:\dev\helenui\batman-aa.json' `
        -OcrConfigPath $TempOcrConfigPath

    $CheckpointFailure = $null
    try {
        Assert-BatmanRecognitionExpectedScreen `
            -Recognition $TitleRecognition `
            -ExpectedScreenName 'Main' `
            -Context 'Recorded main-menu checkpoint'
    } catch {
        $CheckpointFailure = $_.Exception.Message
    }

    if ([string]::IsNullOrWhiteSpace($CheckpointFailure)) {
        throw 'Expected the title screenshot to fail the main-menu checkpoint, but it passed.'
    }

    if ($CheckpointFailure -notmatch "found 'Title'") {
        throw "Expected the main-menu checkpoint failure to mention Title, but found: $CheckpointFailure"
    }

    if ($CheckpointFailure -notmatch 'Top candidates:') {
        throw "Expected the main-menu checkpoint failure to include candidate scores, but found: $CheckpointFailure"
    }

    Write-Output 'PASS'
}
finally {
    if (Test-Path -LiteralPath $TempOcrConfigPath) {
        Remove-Item -LiteralPath $TempOcrConfigPath -Force -ErrorAction SilentlyContinue
    }
}
