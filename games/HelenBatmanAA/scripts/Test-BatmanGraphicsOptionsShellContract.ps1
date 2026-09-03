param(
    [string]$BatmanRoot,
    [string]$BatmanUserIniPath
)

$ErrorActionPreference = 'Stop'
$env:MSBUILDDISABLENODEREUSE = '1'

if ([string]::IsNullOrWhiteSpace($BatmanRoot)) {
    $BatmanRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
} else {
    $BatmanRoot = (Resolve-Path $BatmanRoot).Path
}
if ([string]::IsNullOrWhiteSpace($BatmanUserIniPath)) {
    $documentsPath = [Environment]::GetFolderPath([Environment+SpecialFolder]::MyDocuments)
    $BatmanUserIniPath = Join-Path $documentsPath 'Square Enix\Batman Arkham Asylum GOTY\BmGame\Config\BmEngine.ini'
} else {
    $BatmanUserIniPath = [IO.Path]::GetFullPath($BatmanUserIniPath)
}
if (-not (Test-Path -LiteralPath $BatmanUserIniPath -PathType Leaf)) { throw "Batman user INI was not found as a file: $BatmanUserIniPath" }

$TemplatePath = Join-Path $BatmanRoot 'builder\tools\NativeSubtitleExePatcher\SubtitleSizeModBuilder\GraphicsOptionsShellScriptTemplates.cs'
if (-not (Test-Path -LiteralPath $TemplatePath -PathType Leaf)) {
    throw "Batman graphics-options shell template was not found: $TemplatePath"
}

$TemplateText = Get-Content -LiteralPath $TemplatePath -Raw

$BuilderSourceRoot = Join-Path $BatmanRoot 'builder\tools\NativeSubtitleExePatcher\SubtitleSizeModBuilder'
$BuildPathsPath = Join-Path $BuilderSourceRoot 'GraphicsOptionsShellBuildPaths.cs'
$ProgramPath = Join-Path $BuilderSourceRoot 'Program.cs'
$AssetBuilderPath = Join-Path $BuilderSourceRoot 'GraphicsOptionsAssetBuilder.cs'
$XmlPatcherPath = Join-Path $BuilderSourceRoot 'GraphicsOptionsXmlPatcher.cs'
foreach ($SourcePath in @($BuildPathsPath, $ProgramPath, $AssetBuilderPath, $XmlPatcherPath)) {
    if (-not (Test-Path -LiteralPath $SourcePath -PathType Leaf)) {
        throw "Batman graphics-options shell source file was not found: $SourcePath"
    }
}

$BuildPathsText = Get-Content -LiteralPath $BuildPathsPath -Raw
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
    'GraphicsExitPrompt',
    'YesNoPrompt',
    '601',
    'PatchFrontendScripts'
)

$ShellMethodChecks = @(
    @{ Text = $BuildPathsText; Signature = 'public static GraphicsOptionsShellBuildPaths FromRoot(string root, string ffdecPath, string outputDirectory, string batmanUserIniPath)'; Context = 'Shell build paths FromRoot' },
    @{ Text = $ProgramText; Signature = 'private static int RunBuildMainMenuGraphicsShell(string[] args)'; Context = 'Program shell command' },
    @{ Text = $AssetBuilderText; Signature = 'public static void BuildShell(GraphicsOptionsShellBuildPaths paths)'; Context = 'AssetBuilder BuildShell' },
    @{ Text = $AssetBuilderText; Signature = 'private static void ValidateShellInputs(GraphicsOptionsShellBuildPaths paths)'; Context = 'AssetBuilder ValidateShellInputs' },
    @{ Text = $AssetBuilderText; Signature = 'private static void ValidateShellOutputPaths(GraphicsOptionsShellBuildPaths paths)'; Context = 'AssetBuilder ValidateShellOutputPaths' },
    @{ Text = $AssetBuilderText; Signature = 'private static void ValidateShellOutputDomains(string outputDirectory, string rootPath)'; Context = 'AssetBuilder ValidateShellOutputDomains' },
    @{ Text = $AssetBuilderText; Signature = 'private static void ValidateShellOutputPathComponents(string outputDirectory, string allowedRoot)'; Context = 'AssetBuilder ValidateShellOutputPathComponents' },
    @{ Text = $AssetBuilderText; Signature = 'private static void PatchFrontendShellScripts(string scriptsRoot, BatmanGraphicsIniBootstrapSnapshot bootstrapSnapshot)'; Context = 'AssetBuilder PatchFrontendShellScripts' },
    @{ Text = $AssetBuilderText; Signature = 'private static void WriteGraphicsShellRowClipActions(string scriptsRoot, BatmanGraphicsIniBootstrapSnapshot bootstrapSnapshot)'; Context = 'AssetBuilder WriteGraphicsShellRowClipActions' },
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
    'string batmanUserIniPath = Path.GetFullPath(options.GetValue("--ini") ?? BatmanGraphicsIniBootstrapLoader.GetDefaultIniPath())',
    'GraphicsOptionsShellBuildPaths.FromRoot(root, ffdecPath, outputDirectory, batmanUserIniPath)',
    'GraphicsOptionsAssetBuilder.BuildShell(paths)'
)
Assert-SourceTokens -Text $BuildPathsText -Context 'GraphicsOptionsShellBuildPaths.cs' -Tokens @(
    'string BatmanUserIniPath',
    'string batmanUserIniPath',
    'BatmanUserIniPath: Path.GetFullPath(batmanUserIniPath)'
)
Assert-SourceTokens -Text $AssetBuilderText -Context 'GraphicsOptionsAssetBuilder.cs' -Tokens @(
    'public static void BuildShell(GraphicsOptionsShellBuildPaths paths)',
    'BatmanGraphicsIniBootstrapSnapshot bootstrapSnapshot = BatmanGraphicsIniBootstrapLoader.Load(paths.BatmanUserIniPath)',
    'PatchFrontendShellScripts(paths.FrontendWorkingScriptsPath, bootstrapSnapshot)',
    'GraphicsOptionsXmlPatcher.PatchShell(paths.FrontendXmlPath, paths.FrontendPatchedXmlPath)'
)
Assert-SourceTokens -Text $XmlPatcherText -Context 'GraphicsOptionsXmlPatcher.cs' -Tokens @(
    'public static void PatchShell(string inputXmlPath, string outputXmlPath)',
    'AppendGraphicsShellSpriteAndExport(tags, optionsGamePcSprite)'
)
Assert-SourceTokens -Text $AssetBuilderText -Context 'GraphicsOptionsAssetBuilder.cs' -Tokens @(
    'GraphicsOptionsShellScriptTemplates.CreateScreenFrame1(bootstrapSnapshot)',
    'GraphicsOptionsShellScriptTemplates.CreateRowClipActions(bootstrapSnapshot)',
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

$BuildOutput = & dotnet build $BuilderProjectPath -c Debug --nologo --disable-build-servers -nr:false -p:UseSharedCompilation=false 2>&1
if ($LASTEXITCODE -ne 0) {
    throw "Batman graphics-options builder failed to build:`n$($BuildOutput -join [Environment]::NewLine)"
}

if (-not (Test-Path -LiteralPath $DebugAssemblyPath -PathType Leaf)) {
    throw "Batman graphics-options Debug assembly was not found after build: $DebugAssemblyPath"
}

$DuplicateIniRoot = Join-Path ([System.IO.Path]::GetTempPath()) ('BatmanGraphicsIniDuplicateSemantics-' + [guid]::NewGuid().ToString('N'))
$DuplicateIniPath = Join-Path $DuplicateIniRoot 'BmEngine.ini'
New-Item -ItemType Directory -Path $DuplicateIniRoot -Force | Out-Null
$DuplicateIniText = @'
[SystemSettings]
Fullscreen=False
Fullscreen=True
ResX=1280
ResY=720
UseVsync=False
UseVsync=True
MaxMultisamples=16
Bloom=False
DynamicShadows=False
MotionBlur=False
Distortion=False
FogVolumes=False
DisableSphericalHarmonicLights=True
AmbientOcclusion=False
Stereo=False
[SystemSettings]
Fullscreen=True
ResX=1920
ResY=1080
UseVsync=True
MaxMultisamples=8
[Engine.Engine]
PhysXLevel=1
[Engine.Engine]
PhysXLevel=2
'@
[System.IO.File]::WriteAllText($DuplicateIniPath, $DuplicateIniText)

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
    'GraphicsExitPrompt',
    'YesNoPrompt',
    'CaptureInitialState',
    'GraphicsVsyncController',
    'InitialVsync',
    'DraftVsync',
    '601',
    'Unsaved',
    'RestartRequired',
    'GetInt',
    'SetInt',
    'RunCommand',
    '_root.Prompt',
    'PromptManager'
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

function Get-ActionScriptNamedFunctionBody {
    param(
        [string]$ScriptText,
        [string]$FunctionName,
        [string]$Context
    )

    $FunctionToken = "function $FunctionName("
    $FunctionIndex = $ScriptText.IndexOf($FunctionToken, [System.StringComparison]::Ordinal)
    if ($FunctionIndex -lt 0) {
        throw "$Context is missing function: $FunctionToken"
    }

    $BodyStart = $ScriptText.IndexOf('{', $FunctionIndex)
    if ($BodyStart -lt 0) {
        throw "$Context has an unterminated function: $FunctionToken"
    }

    $BraceDepth = 0
    for ($Index = $BodyStart; $Index -lt $ScriptText.Length; $Index++) {
        if ($ScriptText[$Index] -eq '{') {
            $BraceDepth++
        } elseif ($ScriptText[$Index] -eq '}') {
            $BraceDepth--
            if ($BraceDepth -eq 0) {
                return $ScriptText.Substring($BodyStart + 1, $Index - $BodyStart - 1)
            }
        }
    }

    throw "$Context has an unterminated function: $FunctionToken"
}

function Get-ActionScriptControllerMethods {
    param(
        [string]$ScriptText,
        [string]$Context
    )

    $ClassToken = 'class rs.ui.BatmanGraphicsOptionsController'
    $ClassIndex = $ScriptText.IndexOf($ClassToken, [System.StringComparison]::Ordinal)
    if ($ClassIndex -lt 0) {
        throw "$Context is missing controller class: $ClassToken"
    }

    $ClassBodyStart = $ScriptText.IndexOf('{', $ClassIndex)
    if ($ClassBodyStart -lt 0) {
        throw "$Context controller class has no body."
    }

    $BraceDepth = 0
    $ClassBodyEnd = -1
    for ($Index = $ClassBodyStart; $Index -lt $ScriptText.Length; $Index++) {
        if ($ScriptText[$Index] -eq '{') {
            $BraceDepth++
        } elseif ($ScriptText[$Index] -eq '}') {
            $BraceDepth--
            if ($BraceDepth -eq 0) {
                $ClassBodyEnd = $Index
                break
            }
        }
    }
    if ($ClassBodyEnd -lt 0) {
        throw "$Context controller class has no closing brace."
    }

    $ClassBody = $ScriptText.Substring($ClassBodyStart + 1, $ClassBodyEnd - $ClassBodyStart - 1)
    $MethodMatches = [regex]::Matches($ClassBody, 'function\s+([A-Za-z0-9_]+)\s*\(([^)]*)\)\s*\{')
    $Methods = @()
    foreach ($MethodMatch in $MethodMatches) {
        $MethodBodyStart = $MethodMatch.Index + $MethodMatch.Length - 1
        $MethodBraceDepth = 0
        $MethodBodyEnd = -1
        for ($Index = $MethodBodyStart; $Index -lt $ClassBody.Length; $Index++) {
            if ($ClassBody[$Index] -eq '{') {
                $MethodBraceDepth++
            } elseif ($ClassBody[$Index] -eq '}') {
                $MethodBraceDepth--
                if ($MethodBraceDepth -eq 0) {
                    $MethodBodyEnd = $Index
                    break
                }
            }
        }
        if ($MethodBodyEnd -lt 0) {
            throw "$Context method has no closing brace: $($MethodMatch.Groups[1].Value)"
        }
        $Methods += [pscustomobject]@{
            Name = $MethodMatch.Groups[1].Value
            Arguments = $MethodMatch.Groups[2].Value
            Body = $ClassBody.Substring($MethodBodyStart + 1, $MethodBodyEnd - $MethodBodyStart - 1)
        }
    }
    if ($Methods.Count -eq 0) {
        throw "$Context controller class has no methods."
    }
    return $Methods
}

function Invoke-RequiredProcess {
    param(
        [string]$FilePath,
        [string[]]$Arguments,
        [string]$Context
    )

    $previousErrorActionPreference = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try {
        $output = @(& $FilePath @Arguments 2>&1)
    } finally {
        $ErrorActionPreference = $previousErrorActionPreference
    }
    if ($LASTEXITCODE -ne 0) {
        throw "$Context failed (exit code $LASTEXITCODE): $($output -join [Environment]::NewLine)"
    }
}

function Assert-RetailStartupSpritesPreserved {
    param(
        [string]$RetailXmlPath,
        [string]$BuiltGfxPath,
        [string]$FfdecPath,
        [string]$WorkingDirectory
    )

    $builtXmlPath = Join-Path $WorkingDirectory 'built-shell.xml'
    Invoke-RequiredProcess `
        -FilePath $FfdecPath `
        -Arguments @('-swf2xml', $BuiltGfxPath, $builtXmlPath) `
        -Context 'FFDec shell XML export'

    $retailDocument = New-Object System.Xml.XmlDocument
    $retailDocument.PreserveWhitespace = $true
    $retailDocument.Load($RetailXmlPath)
    $builtDocument = New-Object System.Xml.XmlDocument
    $builtDocument.PreserveWhitespace = $true
    $builtDocument.Load($builtXmlPath)

    $rootTags = @($builtDocument.SelectSingleNode('/swf/tags').ChildNodes | Where-Object { $_ -is [System.Xml.XmlElement] })
    $rootShowFrameIndex = [Array]::FindIndex(
        $rootTags,
        [Predicate[object]] { param($Node) $Node.GetAttribute('type') -eq 'ShowFrameTag' })
    $graphicsSpriteIndex = [Array]::FindIndex(
        $rootTags,
        [Predicate[object]] { param($Node) $Node.GetAttribute('type') -eq 'DefineSpriteTag' -and $Node.GetAttribute('spriteId') -eq '600' })
    $graphicsExportIndex = [Array]::FindIndex(
        $rootTags,
        [Predicate[object]] {
            param($Node)
            $Node.GetAttribute('type') -eq 'ExportAssetsTag' -and
                @($Node.SelectNodes('names/item') | Where-Object { $_.InnerText -eq 'ScreenOptionsGraphics' }).Count -eq 1
        })
    $graphicsInitIndex = [Array]::FindIndex(
        $rootTags,
        [Predicate[object]] { param($Node) $Node.GetAttribute('type') -eq 'DoInitActionTag' -and $Node.GetAttribute('spriteId') -eq '600' })
    if ($rootShowFrameIndex -lt 0 -or $graphicsSpriteIndex -lt 0 -or $graphicsExportIndex -lt 0 -or $graphicsInitIndex -lt 0) {
        throw 'Built graphics shell is missing its root ShowFrame, sprite, export, or class initialization tag.'
    }
    if ($graphicsSpriteIndex -ge $rootShowFrameIndex -or $graphicsExportIndex -ge $rootShowFrameIndex -or $graphicsInitIndex -ge $rootShowFrameIndex) {
        throw "Graphics shell sprite, export, and class initialization must precede the root ShowFrame so Scaleform constructs ScreenOptionsGraphics as rs.ui.Screen. Indices: sprite=$graphicsSpriteIndex export=$graphicsExportIndex init=$graphicsInitIndex showFrame=$rootShowFrameIndex."
    }

    foreach ($spriteId in @(3, 232)) {
        $retailSprites = @($retailDocument.SelectNodes("/swf/tags/item[@type='DefineSpriteTag' and @spriteId='$spriteId']"))
        $builtSprites = @($builtDocument.SelectNodes("/swf/tags/item[@type='DefineSpriteTag' and @spriteId='$spriteId']"))
        if ($retailSprites.Count -ne 1 -or $builtSprites.Count -ne 1) {
            throw "Retail startup sprite $spriteId must occur exactly once in both XML documents. Retail=$($retailSprites.Count), built=$($builtSprites.Count)."
        }

        $retailSpriteXml = $retailSprites[0].OuterXml
        $builtSpriteXml = $builtSprites[0].OuterXml
        if ($retailSpriteXml -cne $builtSpriteXml) {
            throw "Built graphics shell changed retail startup sprite $spriteId nodes or nested action bytes."
        }
    }
}

$BridgeSource = @'
using System.Collections;
using System.Reflection;
using System.Text.Json;

if (args.Length < 1 || args.Length > 2)
{
    throw new ArgumentException("Expected the builder assembly path and an optional duplicate-semantics INI path.");
}

Assembly assembly = Assembly.LoadFrom(args[0]);
Type templateType = assembly.GetType("SubtitleSizeModBuilder.GraphicsOptionsShellScriptTemplates", throwOnError: true)!;
BindingFlags staticFlags = BindingFlags.Static | BindingFlags.Public | BindingFlags.NonPublic;
Type snapshotType = assembly.GetType("SubtitleSizeModBuilder.BatmanGraphicsIniBootstrapSnapshot", throwOnError: true)!;
object snapshot = Activator.CreateInstance(snapshotType, new object?[] { 0, 1920, 1080, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 })!;
Type loaderType = assembly.GetType("SubtitleSizeModBuilder.BatmanGraphicsIniBootstrapLoader", throwOnError: true)!;
MethodInfo screenFrameMethod = templateType.GetMethod("CreateScreenFrame1", staticFlags)
    ?? throw new MissingMethodException(templateType.FullName, "CreateScreenFrame1");
MethodInfo rowClipActionsMethod = templateType.GetMethod("CreateRowClipActions", staticFlags)
    ?? throw new MissingMethodException(templateType.FullName, "CreateRowClipActions");
string screenFrame = (string)screenFrameMethod.Invoke(null, new[] { snapshot })!;
string[] rowClipActions = (string[])rowClipActionsMethod.Invoke(null, new[] { snapshot })!;
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

int normalizedMsaa16 = 0;
string? duplicateParsedFullscreen = null;
string? duplicateParsedUseVsync = null;
string? duplicateParsedMaxMultisamples = null;
string? duplicateParsedPhysx = null;
int duplicateSnapshotFullscreen = 0;
int duplicateSnapshotResolutionWidth = 0;
int duplicateSnapshotResolutionHeight = 0;
int duplicateSnapshotVsync = 0;
int duplicateSnapshotMsaa = 0;
int duplicateSnapshotPhysx = 0;
if (args.Length == 2)
{
    MethodInfo normalizeMsaaMethod = loaderType.GetMethod("NormalizeMsaa", bindingAttr: staticFlags)
        ?? throw new MissingMethodException(loaderType.FullName, "NormalizeMsaa");
    normalizedMsaa16 = (int)normalizeMsaaMethod.Invoke(null, new object?[] { 16 })!;
    MethodInfo parseSectionsMethod = loaderType.GetMethod("ParseSections", staticFlags)
        ?? throw new MissingMethodException(loaderType.FullName, "ParseSections");
    MethodInfo loadIniMethod = loaderType.GetMethod("Load", staticFlags)
        ?? throw new MissingMethodException(loaderType.FullName, "Load");
    IDictionary sections = (IDictionary)parseSectionsMethod.Invoke(null, new object?[] { args[1] })!;
    IDictionary systemSettings = (IDictionary)sections["SystemSettings"]!;
    IDictionary engineSettings = (IDictionary)sections["Engine.Engine"]!;
    duplicateParsedFullscreen = (string)systemSettings["Fullscreen"]!;
    duplicateParsedUseVsync = (string)systemSettings["UseVsync"]!;
    duplicateParsedMaxMultisamples = (string)systemSettings["MaxMultisamples"]!;
    duplicateParsedPhysx = (string)engineSettings["PhysXLevel"]!;
    object duplicateSnapshot = loadIniMethod.Invoke(null, new object?[] { args[1] })!;
    Type duplicateSnapshotType = duplicateSnapshot.GetType();
    duplicateSnapshotFullscreen = (int)duplicateSnapshotType.GetProperty("Fullscreen")!.GetValue(duplicateSnapshot)!;
    duplicateSnapshotResolutionWidth = (int)duplicateSnapshotType.GetProperty("ResolutionWidth")!.GetValue(duplicateSnapshot)!;
    duplicateSnapshotResolutionHeight = (int)duplicateSnapshotType.GetProperty("ResolutionHeight")!.GetValue(duplicateSnapshot)!;
    duplicateSnapshotVsync = (int)duplicateSnapshotType.GetProperty("Vsync")!.GetValue(duplicateSnapshot)!;
    duplicateSnapshotMsaa = (int)duplicateSnapshotType.GetProperty("Msaa")!.GetValue(duplicateSnapshot)!;
    duplicateSnapshotPhysx = (int)duplicateSnapshotType.GetProperty("Physx")!.GetValue(duplicateSnapshot)!;
}

Console.WriteLine(JsonSerializer.Serialize(new ReflectionContract(screenFrame, rowClipActions, graphicsRowDepths, xmlGraphicsRowDepths, escapedValue, nulInnerException, normalizedMsaa16, duplicateParsedFullscreen, duplicateParsedUseVsync, duplicateParsedMaxMultisamples, duplicateParsedPhysx, duplicateSnapshotFullscreen, duplicateSnapshotResolutionWidth, duplicateSnapshotResolutionHeight, duplicateSnapshotVsync, duplicateSnapshotMsaa, duplicateSnapshotPhysx)));

public sealed record ReflectionContract(string ScreenFrame, string[] RowClipActions, int[] GraphicsRowDepths, int[] XmlGraphicsRowDepths, string EscapedValue, string? NulInnerException, int NormalizedMsaa16, string? DuplicateParsedFullscreen, string? DuplicateParsedUseVsync, string? DuplicateParsedMaxMultisamples, string? DuplicateParsedPhysx, int DuplicateSnapshotFullscreen, int DuplicateSnapshotResolutionWidth, int DuplicateSnapshotResolutionHeight, int DuplicateSnapshotVsync, int DuplicateSnapshotMsaa, int DuplicateSnapshotPhysx);
'@

function Get-ReflectionContract {
    param(
        [string]$AssemblyPath,
        [string]$DuplicateIniPath
    )

    if ($PSVersionTable.PSEdition -eq 'Core') {
        $Assembly = [System.Reflection.Assembly]::LoadFrom($AssemblyPath)
        $TemplateType = $Assembly.GetType('SubtitleSizeModBuilder.GraphicsOptionsShellScriptTemplates', $true)
        $StaticFlags = [System.Reflection.BindingFlags]::Static -bor [System.Reflection.BindingFlags]::Public -bor [System.Reflection.BindingFlags]::NonPublic
        $SnapshotType = $Assembly.GetType('SubtitleSizeModBuilder.BatmanGraphicsIniBootstrapSnapshot', $true)
        $Snapshot = [Activator]::CreateInstance($SnapshotType, @([object]0, [object]1920, [object]1080, [object]0, [object]0, [object]0, [object]0, [object]0, [object]0, [object]0, [object]0, [object]0, [object]0, [object]0, [object]0))
        $ScreenFrameMethod = $TemplateType.GetMethod('CreateScreenFrame1', $StaticFlags)
        if ($null -eq $ScreenFrameMethod) {
            throw 'Graphics-options shell template type is missing CreateScreenFrame1.'
        }
        $RowClipActionsMethod = $TemplateType.GetMethod('CreateRowClipActions', $StaticFlags)
        if ($null -eq $RowClipActionsMethod) {
            throw 'Graphics-options shell template type is missing CreateRowClipActions.'
        }

        $EscapeMethod = $TemplateType.GetMethod('EscapeActionScriptString', $StaticFlags, $null, [System.Reflection.CallingConventions]::Any, @([string]), $null)
        if ($null -eq $EscapeMethod) {
            throw 'Graphics-options shell template type is missing EscapeActionScriptString.'
        }

        $ScreenFrame = [string]$ScreenFrameMethod.Invoke($null, @($Snapshot))
        $RowClipActions = @($RowClipActionsMethod.Invoke($null, @($Snapshot)))
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

        $LoaderType = $Assembly.GetType('SubtitleSizeModBuilder.BatmanGraphicsIniBootstrapLoader', $true)
        $NormalizeMsaaMethod = $LoaderType.GetMethod('NormalizeMsaa', $StaticFlags)
        if ($null -eq $NormalizeMsaaMethod) {
            throw 'Batman graphics INI loader is missing NormalizeMsaa.'
        }
        $NormalizedMsaa16 = [int]$NormalizeMsaaMethod.Invoke($null, @([object]16))
        $DuplicateParsedFullscreen = $null
        $DuplicateParsedUseVsync = $null
        $DuplicateParsedMaxMultisamples = $null
        $DuplicateParsedPhysx = $null
        $DuplicateSnapshotFullscreen = 0
        $DuplicateSnapshotResolutionWidth = 0
        $DuplicateSnapshotResolutionHeight = 0
        $DuplicateSnapshotVsync = 0
        $DuplicateSnapshotMsaa = 0
        $DuplicateSnapshotPhysx = 0
        if (-not [string]::IsNullOrWhiteSpace($DuplicateIniPath)) {
            $ParseSectionsMethod = $LoaderType.GetMethod('ParseSections', $StaticFlags)
            $LoadIniMethod = $LoaderType.GetMethod('Load', $StaticFlags)
            if ($null -eq $ParseSectionsMethod -or $null -eq $LoadIniMethod) {
                throw 'Batman graphics INI loader is missing duplicate-semantics coverage methods.'
            }
            $ParsedDuplicateSections = $ParseSectionsMethod.Invoke($null, @($DuplicateIniPath))
            $DuplicateParsedFullscreen = [string]$ParsedDuplicateSections['SystemSettings']['Fullscreen']
            $DuplicateParsedUseVsync = [string]$ParsedDuplicateSections['SystemSettings']['UseVsync']
            $DuplicateParsedMaxMultisamples = [string]$ParsedDuplicateSections['SystemSettings']['MaxMultisamples']
            $DuplicateParsedPhysx = [string]$ParsedDuplicateSections['Engine.Engine']['PhysXLevel']
            $DuplicateSnapshot = $LoadIniMethod.Invoke($null, @($DuplicateIniPath))
            $DuplicateSnapshotFullscreen = [int]$DuplicateSnapshot.Fullscreen
            $DuplicateSnapshotResolutionWidth = [int]$DuplicateSnapshot.ResolutionWidth
            $DuplicateSnapshotResolutionHeight = [int]$DuplicateSnapshot.ResolutionHeight
            $DuplicateSnapshotVsync = [int]$DuplicateSnapshot.Vsync
            $DuplicateSnapshotMsaa = [int]$DuplicateSnapshot.Msaa
            $DuplicateSnapshotPhysx = [int]$DuplicateSnapshot.Physx
        }

        return [pscustomobject]@{
            ScreenFrame = $ScreenFrame
            RowClipActions = $RowClipActions
            GraphicsRowDepths = $GraphicsRowDepths
            XmlGraphicsRowDepths = $XmlGraphicsRowDepths
            EscapedValue = $EscapedValue
            NulInnerException = $NulInnerException
            NormalizedMsaa16 = $NormalizedMsaa16
            DuplicateParsedFullscreen = $DuplicateParsedFullscreen
            DuplicateParsedUseVsync = $DuplicateParsedUseVsync
            DuplicateParsedMaxMultisamples = $DuplicateParsedMaxMultisamples
            DuplicateParsedPhysx = $DuplicateParsedPhysx
            DuplicateSnapshotFullscreen = $DuplicateSnapshotFullscreen
            DuplicateSnapshotResolutionWidth = $DuplicateSnapshotResolutionWidth
            DuplicateSnapshotResolutionHeight = $DuplicateSnapshotResolutionHeight
            DuplicateSnapshotVsync = $DuplicateSnapshotVsync
            DuplicateSnapshotMsaa = $DuplicateSnapshotMsaa
            DuplicateSnapshotPhysx = $DuplicateSnapshotPhysx
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

        $BridgeBuildOutput = & dotnet build $BridgeProjectPath -c Debug --nologo --disable-build-servers -nr:false -p:UseSharedCompilation=false 2>&1
        if ($LASTEXITCODE -ne 0) {
            throw "Reflection bridge failed to build:`n$($BridgeBuildOutput -join [Environment]::NewLine)"
        }

        $BridgeAssemblyPath = Join-Path $BridgeRoot 'bin\Debug\net8.0\ReflectionBridge.dll'
        $BridgeArguments = @($AssemblyPath)
        if (-not [string]::IsNullOrWhiteSpace($DuplicateIniPath)) {
            $BridgeArguments += $DuplicateIniPath
        }
        $ReflectionOutput = & dotnet $BridgeAssemblyPath @BridgeArguments 2>&1
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

try {
    $ReflectionContract = Get-ReflectionContract -AssemblyPath $DebugAssemblyPath -DuplicateIniPath $DuplicateIniPath
    if ($ReflectionContract.NormalizedMsaa16 -ne 5) {
        throw 'Batman graphics INI loader must normalize MaxMultisamples=16 to MSAA state 5.'
    }
    if ($ReflectionContract.DuplicateParsedFullscreen -cne 'False' -or $ReflectionContract.DuplicateParsedUseVsync -cne 'False' -or $ReflectionContract.DuplicateParsedMaxMultisamples -cne '16' -or $ReflectionContract.DuplicateParsedPhysx -cne '1') {
        throw 'Batman graphics INI parser did not preserve first values for duplicate keys or repeated sections.'
    }
    if ($ReflectionContract.DuplicateSnapshotFullscreen -ne 0 -or $ReflectionContract.DuplicateSnapshotResolutionWidth -ne 1280 -or $ReflectionContract.DuplicateSnapshotResolutionHeight -ne 720 -or $ReflectionContract.DuplicateSnapshotVsync -ne 0 -or $ReflectionContract.DuplicateSnapshotMsaa -ne 5 -or $ReflectionContract.DuplicateSnapshotPhysx -ne 1) {
        throw 'Batman graphics INI bootstrap disagrees with runtime first-match semantics for duplicate sections or keys.'
    }
} finally {
    if (Test-Path -LiteralPath $DuplicateIniRoot) { Remove-Item -LiteralPath $DuplicateIniRoot -Recurse -Force }
}

$ScreenFrame = [string]$ReflectionContract.ScreenFrame
foreach ($RequiredScreenToken in @(
    'class rs.ui.BatmanGraphicsOptionsController',
    'this.Settings = new Array(',
    'this.ActiveSettingsByRow = new Object();',
    'this.InitializationIndex = 0;',
    'this.InitializationDeadline = undefined;',
    'this.ApplyQueue = new Array();',
    'this.ApplyQueueIndex = 0;',
    'this.CurrentPendingOperation = "";',
    'this.CurrentPendingCode = undefined;',
    'this.CurrentPendingDeadline = undefined;',
    'this.ApplySignalToggle = 0;',
    'this.RollbackSignalToggle = 0;',
    'this.UiStatus = "";',
    'this.RollbackLocked = false;',
    'this.Screen.Tick = undefined;',
    'function BeginInitialization()',
    'function IsDeadlineReached(deadline)',
    'this.InitializationDeadline = getTimer() + 10000;',
    'flash.external.ExternalInterface.call("FE_SetControlType",this.Settings[this.InitializationIndex].ReadRequest,"");',
    'var rawValue = int(flash.external.ExternalInterface.call("FE_GetControlType"));',
    'this.Settings[this.InitializationIndex].InitialIndex = rawValue - this.Settings[this.InitializationIndex].ReadResponseBase;',
    'this.Settings[this.InitializationIndex].DraftIndex = this.Settings[this.InitializationIndex].InitialIndex;',
    'function IsDirty()',
    'function IncrementSetting(rowIndex)',
    'function DecrementSetting(rowIndex)',
    'function ToggleSetting(rowIndex)',
    'this.Screen.BlockInput(true);',
    'this.Screen.BlockInput(false);',
    'function ApplyChanges()',
    'this.CurrentPendingDeadline = getTimer() + 2000;',
    'flash.external.ExternalInterface.call("FE_SetControlType",this.CurrentPendingSetting.WriteRequestBase + this.CurrentPendingSetting.DraftIndex,"");',
    'if(rawValue == this.CurrentPendingSetting.WriteAcknowledgementBase + this.CurrentPendingSetting.DraftIndex)',
    'flash.external.ExternalInterface.call("FE_SetControlType",4990+this.ApplySignalToggle,"");',
    'if(rawValue == 4980+this.ApplySignalToggle)',
    'flash.external.ExternalInterface.call("FE_SetControlType",4970+this.RollbackSignalToggle,"");',
    'if(rawValue == 4960+this.RollbackSignalToggle)',
    'this.UiStatus = "Apply Failed";',
    'this.UiStatus = "Rollback Failed";',
    'this.RollbackLocked = true;',
    'this.CopyDraftToInitial();',
    'this.Screen.BlockInput(true);',
    'this.Tick = function()',
    'if(this.GraphicsOptionsController != undefined)',
    'this.AddItem(GraphicsRow15,13,0,-1,-1);',
    'GraphicsRow15._visible = true;',
    'this.GraphicsOptionsController.BeginInitialization();'
)) {
    Assert-ContainsOrdinal -Text $ScreenFrame -Token $RequiredScreenToken -Context 'Graphics shell screen frame'
}
if ($ScreenFrame.IndexOf('this.GraphicsOptionsController.Destroy();', [System.StringComparison]::Ordinal) -ge 0 -and
    $ScreenFrame.IndexOf('if(this.GraphicsOptionsController != undefined)', [System.StringComparison]::Ordinal) -lt 0) {
    throw 'CancelScreen must guard an undefined graphics controller.'
}

$ExpectedSettingDefinitions = @(
    '{RowIndex:3,Name:"VSync",Values:new Array("Off","On"),ConfigValues:new Array(0,1),ReadRequest:4200,ReadResponseBase:4210,WriteRequestBase:4220,WriteAcknowledgementBase:4230,FailureResponse:4299,InitialIndex:-1,DraftIndex:-1}',
    '{RowIndex:4,Name:"MSAA",Values:new Array("Off","2x","4x","8x","16x"),ConfigValues:new Array(0,1,2,3,5),ReadRequest:4300,ReadResponseBase:4310,WriteRequestBase:4320,WriteAcknowledgementBase:4330,FailureResponse:4399,InitialIndex:-1,DraftIndex:-1}',
    '{RowIndex:13,Name:"PhysX",Values:new Array("Off","Normal","High"),ConfigValues:new Array(0,1,2),ReadRequest:4400,ReadResponseBase:4410,WriteRequestBase:4420,WriteAcknowledgementBase:4430,FailureResponse:4499,InitialIndex:-1,DraftIndex:-1}',
    '{RowIndex:14,Name:"Stereo 3D",Values:new Array("Off","On"),ConfigValues:new Array(0,1),ReadRequest:4500,ReadResponseBase:4510,WriteRequestBase:4520,WriteAcknowledgementBase:4530,FailureResponse:4599,InitialIndex:-1,DraftIndex:-1}'
)
foreach ($ExpectedSettingDefinition in $ExpectedSettingDefinitions) {
    Assert-ContainsOrdinal -Text $ScreenFrame -Token $ExpectedSettingDefinition -Context 'Graphics declarative setting definition'
}
foreach ($ExpectedSettingRow in @(3, 4, 13, 14)) {
    $DefinitionCount = ([regex]::Matches($ScreenFrame, "RowIndex:$ExpectedSettingRow,")).Count
    if ($DefinitionCount -ne 1) {
        throw "Graphics setting row $ExpectedSettingRow must have exactly one declarative definition, found $DefinitionCount."
    }
}
if (([regex]::Matches($ScreenFrame, 'this.InitializationDeadline = getTimer\(\) \+ 10000;')).Count -ne 1) {
    throw 'Graphics initialization must have one overall 10-second deadline with no per-setting reset.'
}
if ($ScreenFrame.IndexOf('this.onEnterFrame = function()', [System.StringComparison]::Ordinal) -ge 0) {
    throw 'Graphics shell must preserve the inherited onEnterFrame lifecycle.'
}
if ($ScreenFrame.IndexOf('getTimer() >=', [System.StringComparison]::Ordinal) -ge 0) {
    throw 'Graphics deadlines must use the rollover-safe IsDeadlineReached method.'
}
if ($ScreenFrame.IndexOf('FE_SetControlType",4210+', [System.StringComparison]::Ordinal) -ge 0 -or
    $ScreenFrame.IndexOf('FE_SetControlType",4310+', [System.StringComparison]::Ordinal) -ge 0 -or
    $ScreenFrame.IndexOf('FE_SetControlType",4410+', [System.StringComparison]::Ordinal) -ge 0 -or
    $ScreenFrame.IndexOf('FE_SetControlType",4510+', [System.StringComparison]::Ordinal) -ge 0) {
    throw 'Graphics shell must not send read responses as write requests.'
}

$SetDraftIndexBody = Get-ActionScriptNamedFunctionBody -ScriptText $ScreenFrame -FunctionName 'SetDraftIndex' -Context 'Graphics settings edit controller'
Assert-ContainsOrdinal -Text $SetDraftIndexBody -Token 'setting.DraftIndex = index;' -Context 'Graphics settings edit controller'
Assert-ContainsOrdinal -Text $SetDraftIndexBody -Token 'UI_FrontEndSFX.UI_Forward' -Context 'Graphics settings forward sound'
Assert-ContainsOrdinal -Text $SetDraftIndexBody -Token 'UI_FrontEndSFX.UI_Back' -Context 'Graphics settings backward sound'
$DeadlineBody = Get-ActionScriptNamedFunctionBody -ScriptText $ScreenFrame -FunctionName 'IsDeadlineReached' -Context 'Graphics deadline rollover controller'
Assert-ContainsOrdinal -Text $DeadlineBody -Token 'this.NormalizeTimerValue(getTimer())' -Context 'Graphics deadline current timer normalization'
Assert-ContainsOrdinal -Text $DeadlineBody -Token 'this.NormalizeTimerValue(deadline)' -Context 'Graphics deadline target normalization'
Assert-ContainsOrdinal -Text $DeadlineBody -Token '4294967296' -Context 'Graphics AS2 timer rollover modulus'
Assert-ContainsOrdinal -Text $DeadlineBody -Token '2147483648' -Context 'Graphics AS2 timer half-range'
if ($SetDraftIndexBody.IndexOf('FE_SetControlType', [System.StringComparison]::Ordinal) -ge 0 -or
    $SetDraftIndexBody.IndexOf('FE_GetControlType', [System.StringComparison]::Ordinal) -ge 0) {
    throw 'Graphics setting edits must remain local drafts and must not use the frontend carrier.'
}
$ToggleSettingBody = Get-ActionScriptNamedFunctionBody -ScriptText $ScreenFrame -FunctionName 'ToggleSetting' -Context 'Graphics normal row action'
Assert-ContainsOrdinal -Text $ToggleSettingBody -Token 'if(nextIndex >= setting.Values.length)' -Context 'Graphics normal row action boundary'
Assert-ContainsOrdinal -Text $ToggleSettingBody -Token 'nextIndex = 0;' -Context 'Graphics normal row action wrap'
$IncrementSettingBody = Get-ActionScriptNamedFunctionBody -ScriptText $ScreenFrame -FunctionName 'IncrementSetting' -Context 'Graphics right action'
Assert-ContainsOrdinal -Text $IncrementSettingBody -Token 'setting.DraftIndex >= setting.Values.length - 1' -Context 'Graphics right action boundary'
$DecrementSettingBody = Get-ActionScriptNamedFunctionBody -ScriptText $ScreenFrame -FunctionName 'DecrementSetting' -Context 'Graphics left action'
Assert-ContainsOrdinal -Text $DecrementSettingBody -Token 'setting.DraftIndex <= 0' -Context 'Graphics left action boundary'

$ApplyChangesBody = Get-ActionScriptNamedFunctionBody -ScriptText $ScreenFrame -FunctionName 'ApplyChanges' -Context 'Graphics apply controller'
Assert-ContainsOrdinal -Text $ApplyChangesBody -Token 'this.ApplyQueue.push(this.Settings[settingIndex]);' -Context 'Graphics apply dirty queue'
Assert-ContainsOrdinal -Text $ApplyChangesBody -Token 'this.UiStatus = "Applying...";' -Context 'Graphics apply status'
$BeginNextApplyStepBody = Get-ActionScriptNamedFunctionBody -ScriptText $ScreenFrame -FunctionName 'BeginNextApplyStep' -Context 'Graphics setting write controller'
Assert-ContainsOrdinal -Text $BeginNextApplyStepBody -Token 'this.CurrentPendingDeadline = getTimer() + 2000;' -Context 'Graphics setting write deadline'
Assert-ContainsOrdinal -Text $BeginNextApplyStepBody -Token 'this.CurrentPendingSetting.WriteRequestBase + this.CurrentPendingSetting.DraftIndex' -Context 'Graphics setting write request'
$PollTransactionBody = Get-ActionScriptNamedFunctionBody -ScriptText $ScreenFrame -FunctionName 'PollTransaction' -Context 'Graphics transaction poller'
$WriteAckPosition = $PollTransactionBody.IndexOf('this.CurrentPendingSetting.WriteAcknowledgementBase + this.CurrentPendingSetting.DraftIndex', [System.StringComparison]::Ordinal)
$QueueAdvancePosition = $PollTransactionBody.IndexOf('this.ApplyQueueIndex = this.ApplyQueueIndex + 1;', [System.StringComparison]::Ordinal)
if ($WriteAckPosition -lt 0 -or $QueueAdvancePosition -lt 0 -or $WriteAckPosition -ge $QueueAdvancePosition) {
    throw 'Graphics setting queue must advance only after the exact setting acknowledgement.'
}
Assert-ContainsOrdinal -Text $PollTransactionBody -Token 'this.BeginRollback();' -Context 'Graphics setting/commit failure rollback'
Assert-ContainsOrdinal -Text $PollTransactionBody -Token 'this.FailRollback();' -Context 'Graphics rollback timeout'
$BeginCommitBody = Get-ActionScriptNamedFunctionBody -ScriptText $ScreenFrame -FunctionName 'BeginCommit' -Context 'Graphics commit controller'
Assert-ContainsOrdinal -Text $BeginCommitBody -Token 'this.CurrentPendingDeadline = getTimer() + 2000;' -Context 'Graphics commit deadline'
Assert-ContainsOrdinal -Text $BeginCommitBody -Token 'FE_SetControlType",4990+this.ApplySignalToggle,""' -Context 'Graphics commit request'
Assert-ContainsOrdinal -Text $PollTransactionBody -Token 'rawValue == 4980+this.ApplySignalToggle' -Context 'Graphics commit acknowledgement'
$CompleteCommitBody = Get-ActionScriptNamedFunctionBody -ScriptText $ScreenFrame -FunctionName 'CompleteCommit' -Context 'Graphics commit success'
Assert-ContainsOrdinal -Text $CompleteCommitBody -Token 'this.CopyDraftToInitial();' -Context 'Graphics baseline update after commit'
$BeginRollbackBody = Get-ActionScriptNamedFunctionBody -ScriptText $ScreenFrame -FunctionName 'BeginRollback' -Context 'Graphics rollback controller'
Assert-ContainsOrdinal -Text $BeginRollbackBody -Token 'this.CurrentPendingDeadline = getTimer() + 2000;' -Context 'Graphics rollback deadline'
Assert-ContainsOrdinal -Text $BeginRollbackBody -Token 'FE_SetControlType",4970+this.RollbackSignalToggle,""' -Context 'Graphics rollback request'
Assert-ContainsOrdinal -Text $PollTransactionBody -Token 'rawValue == 4960+this.RollbackSignalToggle' -Context 'Graphics rollback acknowledgement'
$CompleteRollbackBody = Get-ActionScriptNamedFunctionBody -ScriptText $ScreenFrame -FunctionName 'CompleteRollback' -Context 'Graphics rollback success'
Assert-ContainsOrdinal -Text $CompleteRollbackBody -Token 'this.UiStatus = "Apply Failed";' -Context 'Graphics rollback success status'
if ($CompleteRollbackBody.IndexOf('DraftIndex =', [System.StringComparison]::Ordinal) -ge 0 -or
    $CompleteRollbackBody.IndexOf('InitialIndex =', [System.StringComparison]::Ordinal) -ge 0) {
    throw 'Successful graphics rollback must preserve draft and initial indices.'
}
$FailRollbackBody = Get-ActionScriptNamedFunctionBody -ScriptText $ScreenFrame -FunctionName 'FailRollback' -Context 'Graphics rollback failure'
Assert-ContainsOrdinal -Text $FailRollbackBody -Token 'this.UiStatus = "Rollback Failed";' -Context 'Graphics rollback failure status'
Assert-ContainsOrdinal -Text $FailRollbackBody -Token 'this.RollbackLocked = true;' -Context 'Graphics rollback failure lock'
if ($FailRollbackBody.IndexOf('DraftIndex =', [System.StringComparison]::Ordinal) -ge 0 -or
    $FailRollbackBody.IndexOf('InitialIndex =', [System.StringComparison]::Ordinal) -ge 0) {
    throw 'Failed graphics rollback must preserve draft and initial indices.'
}
foreach ($ForbiddenScreenToken in @('Helen_', 'GraphicsExitPrompt', 'YesNoPrompt', 'CaptureInitialState', 'loadBatmanGraphicsDraftIntoConfig', 'applyBatmanGraphicsDraft', 'ApplyWasDispatched')) {
    if ($ScreenFrame.IndexOf($ForbiddenScreenToken, [System.StringComparison]::Ordinal) -ge 0) {
        throw "Graphics shell screen frame contains forbidden token: $ForbiddenScreenToken"
    }
}
foreach ($ForbiddenBridgeCall in @('Helen_GetInt', 'Helen_SetInt', 'Helen_RunCommand', 'Helen_Log', 'GetInt', 'SetInt', 'RunCommand', 'ExternalInterface.call("Helen_', '_root.Prompt', 'GraphicsExitPrompt', 'YesNoPrompt', 'Unsaved', 'RestartRequired')) {
    if ($ScreenFrame.IndexOf($ForbiddenBridgeCall, [System.StringComparison]::Ordinal) -ge 0) {
        throw "Graphics shell screen frame contains forbidden bridge/prompt call: $ForbiddenBridgeCall"
    }
}
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

if (args.Length < 6)
{
    throw new ArgumentException("Expected assembly, builder root, repository root, generated output, temp output, and Batman user INI paths.");
}

Assembly assembly = Assembly.LoadFrom(args[0]);
Type pathsType = assembly.GetType("SubtitleSizeModBuilder.GraphicsOptionsShellBuildPaths", throwOnError: true)!;
Type builderType = assembly.GetType("SubtitleSizeModBuilder.GraphicsOptionsAssetBuilder", throwOnError: true)!;
BindingFlags staticFlags = BindingFlags.Static | BindingFlags.Public | BindingFlags.NonPublic;
MethodInfo validateMethod = builderType.GetMethod("ValidateShellInputs", staticFlags)
    ?? throw new MissingMethodException(builderType.FullName, "ValidateShellInputs");

ValidationResult[] results =
[
    RunValidation("repository-root", args[1], args[2], args[5], expectValid: false),
    RunValidation("frontend-scripts", args[1], Path.Combine(args[1], "extracted", "frontend", "mainv2", "frontend-mainv2-export", "scripts"), args[5], expectValid: false),
    RunValidation("generated-child", args[1], args[3], args[5], expectValid: true),
    RunValidation("unrelated-temp", args[1], args[4], args[5], expectValid: true)
];

List<ValidationResult> allResults = results.ToList();
for (int index = 6; index < args.Length; index++)
{
    allResults.Add(RunValidation($"additional-{index - 6}", args[1], args[index], args[5], expectValid: false));
}

Console.WriteLine(JsonSerializer.Serialize(allResults));

ValidationResult RunValidation(string name, string root, string outputDirectory, string iniPath, bool expectValid)
{
    object shellPaths = CreatePaths(root, outputDirectory, iniPath);
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

object CreatePaths(string root, string outputDirectory, string iniPath)
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
        iniPath,
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
        [string]$BatmanUserIniPath,
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

        $BridgeBuildOutput = & dotnet build $BridgeProjectPath -c Debug --nologo --disable-build-servers -nr:false -p:UseSharedCompilation=false 2>&1
        if ($LASTEXITCODE -ne 0) {
            throw "Validation bridge failed to build:`n$($BridgeBuildOutput -join [Environment]::NewLine)"
        }

        $BridgeAssemblyPath = Join-Path $BridgeRoot 'bin\Debug\net8.0\ValidationBridge.dll'
        $BridgeArguments = @($AssemblyPath, $BuilderRootPath, $RepositoryRootPath, $GeneratedOutputPath, $TempOutputPath, $BatmanUserIniPath) + @($AdditionalOutputPaths)
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
        -BatmanUserIniPath $BatmanUserIniPath `
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

$PreservationOutputPath = Join-Path ([System.IO.Path]::GetTempPath()) ("BatmanGraphicsShellPreservation-" + [guid]::NewGuid().ToString('N'))
$PreservationOutputGfxPath = Join-Path $PreservationOutputPath 'MainV2-graphics-options.gfx'
try {
    $FfdecPath = Join-Path $BuilderRootPath 'extracted\ffdec\ffdec-cli.exe'
    Invoke-RequiredProcess `
        -FilePath 'dotnet' `
        -Arguments @(
            'run',
            '--no-build',
            '--project',
            $BuilderProjectPath,
            '-c',
            'Debug',
            '--no-restore',
            '--',
            'build-main-menu-graphics-shell',
            '--root',
            $BuilderRootPath,
            '--output-dir',
            $PreservationOutputPath,
            '--ffdec',
            $FfdecPath,
            '--ini',
            $BatmanUserIniPath) `
        -Context 'Graphics shell preservation build'

    if (-not (Test-Path -LiteralPath $PreservationOutputGfxPath -PathType Leaf)) {
        throw "Graphics shell preservation build did not emit $PreservationOutputGfxPath."
    }

    Assert-RetailStartupSpritesPreserved `
        -RetailXmlPath (Join-Path $BuilderRootPath 'extracted\frontend\mainv2\frontend-mainv2.xml') `
        -BuiltGfxPath $PreservationOutputGfxPath `
        -FfdecPath $FfdecPath `
        -WorkingDirectory $PreservationOutputPath
} finally {
    if (Test-Path -LiteralPath $PreservationOutputPath) {
        Remove-Item -LiteralPath $PreservationOutputPath -Recurse -Force
    }
}

$ExpectedRows = @(
    @{ Label = 'Fullscreen'; Value = 'Not active' },
    @{ Label = 'Resolution'; Value = 'Not active' },
    @{ Label = 'VSync'; Value = $null },
    @{ Label = 'MSAA'; Value = $null },
    @{ Label = 'Detail Level'; Value = 'Not active' },
    @{ Label = 'Bloom'; Value = 'Not active' },
    @{ Label = 'Dynamic Shadows'; Value = 'Not active' },
    @{ Label = 'Motion Blur'; Value = 'Not active' },
    @{ Label = 'Distortion'; Value = 'Not active' },
    @{ Label = 'Fog Volumes'; Value = 'Not active' },
    @{ Label = 'Spherical Harmonic Lighting'; Value = 'Not active' },
    @{ Label = 'Ambient Occlusion'; Value = 'Not active' },
    @{ Label = 'PhysX'; Value = $null },
    @{ Label = 'Stereo 3D'; Value = $null }
)

for ($RowIndex = 0; $RowIndex -lt $ExpectedRows.Count; $RowIndex++) {
    $RowScript = [string]$RowClipActions[$RowIndex]
    $ExpectedLabel = $ExpectedRows[$RowIndex].Label
    $ExpectedValue = $ExpectedRows[$RowIndex].Value
    $RowContext = "Graphics row action $($RowIndex + 1)"

    Assert-ContainsOrdinal -Text $RowScript -Token "this.LabelName = `"$ExpectedLabel`";" -Context "$RowContext stable label state"
    Assert-ContainsOrdinal -Text $RowScript -Token "this.Label.Label.Text.text = `"$ExpectedLabel`";" -Context $RowContext
    Assert-ContainsOrdinal -Text $RowScript -Token 'this._visible = true;' -Context $RowContext
    Assert-NoOpActionScriptFunction -ScriptText $RowScript -FunctionName 'ShowPrompt' -Context $RowContext

    if (@(3, 4, 13, 14) -contains ($RowIndex + 1)) {
        $ActiveRowDefinitions = @{
            3 = @{ Values = 'this.Names = new Array("Off","On");'; RowIndex = 3 }
            4 = @{ Values = 'this.Names = new Array("Off","2x","4x","8x","16x");'; RowIndex = 4 }
            13 = @{ Values = 'this.Names = new Array("Off","Normal","High");'; RowIndex = 13 }
            14 = @{ Values = 'this.Names = new Array("Off","On");'; RowIndex = 14 }
        }
        $ActiveRowDefinition = $ActiveRowDefinitions[$RowIndex + 1]
        Assert-ContainsOrdinal -Text $RowScript -Token 'this.State = -1;' -Context $RowContext
        Assert-ContainsOrdinal -Text $RowScript -Token 'this.Initial = -1;' -Context $RowContext
        Assert-ContainsOrdinal -Text $RowScript -Token 'this.Default = -1;' -Context $RowContext
        Assert-ContainsOrdinal -Text $RowScript -Token $ActiveRowDefinition.Values -Context $RowContext
        Assert-ContainsOrdinal -Text $RowScript -Token "this.RowIndex = $($ActiveRowDefinition.RowIndex);" -Context "$RowContext row binding"
        Assert-ContainsOrdinal -Text $RowScript -Token 'if(_parent.GraphicsOptionsController == undefined)' -Context "$RowContext controller-load guard"
        Assert-ContainsOrdinal -Text $RowScript -Token 'this.ItemText.text = "Loading...";' -Context "$RowContext controller-load guard"
        Assert-ContainsOrdinal -Text $RowScript -Token 'return undefined;' -Context "$RowContext controller-load guard"
        Assert-ContainsOrdinal -Text $RowScript -Token 'this.ItemText.text = this.Names[this.State];' -Context $RowContext
        Assert-ContainsOrdinal -Text $RowScript -Token 'this.State = _parent.GraphicsOptionsController.GetDraftIndex(this.RowIndex);' -Context $RowContext
        Assert-ContainsOrdinal -Text $RowScript -Token 'this.Initial = _parent.GraphicsOptionsController.GetInitialIndex(this.RowIndex);' -Context $RowContext
        Assert-ContainsOrdinal -Text $RowScript -Token 'this.LeftClicker._visible = this.State > 0 && _parent.GraphicsOptionsController.CanEdit(this.RowIndex);' -Context $RowContext
        Assert-ContainsOrdinal -Text $RowScript -Token 'this.RightClicker._visible = this.State < this.Names.length - 1 && _parent.GraphicsOptionsController.CanEdit(this.RowIndex);' -Context $RowContext
        Assert-ContainsOrdinal -Text $RowScript -Token '_parent.GraphicsOptionsController.ToggleSetting(this.RowIndex);' -Context $RowContext
        Assert-ContainsOrdinal -Text $RowScript -Token '_parent.GraphicsOptionsController.IncrementSetting(this.RowIndex);' -Context $RowContext
        Assert-ContainsOrdinal -Text $RowScript -Token '_parent.GraphicsOptionsController.DecrementSetting(this.RowIndex);' -Context $RowContext
        Assert-ContainsOrdinal -Text $RowScript -Token '"Unavailable" : "Loading..."' -Context "$RowContext unresolved state"
        foreach ($ActiveFunctionName in @('RunAction', 'Increment', 'Decrement')) {
            $ActiveFunctionBody = Get-ActionScriptFunctionBody -ScriptText $RowScript -FunctionName $ActiveFunctionName -Context "$RowContext $ActiveFunctionName"
            Assert-ContainsOrdinal -Text $ActiveFunctionBody -Token 'if(_parent.GraphicsOptionsController != undefined)' -Context "$RowContext $ActiveFunctionName controller guard"
        }
    } else {
        Assert-ContainsOrdinal -Text $RowScript -Token 'this.State = 0;' -Context $RowContext
        Assert-ContainsOrdinal -Text $RowScript -Token 'this.Initial = 0;' -Context $RowContext
        Assert-ContainsOrdinal -Text $RowScript -Token 'this.Default = 0;' -Context $RowContext
        Assert-ContainsOrdinal -Text $RowScript -Token "this.ItemText.text = `"$ExpectedValue`";" -Context $RowContext
        Assert-ContainsOrdinal -Text $RowScript -Token 'this.Names = new Array("Not active");' -Context $RowContext
        Assert-ContainsOrdinal -Text $RowScript -Token 'if(this.ItemText != undefined)' -Context "$RowContext ItemText guard"
        Assert-ContainsOrdinal -Text $RowScript -Token 'this.LeftClicker._visible = false;' -Context $RowContext
        Assert-ContainsOrdinal -Text $RowScript -Token 'this.RightClicker._visible = false;' -Context $RowContext
        Assert-NoOpActionScriptFunction -ScriptText $RowScript -FunctionName 'RunAction' -Context $RowContext
        Assert-NoOpActionScriptFunction -ScriptText $RowScript -FunctionName 'Increment' -Context $RowContext
        Assert-NoOpActionScriptFunction -ScriptText $RowScript -FunctionName 'Decrement' -Context $RowContext
    }

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
    if ($RowScript.IndexOf('FE_SetControlType', [System.StringComparison]::Ordinal) -ge 0 -or
        $RowScript.IndexOf('FE_GetControlType', [System.StringComparison]::Ordinal) -ge 0 -or
        $RowScript.IndexOf('onEnterFrame', [System.StringComparison]::Ordinal) -ge 0) {
        throw "$RowContext contains a forbidden bridge call or lifecycle replacement."
    }
}

$ApplyRowScript = [string]$RowClipActions[14]
Assert-ContainsOrdinal -Text $ApplyRowScript -Token 'this.LabelName = "Apply Changes";' -Context 'Graphics row action 15 stable label state'
Assert-ContainsOrdinal -Text $ApplyRowScript -Token 'this._visible = true;' -Context 'Graphics row action 15'
Assert-ContainsOrdinal -Text $ApplyRowScript -Token 'this.ItemText.text = _parent.GraphicsOptionsController.GetApplyStatusText();' -Context 'Graphics row action 15 status'
Assert-ContainsOrdinal -Text $ApplyRowScript -Token '_parent.GraphicsOptionsController.ApplyChanges();' -Context 'Graphics row action 15'
Assert-ContainsOrdinal -Text $ApplyRowScript -Token '_parent.GraphicsOptionsController.CanApply() ? 100 : 40' -Context 'Graphics row action 15 enabled state'
Assert-ContainsOrdinal -Text $ApplyRowScript -Token 'if(_parent.GraphicsOptionsController == undefined)' -Context 'Graphics row action 15 controller-load guard'
Assert-ContainsOrdinal -Text $ApplyRowScript -Token 'this.ItemText.text = "";' -Context 'Graphics row action 15 controller-load guard'
$ApplyRunActionBody = Get-ActionScriptFunctionBody -ScriptText $ApplyRowScript -FunctionName 'RunAction' -Context 'Graphics row action 15 RunAction'
Assert-ContainsOrdinal -Text $ApplyRunActionBody -Token 'if(_parent.GraphicsOptionsController != undefined)' -Context 'Graphics row action 15 RunAction controller guard'
Assert-NoOpActionScriptFunction -ScriptText $ApplyRowScript -FunctionName 'Increment' -Context 'Graphics row action 15'
Assert-NoOpActionScriptFunction -ScriptText $ApplyRowScript -FunctionName 'Decrement' -Context 'Graphics row action 15'
Assert-NoOpActionScriptFunction -ScriptText $ApplyRowScript -FunctionName 'ShowPrompt' -Context 'Graphics row action 15'
$ApplyDestroyBody = Get-ActionScriptFunctionBody -ScriptText $ApplyRowScript -FunctionName 'Destroy' -Context 'Graphics row action 15'
Assert-ContainsOrdinal -Text $ApplyDestroyBody -Token 'this.Names' -Context 'Graphics row action 15 Destroy'
Assert-ContainsOrdinal -Text $ApplyDestroyBody -Token '.pop()' -Context 'Graphics row action 15 Destroy'
if ($ApplyDestroyBody.IndexOf('ExternalInterface', [System.StringComparison]::Ordinal) -ge 0) {
    throw 'Graphics row action 15 Destroy must not call an external interface.'
}
foreach ($ForbiddenToken in $ForbiddenTokens) {
    if ($ApplyRowScript.IndexOf($ForbiddenToken, [System.StringComparison]::Ordinal) -ge 0) {
        throw "Graphics row action 15 contains forbidden token: $ForbiddenToken"
    }
}
if ($ApplyRowScript.IndexOf('FE_SetControlType', [System.StringComparison]::Ordinal) -ge 0 -or
    $ApplyRowScript.IndexOf('FE_GetControlType', [System.StringComparison]::Ordinal) -ge 0 -or
    $ApplyRowScript.IndexOf('onEnterFrame', [System.StringComparison]::Ordinal) -ge 0) {
    throw 'Graphics row action 15 contains a forbidden bridge call or lifecycle replacement.'
}

$NodeCommand = Get-Command node -ErrorAction SilentlyContinue
if ($null -eq $NodeCommand) {
    throw 'Graphics shell state-machine contract requires the installed node executable.'
}
$ControllerMethods = @(Get-ActionScriptControllerMethods -ScriptText $ScreenFrame -Context 'Graphics shell state-machine harness')
$ControllerSourceParts = @()
foreach ($ControllerMethod in $ControllerMethods) {
    if ($ControllerMethod.Name -eq 'BatmanGraphicsOptionsController') {
        $ControllerSourceParts += "function BatmanGraphicsOptionsController($($ControllerMethod.Arguments)){`n$($ControllerMethod.Body)`n}"
    } else {
        $ControllerSourceParts += "BatmanGraphicsOptionsController.prototype.$($ControllerMethod.Name) = function($($ControllerMethod.Arguments)){`n$($ControllerMethod.Body)`n};"
    }
}
$ControllerSource = $ControllerSourceParts -join "`n"
$RowScriptsJson = ConvertTo-Json -InputObject $RowClipActions -Compress
$CancelBodyJson = ConvertTo-Json -InputObject (Get-ActionScriptNamedFunctionBody -ScriptText $ScreenFrame -FunctionName 'CancelScreen' -Context 'Graphics shell lifecycle harness') -Compress
$TickBodyJson = ConvertTo-Json -InputObject (Get-ActionScriptFunctionBody -ScriptText $ScreenFrame -FunctionName 'Tick' -Context 'Graphics shell lifecycle harness') -Compress
$HarnessSource = @'
'use strict';
const assert = require('assert');

__CONTROLLER_SOURCE__
const rowScripts = __ROW_SCRIPTS__;
const cancelBody = __CANCEL_BODY__;
const tickBody = __TICK_BODY__;
let now = 0;
let carrierResponses = [];
let calls = [];

global.getTimer = () => now;
global.int = value => Number(value) || 0;
global.flash = { external: { ExternalInterface: { call: (name, ...args) => {
    calls.push({ name, args });
    if (name === 'FE_GetControlType') {
        return carrierResponses.length > 0 ? carrierResponses.shift() : 0;
    }
    return 0;
} } } };

function setNow(value) {
    now = value;
}

function queueResponses(...values) {
    carrierResponses.push(...values);
}

function clearCalls() {
    calls = [];
}

function settingSignals() {
    return calls.filter(call => call.name === 'FE_SetControlType').map(call => call.args[0]);
}

function makeScreen() {
    const screen = {
        blockStates: [],
        rowUpdates: 0,
        reUpdates: 0,
        BlockInput(value) { this.blockStates.push(value); },
        ReUpdate() { this.reUpdates += 1; }
    };
    for (let rowIndex = 1; rowIndex <= 15; rowIndex += 1) {
        screen['GraphicsRow' + rowIndex] = { Update() { screen.rowUpdates += 1; } };
    }
    return screen;
}

function makeEnvironment() {
    const screen = makeScreen();
    const controller = new BatmanGraphicsOptionsController(screen);
    screen.GraphicsOptionsController = controller;
    return { screen, controller };
}

function initialize(controller, values) {
    controller.BeginInitialization();
    assert.deepStrictEqual(settingSignals(), [4200]);
    const responses = [4210 + values[0], 4310 + values[1], 4410 + values[2], 4510 + values[3]];
    for (const response of responses) {
        queueResponses(response);
        controller.Tick();
    }
    assert.strictEqual(controller.InitializationComplete, true);
    assert.strictEqual(controller.CanApply(), false);
}

function loadRow(script, parent) {
    const bodyStart = script.indexOf('{');
    const bodyEnd = script.lastIndexOf('}');
    const row = {
        _parent: parent,
        Label: { Label: { Text: { text: '' } } },
        ItemText: { text: '', _alpha: 0 },
        LeftClicker: { _visible: true },
        RightClicker: { _visible: true }
    };
    global._parent = parent;
    const load = new Function(script.slice(bodyStart + 1, bodyEnd));
    load.call(row);
    return row;
}

function expectNoThrow(action, message) {
    assert.doesNotThrow(action, message);
}

setNow(0);
clearCalls();
let environment = makeEnvironment();
let controller = environment.controller;
controller.BeginInitialization();
assert.deepStrictEqual(settingSignals(), [4200]);
queueResponses(9999);
controller.Tick();
assert.deepStrictEqual(settingSignals(), [4200]);
queueResponses(4211);
controller.Tick();
assert.deepStrictEqual(settingSignals(), [4200, 4300]);
queueResponses(4314);
controller.Tick();
assert.deepStrictEqual(settingSignals(), [4200, 4300, 4400]);
queueResponses(4412);
controller.Tick();
assert.deepStrictEqual(settingSignals(), [4200, 4300, 4400, 4500]);
queueResponses(4511);
controller.Tick();
assert.deepStrictEqual(controller.Settings.map(setting => setting.InitialIndex), [1, 4, 2, 1]);
assert.deepStrictEqual(controller.Settings.map(setting => setting.DraftIndex), [1, 4, 2, 1]);
assert.strictEqual(controller.InitializationDeadline, 10000);
assert.strictEqual(controller.CanApply(), false);

setNow(0);
environment = makeEnvironment();
controller = environment.controller;
controller.BeginInitialization();
queueResponses(4299);
controller.Tick();
assert.strictEqual(controller.InitializationFailed, true);
assert.deepStrictEqual(controller.Settings.map(setting => setting.InitialIndex), [-1, -1, -1, -1]);
assert.deepStrictEqual(controller.Settings.map(setting => setting.DraftIndex), [-1, -1, -1, -1]);
assert.strictEqual(controller.CanApply(), false);
assert.strictEqual(controller.CanEdit(3), false);

setNow(5000);
assert.strictEqual(controller.IsDeadlineReached(10000), false);
setNow(10000);
assert.strictEqual(controller.IsDeadlineReached(10000), true);
setNow(2147483000);
assert.strictEqual(controller.IsDeadlineReached(2147493000), false);
setNow(-2147474296);
assert.strictEqual(controller.IsDeadlineReached(2147493000), true);
setNow(2147483000);
environment = makeEnvironment();
controller = environment.controller;
controller.BeginInitialization();
assert.strictEqual(controller.InitializationDeadline, 2147493000);
setNow(-2147474296);
controller.Tick();
assert.strictEqual(controller.InitializationFailed, true);
assert.deepStrictEqual(controller.Settings.map(setting => setting.DraftIndex), [-1, -1, -1, -1]);

setNow(0);
clearCalls();
environment = makeEnvironment();
controller = environment.controller;
initialize(controller, [0, 0, 0, 0]);
controller.IncrementSetting(3);
assert.strictEqual(controller.Settings[0].DraftIndex, 1);
assert.strictEqual(settingSignals().filter(value => value >= 4200 && value < 4600).length, 4);
controller.DecrementSetting(3);
controller.DecrementSetting(3);
assert.strictEqual(controller.Settings[0].DraftIndex, 0);
for (let index = 0; index < 5; index += 1) {
    controller.ToggleSetting(4);
}
assert.strictEqual(controller.Settings[1].DraftIndex, 0);
controller.ToggleSetting(4);
controller.IncrementSetting(4);
assert.strictEqual(controller.Settings[1].DraftIndex, 2);
assert.strictEqual(controller.CanApply(), true);

setNow(0);
clearCalls();
environment = makeEnvironment();
controller = environment.controller;
initialize(controller, [0, 0, 0, 0]);
controller.ToggleSetting(14);
controller.IncrementSetting(4);
controller.IncrementSetting(4);
controller.ToggleSetting(3);
controller.ApplyChanges();
assert.deepStrictEqual(settingSignals().slice(4), [4221]);
queueResponses(4230);
controller.Tick();
assert.deepStrictEqual(settingSignals().slice(4), [4221]);
queueResponses(4231);
controller.Tick();
assert.deepStrictEqual(settingSignals().slice(4), [4221, 4322]);
assert.deepStrictEqual(controller.Settings.map(setting => setting.InitialIndex), [0, 0, 0, 0]);
queueResponses(4330);
controller.Tick();
assert.deepStrictEqual(settingSignals().slice(4), [4221, 4322]);
queueResponses(4332);
controller.Tick();
assert.deepStrictEqual(settingSignals().slice(4), [4221, 4322, 4521]);
queueResponses(4530);
controller.Tick();
assert.deepStrictEqual(settingSignals().slice(4), [4221, 4322, 4521]);
queueResponses(4531);
controller.Tick();
const commitSignal = settingSignals()[settingSignals().length - 1];
assert.ok(commitSignal === 4990 || commitSignal === 4991);
assert.deepStrictEqual(controller.Settings.map(setting => setting.InitialIndex), [0, 0, 0, 0]);
queueResponses(commitSignal === 4990 ? 4980 : 4981);
controller.Tick();
assert.deepStrictEqual(controller.Settings.map(setting => setting.InitialIndex), [1, 2, 0, 1]);
assert.strictEqual(controller.GetApplyStatusText(), '');
assert.strictEqual(controller.CanApply(), false);

function applyAndReachSettingFailure(failureResponse) {
    setNow(0);
    clearCalls();
    const failedEnvironment = makeEnvironment();
    const failedController = failedEnvironment.controller;
    initialize(failedController, [0, 0, 0, 0]);
    failedController.ToggleSetting(3);
    failedController.ApplyChanges();
    queueResponses(failureResponse);
    failedController.Tick();
    const rollbackSignal = settingSignals()[settingSignals().length - 1];
    assert.ok(rollbackSignal === 4970 || rollbackSignal === 4971);
    return { controller: failedController, rollbackSignal };
}

let failureEnvironment = applyAndReachSettingFailure(4299);
queueResponses(failureEnvironment.rollbackSignal === 4970 ? 4960 : 4961);
failureEnvironment.controller.Tick();
assert.strictEqual(failureEnvironment.controller.GetApplyStatusText(), 'Apply Failed');
assert.strictEqual(failureEnvironment.controller.Settings[0].DraftIndex, 1);
assert.strictEqual(failureEnvironment.controller.Settings[0].InitialIndex, 0);
assert.strictEqual(failureEnvironment.controller.CanApply(), true);

failureEnvironment = applyAndReachSettingFailure(4299);
setNow(failureEnvironment.controller.CurrentPendingDeadline);
failureEnvironment.controller.Tick();
assert.strictEqual(settingSignals()[settingSignals().length - 1] === 4970 || settingSignals()[settingSignals().length - 1] === 4971, true);

setNow(0);
clearCalls();
environment = makeEnvironment();
controller = environment.controller;
initialize(controller, [0, 0, 0, 0]);
controller.ToggleSetting(3);
controller.ApplyChanges();
queueResponses(4231);
controller.Tick();
const commitFailureSignal = settingSignals()[settingSignals().length - 1];
queueResponses(4989);
controller.Tick();
const commitRollbackSignal = settingSignals()[settingSignals().length - 1];
assert.ok(commitRollbackSignal === 4970 || commitRollbackSignal === 4971);
queueResponses(commitRollbackSignal === 4970 ? 4960 : 4961);
controller.Tick();
assert.strictEqual(controller.GetApplyStatusText(), 'Apply Failed');
assert.strictEqual(controller.Settings[0].DraftIndex, 1);
assert.strictEqual(controller.Settings[0].InitialIndex, 0);

setNow(0);
clearCalls();
environment = makeEnvironment();
controller = environment.controller;
initialize(controller, [0, 0, 0, 0]);
controller.ToggleSetting(3);
controller.ApplyChanges();
queueResponses(4231);
controller.Tick();
setNow(controller.CurrentPendingDeadline);
controller.Tick();
const timeoutRollbackSignal = settingSignals()[settingSignals().length - 1];
assert.ok(timeoutRollbackSignal === 4970 || timeoutRollbackSignal === 4971);
queueResponses(timeoutRollbackSignal === 4970 ? 4960 : 4961);
controller.Tick();
assert.strictEqual(controller.GetApplyStatusText(), 'Apply Failed');

failureEnvironment = applyAndReachSettingFailure(4299);
queueResponses(4969);
failureEnvironment.controller.Tick();
assert.strictEqual(failureEnvironment.controller.GetApplyStatusText(), 'Rollback Failed');
assert.strictEqual(failureEnvironment.controller.RollbackLocked, true);
assert.strictEqual(failureEnvironment.controller.CanApply(), false);
assert.strictEqual(failureEnvironment.controller.CanEdit(3), false);

failureEnvironment = applyAndReachSettingFailure(4299);
setNow(failureEnvironment.controller.CurrentPendingDeadline);
failureEnvironment.controller.Tick();
assert.strictEqual(failureEnvironment.controller.GetApplyStatusText(), 'Rollback Failed');
assert.strictEqual(failureEnvironment.controller.RollbackLocked, true);

const lifecycleParent = { GraphicsOptionsController: undefined };
const guardedActiveRow = loadRow(rowScripts[2], lifecycleParent);
expectNoThrow(() => guardedActiveRow.Update(), 'active row update before controller assignment');
expectNoThrow(() => guardedActiveRow.RunAction(), 'active row action before controller assignment');
expectNoThrow(() => guardedActiveRow.Increment(), 'active row increment before controller assignment');
expectNoThrow(() => guardedActiveRow.Decrement(), 'active row decrement before controller assignment');
assert.strictEqual(guardedActiveRow.ItemText.text, 'Loading...');
assert.strictEqual(guardedActiveRow.LeftClicker._visible, false);
assert.strictEqual(guardedActiveRow.RightClicker._visible, false);
const guardedApplyRow = loadRow(rowScripts[14], lifecycleParent);
expectNoThrow(() => guardedApplyRow.Update(), 'apply row update before controller assignment');
expectNoThrow(() => guardedApplyRow.RunAction(), 'apply row action before controller assignment');
assert.strictEqual(guardedApplyRow.ItemText.text, '');
assert.strictEqual(guardedApplyRow.ItemText._alpha, 40);
assert.strictEqual(guardedApplyRow.Label._alpha, 40);
const sparseActiveRow = { _parent: lifecycleParent };
expectNoThrow(() => new Function(rowScripts[2].slice(rowScripts[2].indexOf('{') + 1, rowScripts[2].lastIndexOf('}'))).call(sparseActiveRow), 'sparse active row load before controller assignment');
const sparseApplyRow = { _parent: lifecycleParent };
expectNoThrow(() => new Function(rowScripts[14].slice(rowScripts[14].indexOf('{') + 1, rowScripts[14].lastIndexOf('}'))).call(sparseApplyRow), 'sparse apply row load before controller assignment');

global.ReturnFromScreen = () => {};
expectNoThrow(() => new Function(cancelBody).call(lifecycleParent), 'CancelScreen before controller assignment');
const lifecycleEnvironment = makeEnvironment();
lifecycleEnvironment.screen.Tick = () => {};
expectNoThrow(() => new Function(cancelBody).call(lifecycleEnvironment.screen), 'CancelScreen with controller');
assert.strictEqual(lifecycleEnvironment.screen.Tick, undefined);
const tickHook = new Function('return function(){' + tickBody + '}')();
const tickScreen = { GraphicsOptionsController: undefined };
expectNoThrow(() => tickHook.call(tickScreen), 'Tick hook before controller assignment');
tickScreen.GraphicsOptionsController = { Tick() { this.called = true; } };
tickHook.call(tickScreen);
assert.strictEqual(tickScreen.GraphicsOptionsController.called, true);

console.log('STATE_MACHINE_PASS');
'@
$HarnessSource = $HarnessSource.Replace('__CONTROLLER_SOURCE__', $ControllerSource).Replace('__ROW_SCRIPTS__', $RowScriptsJson).Replace('__CANCEL_BODY__', $CancelBodyJson).Replace('__TICK_BODY__', $TickBodyJson)
$HarnessRoot = Join-Path ([System.IO.Path]::GetTempPath()) ('BatmanGraphicsShellStateMachine-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $HarnessRoot -Force | Out-Null
try {
    $HarnessPath = Join-Path $HarnessRoot 'state-machine.js'
    Set-Content -LiteralPath $HarnessPath -Value $HarnessSource -Encoding UTF8
    $HarnessOutput = @(& $NodeCommand.Source $HarnessPath 2>&1)
    if ($LASTEXITCODE -ne 0) {
        throw "Batman graphics shell state-machine harness failed (exit code $LASTEXITCODE):`n$($HarnessOutput -join [Environment]::NewLine)"
    }
    if (($HarnessOutput -join [Environment]::NewLine).IndexOf('STATE_MACHINE_PASS', [System.StringComparison]::Ordinal) -lt 0) {
        throw "Batman graphics shell state-machine harness did not report STATE_MACHINE_PASS:`n$($HarnessOutput -join [Environment]::NewLine)"
    }
    Write-Output 'STATE_MACHINE_PASS'
} finally {
    if (Test-Path -LiteralPath $HarnessRoot) {
        Remove-Item -LiteralPath $HarnessRoot -Recurse -Force
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
