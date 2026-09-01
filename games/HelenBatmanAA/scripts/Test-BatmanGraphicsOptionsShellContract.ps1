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

$BuilderProjectPath = Join-Path $BatmanRoot 'builder\tools\NativeSubtitleExePatcher\SubtitleSizeModBuilder\SubtitleSizeModBuilder.csproj'
$DebugAssemblyPath = Join-Path $BatmanRoot 'builder\tools\NativeSubtitleExePatcher\SubtitleSizeModBuilder\bin\Debug\net8.0\SubtitleSizeModBuilder.dll'
if (-not (Test-Path -LiteralPath $BuilderProjectPath -PathType Leaf)) {
    throw "Batman graphics-options builder project was not found: $BuilderProjectPath"
}

$BuildOutput = & dotnet build $BuilderProjectPath -c Debug --nologo 2>&1
if ($LASTEXITCODE -ne 0) {
    throw "Batman graphics-options builder failed to build:`n$($BuildOutput -join [Environment]::NewLine)"
}

if (-not (Test-Path -LiteralPath $DebugAssemblyPath -PathType Leaf)) {
    throw "Batman graphics-options Debug assembly was not found after build: $DebugAssemblyPath"
}

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

function Assert-ContainsOrdinal {
    param(
        [string]$Text,
        [string]$Token,
        [string]$Context
    )

    if ($Text.IndexOf($Token, [System.StringComparison]::Ordinal) -lt 0) {
        throw "$Context is missing required token: $Token"
    }
}

function Get-ActionScriptFunctionBody {
    param(
        [string]$ScriptText,
        [string]$FunctionName,
        [string]$Context
    )

    $FunctionToken = "this.$FunctionName = function()"
    $FunctionIndex = $ScriptText.IndexOf($FunctionToken, [System.StringComparison]::Ordinal)
    if ($FunctionIndex -lt 0) {
        throw "$Context is missing function: $FunctionToken"
    }

    $BodyStart = $ScriptText.IndexOf('{', $FunctionIndex)
    $BodyEnd = $ScriptText.IndexOf('};', $BodyStart)
    if ($BodyStart -lt 0 -or $BodyEnd -lt 0) {
        throw "$Context has an unterminated function: $FunctionToken"
    }

    return $ScriptText.Substring($BodyStart + 1, $BodyEnd - $BodyStart - 1).Trim()
}

function Assert-NoOpActionScriptFunction {
    param(
        [string]$ScriptText,
        [string]$FunctionName,
        [string]$Context
    )

    $FunctionBody = Get-ActionScriptFunctionBody -ScriptText $ScriptText -FunctionName $FunctionName -Context $Context
    if ($FunctionBody.Length -ne 0) {
        throw "$Context function $FunctionName must be a no-op, but contains: $FunctionBody"
    }
}

$BridgeSource = @'
using System.Reflection;
using System.Text.Json;

if (args.Length != 1)
{
    throw new ArgumentException("Expected the builder assembly path as the only argument.");
}

Assembly assembly = Assembly.LoadFrom(args[0]);
Type templateType = assembly.GetType("SubtitleSizeModBuilder.GraphicsOptionsShellScriptTemplates", throwOnError: true)!;
BindingFlags staticFlags = BindingFlags.Static | BindingFlags.Public | BindingFlags.NonPublic;
FieldInfo rowClipActionsField = templateType.GetField("RowClipActions", staticFlags)
    ?? throw new MissingFieldException(templateType.FullName, "RowClipActions");
string[] rowClipActions = (string[])rowClipActionsField.GetValue(null)!;
MethodInfo escapeMethod = templateType.GetMethod(
    "EscapeActionScriptString",
    staticFlags,
    binder: null,
    types: new[] { typeof(string) },
    modifiers: null)
    ?? throw new MissingMethodException(templateType.FullName, "EscapeActionScriptString");

string escapeInput = "slash\\quote\"" + (char)13 + (char)10 + (char)9;
string escapedValue = (string)escapeMethod.Invoke(null, new object?[] { escapeInput })!;
string? nulInnerException = null;
try
{
    _ = escapeMethod.Invoke(null, new object?[] { ((char)0).ToString() });
}
catch (TargetInvocationException exception)
{
    nulInnerException = exception.InnerException?.GetType().FullName;
}

Console.WriteLine(JsonSerializer.Serialize(new ReflectionContract(rowClipActions, escapedValue, nulInnerException)));

public sealed record ReflectionContract(string[] RowClipActions, string EscapedValue, string? NulInnerException);
'@

function Get-ReflectionContract {
    param(
        [string]$AssemblyPath
    )

    if ($PSVersionTable.PSEdition -eq 'Core') {
        $Assembly = [System.Reflection.Assembly]::LoadFrom($AssemblyPath)
        $TemplateType = $Assembly.GetType('SubtitleSizeModBuilder.GraphicsOptionsShellScriptTemplates', $true)
        $StaticFlags = [System.Reflection.BindingFlags]::Static -bor [System.Reflection.BindingFlags]::Public -bor [System.Reflection.BindingFlags]::NonPublic
        $RowClipActionsField = $TemplateType.GetField('RowClipActions', $StaticFlags)
        if ($null -eq $RowClipActionsField) {
            throw 'Graphics-options shell template type is missing RowClipActions.'
        }

        $EscapeMethod = $TemplateType.GetMethod('EscapeActionScriptString', $StaticFlags, $null, [System.Reflection.CallingConventions]::Any, @([string]), $null)
        if ($null -eq $EscapeMethod) {
            throw 'Graphics-options shell template type is missing EscapeActionScriptString.'
        }

        $RowClipActions = @($RowClipActionsField.GetValue($null))
        $EscapeInput = 'slash\quote"' + [char]13 + [char]10 + [char]9
        $EscapedValue = [string]$EscapeMethod.Invoke($null, @($EscapeInput))
        $NulInnerException = $null
        try {
            [void]$EscapeMethod.Invoke($null, @([string][char]0))
        } catch [System.Reflection.TargetInvocationException] {
            $NulInnerException = $_.Exception.InnerException.GetType().FullName
        }

        return [pscustomobject]@{
            RowClipActions = $RowClipActions
            EscapedValue = $EscapedValue
            NulInnerException = $NulInnerException
        }
    }

    $BridgeRoot = Join-Path ([System.IO.Path]::GetTempPath()) ("BatmanGraphicsOptionsShellReflection-" + [guid]::NewGuid().ToString('N'))
    New-Item -ItemType Directory -Path $BridgeRoot -Force | Out-Null
    try {
        $BridgeProjectPath = Join-Path $BridgeRoot 'ReflectionBridge.csproj'
        $BridgeSourcePath = Join-Path $BridgeRoot 'Program.cs'
        $BridgeProject = @'
<Project Sdk="Microsoft.NET.Sdk">
  <PropertyGroup>
    <OutputType>Exe</OutputType>
    <TargetFramework>net8.0</TargetFramework>
    <ImplicitUsings>enable</ImplicitUsings>
    <Nullable>enable</Nullable>
  </PropertyGroup>
</Project>
'@
        Set-Content -LiteralPath $BridgeProjectPath -Value $BridgeProject -Encoding UTF8
        Set-Content -LiteralPath $BridgeSourcePath -Value $BridgeSource -Encoding UTF8

        $BridgeBuildOutput = & dotnet build $BridgeProjectPath -c Debug --nologo 2>&1
        if ($LASTEXITCODE -ne 0) {
            throw "Reflection bridge failed to build:`n$($BridgeBuildOutput -join [Environment]::NewLine)"
        }

        $BridgeAssemblyPath = Join-Path $BridgeRoot 'bin\Debug\net8.0\ReflectionBridge.dll'
        $ReflectionOutput = & dotnet $BridgeAssemblyPath $AssemblyPath 2>&1
        if ($LASTEXITCODE -ne 0) {
            throw "Reflection bridge failed:`n$($ReflectionOutput -join [Environment]::NewLine)"
        }

        $ReflectionJson = [string]($ReflectionOutput | Select-Object -Last 1)
        return ($ReflectionJson | ConvertFrom-Json)
    } finally {
        if (Test-Path -LiteralPath $BridgeRoot) {
            Remove-Item -LiteralPath $BridgeRoot -Recurse -Force
        }
    }
}

$ReflectionContract = Get-ReflectionContract -AssemblyPath $DebugAssemblyPath
$RowClipActions = @($ReflectionContract.RowClipActions)
if ($RowClipActions.Count -ne 15) {
    throw "Expected exactly 15 graphics row clip actions, found $($RowClipActions.Count)."
}

$ExpectedRows = @(
    @{ Label = 'Fullscreen'; Value = 'Not active' },
    @{ Label = 'Resolution'; Value = 'Not active' },
    @{ Label = 'VSync'; Value = 'Not active' },
    @{ Label = 'MSAA'; Value = 'Not active' },
    @{ Label = 'Detail Level'; Value = 'Not active' },
    @{ Label = 'Bloom'; Value = 'Not active' },
    @{ Label = 'Dynamic Shadows'; Value = 'Not active' },
    @{ Label = 'Motion Blur'; Value = 'Not active' },
    @{ Label = 'Distortion'; Value = 'Not active' },
    @{ Label = 'Fog Volumes'; Value = 'Not active' },
    @{ Label = 'Spherical Harmonic Lighting'; Value = 'Not active' },
    @{ Label = 'Ambient Occlusion'; Value = 'Not active' },
    @{ Label = 'PhysX'; Value = 'Not active' },
    @{ Label = 'Stereo 3D'; Value = 'Not active' }
)

for ($RowIndex = 0; $RowIndex -lt $ExpectedRows.Count; $RowIndex++) {
    $RowScript = [string]$RowClipActions[$RowIndex]
    $ExpectedLabel = $ExpectedRows[$RowIndex].Label
    $ExpectedValue = $ExpectedRows[$RowIndex].Value
    $RowContext = "Graphics row action $($RowIndex + 1)"

    Assert-ContainsOrdinal -Text $RowScript -Token "this.Label.Label.Text.text = `"$ExpectedLabel`";" -Context $RowContext
    Assert-ContainsOrdinal -Text $RowScript -Token "this.ItemText.text = `"$ExpectedValue`";" -Context $RowContext
    Assert-ContainsOrdinal -Text $RowScript -Token 'this._visible = true;' -Context $RowContext
    Assert-ContainsOrdinal -Text $RowScript -Token 'this.Names = new Array("Not active");' -Context $RowContext
    Assert-ContainsOrdinal -Text $RowScript -Token 'this.State = 0;' -Context $RowContext
    Assert-ContainsOrdinal -Text $RowScript -Token 'this.Initial = 0;' -Context $RowContext
    Assert-ContainsOrdinal -Text $RowScript -Token 'this.Default = 0;' -Context $RowContext
    Assert-ContainsOrdinal -Text $RowScript -Token 'this.LeftClicker._visible = false;' -Context $RowContext
    Assert-ContainsOrdinal -Text $RowScript -Token 'this.RightClicker._visible = false;' -Context $RowContext
    Assert-NoOpActionScriptFunction -ScriptText $RowScript -FunctionName 'RunAction' -Context $RowContext
    Assert-NoOpActionScriptFunction -ScriptText $RowScript -FunctionName 'Increment' -Context $RowContext
    Assert-NoOpActionScriptFunction -ScriptText $RowScript -FunctionName 'Decrement' -Context $RowContext
    Assert-NoOpActionScriptFunction -ScriptText $RowScript -FunctionName 'ShowPrompt' -Context $RowContext

    $DestroyBody = Get-ActionScriptFunctionBody -ScriptText $RowScript -FunctionName 'Destroy' -Context $RowContext
    Assert-ContainsOrdinal -Text $DestroyBody -Token 'this.Names' -Context "$RowContext Destroy"
    Assert-ContainsOrdinal -Text $DestroyBody -Token '.pop()' -Context "$RowContext Destroy"
    if ($DestroyBody.IndexOf('ExternalInterface', [System.StringComparison]::Ordinal) -ge 0) {
        throw "$RowContext Destroy must not call an external interface."
    }

    foreach ($ForbiddenToken in $ForbiddenTokens) {
        if ($RowScript.IndexOf($ForbiddenToken, [System.StringComparison]::Ordinal) -ge 0) {
            throw "$RowContext contains forbidden token: $ForbiddenToken"
        }
    }
}

$HiddenRowScript = [string]$RowClipActions[14]
Assert-ContainsOrdinal -Text $HiddenRowScript -Token 'this._visible = false;' -Context 'Graphics row action 15'
Assert-ContainsOrdinal -Text $HiddenRowScript -Token 'this.LeftClicker._visible = false;' -Context 'Graphics row action 15'
Assert-ContainsOrdinal -Text $HiddenRowScript -Token 'this.RightClicker._visible = false;' -Context 'Graphics row action 15'
Assert-NoOpActionScriptFunction -ScriptText $HiddenRowScript -FunctionName 'RunAction' -Context 'Graphics row action 15'
Assert-NoOpActionScriptFunction -ScriptText $HiddenRowScript -FunctionName 'Increment' -Context 'Graphics row action 15'
Assert-NoOpActionScriptFunction -ScriptText $HiddenRowScript -FunctionName 'Decrement' -Context 'Graphics row action 15'
Assert-NoOpActionScriptFunction -ScriptText $HiddenRowScript -FunctionName 'ShowPrompt' -Context 'Graphics row action 15'
$HiddenDestroyBody = Get-ActionScriptFunctionBody -ScriptText $HiddenRowScript -FunctionName 'Destroy' -Context 'Graphics row action 15'
Assert-ContainsOrdinal -Text $HiddenDestroyBody -Token 'this.Names' -Context 'Graphics row action 15 Destroy'
Assert-ContainsOrdinal -Text $HiddenDestroyBody -Token '.pop()' -Context 'Graphics row action 15 Destroy'
if ($HiddenDestroyBody.IndexOf('ExternalInterface', [System.StringComparison]::Ordinal) -ge 0) {
    throw 'Graphics row action 15 Destroy must not call an external interface.'
}
foreach ($ForbiddenToken in $ForbiddenTokens) {
    if ($HiddenRowScript.IndexOf($ForbiddenToken, [System.StringComparison]::Ordinal) -ge 0) {
        throw "Graphics row action 15 contains forbidden token: $ForbiddenToken"
    }
}

$ExpectedEscapedValue = 'slash\\quote\"\r\n\t'
if ([string]$ReflectionContract.EscapedValue -cne $ExpectedEscapedValue) {
    throw "EscapeActionScriptString returned '$($ReflectionContract.EscapedValue)' instead of '$ExpectedEscapedValue'."
}

if ([string]$ReflectionContract.NulInnerException -cne 'System.ArgumentException') {
    throw 'EscapeActionScriptString accepted an unsupported NUL control character.'
}

Write-Output 'PASS'
