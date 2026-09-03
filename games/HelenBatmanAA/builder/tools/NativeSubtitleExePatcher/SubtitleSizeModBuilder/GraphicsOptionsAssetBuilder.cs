using System.Diagnostics;
using System.Threading.Tasks;

namespace SubtitleSizeModBuilder;

/// <summary>
/// Builds the frontend graphics-options prototype asset by patching MainV2 XML and scripts.
/// </summary>
internal static class GraphicsOptionsAssetBuilder
{
    /// <summary>
    /// Fixed graphics row depths ordered from top-to-bottom visual layout.
    /// </summary>
    private static readonly int[] GraphicsRowDepths =
    [
        141,
        133,
        125,
        117,
        109,
        101,
        93,
        85,
        77,
        69,
        61,
        53,
        45,
        37,
        29
    ];

    /// <summary>
    /// Fixed graphics row clip actions ordered from row 1 at depth 141 to row 15 at depth 29.
    /// The active emission path intentionally reuses template-owned row scripts so the frame and clip contracts stay aligned.
    /// </summary>
    private static readonly string[] GraphicsRowClipActions =
    [
        GraphicsOptionsScriptTemplates.GraphicsRow1ClipAction,
        GraphicsOptionsScriptTemplates.GraphicsRow2ClipAction,
        GraphicsOptionsScriptTemplates.GraphicsRow3ClipAction,
        GraphicsOptionsScriptTemplates.GraphicsRow4ClipAction,
        GraphicsOptionsScriptTemplates.GraphicsRow5ClipAction,
        GraphicsOptionsScriptTemplates.GraphicsRow6ClipAction,
        GraphicsOptionsScriptTemplates.GraphicsRow7ClipAction,
        GraphicsOptionsScriptTemplates.GraphicsRow8ClipAction,
        GraphicsOptionsScriptTemplates.GraphicsRow9ClipAction,
        GraphicsOptionsScriptTemplates.GraphicsRow10ClipAction,
        GraphicsOptionsScriptTemplates.GraphicsRow11ClipAction,
        GraphicsOptionsScriptTemplates.GraphicsRow12ClipAction,
        GraphicsOptionsScriptTemplates.GraphicsRow13ClipAction,
        GraphicsOptionsScriptTemplates.GraphicsRow14ClipAction,
        GraphicsOptionsScriptTemplates.GraphicsRow15ClipAction
    ];

    /// <summary>
    /// Identifies the only extracted frontend scripts that the shell build replaces. Keeping this
    /// allow-list explicit prevents FFDec from reserializing untouched startup sprites while still
    /// providing every parent directory required by the shell's patched scripts.
    /// </summary>
    private static readonly string[] ShellPatchedScriptRelativePaths =
    [
        "ScreenOptionsGraphics.as",
        "ScreenOptionsAudio_2.as",
        "DefineSprite_333_ScreenOptionsMenu\\frame_1\\DoAction_2.as",
        "DefineSprite_333_ScreenOptionsMenu\\frame_1\\PlaceObject2_117_GenericButton_37\\CLIPACTIONRECORD onClipEvent(load).as",
        "DefineSprite_600_ScreenOptionsGraphics\\frame_1\\DoAction.as",
        "DefineSprite_600_ScreenOptionsGraphics\\frame_15\\DoAction.as",
        "DefineSprite_600_ScreenOptionsGraphics\\frame_1\\PlaceObject2_290_List_Template_141\\CLIPACTIONRECORD onClipEvent(load).as",
        "DefineSprite_600_ScreenOptionsGraphics\\frame_1\\PlaceObject2_290_List_Template_133\\CLIPACTIONRECORD onClipEvent(load).as",
        "DefineSprite_600_ScreenOptionsGraphics\\frame_1\\PlaceObject2_290_List_Template_125\\CLIPACTIONRECORD onClipEvent(load).as",
        "DefineSprite_600_ScreenOptionsGraphics\\frame_1\\PlaceObject2_290_List_Template_117\\CLIPACTIONRECORD onClipEvent(load).as",
        "DefineSprite_600_ScreenOptionsGraphics\\frame_1\\PlaceObject2_290_List_Template_109\\CLIPACTIONRECORD onClipEvent(load).as",
        "DefineSprite_600_ScreenOptionsGraphics\\frame_1\\PlaceObject2_290_List_Template_101\\CLIPACTIONRECORD onClipEvent(load).as",
        "DefineSprite_600_ScreenOptionsGraphics\\frame_1\\PlaceObject2_290_List_Template_93\\CLIPACTIONRECORD onClipEvent(load).as",
        "DefineSprite_600_ScreenOptionsGraphics\\frame_1\\PlaceObject2_290_List_Template_85\\CLIPACTIONRECORD onClipEvent(load).as",
        "DefineSprite_600_ScreenOptionsGraphics\\frame_1\\PlaceObject2_290_List_Template_77\\CLIPACTIONRECORD onClipEvent(load).as",
        "DefineSprite_600_ScreenOptionsGraphics\\frame_1\\PlaceObject2_290_List_Template_69\\CLIPACTIONRECORD onClipEvent(load).as",
        "DefineSprite_600_ScreenOptionsGraphics\\frame_1\\PlaceObject2_290_List_Template_61\\CLIPACTIONRECORD onClipEvent(load).as",
        "DefineSprite_600_ScreenOptionsGraphics\\frame_1\\PlaceObject2_290_List_Template_53\\CLIPACTIONRECORD onClipEvent(load).as",
        "DefineSprite_600_ScreenOptionsGraphics\\frame_1\\PlaceObject2_290_List_Template_45\\CLIPACTIONRECORD onClipEvent(load).as",
        "DefineSprite_600_ScreenOptionsGraphics\\frame_1\\PlaceObject2_290_List_Template_37\\CLIPACTIONRECORD onClipEvent(load).as",
        "DefineSprite_600_ScreenOptionsGraphics\\frame_1\\PlaceObject2_290_List_Template_29\\CLIPACTIONRECORD onClipEvent(load).as"
    ];

    /// <summary>
    /// Builds the graphics-options prototype frontend asset.
    /// </summary>
    /// <param name="paths">The resolved graphics build paths.</param>
    public static void Build(GraphicsOptionsBuildPaths paths)
    {
        ValidateInputs(paths);
        PrepareOutputDirectories(paths);
        BatmanGraphicsIniBootstrapSnapshot bootstrapSnapshot = BatmanGraphicsIniBootstrapLoader.Load(paths.BatmanUserIniPath);

        CopyDirectory(paths.FrontendScriptsPath, paths.FrontendWorkingScriptsPath);
        PatchFrontendScripts(paths.FrontendWorkingScriptsPath, bootstrapSnapshot);
        GraphicsOptionsXmlPatcher.Patch(paths.FrontendXmlPath, paths.FrontendPatchedXmlPath);

        RunProcess(paths.FfdecPath, "-xml2swf", paths.FrontendPatchedXmlPath, paths.FrontendStructuralGfxPath);
        RunProcess(paths.FfdecPath, "-importScript", paths.FrontendStructuralGfxPath, paths.FrontendOutputGfxPath, paths.FrontendWorkingScriptsPath);
    }

    /// <summary>
    /// Builds the graphics-options shell frontend with eleven transmitted settings and a derived
    /// Detail Level preset row. The shell stages the extracted script tree, replaces only the screen
    /// and row scripts needed by the stock navigation path, patches sprite 600 into the frontend XML,
    /// and imports that result.
    /// </summary>
    /// <param name="paths">The resolved shell build paths.</param>
    public static void BuildShell(GraphicsOptionsShellBuildPaths paths)
    {
        ValidateShellInputs(paths);
        PrepareOutputDirectories(paths.OutputDirectory, paths.TempDirectory);
        BatmanGraphicsIniBootstrapSnapshot bootstrapSnapshot = BatmanGraphicsIniBootstrapLoader.Load(paths.BatmanUserIniPath);

        PatchFrontendShellScripts(paths.FrontendWorkingScriptsPath, bootstrapSnapshot);
        ValidateShellPatchedScriptSet(paths.FrontendWorkingScriptsPath);
        GraphicsOptionsXmlPatcher.PatchShell(paths.FrontendXmlPath, paths.FrontendPatchedXmlPath);

        RunProcess(paths.FfdecPath, "-xml2swf", paths.FrontendPatchedXmlPath, paths.FrontendStructuralGfxPath);
        RunProcess(paths.FfdecPath, "-importScript", paths.FrontendStructuralGfxPath, paths.FrontendOutputGfxPath, paths.FrontendWorkingScriptsPath);
    }

    /// <summary>
    /// Recreates output directories so each build starts from a clean state.
    /// </summary>
    /// <param name="paths">The resolved graphics build paths.</param>
    private static void PrepareOutputDirectories(GraphicsOptionsBuildPaths paths)
    {
        PrepareOutputDirectories(paths.OutputDirectory, paths.TempDirectory);
    }

    /// <summary>
    /// Recreates a generated output directory and its temporary staging directory before a build.
    /// </summary>
    /// <param name="outputDirectory">The generated output directory to recreate.</param>
    /// <param name="tempDirectory">The temporary staging directory to create below the output.</param>
    private static void PrepareOutputDirectories(string outputDirectory, string tempDirectory)
    {
        RecreateDirectory(outputDirectory);
        Directory.CreateDirectory(tempDirectory);
    }

    /// <summary>
    /// Applies graphics-options script overrides to a staged frontend script tree.
    /// </summary>
    /// <param name="scriptsRoot">The staged writable frontend script root.</param>
    /// <param name="bootstrapSnapshot">The normalized graphics defaults injected into the frontend fallback reads.</param>
    private static void PatchFrontendScripts(string scriptsRoot, BatmanGraphicsIniBootstrapSnapshot bootstrapSnapshot)
    {
        WriteAllText(
            Path.Combine(scriptsRoot, "ScreenOptionsGraphics.as"),
            GraphicsOptionsScriptTemplates.ScreenOptionsGraphicsRegistration);

        WriteAllText(
            Path.Combine(scriptsRoot, "GraphicsExitPrompt.as"),
            GraphicsOptionsScriptTemplates.GraphicsExitPromptRegistration);

        WriteAllText(
            Path.Combine(scriptsRoot, "ScreenOptionsAudio_2.as"),
            GraphicsOptionsScriptTemplates.ScreenOptionsGraphicsRegistration);

        WriteAllText(
            Path.Combine(scriptsRoot, "YesNoPrompt_2.as"),
            GraphicsOptionsScriptTemplates.GraphicsExitPromptRegistration);

        WriteAllText(
            Path.Combine(scriptsRoot, "DefineSprite_333_ScreenOptionsMenu", "frame_1", "DoAction_2.as"),
            GraphicsOptionsScriptTemplates.OptionsMenuFrame1);

        WriteAllText(
            Path.Combine(
                scriptsRoot,
                "DefineSprite_333_ScreenOptionsMenu",
                "frame_1",
                "PlaceObject2_117_GenericButton_37",
                "CLIPACTIONRECORD onClipEvent(load).as"),
            GraphicsOptionsScriptTemplates.OptionsMenuGraphicsButtonClipAction);

        WriteAllText(
            Path.Combine(scriptsRoot, "DefineSprite_600_ScreenOptionsGraphics", "frame_1", "DoAction.as"),
            GraphicsOptionsScriptTemplates.CreateGraphicsOptionsFrame1(bootstrapSnapshot));

        WriteAllText(
            Path.Combine(scriptsRoot, "DefineSprite_600_ScreenOptionsGraphics", "frame_15", "DoAction.as"),
            GraphicsOptionsScriptTemplates.GraphicsOptionsFrame15);

        WriteGraphicsRowClipActions(scriptsRoot);

        WriteAllText(
            Path.Combine(scriptsRoot, "DefineSprite_601_GraphicsExitPrompt", "frame_1", "DoAction.as"),
            GraphicsOptionsScriptTemplates.GraphicsExitPromptFrame1);

        WriteAllText(
            Path.Combine(scriptsRoot, "DefineSprite_601_GraphicsExitPrompt", "frame_23", "DoAction.as"),
            GraphicsOptionsScriptTemplates.GraphicsExitPromptFrame23);

        WriteAllText(
            Path.Combine(
                scriptsRoot,
                "DefineSprite_601_GraphicsExitPrompt",
                "frame_1",
                "PlaceObject2_117_GenericButton_13",
                "CLIPACTIONRECORD onClipEvent(load).as"),
            GraphicsOptionsScriptTemplates.GraphicsExitPromptNoButton);

        WriteAllText(
            Path.Combine(
                scriptsRoot,
                "DefineSprite_601_GraphicsExitPrompt",
                "frame_1",
                "PlaceObject2_117_GenericButton_15",
                "CLIPACTIONRECORD onClipEvent(load).as"),
            GraphicsOptionsScriptTemplates.GraphicsExitPromptYesButton);
    }

    /// <summary>
    /// Writes only the stock-facing graphics shell scripts into a staged frontend script tree,
    /// injecting the normalized VSync bootstrap value into the focused controller.
    /// The duplicate ScreenOptionsAudio_2 registration is retained because the current FFDec
    /// import pipeline resolves the cloned screen class through both registration paths.
    /// </summary>
    /// <param name="scriptsRoot">The staged writable frontend script root.</param>
    /// <param name="bootstrapSnapshot">The normalized user settings used to initialize VSync.</param>
    private static void PatchFrontendShellScripts(string scriptsRoot, BatmanGraphicsIniBootstrapSnapshot bootstrapSnapshot)
    {
        WriteAllText(
            Path.Combine(scriptsRoot, "ScreenOptionsGraphics.as"),
            GraphicsOptionsShellScriptTemplates.ScreenRegistration);

        WriteAllText(
            Path.Combine(scriptsRoot, "ScreenOptionsAudio_2.as"),
            GraphicsOptionsShellScriptTemplates.ScreenRegistration);

        WriteAllText(
            Path.Combine(scriptsRoot, "DefineSprite_333_ScreenOptionsMenu", "frame_1", "DoAction_2.as"),
            GraphicsOptionsShellScriptTemplates.OptionsMenuFrame1);

        WriteAllText(
            Path.Combine(
                scriptsRoot,
                "DefineSprite_333_ScreenOptionsMenu",
                "frame_1",
                "PlaceObject2_117_GenericButton_37",
                "CLIPACTIONRECORD onClipEvent(load).as"),
            GraphicsOptionsShellScriptTemplates.OptionsMenuGraphicsButtonClipAction);

        WriteAllText(
            Path.Combine(scriptsRoot, "DefineSprite_600_ScreenOptionsGraphics", "frame_1", "DoAction.as"),
            GraphicsOptionsShellScriptTemplates.CreateScreenFrame1(bootstrapSnapshot));

        WriteAllText(
            Path.Combine(scriptsRoot, "DefineSprite_600_ScreenOptionsGraphics", "frame_15", "DoAction.as"),
            GraphicsOptionsShellScriptTemplates.ScreenFrame15);

        WriteGraphicsShellRowClipActions(scriptsRoot, bootstrapSnapshot);
    }

    /// <summary>
    /// Validates required files and directories before invoking FFDec.
    /// </summary>
    /// <param name="paths">The resolved graphics build paths.</param>
    private static void ValidateInputs(GraphicsOptionsBuildPaths paths)
    {
        string[] requiredPaths =
        {
            paths.FrontendXmlPath,
            paths.FrontendSourceGfxPath,
            paths.FrontendScriptsPath,
            paths.FfdecPath,
            paths.BatmanUserIniPath
        };

        foreach (string requiredPath in requiredPaths)
        {
            if (!File.Exists(requiredPath) && !Directory.Exists(requiredPath))
            {
                throw new InvalidOperationException($"Required path not found: {requiredPath}");
            }
        }
    }

    /// <summary>
    /// Validates the XML, source GFX, script tree, FFDec, and user INI inputs required by the shell build.
    /// </summary>
    /// <param name="paths">The resolved shell build paths.</param>
    private static void ValidateShellInputs(GraphicsOptionsShellBuildPaths paths)
    {
        ValidateShellOutputPaths(paths);

        string[] requiredPaths =
        {
            paths.FrontendXmlPath,
            paths.FrontendSourceGfxPath,
            paths.FrontendScriptsPath,
            paths.FfdecPath,
            paths.BatmanUserIniPath
        };

        foreach (string requiredPath in requiredPaths)
        {
            if (!File.Exists(requiredPath) && !Directory.Exists(requiredPath))
            {
                throw new InvalidOperationException($"Required path not found: {requiredPath}");
            }
        }
    }

    /// <summary>
    /// Rejects shell output locations that could recursively delete the workspace or an extracted input.
    /// The check runs before output preparation and therefore before any recursive deletion. A shell
    /// temporary directory is also required to be a strict descendant of the validated output directory.
    /// </summary>
    /// <param name="paths">The resolved shell build paths to validate.</param>
    private static void ValidateShellOutputPaths(GraphicsOptionsShellBuildPaths paths)
    {
        if (HasUnsupportedDevicePrefix(paths.OutputDirectory))
        {
            throw new InvalidOperationException(
                $"Unsafe shell output directory '{paths.OutputDirectory}' uses an unsupported device namespace.");
        }

        string outputDirectory = Path.GetFullPath(paths.OutputDirectory);
        string rootPath = Path.GetFullPath(paths.RootPath);

        if (IsPathEqualOrAncestor(outputDirectory, rootPath))
        {
            throw new InvalidOperationException(
                $"Unsafe shell output directory '{outputDirectory}' conflicts with protected path '{rootPath}'.");
        }

        string[] requiredInputPaths =
        {
            paths.FrontendXmlPath,
            paths.FrontendSourceGfxPath,
            paths.FrontendScriptsPath,
            paths.FfdecPath,
            paths.BatmanUserIniPath
        };

        foreach (string requiredInputPath in requiredInputPaths)
        {
            string protectedPath = Path.GetFullPath(requiredInputPath);
            if (IsPathEqualOrAncestor(outputDirectory, protectedPath))
            {
                throw new InvalidOperationException(
                    $"Unsafe shell output directory '{outputDirectory}' conflicts with protected path '{protectedPath}'.");
            }

            if (IsPathStrictlyUnder(protectedPath, outputDirectory))
            {
                throw new InvalidOperationException(
                    $"Unsafe shell output directory '{outputDirectory}' is inside protected path '{protectedPath}'.");
            }
        }

        ValidateShellOutputDomains(outputDirectory, rootPath);

        string tempDirectory = Path.GetFullPath(paths.TempDirectory);
        if (!IsPathStrictlyUnder(outputDirectory, tempDirectory))
        {
            throw new InvalidOperationException(
                $"Unsafe shell output directory '{outputDirectory}' requires temporary directory '{tempDirectory}' to be strictly beneath it.");
        }
    }

    /// <summary>
    /// Ensures shell output is strictly below the supported generated or system-temporary roots,
    /// then checks every existing component beneath the selected root for reparse-point escapes.
    /// </summary>
    /// <param name="outputDirectory">The fully resolved shell output directory.</param>
    /// <param name="rootPath">The fully resolved Batman builder root.</param>
    private static void ValidateShellOutputDomains(string outputDirectory, string rootPath)
    {
        string generatedRoot = Path.Combine(rootPath, "generated");
        string tempRoot = Path.GetFullPath(Path.GetTempPath());
        bool isUnderGeneratedRoot = IsPathStrictlyUnder(generatedRoot, outputDirectory);
        bool isUnderTempRoot = IsPathStrictlyUnder(tempRoot, outputDirectory);

        if (!isUnderGeneratedRoot && !isUnderTempRoot)
        {
            throw new InvalidOperationException(
                $"Unsafe shell output directory '{outputDirectory}' is outside allowed roots '{Path.GetFullPath(generatedRoot)}' and '{tempRoot}'.");
        }

        string allowedRoot = isUnderGeneratedRoot ? Path.GetFullPath(generatedRoot) : tempRoot;
        ValidateShellOutputPathComponents(outputDirectory, allowedRoot);
    }

    /// <summary>
    /// Walks existing path components from an allowed output root through the requested output,
    /// stopping at the first missing component so not-yet-created output still receives ancestor checks.
    /// </summary>
    /// <param name="outputDirectory">The fully resolved shell output directory.</param>
    /// <param name="allowedRoot">The fully resolved generated or temporary root containing the output.</param>
    private static void ValidateShellOutputPathComponents(string outputDirectory, string allowedRoot)
    {
        string canonicalRoot = Path.GetFullPath(allowedRoot);
        string canonicalOutput = Path.GetFullPath(outputDirectory);
        string relativePath = Path.GetRelativePath(canonicalRoot, canonicalOutput);
        string currentPath = canonicalRoot;

        if (!File.Exists(currentPath) && !Directory.Exists(currentPath))
        {
            return;
        }

        ValidateShellOutputPathComponent(currentPath, canonicalOutput, canonicalRoot);

        string[] components = relativePath.Split(
            [Path.DirectorySeparatorChar, Path.AltDirectorySeparatorChar],
            StringSplitOptions.RemoveEmptyEntries);
        foreach (string component in components)
        {
            currentPath = Path.Combine(currentPath, component);
            if (!File.Exists(currentPath) && !Directory.Exists(currentPath))
            {
                return;
            }

            ValidateShellOutputPathComponent(currentPath, canonicalOutput, canonicalRoot);
        }
    }

    /// <summary>
    /// Rejects one existing shell-output component when its filesystem attributes identify a junction,
    /// symbolic link, or other reparse point that could redirect recursive deletion outside the allowed root.
    /// </summary>
    /// <param name="componentPath">The existing component being inspected.</param>
    /// <param name="outputDirectory">The complete requested shell output directory.</param>
    /// <param name="allowedRoot">The allowed output root that protects the component walk.</param>
    private static void ValidateShellOutputPathComponent(string componentPath, string outputDirectory, string allowedRoot)
    {
        FileAttributes attributes = File.GetAttributes(componentPath);
        if ((attributes & FileAttributes.ReparsePoint) != 0)
        {
            throw new InvalidOperationException(
                $"Unsafe shell output directory '{outputDirectory}' traverses reparse-point component '{componentPath}' under protected output root '{allowedRoot}'.");
        }
    }

    /// <summary>
    /// Detects Windows device and alternate namespace prefixes before any path normalization can reinterpret them.
    /// Such aliases can address the same directories as ordinary paths while bypassing lexical containment assumptions.
    /// </summary>
    /// <param name="path">The raw output path supplied by the caller.</param>
    /// <returns><see langword="true" /> when the path starts with a rejected device namespace prefix.</returns>
    private static bool HasUnsupportedDevicePrefix(string path)
    {
        return path.StartsWith(@"\\?\", StringComparison.OrdinalIgnoreCase) ||
               path.StartsWith(@"\\.\", StringComparison.OrdinalIgnoreCase) ||
               path.StartsWith(@"\??\", StringComparison.OrdinalIgnoreCase);
    }

    /// <summary>
    /// Determines whether a candidate path is equal to or an ancestor of a protected path.
    /// Canonical trailing separators prevent a name such as <c>build</c> from matching a sibling
    /// such as <c>builder</c>, while ordinal case-insensitive comparison matches Windows path rules.
    /// </summary>
    /// <param name="candidatePath">The candidate output or parent path.</param>
    /// <param name="protectedPath">The path that must remain protected.</param>
    /// <returns><see langword="true" /> when the candidate equals or contains the protected path.</returns>
    private static bool IsPathEqualOrAncestor(string candidatePath, string protectedPath)
    {
        string candidateCanonical = EnsureTrailingDirectorySeparator(candidatePath);
        string protectedCanonical = EnsureTrailingDirectorySeparator(protectedPath);
        return protectedCanonical.StartsWith(candidateCanonical, StringComparison.OrdinalIgnoreCase);
    }

    /// <summary>
    /// Determines whether a candidate path is a strict descendant of a parent path.
    /// Both paths are fully resolved and compared with trailing separators so lexical containment
    /// cannot confuse similarly prefixed sibling directories.
    /// </summary>
    /// <param name="parentPath">The expected containing directory.</param>
    /// <param name="candidatePath">The path whose containment should be checked.</param>
    /// <returns><see langword="true" /> when the candidate is strictly below the parent.</returns>
    private static bool IsPathStrictlyUnder(string parentPath, string candidatePath)
    {
        string parentCanonical = EnsureTrailingDirectorySeparator(parentPath);
        string candidateCanonical = EnsureTrailingDirectorySeparator(candidatePath);
        return !string.Equals(parentCanonical, candidateCanonical, StringComparison.OrdinalIgnoreCase) &&
               candidateCanonical.StartsWith(parentCanonical, StringComparison.OrdinalIgnoreCase);
    }

    /// <summary>
    /// Canonicalizes a path and appends one directory separator for safe lexical prefix comparisons.
    /// Existing trailing slash characters are retained so filesystem roots remain valid paths.
    /// </summary>
    /// <param name="path">The path to canonicalize.</param>
    /// <returns>The fully qualified path ending in a directory separator.</returns>
    private static string EnsureTrailingDirectorySeparator(string path)
    {
        string fullPath = Path.GetFullPath(path);
        if (fullPath.EndsWith(Path.DirectorySeparatorChar) || fullPath.EndsWith(Path.AltDirectorySeparatorChar))
        {
            return fullPath;
        }

        return fullPath + Path.DirectorySeparatorChar;
    }

    /// <summary>
    /// Writes one graphics row clip-action script per fixed depth, mapping depth 141 to row 1 and depth 29 to row 15.
    /// </summary>
    /// <param name="scriptsRoot">The staged writable frontend script root.</param>
    private static void WriteGraphicsRowClipActions(string scriptsRoot)
    {
        if (GraphicsRowDepths.Length != GraphicsRowClipActions.Length)
        {
            throw new InvalidOperationException("Graphics row depth/action arrays must be aligned.");
        }

        for (int rowIndex = 0; rowIndex < GraphicsRowDepths.Length; rowIndex++)
        {
            int depth = GraphicsRowDepths[rowIndex];
            string clipAction = GraphicsRowClipActions[rowIndex];

            WriteAllText(
                Path.Combine(
                    scriptsRoot,
                    "DefineSprite_600_ScreenOptionsGraphics",
                    "frame_1",
                    $"PlaceObject2_290_List_Template_{depth}",
                    "CLIPACTIONRECORD onClipEvent(load).as"),
                clipAction);
        }
    }

    /// <summary>
    /// Writes the fifteen shell row clip actions in one-to-one order with the graphics row depths,
    /// using the normalized INI snapshot retained by the shell build contract.
    /// A mismatch is rejected before any row is written so depth/action drift cannot produce a malformed shell.
    /// </summary>
    /// <param name="scriptsRoot">The staged writable frontend script root.</param>
    /// <param name="bootstrapSnapshot">The normalized user settings used to initialize VSync.</param>
    private static void WriteGraphicsShellRowClipActions(string scriptsRoot, BatmanGraphicsIniBootstrapSnapshot bootstrapSnapshot)
    {
        string[] rowClipActions = GraphicsOptionsShellScriptTemplates.CreateRowClipActions(bootstrapSnapshot);
        if (GraphicsRowDepths.Length != rowClipActions.Length)
        {
            throw new InvalidOperationException("Graphics shell row depth/action arrays must be aligned.");
        }

        for (int rowIndex = 0; rowIndex < GraphicsRowDepths.Length; rowIndex++)
        {
            int depth = GraphicsRowDepths[rowIndex];
            string clipAction = rowClipActions[rowIndex];

            WriteAllText(
                Path.Combine(
                    scriptsRoot,
                    "DefineSprite_600_ScreenOptionsGraphics",
                    "frame_1",
                    $"PlaceObject2_290_List_Template_{depth}",
                    "CLIPACTIONRECORD onClipEvent(load).as"),
                clipAction);
        }
    }

    /// <summary>
    /// Runs an external process and fails when the process exits unsuccessfully.
    /// </summary>
    /// <param name="fileName">The executable path.</param>
    /// <param name="arguments">The command-line arguments.</param>
    private static void RunProcess(string fileName, params string[] arguments)
    {
        string argumentString = string.Join(" ", arguments.Select(QuoteArgument));
        var startInfo = new ProcessStartInfo
        {
            FileName = fileName,
            Arguments = argumentString,
            UseShellExecute = false,
            RedirectStandardOutput = true,
            RedirectStandardError = true,
            CreateNoWindow = true
        };

        using var process = Process.Start(startInfo) ?? throw new InvalidOperationException($"Failed to start '{fileName}'.");
        Task<string> stdoutTask = process.StandardOutput.ReadToEndAsync();
        Task<string> stderrTask = process.StandardError.ReadToEndAsync();
        process.WaitForExit();
        Task.WaitAll(stdoutTask, stderrTask);
        string stdout = stdoutTask.Result;
        string stderr = stderrTask.Result;

        if (process.ExitCode != 0)
        {
            string detail = string.IsNullOrWhiteSpace(stderr) ? stdout : stderr;
            throw new InvalidOperationException($"Command failed: {fileName} {argumentString}{Environment.NewLine}{detail}".TrimEnd());
        }
    }

    /// <summary>
    /// Quotes a command-line argument when needed for whitespace-safe process execution.
    /// </summary>
    /// <param name="value">The raw argument value.</param>
    /// <returns>The quoted or unquoted command-line argument.</returns>
    private static string QuoteArgument(string value)
    {
        if (value.Length == 0)
        {
            return "\"\"";
        }

        if (!value.Any(char.IsWhiteSpace) && !value.Contains('"'))
        {
            return value;
        }

        return "\"" + value.Replace("\"", "\\\"") + "\"";
    }

    /// <summary>
    /// Deletes and recreates a directory tree.
    /// </summary>
    /// <param name="path">The directory to recreate.</param>
    private static void RecreateDirectory(string path)
    {
        if (Directory.Exists(path))
        {
            Directory.Delete(path, recursive: true);
        }

        Directory.CreateDirectory(path);
    }

    /// <summary>
    /// Recursively copies all files and directories into a destination path.
    /// </summary>
    /// <param name="sourceDirectory">The source directory path.</param>
    /// <param name="destinationDirectory">The destination directory path.</param>
    private static void CopyDirectory(string sourceDirectory, string destinationDirectory)
    {
        Directory.CreateDirectory(destinationDirectory);

        foreach (string sourceFile in Directory.GetFiles(sourceDirectory))
        {
            string destinationFile = Path.Combine(destinationDirectory, Path.GetFileName(sourceFile));
            File.Copy(sourceFile, destinationFile, overwrite: true);
        }

        foreach (string sourceSubdirectory in Directory.GetDirectories(sourceDirectory))
        {
            string destinationSubdirectory = Path.Combine(destinationDirectory, Path.GetFileName(sourceSubdirectory));
            CopyDirectory(sourceSubdirectory, destinationSubdirectory);
        }
    }

    /// <summary>
    /// Confirms that shell staging contains exactly the scripts emitted by the shell patch. This
    /// keeps the FFDec import scoped to patched files and catches accidental reintroduction of a
    /// copied retail script tree before the generated GFX is produced.
    /// </summary>
    /// <param name="scriptsRoot">The writable staged shell script root.</param>
    private static void ValidateShellPatchedScriptSet(string scriptsRoot)
    {
        string[] expectedPaths = ShellPatchedScriptRelativePaths
            .OrderBy(path => path, StringComparer.OrdinalIgnoreCase)
            .ToArray();
        string[] actualPaths = Directory.GetFiles(scriptsRoot, "*.as", SearchOption.AllDirectories)
            .Select(path => Path.GetRelativePath(scriptsRoot, path))
            .OrderBy(path => path, StringComparer.OrdinalIgnoreCase)
            .ToArray();

        if (!expectedPaths.SequenceEqual(actualPaths, StringComparer.OrdinalIgnoreCase))
        {
            string expected = string.Join(", ", expectedPaths);
            string actual = string.Join(", ", actualPaths);
            throw new InvalidOperationException(
                $"Graphics shell script staging drifted. Expected [{expected}], found [{actual}].");
        }
    }

    /// <summary>
    /// Writes a text file after creating its parent directory when needed.
    /// </summary>
    /// <param name="path">The destination file path.</param>
    /// <param name="contents">The file contents.</param>
    private static void WriteAllText(string path, string contents)
    {
        string? directory = Path.GetDirectoryName(path);
        if (!string.IsNullOrWhiteSpace(directory))
        {
            Directory.CreateDirectory(directory);
        }

        string normalizedContents = contents
            .Replace("\r\n", "\n", StringComparison.Ordinal)
            .Replace("\r", "\n", StringComparison.Ordinal)
            .Replace("\n", Environment.NewLine, StringComparison.Ordinal);

        File.WriteAllText(path, normalizedContents);
    }
}
