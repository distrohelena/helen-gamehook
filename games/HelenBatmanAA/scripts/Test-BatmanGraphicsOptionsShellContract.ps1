param(
    [string]$BatmanRoot
)

$ErrorActionPreference = 'Stop'

if ([string]::IsNullOrWhiteSpace($BatmanRoot)) {
    $BatmanRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
} else {
    $BatmanRoot = (Resolve-Path $BatmanRoot).Path
}

$TemplatePath = Join-Path $BatmanRoot 'builder\tools\NativeSubtitleExePatcher\SubtitleSizeModBuilder\GraphicsOptionsShellScriptTemplates.cs'
if (-not (Test-Path -LiteralPath $TemplatePath -PathType Leaf)) {
    throw "Batman graphics-options shell template was not found: $TemplatePath"
}

$TemplateText = Get-Content -LiteralPath $TemplatePath -Raw

$RequiredTokens = @(
    'Object.registerClass("ScreenOptionsGraphics",rs.ui.Screen)',
    'flash.external.ExternalInterface.call("FE_SetActiveScreenName","Options Menu")',
    'this.ButtonName = "Graphics Options"',
    '_parent.GotoScreen("OptionsGraphics")',
    'flash.external.ExternalInterface.call("FE_SetActiveScreenName","Graphics Options")',
    'function CancelScreen()',
    'ReturnFromScreen();',
    'this.BackScreen = "OptionsMenu"',
    'this.Title.text = "Graphics Options"',
    'this.RunAction = function()',
    'this.LeftClicker._visible = false;',
    'this.RightClicker._visible = false;'
)

foreach ($RequiredToken in $RequiredTokens) {
    if ($TemplateText.IndexOf($RequiredToken, [System.StringComparison]::Ordinal) -lt 0) {
        throw "Graphics-options shell template is missing required token: $RequiredToken"
    }
}

$ForbiddenTokens = @(
    'Helen_',
    'ApplyChanges',
    'GraphicsExitPrompt',
    'Unsaved',
    'RestartRequired',
    'BmEngine.ini'
)

foreach ($ForbiddenToken in $ForbiddenTokens) {
    if ($TemplateText.IndexOf($ForbiddenToken, [System.StringComparison]::Ordinal) -ge 0) {
        throw "Graphics-options shell template contains forbidden token: $ForbiddenToken"
    }
}

Write-Output 'PASS'
