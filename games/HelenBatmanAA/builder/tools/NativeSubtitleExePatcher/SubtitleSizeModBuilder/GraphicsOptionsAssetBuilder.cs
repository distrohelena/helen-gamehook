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
    /// Builds the graphics-options prototype frontend asset.
    /// </summary>
    /// <param name="paths">The resolved graphics build paths.</param>
    public static void Build(GraphicsOptionsBuildPaths paths)
    {
        ValidateInputs(paths);
        PrepareOutputDirectories(paths);

        CopyDirectory(paths.FrontendScriptsPath, paths.FrontendWorkingScriptsPath);
        PatchFrontendScripts(paths.FrontendWorkingScriptsPath);
        GraphicsOptionsXmlPatcher.Patch(paths.FrontendXmlPath, paths.FrontendPatchedXmlPath);

        RunProcess(paths.FfdecPath, "-xml2swf", paths.FrontendPatchedXmlPath, paths.FrontendStructuralGfxPath);
        RunProcess(paths.FfdecPath, "-importScript", paths.FrontendStructuralGfxPath, paths.FrontendOutputGfxPath, paths.FrontendWorkingScriptsPath);
    }

    /// <summary>
    /// Recreates output directories so each build starts from a clean state.
    /// </summary>
    /// <param name="paths">The resolved graphics build paths.</param>
    private static void PrepareOutputDirectories(GraphicsOptionsBuildPaths paths)
    {
        RecreateDirectory(paths.OutputDirectory);
        Directory.CreateDirectory(paths.TempDirectory);
    }

    /// <summary>
    /// Applies graphics-options script overrides to a staged frontend script tree.
    /// </summary>
    /// <param name="scriptsRoot">The staged writable frontend script root.</param>
    private static void PatchFrontendScripts(string scriptsRoot)
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
            GraphicsOptionsScriptTemplates.GraphicsOptionsFrame1);

        WriteGraphicsRowClipActions(scriptsRoot);

        WriteAllText(
            Path.Combine(scriptsRoot, "DefineSprite_601_GraphicsExitPrompt", "frame_1", "DoAction.as"),
            GraphicsOptionsScriptTemplates.GraphicsExitPromptFrame1);

        WriteAllText(
            Path.Combine(
                scriptsRoot,
                "DefineSprite_601_GraphicsExitPrompt",
                "frame_1",
                "PlaceObject2_117_GenericButton_11",
                "CLIPACTIONRECORD onClipEvent(load).as"),
            GraphicsOptionsScriptTemplates.GraphicsExitPromptApplyButton);

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
            paths.FfdecPath
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
