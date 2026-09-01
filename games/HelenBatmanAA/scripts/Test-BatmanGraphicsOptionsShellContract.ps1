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

$BuilderSourceRoot = Join-Path $BatmanRoot 'builder\tools\NativeSubtitleExePatcher\SubtitleSizeModBuilder'
$ProgramPath = Join-Path $BuilderSourceRoot 'Program.cs'
$AssetBuilderPath = Join-Path $BuilderSourceRoot 'GraphicsOptionsAssetBuilder.cs'
$XmlPatcherPath = Join-Path $BuilderSourceRoot 'GraphicsOptionsXmlPatcher.cs'
foreach ($SourcePath in @($ProgramPath, $AssetBuilderPath, $XmlPatcherPath)) {
    if (-not (Test-Path -LiteralPath $SourcePath -PathType Leaf)) {
        throw "Batman graphics-options shell source file was not found: $SourcePath"
    }
}

$ProgramText = Get-Content -LiteralPath $ProgramPath -Raw
$AssetBuilderText = Get-Content -LiteralPath $AssetBuilderPath -Raw
$XmlPatcherText = Get-Content -LiteralPath $XmlPatcherPath -Raw

function Assert-SourceTokens {
    param(
        [string]$Text,
        [string[]]$Tokens,
        [string]$Context
    )

    foreach ($Token in $Tokens) {
        if ($Text.IndexOf($Token, [System.StringComparison]::Ordinal) -lt 0) {
            throw "$Context is missing required token: $Token"
        }
    }
}

function Get-CSharpMethodBody {
    param(
        [string]$Text,
        [string]$MethodSignature,
        [string]$Context
    )

    $MethodIndex = $Text.IndexOf($MethodSignature, [System.StringComparison]::Ordinal)
    if ($MethodIndex -lt 0) {
        throw "$Context is missing method signature: $MethodSignature"
    }

    $BodyStart = $Text.IndexOf('{', $MethodIndex)
    if ($BodyStart -lt 0) {
        throw "$Context method has no body: $MethodSignature"
    }

    $BraceDepth = 0
    for ($Index = $BodyStart; $Index -lt $Text.Length; $Index++) {
        if ($Text[$Index] -eq '{') {
            $BraceDepth++
        } elseif ($Text[$Index] -eq '}') {
            $BraceDepth--
            if ($BraceDepth -eq 0) {
                return $Text.Substring($BodyStart + 1, $Index - $BodyStart - 1)
            }
        }
    }

    throw "$Context method has an unterminated body: $MethodSignature"
}

$ShellForbiddenSourceTokens = @(
    'BatmanGraphicsIniBootstrapLoader',
    'GraphicsOptionsScriptTemplates',
    'GraphicsExitPrompt',
    'YesNoPrompt',
    '601',
    'PatchFrontendScripts'
)

$ShellMethodChecks = @(
    @{ Text = $ProgramText; Signature = 'private static int RunBuildMainMenuGraphicsShell(string[] args)'; Context = 'Program shell command' },
    @{ Text = $AssetBuilderText; Signature = 'public static void BuildShell(GraphicsOptionsShellBuildPaths paths)'; Context = 'AssetBuilder BuildShell' },
    @{ Text = $AssetBuilderText; Signature = 'private static void ValidateShellInputs(GraphicsOptionsShellBuildPaths paths)'; Context = 'AssetBuilder ValidateShellInputs' },
    @{ Text = $AssetBuilderText; Signature = 'private static void ValidateShellOutputPaths(GraphicsOptionsShellBuildPaths paths)'; Context = 'AssetBuilder ValidateShellOutputPaths' },
    @{ Text = $AssetBuilderText; Signature = 'private static void ValidateShellOutputDomains(string outputDirectory, string rootPath)'; Context = 'AssetBuilder ValidateShellOutputDomains' },
    @{ Text = $AssetBuilderText; Signature = 'private static void ValidateShellOutputPathComponents(string outputDirectory, string allowedRoot)'; Context = 'AssetBuilder ValidateShellOutputPathComponents' },
    @{ Text = $AssetBuilderText; Signature = 'private static void PatchFrontendShellScripts(string scriptsRoot)'; Context = 'AssetBuilder PatchFrontendShellScripts' },
    @{ Text = $AssetBuilderText; Signature = 'private static void WriteGraphicsShellRowClipActions(string scriptsRoot)'; Context = 'AssetBuilder WriteGraphicsShellRowClipActions' },
    @{ Text = $XmlPatcherText; Signature = 'public static void PatchShell(string inputXmlPath, string outputXmlPath)'; Context = 'XmlPatcher PatchShell' },
    @{ Text = $XmlPatcherText; Signature = 'private static void AppendGraphicsShellSpriteAndExport(XmlElement tags, XmlElement optionsGamePcSprite)'; Context = 'XmlPatcher AppendGraphicsShellSpriteAndExport' }
)

foreach ($ShellMethodCheck in $ShellMethodChecks) {
    $MethodBody = Get-CSharpMethodBody -Text $ShellMethodCheck.Text -MethodSignature $ShellMethodCheck.Signature -Context $ShellMethodCheck.Context
    foreach ($ForbiddenSourceToken in $ShellForbiddenSourceTokens) {
        if ($MethodBody.IndexOf($ForbiddenSourceToken, [System.StringComparison]::Ordinal) -ge 0) {
            throw "$($ShellMethodCheck.Context) references forbidden token in shell-specific method body: $ForbiddenSourceToken"
        }
    }
}

Assert-SourceTokens -Text $ProgramText -Context 'Program.cs' -Tokens @(
    '"build-main-menu-graphics-shell" => RunBuildMainMenuGraphicsShell(tail)',
    'GraphicsOptionsShellBuildPaths.FromRoot(root, ffdecPath, outputDirectory)',
    'GraphicsOptionsAssetBuilder.BuildShell(paths)'
)
Assert-SourceTokens -Text $AssetBuilderText -Context 'GraphicsOptionsAssetBuilder.cs' -Tokens @(
    'public static void BuildShell(GraphicsOptionsShellBuildPaths paths)',
    'PatchFrontendShellScripts(paths.FrontendWorkingScriptsPath)',
    'GraphicsOptionsXmlPatcher.PatchShell(paths.FrontendXmlPath, paths.FrontendPatchedXmlPath)'
)
Assert-SourceTokens -Text $XmlPatcherText -Context 'GraphicsOptionsXmlPatcher.cs' -Tokens @(
    'public static void PatchShell(string inputXmlPath, string outputXmlPath)',
    'AppendGraphicsShellSpriteAndExport(tags, optionsGamePcSprite)'
)
Assert-SourceTokens -Text $AssetBuilderText -Context 'GraphicsOptionsAssetBuilder.cs' -Tokens @(
    'GraphicsOptionsShellScriptTemplates.RowClipActions',
    'FileAttributes.ReparsePoint',
    'ValidateShellOutputDomains',
    'ValidateShellOutputPathComponents',
    'HasUnsupportedDevicePrefix'
)

$BuildShellMethodBody = Get-CSharpMethodBody -Text $AssetBuilderText -MethodSignature 'public static void BuildShell(GraphicsOptionsShellBuildPaths paths)' -Context 'AssetBuilder BuildShell order'
$ValidateShellInputsPosition = $BuildShellMethodBody.IndexOf('ValidateShellInputs(paths)', [System.StringComparison]::Ordinal)
$PrepareOutputDirectoriesPosition = $BuildShellMethodBody.IndexOf('PrepareOutputDirectories(paths.OutputDirectory, paths.TempDirectory)', [System.StringComparison]::Ordinal)
if ($ValidateShellInputsPosition -lt 0 -or $PrepareOutputDirectoriesPosition -lt 0 -or $ValidateShellInputsPosition -ge $PrepareOutputDirectoriesPosition) {
    throw 'BuildShell must validate shell inputs before preparing output directories.'
}

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
Type builderType = assembly.GetType("SubtitleSizeModBuilder.GraphicsOptionsAssetBuilder", throwOnError: true)!;
FieldInfo graphicsRowDepthsField = builderType.GetField("GraphicsRowDepths", staticFlags)
    ?? throw new MissingFieldException(builderType.FullName, "GraphicsRowDepths");
int[] graphicsRowDepths = (int[])graphicsRowDepthsField.GetValue(null)!;
Type xmlPatcherType = assembly.GetType("SubtitleSizeModBuilder.GraphicsOptionsXmlPatcher", throwOnError: true)!;
FieldInfo xmlGraphicsRowDepthsField = xmlPatcherType.GetField("GraphicsRowDepths", staticFlags)
    ?? throw new MissingFieldException(xmlPatcherType.FullName, "GraphicsRowDepths");
int[] xmlGraphicsRowDepths = (int[])xmlGraphicsRowDepthsField.GetValue(null)!;
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

Console.WriteLine(JsonSerializer.Serialize(new ReflectionContract(rowClipActions, graphicsRowDepths, xmlGraphicsRowDepths, escapedValue, nulInnerException)));

public sealed record ReflectionContract(string[] RowClipActions, int[] GraphicsRowDepths, int[] XmlGraphicsRowDepths, string EscapedValue, string? NulInnerException);
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
        $BuilderType = $Assembly.GetType('SubtitleSizeModBuilder.GraphicsOptionsAssetBuilder', $true)
        $GraphicsRowDepthsField = $BuilderType.GetField('GraphicsRowDepths', $StaticFlags)
        if ($null -eq $GraphicsRowDepthsField) {
            throw 'Graphics-options asset builder type is missing GraphicsRowDepths.'
        }

        $GraphicsRowDepths = @($GraphicsRowDepthsField.GetValue($null))
        $XmlPatcherType = $Assembly.GetType('SubtitleSizeModBuilder.GraphicsOptionsXmlPatcher', $true)
        $XmlGraphicsRowDepthsField = $XmlPatcherType.GetField('GraphicsRowDepths', $StaticFlags)
        if ($null -eq $XmlGraphicsRowDepthsField) {
            throw 'Graphics-options XML patcher type is missing GraphicsRowDepths.'
        }

        $XmlGraphicsRowDepths = @($XmlGraphicsRowDepthsField.GetValue($null))
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
            GraphicsRowDepths = $GraphicsRowDepths
            XmlGraphicsRowDepths = $XmlGraphicsRowDepths
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

$ExpectedGraphicsRowDepths = @(141, 133, 125, 117, 109, 101, 93, 85, 77, 69, 61, 53, 45, 37, 29)
$GraphicsRowDepths = @($ReflectionContract.GraphicsRowDepths)
if ($GraphicsRowDepths.Count -ne $ExpectedGraphicsRowDepths.Count) {
    throw "Expected exactly $($ExpectedGraphicsRowDepths.Count) graphics row depths, found $($GraphicsRowDepths.Count)."
}

for ($DepthIndex = 0; $DepthIndex -lt $ExpectedGraphicsRowDepths.Count; $DepthIndex++) {
    if ([int]$GraphicsRowDepths[$DepthIndex] -ne $ExpectedGraphicsRowDepths[$DepthIndex]) {
        throw "Graphics row depth $($DepthIndex + 1) was $($GraphicsRowDepths[$DepthIndex]), expected $($ExpectedGraphicsRowDepths[$DepthIndex])."
    }
}

$XmlGraphicsRowDepths = @($ReflectionContract.XmlGraphicsRowDepths)
if ($XmlGraphicsRowDepths.Count -ne $ExpectedGraphicsRowDepths.Count) {
    throw "Expected exactly $($ExpectedGraphicsRowDepths.Count) XML graphics row depths, found $($XmlGraphicsRowDepths.Count)."
}

for ($DepthIndex = 0; $DepthIndex -lt $ExpectedGraphicsRowDepths.Count; $DepthIndex++) {
    if ([int]$XmlGraphicsRowDepths[$DepthIndex] -ne $ExpectedGraphicsRowDepths[$DepthIndex]) {
        throw "XML graphics row depth $($DepthIndex + 1) was $($XmlGraphicsRowDepths[$DepthIndex]), expected $($ExpectedGraphicsRowDepths[$DepthIndex])."
    }
}

$ValidationBridgeSource = @'
using System.Reflection;
using System.Text.Json;

if (args.Length < 5)
{
    throw new ArgumentException("Expected assembly, builder root, repository root, generated output, and temp output paths.");
}

Assembly assembly = Assembly.LoadFrom(args[0]);
Type pathsType = assembly.GetType("SubtitleSizeModBuilder.GraphicsOptionsShellBuildPaths", throwOnError: true)!;
Type builderType = assembly.GetType("SubtitleSizeModBuilder.GraphicsOptionsAssetBuilder", throwOnError: true)!;
BindingFlags staticFlags = BindingFlags.Static | BindingFlags.Public | BindingFlags.NonPublic;
MethodInfo validateMethod = builderType.GetMethod("ValidateShellInputs", staticFlags)
    ?? throw new MissingMethodException(builderType.FullName, "ValidateShellInputs");

ValidationResult[] results =
[
    RunValidation("repository-root", args[1], args[2], expectValid: false),
    RunValidation("frontend-scripts", args[1], Path.Combine(args[1], "extracted", "frontend", "mainv2", "frontend-mainv2-export", "scripts"), expectValid: false),
    RunValidation("generated-child", args[1], args[3], expectValid: true),
    RunValidation("unrelated-temp", args[1], args[4], expectValid: true)
];

List<ValidationResult> allResults = results.ToList();
for (int index = 5; index < args.Length; index++)
{
    allResults.Add(RunValidation($"additional-{index - 5}", args[1], args[index], expectValid: false));
}

Console.WriteLine(JsonSerializer.Serialize(allResults));

ValidationResult RunValidation(string name, string root, string outputDirectory, bool expectValid)
{
    object shellPaths = CreatePaths(root, outputDirectory);
    try
    {
        _ = validateMethod.Invoke(null, new[] { shellPaths });
        return new ValidationResult(name, outputDirectory, expectValid, false, null);
    }
    catch (TargetInvocationException exception)
    {
        string message = exception.InnerException?.Message ?? exception.Message;
        return new ValidationResult(name, outputDirectory, !expectValid, true, message);
    }
}

object CreatePaths(string root, string outputDirectory)
{
    string frontendRoot = Path.Combine(root, "extracted", "frontend", "mainv2");
    string tempDirectory = Path.Combine(outputDirectory, "_build");

    return Activator.CreateInstance(pathsType, new object?[]
    {
        root,
        outputDirectory,
        tempDirectory,
        Path.Combine(frontendRoot, "frontend-mainv2.xml"),
        Path.Combine(frontendRoot, "frontend-mainv2.gfx"),
        Path.Combine(frontendRoot, "frontend-mainv2-export", "scripts"),
        Path.Combine(root, "extracted", "ffdec", "ffdec-cli.exe"),
        Path.Combine(tempDirectory, "frontend-scripts"),
        Path.Combine(tempDirectory, "MainV2-graphics-options-shell.xml"),
        Path.Combine(tempDirectory, "MainV2-graphics-options-shell-structural.gfx"),
        Path.Combine(outputDirectory, "MainV2-graphics-options.gfx")
    })!;
}

public sealed record ValidationResult(string Name, string OutputDirectory, bool ValidationPassed, bool Rejected, string? Error);
'@

function Get-ShellOutputValidationContract {
    param(
        [string]$AssemblyPath,
        [string]$BuilderRootPath,
        [string]$RepositoryRootPath,
        [string]$GeneratedOutputPath,
        [string]$TempOutputPath,
        [string[]]$AdditionalOutputPaths
    )

    $BridgeRoot = Join-Path ([System.IO.Path]::GetTempPath()) ("BatmanGraphicsShellValidation-" + [guid]::NewGuid().ToString('N'))
    New-Item -ItemType Directory -Path $BridgeRoot -Force | Out-Null
    try {
        $BridgeProjectPath = Join-Path $BridgeRoot 'ValidationBridge.csproj'
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
        Set-Content -LiteralPath $BridgeSourcePath -Value $ValidationBridgeSource -Encoding UTF8

        $BridgeBuildOutput = & dotnet build $BridgeProjectPath -c Debug --nologo 2>&1
        if ($LASTEXITCODE -ne 0) {
            throw "Validation bridge failed to build:`n$($BridgeBuildOutput -join [Environment]::NewLine)"
        }

        $BridgeAssemblyPath = Join-Path $BridgeRoot 'bin\Debug\net8.0\ValidationBridge.dll'
        $BridgeArguments = @($AssemblyPath, $BuilderRootPath, $RepositoryRootPath, $GeneratedOutputPath, $TempOutputPath) + @($AdditionalOutputPaths)
        $ReflectionOutput = & dotnet $BridgeAssemblyPath @BridgeArguments 2>&1
        if ($LASTEXITCODE -ne 0) {
            throw "Validation bridge failed:`n$($ReflectionOutput -join [Environment]::NewLine)"
        }

        $ReflectionJson = [string]($ReflectionOutput | Select-Object -Last 1)
        return ($ReflectionJson | ConvertFrom-Json)
    } finally {
        if (Test-Path -LiteralPath $BridgeRoot) {
            Remove-Item -LiteralPath $BridgeRoot -Recurse -Force
        }
    }
}

$BuilderRootPath = (Resolve-Path (Join-Path $BatmanRoot 'builder')).Path
$RepositoryRootPath = (Resolve-Path (Join-Path $BatmanRoot '..\..')).Path
$GeneratedOutputPath = Join-Path $BuilderRootPath 'generated\graphics-options-shell-contract-safety'
$TempOutputPath = Join-Path ([System.IO.Path]::GetTempPath()) ("BatmanGraphicsShellOutput-" + [guid]::NewGuid().ToString('N'))
$GeneratedRootPath = Join-Path $BuilderRootPath 'generated'
$TempRootPath = [System.IO.Path]::GetTempPath()
$SiblingPrefixOutputPath = Join-Path $BuilderRootPath 'generated2\graphics-options-shell-contract-safety'
$ArbitraryExternalOutputPath = Join-Path (Split-Path -Parent $BuilderRootPath) 'graphics-options-shell-contract-arbitrary'
$FrontendRootPath = Join-Path $BuilderRootPath 'extracted'
$FrontendScriptsPath = Join-Path $BuilderRootPath 'extracted\frontend\mainv2\frontend-mainv2-export\scripts'
$FrontendScriptsDescendantPath = Join-Path $FrontendScriptsPath 'unsafe-child'
$DeviceAliasOutputPaths = @(
    "\\?\$GeneratedOutputPath",
    "\\.\$GeneratedOutputPath",
    "\??\$GeneratedOutputPath"
)
$ReparseFixtureRoot = Join-Path ([System.IO.Path]::GetTempPath()) ("BatmanGraphicsShellReparse-" + [guid]::NewGuid().ToString('N'))
$ReparseFixtureTarget = Join-Path ([System.IO.Path]::GetTempPath()) ("BatmanGraphicsShellReparseTarget-" + [guid]::NewGuid().ToString('N'))
$ReparseOutputPath = $null
try {
    New-Item -ItemType Directory -Path $ReparseFixtureRoot -Force | Out-Null
    New-Item -ItemType Directory -Path $ReparseFixtureTarget -Force | Out-Null
    try {
        New-Item -ItemType SymbolicLink -Path (Join-Path $ReparseFixtureRoot 'link') -Target $ReparseFixtureTarget -ErrorAction Stop | Out-Null
        $ReparseOutputPath = Join-Path $ReparseFixtureRoot 'link\output'
    } catch {
        $ReparseOutputPath = $null
    }
    $AdditionalOutputPaths = @(
        $GeneratedRootPath,
        $TempRootPath,
        $SiblingPrefixOutputPath,
        $ArbitraryExternalOutputPath,
        $FrontendRootPath,
        $FrontendScriptsDescendantPath
    ) + $DeviceAliasOutputPaths
    if ($null -ne $ReparseOutputPath) {
        $AdditionalOutputPaths += $ReparseOutputPath
    }
    $ValidationContract = @(Get-ShellOutputValidationContract `
        -AssemblyPath $DebugAssemblyPath `
        -BuilderRootPath $BuilderRootPath `
        -RepositoryRootPath $RepositoryRootPath `
        -GeneratedOutputPath $GeneratedOutputPath `
        -TempOutputPath $TempOutputPath `
        -AdditionalOutputPaths $AdditionalOutputPaths)

$RepositoryRootValidation = $ValidationContract | Where-Object Name -eq 'repository-root'
if (-not $RepositoryRootValidation.ValidationPassed -or -not $RepositoryRootValidation.Rejected) {
    throw "Repository root output safety validation did not reject the unsafe output: $($RepositoryRootValidation.Error)"
}
Assert-ContainsOrdinal -Text ([string]$RepositoryRootValidation.Error) -Token 'Unsafe shell output directory' -Context 'Repository root output safety validation'
Assert-ContainsOrdinal -Text ([string]$RepositoryRootValidation.Error) -Token $BuilderRootPath -Context 'Repository root output safety validation'

$FrontendScriptsValidation = $ValidationContract | Where-Object Name -eq 'frontend-scripts'
if (-not $FrontendScriptsValidation.ValidationPassed -or -not $FrontendScriptsValidation.Rejected) {
    throw "Frontend scripts output safety validation did not reject the unsafe output: $($FrontendScriptsValidation.Error)"
}
Assert-ContainsOrdinal -Text ([string]$FrontendScriptsValidation.Error) -Token 'Unsafe shell output directory' -Context 'Frontend scripts output safety validation'
Assert-ContainsOrdinal -Text ([string]$FrontendScriptsValidation.Error) -Token (Join-Path $BuilderRootPath 'extracted\frontend\mainv2\frontend-mainv2-export\scripts') -Context 'Frontend scripts output safety validation'

foreach ($AcceptedValidationName in @('generated-child', 'unrelated-temp')) {
    $AcceptedValidation = $ValidationContract | Where-Object Name -eq $AcceptedValidationName
    if (-not $AcceptedValidation.ValidationPassed -or $AcceptedValidation.Rejected) {
        throw "$AcceptedValidationName output safety validation rejected a safe output: $($AcceptedValidation.Error)"
    }
}

$AdditionalValidationIndex = 0
$AdditionalValidationCases = @(
    @{ Name = 'generated-root'; Path = $GeneratedRootPath },
    @{ Name = 'temp-root'; Path = $TempRootPath },
    @{ Name = 'sibling-prefix'; Path = $SiblingPrefixOutputPath },
    @{ Name = 'arbitrary-external'; Path = $ArbitraryExternalOutputPath },
    @{ Name = 'input-ancestor'; Path = $FrontendRootPath },
    @{ Name = 'input-descendant'; Path = $FrontendScriptsDescendantPath }
)
foreach ($DeviceAliasOutputPath in $DeviceAliasOutputPaths) {
    $AdditionalValidationCases += @{ Name = "device-alias-$AdditionalValidationIndex"; Path = $DeviceAliasOutputPath }
    $AdditionalValidationIndex++
}
if ($null -ne $ReparseOutputPath) {
    $AdditionalValidationCases += @{ Name = 'reparse-point'; Path = $ReparseOutputPath }
}

foreach ($AdditionalValidationCase in $AdditionalValidationCases) {
    $AdditionalValidation = $ValidationContract | Where-Object OutputDirectory -eq $AdditionalValidationCase.Path
    if (-not $AdditionalValidation.ValidationPassed -or -not $AdditionalValidation.Rejected) {
        throw "$($AdditionalValidationCase.Name) output safety validation did not reject the unsafe output: $($AdditionalValidation.Error)"
    }
    Assert-ContainsOrdinal -Text ([string]$AdditionalValidation.Error) -Token 'Unsafe shell output directory' -Context "$($AdditionalValidationCase.Name) output safety validation"
    if ($AdditionalValidationCase.Name.StartsWith('device-alias-', [System.StringComparison]::Ordinal)) {
        Assert-ContainsOrdinal -Text ([string]$AdditionalValidation.Error) -Token 'unsupported device namespace' -Context "$($AdditionalValidationCase.Name) output safety validation"
    }
}

if ($null -eq $ReparseOutputPath) {
    Assert-SourceTokens -Text $AssetBuilderText -Context 'GraphicsOptionsAssetBuilder.cs reparse coverage' -Tokens @(
        'FileAttributes.ReparsePoint',
        'ValidateShellOutputPathComponents'
    )
}
} finally {
    if (Test-Path -LiteralPath $ReparseFixtureRoot) {
        Remove-Item -LiteralPath $ReparseFixtureRoot -Recurse -Force
    }
    if (Test-Path -LiteralPath $ReparseFixtureTarget) {
        Remove-Item -LiteralPath $ReparseFixtureTarget -Recurse -Force
    }
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
