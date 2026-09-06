param(
    [string]$BatmanRoot,
    [string]$BatmanUserIniPath,
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Debug'
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
$ContractScriptText = Get-Content -LiteralPath $PSCommandPath -Raw

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
$DebugAssemblyPath = Join-Path $BatmanRoot "builder\tools\NativeSubtitleExePatcher\SubtitleSizeModBuilder\bin\$Configuration\net8.0\SubtitleSizeModBuilder.dll"
if (-not (Test-Path -LiteralPath $BuilderProjectPath -PathType Leaf)) {
    throw "Batman graphics-options builder project was not found: $BuilderProjectPath"
}

$BuildOutput = & dotnet build $BuilderProjectPath -c $Configuration --nologo --disable-build-servers -nr:false -p:UseSharedCompilation=false 2>&1
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

    $FunctionToken = "this.$FunctionName = function("
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

    $resolvedFilePath = $null
    try {
        $isPath = [IO.Path]::IsPathRooted($FilePath) -or $FilePath.IndexOf('\', [System.StringComparison]::Ordinal) -ge 0 -or $FilePath.IndexOf('/', [System.StringComparison]::Ordinal) -ge 0
        if ($isPath) {
            if (-not (Test-Path -LiteralPath $FilePath -PathType Leaf)) {
                throw "Executable path was not found: $FilePath"
            }
            $resolvedFilePath = (Resolve-Path -LiteralPath $FilePath -ErrorAction Stop).Path
        } else {
            $command = Get-Command -Name $FilePath -ErrorAction Stop
            if ($command.CommandType -ne 'Application' -and $command.CommandType -ne 'ExternalScript') {
                throw "Resolved command is not an executable: $FilePath"
            }
            $resolvedFilePath = $command.Path
            if ([string]::IsNullOrWhiteSpace($resolvedFilePath)) {
                $resolvedFilePath = $command.Source
            }
        }
    } catch {
        throw "$Context could not resolve executable '$FilePath': $($_.Exception.Message)"
    }

    $previousErrorActionPreference = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    $output = @()
    $exitCode = $null
    try {
        $global:LASTEXITCODE = $null
        $output = @(& $resolvedFilePath @Arguments 2>&1)
        $exitCode = $global:LASTEXITCODE
    } catch {
        throw "$Context failed to start '$resolvedFilePath': $($_.Exception.Message)`n$($output -join [Environment]::NewLine)"
    } finally {
        $ErrorActionPreference = $previousErrorActionPreference
    }
    if ($null -eq $exitCode) {
        throw "$Context failed to start '$resolvedFilePath': no process exit code was reported. $($output -join [Environment]::NewLine)"
    }
    if ($exitCode -ne 0) {
        throw "$Context failed (exit code $exitCode): $($output -join [Environment]::NewLine)"
    }
}

foreach ($RequiredProcessToken in @('Get-Command -Name $FilePath', 'Test-Path -LiteralPath $FilePath -PathType Leaf', 'Resolve-Path -LiteralPath $FilePath', 'failed to start')) {
    if ($ContractScriptText.IndexOf($RequiredProcessToken, [System.StringComparison]::Ordinal) -lt 0) {
        throw "Invoke-RequiredProcess is missing required process-resolution behavior: $RequiredProcessToken"
    }
}

$MissingProcessPath = Join-Path ([System.IO.Path]::GetTempPath()) ('BatmanGraphicsShellMissingProcess-' + [guid]::NewGuid().ToString('N') + '.exe')
$MissingProcessThrew = $false
try {
    Invoke-RequiredProcess -FilePath $MissingProcessPath -Arguments @() -Context 'Missing process contract'
} catch {
    $MissingProcessThrew = $true
}
if (-not $MissingProcessThrew) {
    throw 'Invoke-RequiredProcess must throw when the executable path cannot be resolved.'
}
$MissingProcessCommandThrew = $false
try {
    Invoke-RequiredProcess -FilePath ('BatmanGraphicsShellMissingCommand-' + [guid]::NewGuid().ToString('N')) -Arguments @() -Context 'Missing command contract'
} catch {
    $MissingProcessCommandThrew = $true
}
if (-not $MissingProcessCommandThrew) {
    throw 'Invoke-RequiredProcess must throw when a command name cannot be resolved.'
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
    'this.ResolutionModes = new Array();',
    'this.ResolutionInitialIndex = -1;',
    'this.ResolutionDraftIndex = -1;',
    'this.ResolutionCatalogCount = 0;',
    'this.ResolutionCatalogRequest = 5200;',
    'this.WindowedResolutionModes = new Array();',
    'this.FullscreenResolutionModes = new Array();',
    'this.ResolutionInitialWidth = undefined;',
    'this.ResolutionDraftWidth = undefined;',
    'this.ResolutionCatalogDeadline = undefined;',
    'this.ApplyQueue = new Array();',
    'this.ApplyQueueIndex = 0;',
    'this.CurrentPendingOperation = "";',
    'this.CurrentPendingCode = undefined;',
    'this.CurrentPendingDeadline = undefined;',
    'this.ApplySignalToggle = 0;',
    'this.RollbackSignalToggle = 0;',
    'this.UiStatus = "";',
    'this.RollbackLocked = false;',
    'this.Tick = function()',
    'function BeginInitialization()',
    'function ResetResolutionCatalogState()',
    'function BeginResolutionCatalogInitialization()',
    'function DecodeResolutionScalar(rawValue)',
    'function GetResolutionRequestOrdinal(request)',
    'function SelectResolutionKind(targetIndex)',
    'function PollDesktopActualMode(rawValue)',
    'function IsDeadlineReached(deadline)',
    'this.InitializationDeadline = getTimer() + 10000;',
    'flash.external.ExternalInterface.call("FE_SetControlType",this.Settings[this.InitializationIndex].ReadRequest,"");',
    'var rawValue = int(flash.external.ExternalInterface.call("FE_GetControlType"));',
    'if(rawValue == this.CurrentPendingCode)',
    'this.Settings[this.InitializationIndex].InitialIndex = rawValue - this.Settings[this.InitializationIndex].ReadResponseBase;',
    'this.Settings[this.InitializationIndex].DraftIndex = this.Settings[this.InitializationIndex].InitialIndex;',
    'function IsDirty()',
    'function IncrementSetting(rowIndex)',
    'function DecrementSetting(rowIndex)',
    'function ToggleSetting(rowIndex)',
    'function GetDetailLevelDraftIndex()',
    'function GetDetailLevelInitialIndex()',
    'function CanEditDetailLevel()',
    'function SetDetailPreset(index,forward)',
    'function ToggleDetailPreset()',
    'function IncrementDetailPreset()',
    'function DecrementDetailPreset()',
    'this.DetailLeafRows = new Array(6,7,8,9,10,11,12);',
    'this.Screen.BlockInput(true);',
    'this.Screen.BlockInput(false);',
    'function ApplyChanges()',
    'this.CurrentPendingDeadline = getTimer() + 2000;',
    'flash.external.ExternalInterface.call("FE_SetControlType",this.CurrentPendingCode,"");',
    'var expectedAcknowledgement = this.CurrentPendingSetting.IsResolution ? this.CurrentPendingSetting.WriteAcknowledgement : this.CurrentPendingSetting.WriteAcknowledgementBase + this.CurrentPendingSetting.DraftIndex;',
    'if(rawValue == expectedAcknowledgement)',
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

Assert-ContainsOrdinal -Text $ScreenFrame -Token 'this.ResolutionModes.push({Width:widthValue,Height:heightValue,Label:widthValue + " x " + heightValue});' -Context 'Resolution catalog label construction'
Assert-ContainsOrdinal -Text $ScreenFrame -Token 'this.ResolutionCatalogRequest = 5600;' -Context 'Desktop current width request'
Assert-ContainsOrdinal -Text $ScreenFrame -Token 'this.ResolutionCatalogRequest = 5601;' -Context 'Desktop current height request'
Assert-ContainsOrdinal -Text $ScreenFrame -Token 'this.ResolutionCatalogRequest == this.ResolutionCatalogBase + 197' -Context 'Catalog persisted width request'
Assert-ContainsOrdinal -Text $ScreenFrame -Token 'this.ResolutionCatalogRequest == this.ResolutionCatalogBase + 198' -Context 'Catalog persisted height request'
Assert-ContainsOrdinal -Text $ScreenFrame -Token 'rawValue == 4899' -Context 'Shared catalog failure response'
Assert-ContainsOrdinal -Text $ScreenFrame -Token 'rawValue == 5199' -Context 'Resolution apply failure'
Assert-ContainsOrdinal -Text $ScreenFrame -Token 'var ordinal = Math.floor((magnitude - 1) / 32768);' -Context 'Resolution response ordinal decoder'
Assert-ContainsOrdinal -Text $ScreenFrame -Token 'var expectedOrdinal = this.GetResolutionRequestOrdinal(this.ResolutionCatalogRequest);' -Context 'Resolution response request correlation'

$ExpectedSettingDefinitions = @(
    '{RowIndex:1,Name:"Fullscreen",Values:new Array("Windowed","Fullscreen"),ConfigValues:new Array(0,1),ReadRequest:4670,ReadResponseBase:4671,WriteRequestBase:4673,WriteAcknowledgementBase:4675,FailureResponse:4679,InitialIndex:-1,DraftIndex:-1}',
    '{RowIndex:3,Name:"VSync",Values:new Array("Off","On"),ConfigValues:new Array(0,1),ReadRequest:4200,ReadResponseBase:4210,WriteRequestBase:4220,WriteAcknowledgementBase:4230,FailureResponse:4299,InitialIndex:-1,DraftIndex:-1}',
    '{RowIndex:4,Name:"MSAA",Values:new Array("Off","2x","4x","8x","16x"),ConfigValues:new Array(0,1,2,3,5),ReadRequest:4300,ReadResponseBase:4310,WriteRequestBase:4320,WriteAcknowledgementBase:4330,FailureResponse:4399,InitialIndex:-1,DraftIndex:-1}',
    '{RowIndex:6,Name:"Bloom",Values:new Array("Off","On"),ConfigValues:new Array(0,1),ReadRequest:4600,ReadResponseBase:4601,WriteRequestBase:4603,WriteAcknowledgementBase:4605,FailureResponse:4609,InitialIndex:-1,DraftIndex:-1}',
    '{RowIndex:7,Name:"Dynamic Shadows",Values:new Array("Off","On"),ConfigValues:new Array(0,1),ReadRequest:4610,ReadResponseBase:4611,WriteRequestBase:4613,WriteAcknowledgementBase:4615,FailureResponse:4619,InitialIndex:-1,DraftIndex:-1}',
    '{RowIndex:8,Name:"Motion Blur",Values:new Array("Off","On"),ConfigValues:new Array(0,1),ReadRequest:4620,ReadResponseBase:4621,WriteRequestBase:4623,WriteAcknowledgementBase:4625,FailureResponse:4629,InitialIndex:-1,DraftIndex:-1}',
    '{RowIndex:9,Name:"Distortion",Values:new Array("Off","On"),ConfigValues:new Array(0,1),ReadRequest:4630,ReadResponseBase:4631,WriteRequestBase:4633,WriteAcknowledgementBase:4635,FailureResponse:4639,InitialIndex:-1,DraftIndex:-1}',
    '{RowIndex:10,Name:"Fog Volumes",Values:new Array("Off","On"),ConfigValues:new Array(0,1),ReadRequest:4640,ReadResponseBase:4641,WriteRequestBase:4643,WriteAcknowledgementBase:4645,FailureResponse:4649,InitialIndex:-1,DraftIndex:-1}',
    '{RowIndex:11,Name:"Spherical Harmonic Lighting",Values:new Array("Off","On"),ConfigValues:new Array(0,1),ReadRequest:4650,ReadResponseBase:4651,WriteRequestBase:4653,WriteAcknowledgementBase:4655,FailureResponse:4659,InitialIndex:-1,DraftIndex:-1}',
    '{RowIndex:12,Name:"Ambient Occlusion",Values:new Array("Off","On"),ConfigValues:new Array(0,1),ReadRequest:4660,ReadResponseBase:4661,WriteRequestBase:4663,WriteAcknowledgementBase:4665,FailureResponse:4669,InitialIndex:-1,DraftIndex:-1}',
    '{RowIndex:13,Name:"PhysX",Values:new Array("Off","Normal","High"),ConfigValues:new Array(0,1,2),ReadRequest:4400,ReadResponseBase:4410,WriteRequestBase:4420,WriteAcknowledgementBase:4430,FailureResponse:4499,InitialIndex:-1,DraftIndex:-1}',
    '{RowIndex:14,Name:"Stereo 3D",Values:new Array("Off","On"),ConfigValues:new Array(0,1),ReadRequest:4500,ReadResponseBase:4510,WriteRequestBase:4520,WriteAcknowledgementBase:4530,FailureResponse:4599,InitialIndex:-1,DraftIndex:-1}'
)
if ($ScreenFrame.IndexOf('RowIndex:5,', [System.StringComparison]::Ordinal) -ge 0) {
    throw 'Detail Level must remain a derived shell-local controller and must not be a Settings record.'
}
if ($ScreenFrame.IndexOf('Name:"Detail Level"', [System.StringComparison]::Ordinal) -ge 0) {
    throw 'Detail Level must not be serialized as a transmitted Settings record.'
}
if ($ScreenFrame.IndexOf('this.Settings.length != 11', [System.StringComparison]::Ordinal) -ge 0) {
    throw 'Graphics setting count checks must not replace the declarative Settings contract.'
}
foreach ($ExpectedSettingRow in @(3, 4, 6, 7, 8, 9, 10, 11, 12, 13, 14)) {
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
if ($ScreenFrame.IndexOf('this.Screen.Tick = undefined;', [System.StringComparison]::Ordinal) -ge 0) {
    throw 'Graphics controller destruction must leave the inherited Tick hook installed during exit.'
}
$CancelScreenBody = Get-ActionScriptNamedFunctionBody -ScriptText $ScreenFrame -FunctionName 'CancelScreen' -Context 'Graphics screen cancellation'
$CancelDestroyPosition = $CancelScreenBody.IndexOf('this.GraphicsOptionsController.Destroy();', [System.StringComparison]::Ordinal)
$CancelReturnPosition = $CancelScreenBody.IndexOf('ReturnFromScreen();', [System.StringComparison]::Ordinal)
if ($CancelDestroyPosition -lt 0 -or $CancelReturnPosition -lt 0 -or $CancelDestroyPosition -ge $CancelReturnPosition) {
    throw 'CancelScreen must destroy the graphics controller before starting the return transition.'
}
if ($ScreenFrame.IndexOf('getTimer() >=', [System.StringComparison]::Ordinal) -ge 0) {
    throw 'Graphics deadlines must use the rollover-safe IsDeadlineReached method.'
}
if ($ScreenFrame.IndexOf('FE_SetControlType",4210+', [System.StringComparison]::Ordinal) -ge 0 -or
    $ScreenFrame.IndexOf('FE_SetControlType",4310+', [System.StringComparison]::Ordinal) -ge 0 -or
    $ScreenFrame.IndexOf('FE_SetControlType",4601+', [System.StringComparison]::Ordinal) -ge 0 -or
    $ScreenFrame.IndexOf('FE_SetControlType",4611+', [System.StringComparison]::Ordinal) -ge 0 -or
    $ScreenFrame.IndexOf('FE_SetControlType",4621+', [System.StringComparison]::Ordinal) -ge 0 -or
    $ScreenFrame.IndexOf('FE_SetControlType",4631+', [System.StringComparison]::Ordinal) -ge 0 -or
    $ScreenFrame.IndexOf('FE_SetControlType",4641+', [System.StringComparison]::Ordinal) -ge 0 -or
    $ScreenFrame.IndexOf('FE_SetControlType",4651+', [System.StringComparison]::Ordinal) -ge 0 -or
    $ScreenFrame.IndexOf('FE_SetControlType",4661+', [System.StringComparison]::Ordinal) -ge 0 -or
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
$PresetMethodNames = @('GetDetailLevelDraftIndex', 'GetDetailLevelInitialIndex', 'CanEditDetailLevel', 'SetDetailPreset', 'ToggleDetailPreset', 'IncrementDetailPreset', 'DecrementDetailPreset')
$ForbiddenLocalPresetTokens = @('FE_SetControlType', 'FE_GetControlType', 'Helen_', 'ReadRequest', 'ReadResponseBase', 'WriteRequestBase', 'WriteAcknowledgementBase', 'FailureResponse', 'ApplyQueue', 'CurrentPendingCode')
foreach ($PresetMethodName in $PresetMethodNames) {
    $PresetMethodBody = Get-ActionScriptNamedFunctionBody -ScriptText $ScreenFrame -FunctionName $PresetMethodName -Context "Graphics local method $PresetMethodName"
    foreach ($ForbiddenLocalPresetToken in $ForbiddenLocalPresetTokens) {
        if ($PresetMethodBody.IndexOf($ForbiddenLocalPresetToken, [System.StringComparison]::Ordinal) -ge 0) {
            throw "Graphics local method $PresetMethodName contains forbidden transport/write token: $ForbiddenLocalPresetToken"
        }
    }
}
$SetDetailPresetBody = Get-ActionScriptNamedFunctionBody -ScriptText $ScreenFrame -FunctionName 'SetDetailPreset' -Context 'Graphics detail preset direction controller'
Assert-ContainsOrdinal -Text $SetDetailPresetBody -Token 'if(forward)' -Context 'Graphics detail preset forward/back direction'
Assert-ContainsOrdinal -Text $SetDetailPresetBody -Token 'UI_FrontEndSFX.UI_Forward' -Context 'Graphics detail preset forward sound'
Assert-ContainsOrdinal -Text $SetDetailPresetBody -Token 'UI_FrontEndSFX.UI_Back' -Context 'Graphics detail preset backward sound'
$ToggleDetailPresetBody = Get-ActionScriptNamedFunctionBody -ScriptText $ScreenFrame -FunctionName 'ToggleDetailPreset' -Context 'Graphics detail preset activation controller'
Assert-ContainsOrdinal -Text $ToggleDetailPresetBody -Token 'this.SetDetailPreset(targetIndex,true);' -Context 'Graphics detail preset activation direction'
$IncrementDetailPresetBody = Get-ActionScriptNamedFunctionBody -ScriptText $ScreenFrame -FunctionName 'IncrementDetailPreset' -Context 'Graphics detail preset right controller'
Assert-ContainsOrdinal -Text $IncrementDetailPresetBody -Token 'this.SetDetailPreset(currentIndex + 1,true);' -Context 'Graphics detail preset right direction'
$DecrementDetailPresetBody = Get-ActionScriptNamedFunctionBody -ScriptText $ScreenFrame -FunctionName 'DecrementDetailPreset' -Context 'Graphics detail preset left controller'
Assert-ContainsOrdinal -Text $DecrementDetailPresetBody -Token 'this.SetDetailPreset(targetIndex,false);' -Context 'Graphics detail preset left direction'
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
Assert-ContainsOrdinal -Text $BeginNextApplyStepBody -Token 'this.CurrentPendingCode = this.CurrentPendingSetting.WriteRequestBase + this.CurrentPendingSetting.DraftIndex' -Context 'Graphics setting write request'
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
$DestroyControllerBody = Get-ActionScriptNamedFunctionBody -ScriptText $ScreenFrame -FunctionName 'Destroy' -Context 'Graphics controller destruction'
Assert-ContainsOrdinal -Text $DestroyControllerBody -Token 'this.CurrentPendingOperation = "";' -Context 'Graphics controller destruction pending operation'
Assert-ContainsOrdinal -Text $DestroyControllerBody -Token 'this.Screen.BlockInput(false);' -Context 'Graphics controller destruction input recovery'
if ($DestroyControllerBody.IndexOf('this.Screen.Tick', [System.StringComparison]::Ordinal) -ge 0) {
    throw 'Graphics controller destruction must not replace the inherited screen Tick hook.'
}
$CompleteRollbackBody = Get-ActionScriptNamedFunctionBody -ScriptText $ScreenFrame -FunctionName 'CompleteRollback' -Context 'Graphics rollback success'
Assert-ContainsOrdinal -Text $CompleteRollbackBody -Token 'this.UiStatus = "Apply Failed";' -Context 'Graphics rollback success status'
$FailInitializationBody = Get-ActionScriptNamedFunctionBody -ScriptText $ScreenFrame -FunctionName 'FailInitialization' -Context 'Graphics initialization failure reset'
Assert-ContainsOrdinal -Text $FailInitializationBody -Token 'this.ResetResolutionCatalogState();' -Context 'Graphics initialization failure catalog reset'
Assert-ContainsOrdinal -Text $DestroyControllerBody -Token 'this.ResetResolutionCatalogState();' -Context 'Graphics controller destruction catalog reset'
if ($CompleteRollbackBody.IndexOf('setting.DraftIndex =', [System.StringComparison]::Ordinal) -ge 0 -or
    $CompleteRollbackBody.IndexOf('setting.InitialIndex =', [System.StringComparison]::Ordinal) -ge 0) {
    throw 'Successful graphics rollback must preserve finite setting draft and initial indices.'
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

$FullscreenRowScript = [string]$RowClipActions[0]
Assert-ContainsOrdinal -Text $FullscreenRowScript -Token 'this.RowIndex = 1;' -Context 'Graphics row action 1 fullscreen row binding'
Assert-ContainsOrdinal -Text $FullscreenRowScript -Token 'this.Names = new Array("Windowed","Fullscreen");' -Context 'Graphics row action 1 fullscreen values'
Assert-ContainsOrdinal -Text $FullscreenRowScript -Token '_parent.GraphicsOptionsController.ToggleSetting(this.RowIndex);' -Context 'Graphics row action 1 fullscreen activation'
Assert-ContainsOrdinal -Text $FullscreenRowScript -Token '_parent.GraphicsOptionsController.IncrementSetting(this.RowIndex);' -Context 'Graphics row action 1 fullscreen right action'
Assert-ContainsOrdinal -Text $FullscreenRowScript -Token '_parent.GraphicsOptionsController.DecrementSetting(this.RowIndex);' -Context 'Graphics row action 1 fullscreen left action'

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
            $Configuration,
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
    @{ Label = 'Fullscreen'; Value = $null },
    @{ Label = 'Resolution'; Value = $null },
    @{ Label = 'VSync'; Value = $null },
    @{ Label = 'MSAA'; Value = $null },
    @{ Label = 'Detail Level'; Value = $null },
    @{ Label = 'Bloom'; Value = $null },
    @{ Label = 'Dynamic Shadows'; Value = $null },
    @{ Label = 'Motion Blur'; Value = $null },
    @{ Label = 'Distortion'; Value = $null },
    @{ Label = 'Fog Volumes'; Value = $null },
    @{ Label = 'Spherical Harmonic Lighting'; Value = $null },
    @{ Label = 'Ambient Occlusion'; Value = $null },
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

    if (($RowIndex + 1) -eq 2) {
        Assert-ContainsOrdinal -Text $RowScript -Token 'this.RowIndex = 2;' -Context "$RowContext row binding"
        Assert-ContainsOrdinal -Text $RowScript -Token 'this.State = -1;' -Context "$RowContext state initialization"
        Assert-ContainsOrdinal -Text $RowScript -Token 'this.Initial = -1;' -Context "$RowContext initial initialization"
        Assert-ContainsOrdinal -Text $RowScript -Token 'this.Default = -1;' -Context "$RowContext default initialization"
        Assert-ContainsOrdinal -Text $RowScript -Token 'this.State = _parent.GraphicsOptionsController.GetResolutionDraftIndex();' -Context "$RowContext state synchronization"
        Assert-ContainsOrdinal -Text $RowScript -Token 'this.Initial = _parent.GraphicsOptionsController.GetResolutionInitialIndex();' -Context "$RowContext initial synchronization"
        Assert-ContainsOrdinal -Text $RowScript -Token 'this.Default = this.Initial;' -Context "$RowContext default synchronization"
        Assert-ContainsOrdinal -Text $RowScript -Token 'GetResolutionLabel();' -Context "$RowContext label state"
        Assert-ContainsOrdinal -Text $RowScript -Token 'CanDecrementResolution();' -Context "$RowContext left enabled state"
        Assert-ContainsOrdinal -Text $RowScript -Token 'CanIncrementResolution();' -Context "$RowContext right enabled state"
        Assert-ContainsOrdinal -Text $RowScript -Token 'ToggleResolution();' -Context "$RowContext activation"
        Assert-ContainsOrdinal -Text $RowScript -Token 'IncrementResolution();' -Context "$RowContext right action"
        Assert-ContainsOrdinal -Text $RowScript -Token 'DecrementResolution();' -Context "$RowContext left action"
        Assert-ContainsOrdinal -Text $RowScript -Token 'this.HasChanged = function()' -Context "$RowContext controller-backed HasChanged override"
        Assert-ContainsOrdinal -Text $RowScript -Token 'this.IsDefault = function()' -Context "$RowContext controller-backed IsDefault override"
        Assert-ContainsOrdinal -Text $RowScript -Token 'this.RestoreInitialValue = function()' -Context "$RowContext controller-backed RestoreInitialValue override"
        Assert-ContainsOrdinal -Text $RowScript -Token 'this.SetDefault = function()' -Context "$RowContext controller-backed SetDefault override"
        Assert-ContainsOrdinal -Text $RowScript -Token 'RestoreResolutionInitial();' -Context "$RowContext controller-backed default restore"
        Assert-ContainsOrdinal -Text $RowScript -Token 'this.Names.push(_parent.GraphicsOptionsController.ResolutionModes[resolutionModeIndex].Label);' -Context "$RowContext catalog marker labels"
        if ($RowScript.IndexOf('FE_Set?name?', [System.StringComparison]::Ordinal) -ge 0 -or
            $RowScript.IndexOf('UpdateLRMarkers();', [System.StringComparison]::Ordinal) -ge 0) {
            throw "$RowContext must not use inherited marker transport behavior."
        }
    } elseif (@(1, 3, 4, 6, 7, 8, 9, 10, 11, 12, 13, 14) -contains ($RowIndex + 1)) {
        $ActiveRowDefinitions = @{
            1 = @{ Values = 'this.Names = new Array("Windowed","Fullscreen");'; RowIndex = 1 }
            3 = @{ Values = 'this.Names = new Array("Off","On");'; RowIndex = 3 }
            4 = @{ Values = 'this.Names = new Array("Off","2x","4x","8x","16x");'; RowIndex = 4 }
            6 = @{ Values = 'this.Names = new Array("Off","On");'; RowIndex = 6 }
            7 = @{ Values = 'this.Names = new Array("Off","On");'; RowIndex = 7 }
            8 = @{ Values = 'this.Names = new Array("Off","On");'; RowIndex = 8 }
            9 = @{ Values = 'this.Names = new Array("Off","On");'; RowIndex = 9 }
            10 = @{ Values = 'this.Names = new Array("Off","On");'; RowIndex = 10 }
            11 = @{ Values = 'this.Names = new Array("Off","On");'; RowIndex = 11 }
            12 = @{ Values = 'this.Names = new Array("Off","On");'; RowIndex = 12 }
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
        Assert-ContainsOrdinal -Text $RowScript -Token 'this.ItemText.text = _parent.GraphicsOptionsController.IsUnavailable(this.RowIndex) ? _parent.GraphicsOptionsController.GetUnavailableLabel(this.RowIndex) : this.Names[this.State];' -Context $RowContext
        Assert-ContainsOrdinal -Text $RowScript -Token 'this.State = _parent.GraphicsOptionsController.GetDraftIndex(this.RowIndex);' -Context $RowContext
        Assert-ContainsOrdinal -Text $RowScript -Token 'this.Initial = _parent.GraphicsOptionsController.GetInitialIndex(this.RowIndex);' -Context $RowContext
        Assert-ContainsOrdinal -Text $RowScript -Token 'this.LeftClicker._visible = this.State > 0 && _parent.GraphicsOptionsController.CanEdit(this.RowIndex);' -Context $RowContext
        Assert-ContainsOrdinal -Text $RowScript -Token 'this.RightClicker._visible = this.State < this.Names.length - 1 && _parent.GraphicsOptionsController.CanEdit(this.RowIndex);' -Context $RowContext
        Assert-ContainsOrdinal -Text $RowScript -Token '_parent.GraphicsOptionsController.ToggleSetting(this.RowIndex);' -Context $RowContext
        Assert-ContainsOrdinal -Text $RowScript -Token '_parent.GraphicsOptionsController.IncrementSetting(this.RowIndex);' -Context $RowContext
        Assert-ContainsOrdinal -Text $RowScript -Token '_parent.GraphicsOptionsController.DecrementSetting(this.RowIndex);' -Context $RowContext
        Assert-ContainsOrdinal -Text $RowScript -Token '"Unavailable" : "Loading..."' -Context "$RowContext unresolved state"
        $ActiveUpdateBody = Get-ActionScriptFunctionBody -ScriptText $RowScript -FunctionName 'Update' -Context "$RowContext Update"
        $ActiveLabelPosition = $ActiveUpdateBody.IndexOf('this.Label.Label.Text.text =', [System.StringComparison]::Ordinal)
        $ActiveControllerGuardPosition = $ActiveUpdateBody.IndexOf('if(_parent.GraphicsOptionsController == undefined)', [System.StringComparison]::Ordinal)
        if ($ActiveLabelPosition -lt 0 -or $ActiveControllerGuardPosition -lt 0 -or $ActiveLabelPosition -ge $ActiveControllerGuardPosition) {
            throw "$RowContext must assign its stable label before checking for an absent controller."
        }
        Assert-ContainsOrdinal -Text $ActiveUpdateBody -Token 'if(this.ItemText != undefined)' -Context "$RowContext ItemText guard"
        Assert-ContainsOrdinal -Text $ActiveUpdateBody -Token 'if(this.LeftClicker != undefined)' -Context "$RowContext left clicker guard"
        Assert-ContainsOrdinal -Text $ActiveUpdateBody -Token 'if(this.RightClicker != undefined)' -Context "$RowContext right clicker guard"
        foreach ($ActiveFunctionName in @('RunAction', 'Increment', 'Decrement')) {
            $ActiveFunctionBody = Get-ActionScriptFunctionBody -ScriptText $RowScript -FunctionName $ActiveFunctionName -Context "$RowContext $ActiveFunctionName"
            Assert-ContainsOrdinal -Text $ActiveFunctionBody -Token 'if(_parent.GraphicsOptionsController != undefined)' -Context "$RowContext $ActiveFunctionName controller guard"
        }
    } elseif (($RowIndex + 1) -eq 5) {
        Assert-ContainsOrdinal -Text $RowScript -Token 'this.Names = new Array("Low","Medium","High","Very High","Custom");' -Context $RowContext
        Assert-ContainsOrdinal -Text $RowScript -Token 'this.State = -1;' -Context $RowContext
        Assert-ContainsOrdinal -Text $RowScript -Token 'this.Initial = -1;' -Context $RowContext
        Assert-ContainsOrdinal -Text $RowScript -Token 'this.Default = -1;' -Context $RowContext
        Assert-ContainsOrdinal -Text $RowScript -Token 'GetDetailLevelDraftIndex();' -Context $RowContext
        Assert-ContainsOrdinal -Text $RowScript -Token 'GetDetailLevelInitialIndex();' -Context $RowContext
        Assert-ContainsOrdinal -Text $RowScript -Token 'ToggleDetailPreset();' -Context $RowContext
        Assert-ContainsOrdinal -Text $RowScript -Token 'IncrementDetailPreset();' -Context $RowContext
        Assert-ContainsOrdinal -Text $RowScript -Token 'DecrementDetailPreset();' -Context $RowContext
        if ($RowScript.IndexOf('ToggleSetting', [System.StringComparison]::Ordinal) -ge 0 -or
            $RowScript.IndexOf('IncrementSetting', [System.StringComparison]::Ordinal) -ge 0 -or
            $RowScript.IndexOf('DecrementSetting', [System.StringComparison]::Ordinal) -ge 0) {
            throw "$RowContext must delegate only to detail preset methods."
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

foreach ($ExpectedSettingDefinition in $ExpectedSettingDefinitions) {
    Assert-ContainsOrdinal -Text $ScreenFrame -Token $ExpectedSettingDefinition -Context 'Graphics declarative setting definition'
}
if ($ExpectedSettingDefinitions.Count -ne 12) {
    throw "The graphics shell contract must define exactly twelve transmitted settings, found $($ExpectedSettingDefinitions.Count)."
}
$FullscreenDefinitionPosition = $ScreenFrame.IndexOf($ExpectedSettingDefinitions[0], [System.StringComparison]::Ordinal)
$VSyncDefinitionPosition = $ScreenFrame.IndexOf($ExpectedSettingDefinitions[1], [System.StringComparison]::Ordinal)
if ($FullscreenDefinitionPosition -lt 0 -or $VSyncDefinitionPosition -lt 0 -or $FullscreenDefinitionPosition -ge $VSyncDefinitionPosition) {
    throw 'Fullscreen must be the first declarative graphics setting before VSync.'
}

$ApplyRowScript = [string]$RowClipActions[14]
Assert-ContainsOrdinal -Text $ApplyRowScript -Token 'this.LabelName = "Apply Changes";' -Context 'Graphics row action 15 stable label state'
Assert-ContainsOrdinal -Text $ApplyRowScript -Token 'this._visible = true;' -Context 'Graphics row action 15'
Assert-ContainsOrdinal -Text $ApplyRowScript -Token 'this.ItemText.text = _parent.GraphicsOptionsController.GetApplyStatusText();' -Context 'Graphics row action 15 status'
Assert-ContainsOrdinal -Text $ApplyRowScript -Token '_parent.GraphicsOptionsController.ApplyChanges();' -Context 'Graphics row action 15'
Assert-ContainsOrdinal -Text $ApplyRowScript -Token '_parent.GraphicsOptionsController.CanApply() ? 100 : 40' -Context 'Graphics row action 15 enabled state'
Assert-ContainsOrdinal -Text $ApplyRowScript -Token 'if(_parent.GraphicsOptionsController == undefined)' -Context 'Graphics row action 15 controller-load guard'
Assert-ContainsOrdinal -Text $ApplyRowScript -Token 'this.ItemText.text = "";' -Context 'Graphics row action 15 controller-load guard'
$ApplyUpdateBody = Get-ActionScriptFunctionBody -ScriptText $ApplyRowScript -FunctionName 'Update' -Context 'Graphics row action 15 Update'
Assert-ContainsOrdinal -Text $ApplyUpdateBody -Token 'if(this.ItemText != undefined)' -Context 'Graphics row action 15 ItemText guard'
Assert-ContainsOrdinal -Text $ApplyUpdateBody -Token 'if(this.Label != undefined)' -Context 'Graphics row action 15 alpha Label guard'
Assert-ContainsOrdinal -Text $ApplyUpdateBody -Token 'if(this.LeftClicker != undefined)' -Context 'Graphics row action 15 left clicker guard'
Assert-ContainsOrdinal -Text $ApplyUpdateBody -Token 'if(this.RightClicker != undefined)' -Context 'Graphics row action 15 right clicker guard'
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
let currentScreen = null;

global.getTimer = () => now;
/** Converts a JavaScript value using the ECMAScript ToInt32 operation used by AS2 int(). */
function as2ToInt32(value) {
    const number = Number(value);
    if (!Number.isFinite(number) || number === 0) {
        return 0;
    }
    const integer = number < 0 ? Math.ceil(number) : Math.floor(number);
    let modulo = integer % 4294967296;
    if (modulo < 0) {
        modulo += 4294967296;
    }
    if (modulo >= 2147483648) {
        modulo -= 4294967296;
    }
    return modulo;
}

global.int = as2ToInt32;
global.flash = { external: { ExternalInterface: { call: (name, ...args) => {
    calls.push({ name, args });
    if (name === 'FE_GetControlType') {
        return carrierResponses.length > 0 ? carrierResponses.shift() : 0;
    }
    return 0;
} } } };
global.ReturnFromScreen = () => {
    if (currentScreen !== null) {
        currentScreen.outTransitionStarted = true;
        currentScreen.returnFromScreenCount += 1;
    }
};

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

function soundSignals() {
    return calls.filter(call => call.name === 'FE_PlaySoundFromString').map(call => call.args[0]);
}

function makeScreen() {
    const screen = {
        blockStates: [],
        bBlockInput: false,
        bLockInput: false,
        rowUpdates: 0,
        reUpdates: 0,
        BackScreen: 'OptionsMenu',
        BackScreenIndex: 1,
        outTransitionStarted: false,
        returnFromScreenCount: 0,
        BlockInput(value) {
            this.blockStates.push(value);
            this.bBlockInput = value;
            this.bLockInput = value;
        },
        ReUpdate() { this.reUpdates += 1; },
        TryBack() {
            if (this.bBlockInput || this.bLockInput || this.BackScreen === '') {
                return false;
            }
            new Function(cancelBody).call(this);
            return true;
        },
        onKeyDown() { return this.TryBack(); },
        onEnterFrame() { this.Tick(); },
        SetDefaults() {
            for (let rowIndex = 1; rowIndex <= 15; rowIndex += 1) {
                const row = this['GraphicsRow' + rowIndex];
                if (row !== undefined && typeof row.SetDefault === 'function') {
                    row.SetDefault();
                }
            }
        },
        onPressY() { return this.SetDefaults(); }
    };
    for (let rowIndex = 1; rowIndex <= 15; rowIndex += 1) {
        screen['GraphicsRow' + rowIndex] = { Update() { screen.rowUpdates += 1; } };
    }
    return screen;
}

function makeEnvironment() {
    clearCalls();
    carrierResponses = [];
    const screen = makeScreen();
    const controller = new BatmanGraphicsOptionsController(screen);
    screen.GraphicsOptionsController = controller;
    screen.Tick = new Function(tickBody);
    currentScreen = screen;
    return { screen, controller };
}

function requestOrdinal(request) {
    if (request >= 4700 && request <= 4898) return request - 4700;
    if (request >= 5200 && request <= 5398) return 199 + request - 5200;
    if (request >= 5400 && request <= 5598) return 398 + request - 5400;
    if (request >= 5600 && request <= 5601) return 597 + request - 5600;
    return -1;
}

function encodeResolutionScalar(request, scalar) {
    return -(scalar + requestOrdinal(request) * 32768);
}

function feedCatalog(controller, base, pairs, persistedWidth, persistedHeight) {
    queueResponses(encodeResolutionScalar(base, pairs.length)); controller.Tick();
    for (let index = 0; index < pairs.length; index += 1) {
        const request = base + 1 + index * 2;
        queueResponses(encodeResolutionScalar(request, pairs[index][0])); controller.Tick();
        queueResponses(encodeResolutionScalar(request + 1, pairs[index][1])); controller.Tick();
    }
    queueResponses(encodeResolutionScalar(base + 197, persistedWidth)); controller.Tick();
    queueResponses(encodeResolutionScalar(base + 198, persistedHeight)); controller.Tick();
}

function initialize(controller, values) {
    controller.BeginInitialization();
    assert.deepStrictEqual(settingSignals(), [4670]);
    const startupRequests = [4670, 4200, 4300, 4600, 4610, 4620, 4630, 4640, 4650, 4660, 4400, 4500, 5200, 5201, 5202, 5203, 5204, 5205, 5206, 5397, 5398, 5400, 5401, 5402, 5403, 5404, 5405, 5406, 5597, 5598, 5600, 5601];
    const responses = [4671 + values[0]];
    const responseBases = [4210, 4310, 4601, 4611, 4621, 4631, 4641, 4651, 4661, 4410, 4510];
    for (let index = 1; index < values.length; index += 1) responses.push(responseBases[index - 1] + values[index]);
    for (const response of responses) { queueResponses(response); controller.Tick(); }
    const pairs = [[1280, 720], [1920, 1080], [3440, 1440]];
    feedCatalog(controller, 5200, pairs, 1920, 1080);
    feedCatalog(controller, 5400, pairs, 1920, 1080);
    queueResponses(encodeResolutionScalar(5600, 1920)); controller.Tick();
    queueResponses(encodeResolutionScalar(5601, 1080)); controller.Tick();
    assert.deepStrictEqual(settingSignals(), startupRequests);
    if (!controller.InitializationComplete) {
        throw new Error(`corrected initialization incomplete: stage=${controller.ResolutionInitializationStage} request=${controller.ResolutionCatalogRequest} pending=${controller.CurrentPendingCode} kind=${controller.ResolutionCatalogKind} windowed=${controller.WindowedResolutionModes.length}/${controller.WindowedResolutionAvailable} fullscreen=${controller.FullscreenResolutionModes.length}/${controller.FullscreenResolutionAvailable} desktop=${controller.DesktopActualWidth}x${controller.DesktopActualHeight}`);
    }
    assert.strictEqual(controller.CanApply(), false);
}

function expectInitializationRequestEcho(controller, request) {
    assert.strictEqual(controller.CurrentPendingCode, request, 'initialization must expose the request being awaited before its echo is polled');
    queueResponses(request);
    controller.Tick();
    assert.strictEqual(controller.InitializationFailed, false, 'an initialization request echo must remain pending');
    assert.strictEqual(controller.InitializationComplete, false, 'an initialization request echo must not complete initialization');
}

function initializeWithPendingRequestEchoes(values) {
    setNow(0);
    const echoedEnvironment = makeEnvironment();
    const echoedController = echoedEnvironment.controller;
    echoedController.BeginInitialization();
    assert.deepStrictEqual(settingSignals().slice(0, 1), [4670]);

    expectInitializationRequestEcho(echoedController, 4670);
    queueResponses(4671 + values[0]);
    echoedController.Tick();

    const staticRequests = [4200, 4300, 4600, 4610, 4620, 4630, 4640, 4650, 4660, 4400, 4500];
    const staticBases = [4210, 4310, 4601, 4611, 4621, 4631, 4641, 4651, 4661, 4410, 4510];
    for (let staticIndex = 0; staticIndex < staticRequests.length; staticIndex += 1) {
        expectInitializationRequestEcho(echoedController, staticRequests[staticIndex]);
        queueResponses(staticBases[staticIndex] + (staticIndex === 0 ? values[1] : values[staticIndex + 1]));
        echoedController.Tick();
    }
    function echoAndRespond(request, scalar) {
        expectInitializationRequestEcho(echoedController, request);
        queueResponses(encodeResolutionScalar(request, scalar));
        echoedController.Tick();
    }
    echoAndRespond(5200, 3);
    echoAndRespond(5201, 1280); echoAndRespond(5202, 720);
    echoAndRespond(5203, 1920); echoAndRespond(5204, 1080);
    echoAndRespond(5205, 3440); echoAndRespond(5206, 1440);
    echoAndRespond(5397, 1920); echoAndRespond(5398, 1080);
    echoAndRespond(5400, 3);
    echoAndRespond(5401, 1280); echoAndRespond(5402, 720);
    echoAndRespond(5403, 1920); echoAndRespond(5404, 1080);
    echoAndRespond(5405, 3440); echoAndRespond(5406, 1440);
    echoAndRespond(5597, 1920); echoAndRespond(5598, 1080);
    echoAndRespond(5600, 1920); echoAndRespond(5601, 1080);
    assert.strictEqual(echoedController.InitializationFailed, false);
    assert.strictEqual(echoedController.InitializationComplete, true);
    assert.strictEqual(echoedController.CanApply(), false);
    return echoedEnvironment;
}

function loadRow(script, parent, children) {
    const bodyStart = script.indexOf('{');
    const bodyEnd = script.lastIndexOf('}');
    const row = Object.assign({
        _parent: parent,
        GameVariable: '?name?',
        Label: { Label: { Text: { text: '' } } },
        ItemText: { text: '', _alpha: 0 },
        LeftClicker: { _visible: true, _x: 100 },
        RightClicker: { _visible: true, _x: 200 }
    }, children || {});
    row.HasChanged = function() { return this.State != this.Initial; };
    row.UpdateLRMarkers = function() {
        flash.external.ExternalInterface.call('FE_Set?name?', this.State, this.Names[this.State]);
    };
    row.RestoreInitialValue = function() {
        if (this.HasChanged()) {
            this.State = this.Initial;
            this.UpdateLRMarkers();
        }
    };
    row.IsDefault = function() { return this.State == this.Default; };
    row.SetDefault = function() {
        this.State = this.Default;
        this.UpdateLRMarkers();
    };
    global._parent = parent;
    const load = new Function(script.slice(bodyStart + 1, bodyEnd));
    load.call(row);
    return row;
}

function expectNoThrow(action, message) {
    assert.doesNotThrow(action, message);
}

assert.strictEqual(as2ToInt32(3.9), 3);
assert.strictEqual(as2ToInt32(-3.9), -3);
assert.strictEqual(as2ToInt32(Infinity), 0);
assert.strictEqual(as2ToInt32(NaN), 0);
assert.strictEqual(as2ToInt32(4294967297), 1);
assert.strictEqual(as2ToInt32(-4294967297), -1);
assert.strictEqual(as2ToInt32(2147483648), -2147483648);
assert.strictEqual(as2ToInt32(-2147483649), 2147483647);
assert.strictEqual(as2ToInt32(-2147483648), -2147483648);

function expectCatalogFailure(responses, message) {
    setNow(0);
    const failedEnvironment = makeEnvironment();
    const failedController = failedEnvironment.controller;
    failedController.BeginInitialization();
    for (const response of [4671, 4210, 4310, 4601, 4611, 4621, 4631, 4641, 4651, 4661, 4410, 4510]) {
        queueResponses(response);
        failedController.Tick();
    }
    for (const response of responses) {
        queueResponses(response);
        failedController.Tick();
    }
    assert.strictEqual(failedController.ResolutionInitializationStage, 'fullscreen', message);
    assert.strictEqual(failedController.WindowedResolutionModes.length, 0, 'failed catalog must discard partial entries');
    feedCatalog(failedController, 5400, [[1920, 1080]], 2560, 1600);
    queueResponses(encodeResolutionScalar(5600, 1920)); failedController.Tick();
    queueResponses(encodeResolutionScalar(5601, 1080)); failedController.Tick();
    assert.strictEqual(failedController.InitializationComplete, true, message);
    assert.strictEqual(failedController.IsUnavailable(2), true, message);
    assert.strictEqual(failedController.CanEdit(3), true, 'catalog rejection must preserve VSync');
    return failedEnvironment;
}

expectCatalogFailure([0], 'zero catalog count is rejected');
expectCatalogFailure([encodeResolutionScalar(5200, 99)], 'oversized catalog count is rejected');
expectCatalogFailure([encodeResolutionScalar(5200, 2), encodeResolutionScalar(5201, 1280), encodeResolutionScalar(5202, 720), encodeResolutionScalar(5203, 1280), encodeResolutionScalar(5204, 720)], 'duplicate catalog pairs are rejected');
expectCatalogFailure([encodeResolutionScalar(5200, 1), encodeResolutionScalar(5201, 1280), 0], 'incomplete pair is rejected');
expectCatalogFailure([encodeResolutionScalar(5200, 1), 5202], 'out-of-order request is rejected');
expectCatalogFailure([encodeResolutionScalar(5200, 1), 1280], 'positive scalar is rejected');
expectCatalogFailure([4672], 'unrelated static response is rejected');
expectCatalogFailure([encodeResolutionScalar(5200, 1), encodeResolutionScalar(5200, 1)], 'stale prior response is rejected');
expectCatalogFailure([encodeResolutionScalar(5200, 1), encodeResolutionScalar(5202, 1280)], 'future response is rejected');
expectCatalogFailure([encodeResolutionScalar(5200, 1), -2147483648], 'INT_MIN response is rejected');

let isolatedLegacyFailure = initializeCorrected(Array(12).fill(0), { failLegacyIndex: 1 });
assert.strictEqual(isolatedLegacyFailure.controller.InitializationComplete, true, 'a legacy scalar failure must not abort initialization');
assert.strictEqual(isolatedLegacyFailure.controller.Settings[1].InitialIndex, -1, 'the failed legacy row is unavailable');
assert.strictEqual(isolatedLegacyFailure.controller.Settings[2].InitialIndex, 0, 'later legacy rows still initialize after a failure');
let isolatedWindowedFailure = initializeCorrected(Array(12).fill(0), { failWindowed: true });
assert.strictEqual(isolatedWindowedFailure.controller.InitializationComplete, true, 'windowed catalog failure is isolated');
assert.strictEqual(isolatedWindowedFailure.controller.WindowedResolutionAvailable, false, 'windowed resolution is unavailable after catalog failure');
assert.strictEqual(isolatedWindowedFailure.controller.Settings[1].InitialIndex, 0, 'VSync remains initialized after catalog failure');
let isolatedFullscreenFailure = initializeCorrected(Array(12).fill(0), { failFullscreen: true });
assert.strictEqual(isolatedFullscreenFailure.controller.InitializationComplete, true, 'fullscreen catalog failure is isolated');
assert.strictEqual(isolatedFullscreenFailure.controller.FullscreenResolutionAvailable, false, 'fullscreen resolution is unavailable after catalog failure');

initializeWithPendingRequestEchoes(Array(12).fill(0));

setNow(0);
clearCalls();
environment = makeEnvironment();
controller = environment.controller;
initialize(controller, Array(12).fill(0));
assert.deepStrictEqual(controller.ResolutionModes.map(mode => mode.Label), ['1280 x 720', '1920 x 1080', '3440 x 1440']);
controller.DecrementResolution();
controller.DecrementResolution();
assert.strictEqual(controller.ResolutionDraftIndex, 0, 'resolution left action must clamp at first mode');
controller.IncrementResolution();
controller.IncrementResolution();
controller.IncrementResolution();
assert.strictEqual(controller.ResolutionDraftIndex, 2, 'resolution right action must clamp at last mode');
controller.ToggleResolution();
assert.strictEqual(controller.ResolutionDraftIndex, 0, 'resolution activation must wrap from last mode');
controller.ToggleResolution();
assert.strictEqual(controller.ResolutionDraftIndex, 1, 'resolution activation must advance one mode');
clearCalls();
controller.DecrementResolution();
assert.deepStrictEqual(settingSignals(), [], 'resolution selection must remain shell-local before Apply');
assert.strictEqual(controller.CanApply(), true);
controller.ApplyChanges();
assert.strictEqual(settingSignals()[settingSignals().length - 1], 5000, 'resolution Apply must send selected index request');
queueResponses(5100);
controller.Tick();
assert.strictEqual(settingSignals()[settingSignals().length - 1], 4991);
queueResponses(4981);
controller.Tick();
assert.strictEqual(controller.ResolutionInitialIndex, 0, 'resolution commit must copy draft to initial');
assert.strictEqual(controller.ResolutionDraftIndex, 0);
const committedResolutionRow = loadRow(rowScripts[1], environment.screen);
assert.strictEqual(committedResolutionRow.State, 0);
assert.strictEqual(committedResolutionRow.Initial, 0);
assert.strictEqual(committedResolutionRow.Default, 0);
assert.strictEqual(committedResolutionRow.HasChanged(), false);
assert.strictEqual(committedResolutionRow.IsDefault(), true);

function applyResolutionFailure(failureResponse) {
    setNow(0);
    clearCalls();
    const failedEnvironment = makeEnvironment();
    initialize(failedEnvironment.controller, Array(12).fill(0));
    failedEnvironment.controller.DecrementResolution();
    failedEnvironment.controller.ApplyChanges();
    assert.strictEqual(settingSignals()[settingSignals().length - 1], 5000);
    queueResponses(failureResponse);
    failedEnvironment.controller.Tick();
    const rollbackSignal = settingSignals()[settingSignals().length - 1];
    assert.ok(rollbackSignal === 4970 || rollbackSignal === 4971);
    return { environment: failedEnvironment, rollbackSignal };
}

let resolutionFailure = applyResolutionFailure(5199);
queueResponses(resolutionFailure.rollbackSignal === 4970 ? 4960 : 4961);
resolutionFailure.environment.controller.Tick();
assert.strictEqual(resolutionFailure.environment.controller.GetApplyStatusText(), 'Apply Failed');
assert.strictEqual(resolutionFailure.environment.controller.ResolutionDraftIndex, resolutionFailure.environment.controller.ResolutionInitialIndex);
const rollbackResolutionRow = loadRow(rowScripts[1], resolutionFailure.environment.screen);
assert.strictEqual(rollbackResolutionRow.State, resolutionFailure.environment.controller.ResolutionInitialIndex);
assert.strictEqual(rollbackResolutionRow.Initial, resolutionFailure.environment.controller.ResolutionInitialIndex);
assert.strictEqual(rollbackResolutionRow.Default, resolutionFailure.environment.controller.ResolutionInitialIndex);
assert.strictEqual(rollbackResolutionRow.HasChanged(), false);

resolutionFailure = applyResolutionFailure(5199);
setNow(resolutionFailure.environment.controller.CurrentPendingDeadline);
resolutionFailure.environment.controller.Tick();
assert.strictEqual(settingSignals()[settingSignals().length - 1] === 4970 || settingSignals()[settingSignals().length - 1] === 4971, true);

setNow(0);
clearCalls();
environment = makeEnvironment();
initialize(environment.controller, Array(12).fill(0));
environment.controller.DecrementResolution();
environment.controller.ApplyChanges();
queueResponses(5100);
environment.controller.Tick();
queueResponses(4989);
environment.controller.Tick();
const commitFailureRollbackSignal = settingSignals()[settingSignals().length - 1];
queueResponses(commitFailureRollbackSignal === 4970 ? 4960 : 4961);
environment.controller.Tick();
assert.strictEqual(environment.controller.GetApplyStatusText(), 'Apply Failed');

resolutionFailure = applyResolutionFailure(5199);
queueResponses(4969);
resolutionFailure.environment.controller.Tick();
assert.strictEqual(resolutionFailure.environment.controller.GetApplyStatusText(), 'Rollback Failed');
assert.strictEqual(resolutionFailure.environment.controller.RollbackLocked, true);

setNow(0);
clearCalls();
environment = makeEnvironment();
initialize(environment.controller, Array(12).fill(0));
environment.controller.DecrementResolution();
clearCalls();
assert.strictEqual(environment.screen.TryBack(), true);
assert.strictEqual(calls.some(call => call.name === 'FE_SetControlType' && call.args[0] >= 5000 && call.args[0] <= 5097), false, 'Back must not emit a resolution write request');

function initializeBoundaryCatalog(controller, currentWidth, currentHeight, expectedCurrentIndex) {
    controller.BeginInitialization();
    assert.deepStrictEqual(settingSignals(), [4670]);
    const responses = [4671];
    const staticResponses = [4210, 4310, 4601, 4611, 4621, 4631, 4641, 4651, 4661, 4410, 4510];
    for (const response of staticResponses) responses.push(response);
    for (let settingIndex = 0; settingIndex < responses.length; settingIndex += 1) {
        queueResponses(responses[settingIndex]);
        controller.Tick();
    }
    const boundaryPairs = [];
    for (let modeIndex = 0; modeIndex < 98; modeIndex += 1) {
        const widthValue = modeIndex == 97 ? 32767 : modeIndex + 1;
        const heightValue = modeIndex == 97 ? 32767 : 1000 + modeIndex;
        boundaryPairs.push([widthValue, heightValue]);
    }
    feedCatalog(controller, 5200, boundaryPairs, currentWidth, currentHeight);
    feedCatalog(controller, 5400, boundaryPairs, currentWidth, currentHeight);
    queueResponses(encodeResolutionScalar(5600, 1920)); controller.Tick();
    queueResponses(encodeResolutionScalar(5601, 1080)); controller.Tick();
    assert.strictEqual(controller.InitializationComplete, true);
    assert.strictEqual(controller.ResolutionModes.length, 98);
    assert.strictEqual(controller.ResolutionInitialIndex, expectedCurrentIndex);
    assert.strictEqual(controller.ResolutionModes[97].Label, '32767 x 32767');
}

setNow(0);
clearCalls();
const boundaryEnvironment = makeEnvironment();
initializeBoundaryCatalog(boundaryEnvironment.controller, 1, 1000, 0);
for (let boundaryIndex = 0; boundaryIndex < 97; boundaryIndex += 1) {
    boundaryEnvironment.controller.IncrementResolution();
}
assert.strictEqual(boundaryEnvironment.controller.ResolutionDraftIndex, 97);
boundaryEnvironment.controller.ApplyChanges();
assert.strictEqual(settingSignals()[settingSignals().length - 1], 5097, 'last resolution index must use request 5097');
queueResponses(5197);
boundaryEnvironment.controller.Tick();
assert.strictEqual(settingSignals()[settingSignals().length - 1], 4991);
queueResponses(4981);
boundaryEnvironment.controller.Tick();
assert.strictEqual(boundaryEnvironment.controller.ResolutionInitialIndex, 97);

setNow(0);
clearCalls();
const currentScalarEnvironment = makeEnvironment();
initializeBoundaryCatalog(currentScalarEnvironment.controller, 32767, 32767, 97);
assert.strictEqual(currentScalarEnvironment.controller.ResolutionInitialIndex, 97, '32767 scalar must be accepted for current dimensions');

setNow(0);
clearCalls();
const retryEnvironment = makeEnvironment();
retryEnvironment.controller.BeginInitialization();
queueResponses(4671);
retryEnvironment.controller.Tick();
queueResponses(0);
retryEnvironment.controller.Tick();
assert.strictEqual(retryEnvironment.controller.InitializationFailed, false);
assert.strictEqual(retryEnvironment.controller.Settings[1].InitialIndex, -1, 'a malformed VSync response isolates that legacy row');
clearCalls();
initialize(retryEnvironment.controller, Array(12).fill(0));
assert.strictEqual(retryEnvironment.controller.ResolutionInitialIndex, 1, `same controller retry must rebuild a clean catalog (${retryEnvironment.controller.ResolutionInitialWidth}x${retryEnvironment.controller.ResolutionInitialHeight}, ${retryEnvironment.controller.ResolutionModes.map(mode => mode.Label).join(',')})`);
retryEnvironment.controller.Destroy();
assert.deepStrictEqual(retryEnvironment.controller.ResolutionModes, []);
assert.strictEqual(retryEnvironment.controller.ResolutionCatalogCount, 0);
assert.strictEqual(retryEnvironment.controller.ResolutionCatalogRequest, undefined);
assert.strictEqual(retryEnvironment.controller.ResolutionCatalogDeadline, undefined);
assert.strictEqual(retryEnvironment.controller.ResolutionCatalogIndex, 0);
assert.strictEqual(retryEnvironment.controller.ResolutionPendingWidth, undefined);
assert.strictEqual(retryEnvironment.controller.ResolutionInitialIndex, -1);
assert.strictEqual(retryEnvironment.controller.ResolutionDraftIndex, -1);

const geometryParent = { GraphicsOptionsController: undefined };
const editableGeometryRows = [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13];
for (const rowIndex of editableGeometryRows) {
    const row = loadRow(rowScripts[rowIndex], geometryParent);
    assert.strictEqual(row.LeftClicker._x, 88, `editable row ${rowIndex + 1} shifts the left clicker exactly once on load`);
    assert.strictEqual(row.RightClicker._x, 212, `editable row ${rowIndex + 1} shifts the right clicker exactly once on load`);
    row.Update();
    row.Update();
    assert.strictEqual(row.LeftClicker._x, 88, `editable row ${rowIndex + 1} does not drift during Update`);
    assert.strictEqual(row.RightClicker._x, 212, `editable row ${rowIndex + 1} does not drift during Update`);
}

for (const rowIndex of [14]) {
    const row = loadRow(rowScripts[rowIndex], geometryParent);
    assert.strictEqual(row.LeftClicker._x, 100, `arrowless row ${rowIndex + 1} keeps the left clicker unshifted`);
    assert.strictEqual(row.RightClicker._x, 200, `arrowless row ${rowIndex + 1} keeps the right clicker unshifted`);
}

const unpositionedEditableRow = loadRow(rowScripts[2], geometryParent, {
    LeftClicker: { _visible: true },
    RightClicker: { _visible: true }
});
assert.strictEqual(unpositionedEditableRow.LeftClicker._x, undefined, 'editable row leaves an unpositioned left clicker unchanged');
assert.strictEqual(unpositionedEditableRow.RightClicker._x, undefined, 'editable row leaves an unpositioned right clicker unchanged');
assert.strictEqual(Number.isNaN(unpositionedEditableRow.LeftClicker._x), false, 'editable row does not create NaN for an unpositioned left clicker');
assert.strictEqual(Number.isNaN(unpositionedEditableRow.RightClicker._x), false, 'editable row does not create NaN for an unpositioned right clicker');

setNow(0);
clearCalls();
var environment = makeEnvironment();
var controller = environment.controller;
controller.BeginInitialization();
assert.deepStrictEqual(settingSignals(), [4670]);
queueResponses(9999);
controller.Tick();
assert.deepStrictEqual(settingSignals(), [4670, 4200]);
assert.strictEqual(controller.InitializationFailed, false);
assert.strictEqual(controller.Settings[0].InitialIndex, -1, 'Fullscreen timeout isolates the new row');
environment = makeEnvironment();
controller = environment.controller;
clearCalls();
const startupValues = [0, 1, 4, 0, 0, 0, 0, 0, 0, 0, 2, 1];
initialize(controller, startupValues);
assert.deepStrictEqual(controller.Settings.map(setting => setting.InitialIndex), startupValues);
assert.deepStrictEqual(controller.Settings.map(setting => setting.DraftIndex), startupValues);
assert.strictEqual(controller.Settings.length, 12);
assert.deepStrictEqual(controller.ResolutionModes.map(mode => mode.Label), ['1280 x 720', '1920 x 1080', '3440 x 1440']);
assert.strictEqual(controller.ResolutionInitialIndex, 1, `startup resolution mismatch: ${controller.ResolutionInitialWidth}x${controller.ResolutionInitialHeight} modes=${controller.ResolutionModes.map(mode => mode.Label).join(',')} kind=${controller.Settings[0].InitialIndex}`);
assert.strictEqual(controller.ResolutionDraftIndex, 1, `startup draft mismatch: ${controller.ResolutionInitialWidth}x${controller.ResolutionInitialHeight} index=${controller.ResolutionDraftIndex}`);
const resolutionRow = loadRow(rowScripts[1], environment.screen);
assert.strictEqual(resolutionRow.State, 1, `resolution row startup state mismatch: ${resolutionRow.State}`);
assert.strictEqual(resolutionRow.Initial, 1, `resolution row startup initial mismatch: ${resolutionRow.Initial}`);
assert.strictEqual(resolutionRow.Default, 1, `resolution row startup default mismatch: ${resolutionRow.Default}`);
assert.strictEqual(resolutionRow.HasChanged(), false);
assert.strictEqual(resolutionRow.IsDefault(), true);
controller.DecrementResolution();
resolutionRow.Update();
assert.strictEqual(resolutionRow.State, 0);
assert.strictEqual(resolutionRow.Initial, 1);
assert.strictEqual(resolutionRow.HasChanged(), true);
assert.strictEqual(resolutionRow.IsDefault(), false);
resolutionRow.RestoreInitialValue();
assert.strictEqual(controller.ResolutionDraftIndex, 1, 'resolution RestoreInitialValue must delegate to the controller');
assert.strictEqual(resolutionRow.State, 1);
assert.strictEqual(resolutionRow.HasChanged(), false);
controller.DecrementResolution();
assert.strictEqual(controller.ResolutionDraftIndex, 0);
controller.IncrementResolution();
assert.strictEqual(controller.InitializationDeadline, 10000);
assert.strictEqual(controller.GetDetailLevelInitialIndex(), 0);
assert.strictEqual(controller.GetDetailLevelDraftIndex(), 0);
assert.strictEqual(controller.CanEditDetailLevel(), true);
assert.strictEqual(controller.CanApply(), false);

setNow(0);
clearCalls();
const defaultsEnvironment = makeEnvironment();
const defaultsController = defaultsEnvironment.controller;
initialize(defaultsController, Array(12).fill(0));
const defaultsResolutionRow = loadRow(rowScripts[1], defaultsEnvironment.screen);
defaultsEnvironment.screen.GraphicsRow2 = defaultsResolutionRow;
defaultsResolutionRow.Update();
defaultsController.DecrementResolution();
assert.strictEqual(defaultsController.ResolutionDraftIndex, 0);
assert.strictEqual(defaultsResolutionRow.HasChanged(), true);
clearCalls();
defaultsEnvironment.screen.onPressY();
assert.strictEqual(defaultsController.ResolutionDraftIndex, defaultsController.ResolutionInitialIndex, 'Screen onPressY defaults must restore the saved resolution index');
assert.strictEqual(defaultsResolutionRow.State, defaultsController.ResolutionInitialIndex, 'Screen defaults must refresh the resolution row state');
assert.strictEqual(defaultsResolutionRow.HasChanged(), false, 'Screen defaults must clear resolution HasChanged');
assert.strictEqual(defaultsResolutionRow.IsDefault(), true, 'Screen defaults must make the resolution row default');
assert.strictEqual(calls.some(call => call.name === 'FE_Set?name?'), false, 'resolution defaults must not issue an invalid FE_Set?name? call');
defaultsController.ApplyChanges();
assert.deepStrictEqual(settingSignals(), [], 'Apply after Screen defaults must not queue a hidden resolution write');
defaultsController.DecrementResolution();
clearCalls();
defaultsController.ApplyChanges();
assert.strictEqual(settingSignals()[settingSignals().length - 1], 5000, 'a normal changed resolution must still queue its explicit write');

setNow(0);
clearCalls();
const fullscreenEnvironment = makeEnvironment();
const fullscreenController = fullscreenEnvironment.controller;
initialize(fullscreenController, Array(12).fill(0));
fullscreenController.ToggleSetting(1);
assert.strictEqual(fullscreenController.Settings[0].DraftIndex, 1);
assert.strictEqual(fullscreenController.Settings[0].InitialIndex, 0);
assert.strictEqual(fullscreenController.CanApply(), true);
fullscreenController.ApplyChanges();
assert.strictEqual(settingSignals()[settingSignals().length - 1], 4674);
queueResponses(4676);
fullscreenController.Tick();
assert.strictEqual(settingSignals()[settingSignals().length - 1], 4991);
queueResponses(4981);
fullscreenController.Tick();
assert.strictEqual(fullscreenController.Settings[0].InitialIndex, 1);
assert.strictEqual(fullscreenController.Settings[0].DraftIndex, 1);
assert.strictEqual(fullscreenController.CanApply(), false);

setNow(0);
clearCalls();
const fullscreenFailureEnvironment = makeEnvironment();
const fullscreenFailureController = fullscreenFailureEnvironment.controller;
initialize(fullscreenFailureController, Array(12).fill(0));
fullscreenFailureController.ToggleSetting(1);
fullscreenFailureController.ApplyChanges();
queueResponses(4679);
fullscreenFailureController.Tick();
const fullscreenRollbackSignal = settingSignals()[settingSignals().length - 1];
assert.ok(fullscreenRollbackSignal === 4970 || fullscreenRollbackSignal === 4971);
queueResponses(fullscreenRollbackSignal === 4970 ? 4960 : 4961);
fullscreenFailureController.Tick();
assert.strictEqual(fullscreenFailureController.GetApplyStatusText(), 'Apply Failed');
assert.strictEqual(fullscreenFailureController.Settings[0].DraftIndex, 0, 'failed fullscreen Apply restores the saved windowed mode');
assert.strictEqual(fullscreenFailureController.Settings[0].InitialIndex, 0);

setNow(0);
environment = makeEnvironment();
controller = environment.controller;
controller.BeginInitialization();
for (const initializationFailureResponse of [4679, 4299, 4399, 4609, 4619, 4629, 4639, 4649, 4659, 4669, 4499, 4599, 4899, 4899, 4899]) {
    queueResponses(initializationFailureResponse);
    controller.Tick();
}
assert.strictEqual(controller.InitializationComplete, true, 'every row failure still completes the bounded initialization flow');
assert.deepStrictEqual(controller.Settings.map(setting => setting.InitialIndex), Array(12).fill(-1));
assert.deepStrictEqual(controller.Settings.map(setting => setting.DraftIndex), Array(12).fill(-1));
assert.strictEqual(controller.GetDetailLevelDraftIndex(), 4);
assert.strictEqual(controller.CanEditDetailLevel(), false);
assert.strictEqual(controller.CanApply(), false);
assert.strictEqual(controller.CanEdit(3), false);
for (const unavailableRowIndex of [0, 2, 3, 5, 6, 7, 8, 9, 10, 11, 12]) {
    const unavailableRow = loadRow(rowScripts[unavailableRowIndex], environment.screen);
    assert.strictEqual(unavailableRow.ItemText.text, unavailableRowIndex === 0 ? 'Read failed' : 'Unavailable', 'failed scalar row must render an explicit unavailable value');
    assert.strictEqual(unavailableRow.LeftClicker._visible, false);
    assert.strictEqual(unavailableRow.RightClicker._visible, false);
}
assert.strictEqual(environment.screen.bBlockInput, false);
assert.strictEqual(environment.screen.bLockInput, false);
assert.strictEqual(environment.screen.TryBack(), true);
assert.strictEqual(environment.screen.returnFromScreenCount, 1);

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
assert.strictEqual(controller.InitializationFailed, false, 'timer rollover timeout isolates the current setting');
assert.strictEqual(controller.CurrentPendingCode, 4200, 'initialization continues with VSync after the first timeout');
assert.deepStrictEqual(controller.Settings.map(setting => setting.DraftIndex), Array(12).fill(-1));

setNow(0);
clearCalls();
environment = makeEnvironment();
controller = environment.controller;
initialize(controller, Array(12).fill(0));
controller.IncrementSetting(3);
assert.strictEqual(controller.Settings[1].DraftIndex, 1);
assert.strictEqual(settingSignals().length, 32);
controller.DecrementSetting(3);
controller.DecrementSetting(3);
assert.strictEqual(controller.Settings[1].DraftIndex, 0);
for (let index = 0; index < 5; index += 1) {
    controller.ToggleSetting(4);
}
assert.strictEqual(controller.Settings[2].DraftIndex, 0);
controller.ToggleSetting(4);
controller.IncrementSetting(4);
assert.strictEqual(controller.Settings[2].DraftIndex, 2);
assert.strictEqual(controller.CanApply(), true);

controller.SetDetailPreset(0, true);
assert.strictEqual(controller.GetDetailLevelDraftIndex(), 0);
controller.SetDetailPreset(1, true);
assert.deepStrictEqual(controller.Settings.slice(3, 10).map(setting => setting.DraftIndex), [1, 1, 0, 0, 0, 0, 0]);
assert.strictEqual(controller.GetDetailLevelDraftIndex(), 1);
controller.SetDetailPreset(2, true);
assert.deepStrictEqual(controller.Settings.slice(3, 10).map(setting => setting.DraftIndex), [1, 1, 1, 1, 1, 1, 0]);
assert.strictEqual(controller.GetDetailLevelDraftIndex(), 2);
controller.SetDetailPreset(3, true);
assert.deepStrictEqual(controller.Settings.slice(3, 10).map(setting => setting.DraftIndex), [1, 1, 1, 1, 1, 1, 1]);
assert.strictEqual(controller.GetDetailLevelDraftIndex(), 3);
controller.IncrementDetailPreset();
assert.strictEqual(controller.GetDetailLevelDraftIndex(), 3);
controller.DecrementDetailPreset();
assert.strictEqual(controller.GetDetailLevelDraftIndex(), 2);
controller.SetDetailPreset(3, true);
controller.DecrementSetting(9);
assert.strictEqual(controller.GetDetailLevelDraftIndex(), 4);
controller.IncrementDetailPreset();
assert.strictEqual(controller.GetDetailLevelDraftIndex(), 4);
controller.DecrementDetailPreset();
assert.strictEqual(controller.GetDetailLevelDraftIndex(), 3);
controller.ToggleDetailPreset();
assert.strictEqual(controller.GetDetailLevelDraftIndex(), 0);
assert.strictEqual(settingSignals().length, 32);

setNow(0);
environment = makeEnvironment();
controller = environment.controller;
initialize(controller, Array(12).fill(0));
const detailRow = loadRow(rowScripts[4], environment.screen);
function assertDetailRowState(expectedText, expectedLeft, expectedRight) {
    detailRow.Update();
    assert.strictEqual(detailRow.ItemText.text, expectedText);
    assert.strictEqual(detailRow.LeftClicker._visible, expectedLeft);
    assert.strictEqual(detailRow.RightClicker._visible, expectedRight);
}
assertDetailRowState('Low', false, true);
clearCalls();
detailRow.Increment();
assert.deepStrictEqual(soundSignals(), ['UI_FrontEndSFX.UI_Forward']);
assertDetailRowState('Medium', true, true);
clearCalls();
detailRow.RunAction();
assert.deepStrictEqual(soundSignals(), ['UI_FrontEndSFX.UI_Forward']);
assertDetailRowState('High', true, true);
clearCalls();
detailRow.Increment();
assert.deepStrictEqual(soundSignals(), ['UI_FrontEndSFX.UI_Forward']);
assertDetailRowState('Very High', true, false);
clearCalls();
detailRow.Increment();
assert.deepStrictEqual(soundSignals(), []);
assertDetailRowState('Very High', true, false);
controller.DecrementSetting(9);
assertDetailRowState('Custom', true, false);
clearCalls();
detailRow.Increment();
assert.deepStrictEqual(soundSignals(), []);
assertDetailRowState('Custom', true, false);
clearCalls();
detailRow.Decrement();
assert.deepStrictEqual(soundSignals(), ['UI_FrontEndSFX.UI_Back']);
assertDetailRowState('Very High', true, false);
clearCalls();
detailRow.RunAction();
assert.deepStrictEqual(soundSignals(), ['UI_FrontEndSFX.UI_Forward']);
assertDetailRowState('Low', false, true);
clearCalls();
detailRow.Decrement();
assert.deepStrictEqual(soundSignals(), []);
assertDetailRowState('Low', false, true);

setNow(0);
environment = makeEnvironment();
controller = environment.controller;
initialize(controller, Array(12).fill(0));
clearCalls();
controller.ToggleSetting(1);
controller.IncrementSetting(6);
assert.strictEqual(controller.Settings[3].DraftIndex, 1);
assert.strictEqual(environment.screen.TryBack(), true);
assert.strictEqual(environment.screen.outTransitionStarted, true);
assert.strictEqual(environment.screen.returnFromScreenCount, 1);
assert.strictEqual(controller.ApplyInProgress, false);
assert.deepStrictEqual(controller.Settings.map(setting => setting.DraftIndex), controller.Settings.map(setting => setting.InitialIndex));
assert.deepStrictEqual(settingSignals(), []);
assert.strictEqual(calls.some(call => call.name === 'FE_SetControlType' && [4673, 4674, 4990, 4991, 4970, 4971].includes(call.args[0])), false);

setNow(0);
clearCalls();
environment = makeEnvironment();
controller = environment.controller;
initialize(controller, Array(12).fill(0));
controller.SetDetailPreset(2, true);
controller.ApplyChanges();
assert.strictEqual(environment.screen.bBlockInput, true);
assert.strictEqual(environment.screen.bLockInput, true);
assert.deepStrictEqual(environment.screen.blockStates.slice(-1), [true]);
assert.strictEqual(environment.screen.TryBack(), false);
assert.strictEqual(environment.screen.returnFromScreenCount, 0);
assert.deepStrictEqual(settingSignals().slice(-1), [4604]);
queueResponses(4606); controller.Tick();
assert.deepStrictEqual(settingSignals().slice(-1), [4614]);
queueResponses(4616); controller.Tick();
assert.deepStrictEqual(settingSignals().slice(-1), [4624]);
queueResponses(4626); controller.Tick();
assert.deepStrictEqual(settingSignals().slice(-1), [4634]);
queueResponses(4636); controller.Tick();
assert.deepStrictEqual(settingSignals().slice(-1), [4644]);
queueResponses(4646); controller.Tick();
assert.deepStrictEqual(settingSignals().slice(-1), [4654]);
queueResponses(4656); controller.Tick();
const commitSignal = settingSignals()[settingSignals().length - 1];
assert.strictEqual(commitSignal, 4991);
assert.deepStrictEqual(controller.Settings.map(setting => setting.InitialIndex), Array(12).fill(0));
queueResponses(4981);
controller.Tick();
assert.deepStrictEqual(controller.Settings.map(setting => setting.InitialIndex), [0, 0, 0, 1, 1, 1, 1, 1, 1, 0, 0, 0]);
assert.strictEqual(controller.GetDetailLevelInitialIndex(), 2);
assert.strictEqual(controller.GetApplyStatusText(), '');
assert.strictEqual(controller.CanApply(), false);
assert.strictEqual(environment.screen.bBlockInput, false);
assert.strictEqual(environment.screen.bLockInput, false);
assert.deepStrictEqual(environment.screen.blockStates.slice(-1), [false]);

controller.ToggleSetting(3);
controller.ApplyChanges();
assert.deepStrictEqual(settingSignals().slice(-1), [4221]);
queueResponses(4231);
controller.Tick();
const secondCommitSignal = settingSignals()[settingSignals().length - 1];
assert.strictEqual(secondCommitSignal, 4990);
queueResponses(4980);
controller.Tick();
assert.deepStrictEqual(controller.Settings.map(setting => setting.InitialIndex), [0, 1, 0, 1, 1, 1, 1, 1, 1, 0, 0, 0]);
assert.strictEqual(environment.screen.bBlockInput, false);
assert.strictEqual(environment.screen.bLockInput, false);
assert.strictEqual(environment.screen.onKeyDown(), true);
assert.strictEqual(environment.screen.returnFromScreenCount, 1);

function applyAndReachSettingFailure(failureResponse) {
    setNow(0);
    clearCalls();
    const failedEnvironment = makeEnvironment();
    const failedController = failedEnvironment.controller;
    initialize(failedController, Array(12).fill(0));
    failedController.ToggleSetting(3);
    failedController.ApplyChanges();
    queueResponses(failureResponse);
    failedController.Tick();
    const rollbackSignal = settingSignals()[settingSignals().length - 1];
    assert.ok(rollbackSignal === 4970 || rollbackSignal === 4971);
    return { controller: failedController, screen: failedEnvironment.screen, rollbackSignal };
}

let failureEnvironment = applyAndReachSettingFailure(4299);
queueResponses(failureEnvironment.rollbackSignal === 4970 ? 4960 : 4961);
failureEnvironment.controller.Tick();
assert.strictEqual(failureEnvironment.controller.GetApplyStatusText(), 'Apply Failed');
assert.strictEqual(failureEnvironment.controller.Settings[1].DraftIndex, 1);
assert.strictEqual(failureEnvironment.controller.Settings[1].InitialIndex, 0);
assert.strictEqual(failureEnvironment.controller.CanApply(), true);
assert.strictEqual(failureEnvironment.screen.bBlockInput, false);
assert.strictEqual(failureEnvironment.screen.bLockInput, false);
assert.strictEqual(failureEnvironment.screen.TryBack(), true);

setNow(0);
clearCalls();
environment = makeEnvironment();
controller = environment.controller;
initialize(controller, Array(12).fill(0));
controller.ToggleSetting(3);
controller.ApplyChanges();
queueResponses(4299);
controller.Tick();
assert.strictEqual(settingSignals()[settingSignals().length - 1], 4971);
queueResponses(4961);
controller.Tick();
assert.strictEqual(controller.GetApplyStatusText(), 'Apply Failed');
controller.ToggleSetting(4);
controller.ApplyChanges();
assert.strictEqual(settingSignals()[settingSignals().length - 1], 4221);
queueResponses(4231);
controller.Tick();
queueResponses(4399);
controller.Tick();
assert.strictEqual(settingSignals()[settingSignals().length - 1], 4970);
queueResponses(4960);
controller.Tick();
assert.strictEqual(controller.GetApplyStatusText(), 'Apply Failed');
assert.strictEqual(environment.screen.bBlockInput, false);
assert.strictEqual(environment.screen.bLockInput, false);
assert.strictEqual(environment.screen.TryBack(), true);

failureEnvironment = applyAndReachSettingFailure(4299);
setNow(failureEnvironment.controller.CurrentPendingDeadline);
failureEnvironment.controller.Tick();
assert.strictEqual(settingSignals()[settingSignals().length - 1] === 4970 || settingSignals()[settingSignals().length - 1] === 4971, true);

setNow(0);
clearCalls();
environment = makeEnvironment();
controller = environment.controller;
initialize(controller, Array(12).fill(0));
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
assert.strictEqual(environment.screen.bBlockInput, false);
assert.strictEqual(environment.screen.bLockInput, false);
assert.strictEqual(environment.screen.TryBack(), true);
assert.strictEqual(controller.Settings[1].DraftIndex, 0);
assert.strictEqual(controller.Settings[1].InitialIndex, 0);

setNow(0);
clearCalls();
environment = makeEnvironment();
controller = environment.controller;
initialize(controller, Array(12).fill(0));
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
assert.strictEqual(environment.screen.bBlockInput, false);
assert.strictEqual(environment.screen.bLockInput, false);
assert.strictEqual(environment.screen.TryBack(), true);

failureEnvironment = applyAndReachSettingFailure(4299);
const rollbackFailureInitial = failureEnvironment.controller.Settings.map(setting => setting.InitialIndex);
const rollbackFailureDraft = failureEnvironment.controller.Settings.map(setting => setting.DraftIndex);
queueResponses(4969);
failureEnvironment.controller.Tick();
assert.strictEqual(failureEnvironment.controller.GetApplyStatusText(), 'Rollback Failed');
assert.strictEqual(failureEnvironment.controller.RollbackLocked, true);
assert.strictEqual(failureEnvironment.controller.CanApply(), false);
assert.strictEqual(failureEnvironment.controller.CanEdit(3), false);
assert.deepStrictEqual(failureEnvironment.controller.Settings.map(setting => setting.InitialIndex), rollbackFailureInitial);
assert.deepStrictEqual(failureEnvironment.controller.Settings.map(setting => setting.DraftIndex), rollbackFailureDraft);
assert.strictEqual(failureEnvironment.screen.bBlockInput, false);
assert.strictEqual(failureEnvironment.screen.bLockInput, false);
assert.strictEqual(failureEnvironment.screen.TryBack(), true);

failureEnvironment = applyAndReachSettingFailure(4299);
const rollbackTimeoutInitial = failureEnvironment.controller.Settings.map(setting => setting.InitialIndex);
const rollbackTimeoutDraft = failureEnvironment.controller.Settings.map(setting => setting.DraftIndex);
setNow(failureEnvironment.controller.CurrentPendingDeadline);
failureEnvironment.controller.Tick();
assert.strictEqual(failureEnvironment.controller.GetApplyStatusText(), 'Rollback Failed');
assert.strictEqual(failureEnvironment.controller.RollbackLocked, true);
assert.deepStrictEqual(failureEnvironment.controller.Settings.map(setting => setting.InitialIndex), rollbackTimeoutInitial);
assert.deepStrictEqual(failureEnvironment.controller.Settings.map(setting => setting.DraftIndex), rollbackTimeoutDraft);
assert.strictEqual(failureEnvironment.screen.bBlockInput, false);
assert.strictEqual(failureEnvironment.screen.bLockInput, false);
assert.strictEqual(failureEnvironment.screen.TryBack(), true);

const lifecycleParent = { GraphicsOptionsController: undefined, outTransitionStarted: false, returnFromScreenCount: 0 };
const guardedActiveRow = loadRow(rowScripts[2], lifecycleParent);
expectNoThrow(() => guardedActiveRow.Update(), 'active row update before controller assignment');
expectNoThrow(() => guardedActiveRow.RunAction(), 'active row action before controller assignment');
expectNoThrow(() => guardedActiveRow.Increment(), 'active row increment before controller assignment');
expectNoThrow(() => guardedActiveRow.Decrement(), 'active row decrement before controller assignment');
assert.strictEqual(guardedActiveRow.ItemText.text, 'Loading...');
assert.strictEqual(guardedActiveRow.Label.Label.Text.text, 'VSync');
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

const partialEnvironment = makeEnvironment();
global._parent = partialEnvironment.screen;
const partialActiveRow = { _parent: partialEnvironment.screen, ItemText: { text: '' } };
const partialActiveLoad = new Function(rowScripts[2].slice(rowScripts[2].indexOf('{') + 1, rowScripts[2].lastIndexOf('}')));
expectNoThrow(() => partialActiveLoad.call(partialActiveRow), 'partial active row load with controller initializing');
const partialApplyRow = { _parent: partialEnvironment.screen, ItemText: { text: '', _alpha: 0 } };
const partialApplyLoad = new Function(rowScripts[14].slice(rowScripts[14].indexOf('{') + 1, rowScripts[14].lastIndexOf('}')));
expectNoThrow(() => partialApplyLoad.call(partialApplyRow), 'partial apply row load with controller initializing');
const activeMissingItemText = loadRow(rowScripts[2], partialEnvironment.screen, { ItemText: undefined });
const activeMissingLabel = loadRow(rowScripts[2], partialEnvironment.screen, { Label: undefined });
const activeMissingLeftClicker = loadRow(rowScripts[2], partialEnvironment.screen, { LeftClicker: undefined });
const activeMissingRightClicker = loadRow(rowScripts[2], partialEnvironment.screen, { RightClicker: undefined });
const applyMissingItemText = loadRow(rowScripts[14], partialEnvironment.screen, { ItemText: undefined });
const applyMissingLabel = loadRow(rowScripts[14], partialEnvironment.screen, { Label: undefined });
const applyMissingLeftClicker = loadRow(rowScripts[14], partialEnvironment.screen, { LeftClicker: undefined });
const applyMissingRightClicker = loadRow(rowScripts[14], partialEnvironment.screen, { RightClicker: undefined });
assert.strictEqual(activeMissingItemText.LeftClicker._visible, false);
assert.strictEqual(activeMissingItemText.RightClicker._visible, false);
assert.strictEqual(activeMissingLabel.ItemText.text, 'Loading...');
assert.strictEqual(activeMissingLabel.LeftClicker._visible, false);
assert.strictEqual(activeMissingLabel.RightClicker._visible, false);
assert.strictEqual(applyMissingItemText.Label._alpha, 40);
assert.strictEqual(applyMissingLabel.ItemText.text, '');
assert.strictEqual(applyMissingLabel.ItemText._alpha, 40);
initialize(partialEnvironment.controller, Array(12).fill(0));
expectNoThrow(() => partialActiveRow.Update(), 'partial active row update after controller initialization');
expectNoThrow(() => partialApplyRow.Update(), 'partial apply row update after controller initialization');
expectNoThrow(() => activeMissingItemText.Update(), 'active row update without ItemText');
expectNoThrow(() => activeMissingLabel.Update(), 'active row update without Label');
expectNoThrow(() => activeMissingLeftClicker.Update(), 'active row update without LeftClicker');
expectNoThrow(() => activeMissingRightClicker.Update(), 'active row update without RightClicker');
expectNoThrow(() => applyMissingItemText.Update(), 'apply row update without ItemText');
expectNoThrow(() => applyMissingLabel.Update(), 'apply row update without Label');
expectNoThrow(() => applyMissingLeftClicker.Update(), 'apply row update without LeftClicker');
expectNoThrow(() => applyMissingRightClicker.Update(), 'apply row update without RightClicker');
assert.strictEqual(activeMissingItemText.Label.Label.Text.text, 'VSync');
assert.strictEqual(activeMissingItemText.LeftClicker._visible, false);
assert.strictEqual(activeMissingItemText.RightClicker._visible, true);
assert.strictEqual(activeMissingLabel.ItemText.text, 'Off');
assert.strictEqual(activeMissingLabel.LeftClicker._visible, false);
assert.strictEqual(activeMissingLabel.RightClicker._visible, true);
assert.strictEqual(activeMissingLeftClicker.ItemText.text, 'Off');
assert.strictEqual(activeMissingLeftClicker.RightClicker._visible, true);
assert.strictEqual(activeMissingRightClicker.ItemText.text, 'Off');
assert.strictEqual(activeMissingRightClicker.LeftClicker._visible, false);
assert.strictEqual(applyMissingItemText.Label.Label.Text.text, 'Apply Changes');
assert.strictEqual(applyMissingItemText.Label._alpha, 40);
assert.strictEqual(applyMissingLabel.ItemText.text, '');
assert.strictEqual(applyMissingLabel.ItemText._alpha, 40);
assert.strictEqual(applyMissingLeftClicker.ItemText.text, '');
assert.strictEqual(applyMissingLeftClicker.Label._alpha, 40);
assert.strictEqual(applyMissingRightClicker.ItemText.text, '');
assert.strictEqual(applyMissingRightClicker.Label._alpha, 40);

currentScreen = lifecycleParent;
expectNoThrow(() => new Function(cancelBody).call(lifecycleParent), 'CancelScreen before controller assignment');
const lifecycleEnvironment = makeEnvironment();
initialize(lifecycleEnvironment.controller, Array(12).fill(0));
lifecycleEnvironment.controller.ToggleSetting(3);
lifecycleEnvironment.controller.ApplyChanges();
assert.strictEqual(lifecycleEnvironment.screen.bBlockInput, true);
assert.strictEqual(lifecycleEnvironment.screen.bLockInput, true);
assert.strictEqual(lifecycleEnvironment.screen.TryBack(), false);
assert.strictEqual(lifecycleEnvironment.screen.returnFromScreenCount, 0);
const callsBeforeCancel = calls.length;
expectNoThrow(() => new Function(cancelBody).call(lifecycleEnvironment.screen), 'CancelScreen with controller');
assert.strictEqual(lifecycleEnvironment.screen.bBlockInput, false);
assert.strictEqual(lifecycleEnvironment.screen.bLockInput, false);
assert.strictEqual(lifecycleEnvironment.screen.outTransitionStarted, true);
assert.strictEqual(lifecycleEnvironment.screen.returnFromScreenCount, 1);
expectNoThrow(() => lifecycleEnvironment.screen.onEnterFrame(), 'inherited onEnterFrame during exit transition');
assert.strictEqual(calls.length, callsBeforeCancel);
const tickHook = new Function('return function(){' + tickBody + '}')();
const tickScreen = { GraphicsOptionsController: undefined, onEnterFrame() { this.Tick(); } };
tickScreen.Tick = tickHook;
expectNoThrow(() => tickScreen.onEnterFrame(), 'Tick hook before controller assignment');
tickScreen.GraphicsOptionsController = { Tick() { this.called = true; } };
expectNoThrow(() => tickScreen.onEnterFrame(), 'Tick hook through inherited onEnterFrame');
assert.strictEqual(tickScreen.GraphicsOptionsController.called, true);
const lockedBackEnvironment = makeEnvironment();
lockedBackEnvironment.screen.bLockInput = true;
assert.strictEqual(lockedBackEnvironment.screen.TryBack(), false);
assert.strictEqual(lockedBackEnvironment.screen.returnFromScreenCount, 0);
lockedBackEnvironment.screen.bLockInput = false;
assert.strictEqual(lockedBackEnvironment.screen.onKeyDown(), true);
assert.strictEqual(lockedBackEnvironment.screen.returnFromScreenCount, 1);

function initializeCorrected(values, options) {
    const environment = makeEnvironment();
    const controller = environment.controller;
    const settingsValues = values || Array(12).fill(0);
    const catalogOptions = options || {};
    controller.BeginInitialization();
    assert.deepStrictEqual(settingSignals(), [4670]);
    queueResponses(4670);
    controller.Tick();
    queueResponses(4671 + settingsValues[0]);
    controller.Tick();
    const staticRequests = [4200, 4300, 4600, 4610, 4620, 4630, 4640, 4650, 4660, 4400, 4500];
    const staticBases = [4210, 4310, 4601, 4611, 4621, 4631, 4641, 4651, 4661, 4410, 4510];
    for (let index = 0; index < staticRequests.length; index += 1) {
        if (catalogOptions.failLegacyIndex === index + 1) queueResponses(4999);
        else queueResponses(staticBases[index] + settingsValues[index + 1]);
        controller.Tick();
    }
    if (catalogOptions.failWindowed) { queueResponses(4899); controller.Tick(); }
    else { feedCatalog(controller, 5200, [[1280, 720], [1920, 1080], [2560, 1600]], catalogOptions.windowedWidth || 2560, catalogOptions.windowedHeight || 1600); }
    if (catalogOptions.failFullscreen) { queueResponses(4899); controller.Tick(); }
    else { feedCatalog(controller, 5400, [[1280, 720], [1920, 1080]], catalogOptions.fullscreenWidth || 1920, catalogOptions.fullscreenHeight || 1080); }
    queueResponses(encodeResolutionScalar(5600, 1920)); controller.Tick();
    queueResponses(encodeResolutionScalar(5601, 1080)); controller.Tick();
    if (!controller.InitializationComplete) {
        throw new Error(`corrected initialization incomplete: stage=${controller.ResolutionInitializationStage} request=${controller.ResolutionCatalogRequest} pending=${controller.CurrentPendingCode} kind=${controller.ResolutionCatalogKind} windowed=${controller.WindowedResolutionModes.length}/${controller.WindowedResolutionAvailable} fullscreen=${controller.FullscreenResolutionModes.length}/${controller.FullscreenResolutionAvailable} desktop=${controller.DesktopActualWidth}x${controller.DesktopActualHeight}`);
    }
    assert.strictEqual(controller.InitializationFailed, false);
    return environment;
}

let corrected = initializeCorrected();
assert.strictEqual(corrected.controller.ResolutionInitialWidth, 2560, 'windowed startup keeps the exact custom width');
assert.strictEqual(corrected.controller.ResolutionInitialHeight, 1600, 'windowed startup keeps the exact custom height');
assert.strictEqual(corrected.controller.GetResolutionLabel(), '2560 x 1600', 'custom window size is visible');

corrected.controller.ResolutionDraftIndex = 1;
corrected.controller.ResolutionDraftWidth = corrected.controller.ResolutionInitialWidth;
corrected.controller.ResolutionDraftHeight = corrected.controller.ResolutionInitialHeight;
assert.strictEqual(corrected.controller.IsDirty(), false, 'resolution dirty state compares dimensions, not catalog indices');

corrected = initializeCorrected();
corrected.controller.ToggleSetting(1);
assert.strictEqual(corrected.controller.Settings[0].DraftIndex, 1);
assert.strictEqual(corrected.controller.GetResolutionLabel(), '1920 x 1080', 'fullscreen switch selects the remembered supported pair');
corrected.controller.ToggleSetting(1);
assert.strictEqual(corrected.controller.GetResolutionLabel(), '2560 x 1600', 'windowed switch restores the remembered custom pair');

corrected = initializeCorrected(Array(12).fill(0), { failWindowed: true });
corrected.controller.Settings[1].DraftIndex = 1;
assert.strictEqual(corrected.controller.InitializationComplete, true, 'windowed catalog failure does not abort legacy initialization');
assert.strictEqual(corrected.controller.CanApply(), true, 'legacy settings remain applicable after a new-row failure');
corrected.controller.ApplyChanges();
assert.strictEqual(settingSignals()[settingSignals().length - 1], 4221, 'legacy Apply excludes unavailable resolution');

corrected = initializeCorrected();
corrected.controller.ToggleSetting(1);
corrected.controller.IncrementResolution();
corrected.controller.ApplyChanges();
assert.strictEqual(settingSignals()[settingSignals().length - 1], 4674, 'Fullscreen is queued before resolution');
queueResponses(4676); corrected.controller.Tick();
assert.strictEqual(settingSignals()[settingSignals().length - 1], 5001, 'resolution uses the selected destination index');
queueResponses(5101); corrected.controller.Tick();
queueResponses(4989); corrected.controller.Tick();
const rollbackSignal = settingSignals()[settingSignals().length - 1];
queueResponses(rollbackSignal === 4970 ? 4960 : 4961); corrected.controller.Tick();
assert.strictEqual(corrected.controller.Settings[0].DraftIndex, 0, 'commit failure restores the original mode');
assert.strictEqual(corrected.controller.ResolutionDraftWidth, 2560, 'commit failure restores the exact original width');
assert.strictEqual(corrected.controller.ResolutionDraftHeight, 1600, 'commit failure restores the exact original height');

setNow(0);
const progressiveEnvironment = makeEnvironment();
const progressiveController = progressiveEnvironment.controller;
const progressiveVsyncRow = loadRow(rowScripts[2], progressiveEnvironment.screen);
progressiveEnvironment.screen.GraphicsRow3 = progressiveVsyncRow;
const progressiveResolutionRow = loadRow(rowScripts[1], progressiveEnvironment.screen);
progressiveEnvironment.screen.GraphicsRow2 = progressiveResolutionRow;
const progressiveDetailRow = loadRow(rowScripts[4], progressiveEnvironment.screen);
progressiveEnvironment.screen.GraphicsRow5 = progressiveDetailRow;
progressiveController.BeginInitialization();
queueResponses(4671); progressiveController.Tick();
queueResponses(4210); progressiveController.Tick();
assert.strictEqual(progressiveVsyncRow.ItemText.text, 'Off', 'VSync must appear when read, without waiting for resolution catalogs');
assert.strictEqual(progressiveResolutionRow.ItemText.text, 'Loading...');
for (const response of [4310, 4601, 4611, 4621, 4631, 4641, 4651, 4661, 4410, 4510]) {
    queueResponses(response); progressiveController.Tick();
}
assert.strictEqual(progressiveController.CurrentPendingCode, 5200, 'all scalar settings precede the deferred catalogs');
assert.strictEqual(progressiveController.InitializationComplete, false);
assert.strictEqual(progressiveController.CanEdit(3), true, 'loaded scalar drafts can be edited while resolutions load');
assert.strictEqual(progressiveController.CanEditDetailLevel(), true);
assert.strictEqual(progressiveDetailRow.ItemText.text, 'Low', 'the composite detail row appears before resolutions finish');
assert.strictEqual(progressiveController.CanEdit(1), false, 'mode switching waits for complete catalogs');
assert.strictEqual(progressiveController.CanEditResolution(), false);
assert.strictEqual(progressiveEnvironment.screen.bBlockInput, false);
progressiveController.ToggleSetting(3);
assert.strictEqual(progressiveVsyncRow.ItemText.text, 'On');
const pendingSignals = settingSignals().slice();
assert.strictEqual(progressiveController.CanApply(), false, 'Apply cannot interrupt the catalog transport');
progressiveController.ApplyChanges();
assert.deepStrictEqual(settingSignals(), pendingSignals);
feedCatalog(progressiveController, 5200, [[1280, 720], [2560, 1600]], 2560, 1600);
feedCatalog(progressiveController, 5400, [[1920, 1080]], 2560, 1600);
queueResponses(encodeResolutionScalar(5600, 1920)); progressiveController.Tick();
queueResponses(encodeResolutionScalar(5601, 1080)); progressiveController.Tick();
assert.strictEqual(progressiveController.Settings[1].DraftIndex, 1, 'catalog completion preserves edits made during loading');
assert.strictEqual(progressiveController.CanApply(), true);
assert.strictEqual(progressiveResolutionRow.ItemText.text, '2560 x 1600');
progressiveController.ApplyChanges();
assert.strictEqual(settingSignals()[settingSignals().length - 1], 4221, 'the pending scalar edit uses the unchanged Apply protocol');
queueResponses(4231); progressiveController.Tick();
queueResponses(4981); progressiveController.Tick();
assert.strictEqual(progressiveController.Settings[1].InitialIndex, 1);
assert.strictEqual(progressiveController.CanApply(), false);

for (const cancelDuringLoad of [false, true]) {
    setNow(0);
    const deferredEnvironment = makeEnvironment();
    const deferredController = deferredEnvironment.controller;
    deferredController.BeginInitialization();
    for (const response of [4671, 4210, 4310, 4601, 4611, 4621, 4631, 4641, 4651, 4661, 4410, 4510]) {
        queueResponses(response); deferredController.Tick();
    }
    deferredController.ToggleSetting(3);
    if (cancelDuringLoad) {
        const beforeBack = settingSignals().slice();
        assert.strictEqual(deferredEnvironment.screen.TryBack(), true, 'Back remains available while catalogs load');
        assert.deepStrictEqual(settingSignals(), beforeBack, 'Back must not write a pending draft');
        assert.strictEqual(deferredController.Settings[1].DraftIndex, 0);
        assert.strictEqual(deferredController.CurrentPendingOperation, '');
    } else {
        for (let phase = 0; phase < 3; phase += 1) {
            setNow(deferredController.ResolutionCatalogDeadline);
            deferredController.Tick();
        }
        assert.strictEqual(deferredController.InitializationComplete, true);
        assert.strictEqual(deferredController.Settings[1].DraftIndex, 1, 'catalog timeouts preserve the loaded scalar edit');
        assert.strictEqual(deferredController.CanEdit(3), true);
        assert.strictEqual(deferredController.CanApply(), true);
        assert.strictEqual(deferredController.IsUnavailable(2), true);
    }
}

setNow(0);
const mouseEnvironment = makeEnvironment();
initialize(mouseEnvironment.controller, Array(12).fill(0));
const mouseMsaaRow = loadRow(rowScripts[3], mouseEnvironment.screen);
mouseMsaaRow._xmouse = 100;
mouseMsaaRow.RunAction(true);
assert.strictEqual(mouseEnvironment.controller.GetDraftIndex(4), 1, 'right mouse click increments MSAA');
mouseMsaaRow._xmouse = -100;
mouseMsaaRow.RunAction(true);
assert.strictEqual(mouseEnvironment.controller.GetDraftIndex(4), 0, 'left mouse click decrements MSAA rather than cycling forward');
mouseMsaaRow.RunAction(true);
assert.strictEqual(mouseEnvironment.controller.GetDraftIndex(4), 0, 'left mouse click clamps at the minimum');
mouseMsaaRow.RunAction(false);
assert.strictEqual(mouseEnvironment.controller.GetDraftIndex(4), 1, 'keyboard activation still cycles regardless of mouse position');
const mouseBooleanRow = loadRow(rowScripts[2], mouseEnvironment.screen);
mouseBooleanRow._xmouse = -100;
mouseBooleanRow.RunAction(true);
assert.strictEqual(mouseEnvironment.controller.GetDraftIndex(3), 0, 'left on Off must not toggle On');
mouseBooleanRow._xmouse = 100;
mouseBooleanRow.RunAction(true);
mouseBooleanRow.RunAction(true);
assert.strictEqual(mouseEnvironment.controller.GetDraftIndex(3), 1, 'right on On stays at the maximum');
mouseBooleanRow._xmouse = -100;
mouseBooleanRow.RunAction(true);
assert.strictEqual(mouseEnvironment.controller.GetDraftIndex(3), 0);
const mouseDetailRow = loadRow(rowScripts[4], mouseEnvironment.screen);
mouseDetailRow._xmouse = 100;
mouseDetailRow.RunAction(true);
mouseDetailRow._xmouse = -100;
mouseDetailRow.RunAction(true);
assert.strictEqual(mouseEnvironment.controller.GetDetailLevelDraftIndex(), 0, 'detail preset left mouse click decrements');
const mouseResolutionRow = loadRow(rowScripts[1], mouseEnvironment.screen);
mouseResolutionRow._xmouse = -100;
mouseResolutionRow.RunAction(true);
assert.strictEqual(mouseEnvironment.controller.GetResolutionDraftIndex(), 0, 'resolution left mouse click decrements');
mouseResolutionRow._xmouse = 100;
mouseResolutionRow.RunAction(true);
assert.strictEqual(mouseEnvironment.controller.GetResolutionDraftIndex(), 1);

for (const firstReadCase of [
    { reply: 4679, expected: 'Read failed' },
    { reply: 4210, expected: 'Reply 4210' },
    { reply: -19597424, expected: 'Reply -19597424' },
    { reply: 4670, expected: 'Timeout', timeout: true }
]) {
    setNow(0);
    const diagnosticEnvironment = makeEnvironment();
    const diagnosticController = diagnosticEnvironment.controller;
    const diagnosticRow = loadRow(rowScripts[0], diagnosticEnvironment.screen);
    diagnosticEnvironment.screen.GraphicsRow1 = diagnosticRow;
    diagnosticController.BeginInitialization();
    queueResponses(firstReadCase.reply); diagnosticController.Tick();
    if (firstReadCase.timeout) {
        assert.strictEqual(diagnosticController.CurrentPendingCode, 4670, 'a pending echo is not an invalid reply');
        setNow(diagnosticController.CurrentPendingDeadline); diagnosticController.Tick();
    }
    assert.strictEqual(diagnosticController.CurrentPendingCode, 4200, 'diagnostics must not alter the request sequence');
    for (const response of [4210, 4310, 4601, 4611, 4621, 4631, 4641, 4651, 4661, 4410, 4510]) {
        queueResponses(response); diagnosticController.Tick();
    }
    assert.strictEqual(diagnosticRow.ItemText.text, firstReadCase.expected, 'Fullscreen must expose the first-read failure reason');
    assert.strictEqual(diagnosticController.CanEdit(1), false);
    assert.strictEqual(diagnosticController.CanEdit(3), true, 'first-read diagnostics must not block the loaded settings');
    diagnosticController.BeginInitialization();
    assert.strictEqual(diagnosticRow.ItemText.text, 'Loading...', 'a new attempt clears the diagnostic and stale first-row value');
}

console.log('STATE_MACHINE_PASS');
'@
$HarnessSource = $HarnessSource.Replace('__CONTROLLER_SOURCE__', $ControllerSource).Replace('__ROW_SCRIPTS__', $RowScriptsJson).Replace('__CANCEL_BODY__', $CancelBodyJson).Replace('__TICK_BODY__', $TickBodyJson)
$HarnessRoot = Join-Path ([System.IO.Path]::GetTempPath()) ('BatmanGraphicsShellStateMachine-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $HarnessRoot -Force | Out-Null
try {
    $HarnessPath = Join-Path $HarnessRoot 'state-machine.js'
    Set-Content -LiteralPath $HarnessPath -Value $HarnessSource -Encoding UTF8
    $HarnessErrorActionPreference = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try {
        $HarnessOutput = @(& $NodeCommand.Source $HarnessPath 2>&1)
    } finally {
        $ErrorActionPreference = $HarnessErrorActionPreference
    }
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
