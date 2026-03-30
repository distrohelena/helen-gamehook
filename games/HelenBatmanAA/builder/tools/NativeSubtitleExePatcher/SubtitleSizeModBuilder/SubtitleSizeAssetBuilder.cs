using System.Diagnostics;
using System.Text.RegularExpressions;

namespace SubtitleSizeModBuilder;

/// <summary>
/// Resolves the filesystem layout used by the subtitle-size asset builders.
/// </summary>
internal sealed record BuildPaths(
    string RootPath,
    string BuildVersion,
    string OutputDirectory,
    string TempDirectory,
    string PauseXmlPath,
    string PauseSourceGfxPath,
    string PauseScriptsPath,
    string HudSourceGfxPath,
    string HudScriptsPath,
    string FrontendXmlPath,
    string FrontendSourceGfxPath,
    string FrontendScriptsPath,
    string FfdecPath,
    string PauseWorkingScriptsPath,
    string HudWorkingScriptsPath,
    string FrontendWorkingScriptsPath,
    string PausePatchedXmlPath,
    string PauseStructuralGfxPath,
    string PauseOutputGfxPath,
    string HudOutputGfxPath,
    string FrontendPatchedXmlPath,
    string FrontendStructuralGfxPath,
    string FrontendOutputGfxPath,
    string BmGameManifestPath,
    string FrontendManifestPath)
{
    /// <summary>
    /// Creates a <see cref="BuildPaths" /> instance rooted at the builder workspace.
    /// </summary>
    /// <param name="root">The Batman builder root directory.</param>
    /// <param name="ffdecPath">The FFDec CLI path.</param>
    /// <param name="outputDirectory">The generated output directory.</param>
    /// <param name="buildVersion">The build label injected into the frontend root script.</param>
    /// <returns>The resolved build path set.</returns>
    public static BuildPaths FromRoot(string root, string ffdecPath, string outputDirectory, string buildVersion)
    {
        string fullRoot = Path.GetFullPath(root);
        string fullOutput = Path.GetFullPath(outputDirectory);
        string tempDirectory = Path.Combine(fullOutput, "_build");

        return new BuildPaths(
            RootPath: fullRoot,
            BuildVersion: buildVersion,
            OutputDirectory: fullOutput,
            TempDirectory: tempDirectory,
            PauseXmlPath: Path.Combine(fullRoot, "extracted", "pause", "Pause.xml"),
            PauseSourceGfxPath: Path.Combine(fullRoot, "extracted", "pause", "Pause-extracted.gfx"),
            PauseScriptsPath: Path.Combine(fullRoot, "extracted", "pause", "pause-ffdec-export", "scripts"),
            HudSourceGfxPath: Path.Combine(fullRoot, "extracted", "hud", "HUD-extracted.gfx"),
            HudScriptsPath: Path.Combine(fullRoot, "extracted", "hud", "hud-ffdec-scripts", "scripts"),
            FrontendXmlPath: Path.Combine(fullRoot, "extracted", "frontend", "mainv2", "frontend-mainv2.xml"),
            FrontendSourceGfxPath: Path.Combine(fullRoot, "extracted", "frontend", "mainv2", "frontend-mainv2.gfx"),
            FrontendScriptsPath: Path.Combine(fullRoot, "extracted", "frontend", "mainv2", "frontend-mainv2-export", "scripts"),
            FfdecPath: Path.GetFullPath(ffdecPath),
            PauseWorkingScriptsPath: Path.Combine(tempDirectory, "pause-scripts"),
            HudWorkingScriptsPath: Path.Combine(tempDirectory, "hud-scripts"),
            FrontendWorkingScriptsPath: Path.Combine(tempDirectory, "frontend-scripts"),
            PausePatchedXmlPath: Path.Combine(tempDirectory, "Pause-subtitle-size.xml"),
            PauseStructuralGfxPath: Path.Combine(tempDirectory, "Pause-subtitle-size-structural.gfx"),
            PauseOutputGfxPath: Path.Combine(fullOutput, "Pause-subtitle-size.gfx"),
            HudOutputGfxPath: Path.Combine(fullOutput, "HUD-subtitle-size.gfx"),
            FrontendPatchedXmlPath: Path.Combine(tempDirectory, "MainV2-subtitle-size.xml"),
            FrontendStructuralGfxPath: Path.Combine(tempDirectory, "MainV2-subtitle-size-structural.gfx"),
            FrontendOutputGfxPath: Path.Combine(fullOutput, "MainV2-subtitle-size.gfx"),
            BmGameManifestPath: Path.Combine(fullOutput, "subtitle-size-bmgame.manifest.jsonc"),
            FrontendManifestPath: Path.Combine(fullOutput, "subtitle-size-frontend.manifest.jsonc"));
    }
}

/// <summary>
/// Builds the pause, HUD, and frontend subtitle-size assets from extracted FFDec sources.
/// </summary>
internal static class SubtitleSizeAssetBuilder
{
    /// <summary>
    /// Builds the combined pause, HUD, and frontend subtitle-size package.
    /// </summary>
    /// <param name="paths">The resolved build paths.</param>
    public static void Build(BuildPaths paths)
    {
        ValidateAllInputs(paths);
        PrepareOutputDirectories(paths);

        BuildPauseAssets(paths);
        BuildHudAssets(paths);
        BuildFrontendAssets(paths);

        WriteBmGameManifest(paths.BmGameManifestPath);
        WriteFrontendManifest(paths.FrontendManifestPath);
    }

    /// <summary>
    /// Builds only the frontend main-menu audio asset and manifest.
    /// </summary>
    /// <param name="paths">The resolved build paths.</param>
    public static void BuildFrontend(BuildPaths paths)
    {
        ValidateFrontendInputs(paths);
        PrepareOutputDirectories(paths);

        BuildFrontendAssets(paths);
        WriteFrontendManifest(paths.FrontendManifestPath);
    }

    /// <summary>
    /// Builds only the frontend root version-label patch and imports the untouched frontend scripts into MainV2.
    /// </summary>
    /// <param name="paths">The resolved build paths.</param>
    public static void BuildFrontendVersionLabel(BuildPaths paths)
    {
        ValidateFrontendVersionLabelInputs(paths);
        PrepareOutputDirectories(paths);

        BuildFrontendVersionLabelAssets(paths);
        WriteFrontendManifest(paths.FrontendManifestPath);
    }

    /// <summary>
    /// Recreates the output directory so each build starts from a clean generated state.
    /// </summary>
    /// <param name="paths">The resolved build paths.</param>
    private static void PrepareOutputDirectories(BuildPaths paths)
    {
        RecreateDirectory(paths.OutputDirectory);
        Directory.CreateDirectory(paths.TempDirectory);
    }

    /// <summary>
    /// Builds the pause-menu asset from a structurally patched XML and patched scripts.
    /// </summary>
    /// <param name="paths">The resolved build paths.</param>
    private static void BuildPauseAssets(BuildPaths paths)
    {
        CopyDirectory(paths.PauseScriptsPath, paths.PauseWorkingScriptsPath);
        PatchPauseScripts(paths.PauseWorkingScriptsPath);
        PauseXmlPatcher.Patch(paths.PauseXmlPath, paths.PausePatchedXmlPath);

        RunProcess(paths.FfdecPath, "-xml2swf", paths.PausePatchedXmlPath, paths.PauseStructuralGfxPath);
        RunProcess(paths.FfdecPath, "-importScript", paths.PauseStructuralGfxPath, paths.PauseOutputGfxPath, paths.PauseWorkingScriptsPath);
    }

    /// <summary>
    /// Builds the HUD asset by importing the patched subtitle scripts into the extracted HUD movie.
    /// </summary>
    /// <param name="paths">The resolved build paths.</param>
    private static void BuildHudAssets(BuildPaths paths)
    {
        CopyDirectory(paths.HudScriptsPath, paths.HudWorkingScriptsPath);
        PatchHudScripts(paths.HudWorkingScriptsPath);

        RunProcess(paths.FfdecPath, "-importScript", paths.HudSourceGfxPath, paths.HudOutputGfxPath, paths.HudWorkingScriptsPath);
    }

    /// <summary>
    /// Builds the frontend main-menu audio asset by rebuilding the movie XML, then importing the patched scripts.
    /// </summary>
    /// <param name="paths">The resolved build paths.</param>
    private static void BuildFrontendAssets(BuildPaths paths)
    {
        CopyDirectory(paths.FrontendScriptsPath, paths.FrontendWorkingScriptsPath);
        PatchFrontendScripts(paths.FrontendWorkingScriptsPath, paths.BuildVersion);
        MainMenuXmlPatcher.Patch(paths.FrontendXmlPath, paths.FrontendPatchedXmlPath);

        RunProcess(paths.FfdecPath, "-xml2swf", paths.FrontendPatchedXmlPath, paths.FrontendStructuralGfxPath);
        RunProcess(paths.FfdecPath, "-importScript", paths.FrontendStructuralGfxPath, paths.FrontendOutputGfxPath, paths.FrontendWorkingScriptsPath);
    }

    /// <summary>
    /// Builds the frontend MainV2 movie with only the root version label changed.
    /// </summary>
    /// <param name="paths">The resolved build paths.</param>
    private static void BuildFrontendVersionLabelAssets(BuildPaths paths)
    {
        CopyDirectory(paths.FrontendScriptsPath, paths.FrontendWorkingScriptsPath);
        PatchFrontendVersionLabelScript(paths.FrontendWorkingScriptsPath, paths.BuildVersion);

        RunProcess(paths.FfdecPath, "-importScript", paths.FrontendSourceGfxPath, paths.FrontendOutputGfxPath, paths.FrontendWorkingScriptsPath);
    }

    /// <summary>
    /// Writes the pause-menu list item and frame script overrides used by the runtime subtitle-size build.
    /// </summary>
    /// <param name="scriptsRoot">The copied pause script directory.</param>
    private static void PatchPauseScripts(string scriptsRoot)
    {
        WriteAllText(
            Path.Combine(scriptsRoot, "__Packages", "rs", "ui", "ListItem.as"),
            ScriptTemplates.PauseListItem);

        WriteAllText(
            Path.Combine(scriptsRoot, "DefineSprite_394_ScreenOptionsAudio", "frame_1", "DoAction.as"),
            ScriptTemplates.PauseAudioFrame1);

        WriteAllText(
            Path.Combine(scriptsRoot, "DefineSprite_394_ScreenOptionsAudio", "frame_12", "DoAction.as"),
            ScriptTemplates.PauseAudioFrame12);

        WriteAllText(
            Path.Combine(
                scriptsRoot,
                "DefineSprite_394_ScreenOptionsAudio",
                "frame_1",
                "PlaceObject2_322_List_Template_61",
                "CLIPACTIONRECORD onClipEvent(load).as"),
            ScriptTemplates.PauseAudioSubtitleSizeClipAction);
    }

    /// <summary>
    /// Writes the HUD subtitle overrides used by the runtime subtitle-size build.
    /// </summary>
    /// <param name="scriptsRoot">The copied HUD script directory.</param>
    private static void PatchHudScripts(string scriptsRoot)
    {
        WriteAllText(
            Path.Combine(scriptsRoot, "__Packages", "rs", "hud", "Subtitle.as"),
            ScriptTemplates.HudSubtitle);

        WriteAllText(
            Path.Combine(scriptsRoot, "DefineSprite_987", "frame_1", "DoAction.as"),
            ScriptTemplates.HudContentsFrame1);
    }

    /// <summary>
    /// Writes the frontend main-menu audio script overrides and injects the build version label.
    /// </summary>
    /// <param name="scriptsRoot">The copied frontend script directory.</param>
    /// <param name="buildVersion">The build label shown in the root frontend script.</param>
    private static void PatchFrontendScripts(string scriptsRoot, string buildVersion)
    {
        WriteAllText(
            Path.Combine(scriptsRoot, "__Packages", "rs", "ui", "ListItem.as"),
            ScriptTemplates.FrontendListItem);

        WriteAllText(
            Path.Combine(scriptsRoot, "DefineSprite_359_ScreenOptionsAudio", "frame_1", "DoAction.as"),
            ScriptTemplates.FrontendAudioFrame1);

        WriteAllText(
            Path.Combine(
                scriptsRoot,
                "DefineSprite_359_ScreenOptionsAudio",
                "frame_1",
                "PlaceObject2_290_List_Template_61",
                "CLIPACTIONRECORD onClipEvent(load).as"),
            ScriptTemplates.FrontendAudioSubtitleSizeClipAction);

        PatchFrontendVersionLabelScript(scriptsRoot, buildVersion);
    }

    /// <summary>
    /// Rewrites the frontend root <c>PCVersionString</c> declaration to the requested build label.
    /// </summary>
    /// <param name="scriptsRoot">The copied frontend script directory.</param>
    /// <param name="buildVersion">The build label shown in the root frontend script.</param>
    private static void PatchFrontendVersionLabelScript(string scriptsRoot, string buildVersion)
    {
        string rootScriptPath = Path.Combine(scriptsRoot, "frame_1", "DoAction.as");
        string rootScript = File.ReadAllText(rootScriptPath);
        const string declarationPattern = """var PCVersionString(?:\s*=\s*"[^"]*")?;""";
        Regex declarationRegex = new(declarationPattern, RegexOptions.CultureInvariant);
        Match declarationMatch = declarationRegex.Match(rootScript);
        if (!declarationMatch.Success)
        {
            throw new InvalidOperationException($"Could not find a PCVersionString declaration in {rootScriptPath}");
        }

        string patched = declarationRegex.Replace(
            rootScript,
            $"var PCVersionString = \"{EscapeActionScriptString(buildVersion)}\";",
            1);

        WriteAllText(rootScriptPath, patched);
    }

    /// <summary>
    /// Writes the BmGame manifest for the combined pause and HUD replacement package.
    /// </summary>
    /// <param name="manifestPath">The output manifest path.</param>
    private static void WriteBmGameManifest(string manifestPath)
    {
        const string manifest = """
        {
          "name": "Subtitle size mod",
          "patches": [
            {
              "owner": "PauseMenu",
              "exportName": "Pause",
              "exportType": "GFxMovieInfo",
              "replacementPath": "Pause-subtitle-size.gfx",
              "payloadMagic": "GFX"
            },
            {
              "owner": "GameHUD",
              "exportName": "HUD",
              "exportType": "GFxMovieInfo",
              "replacementPath": "HUD-subtitle-size.gfx",
              "payloadMagic": "GFX"
            }
          ]
        }
        """;

        File.WriteAllText(manifestPath, manifest);
    }

    /// <summary>
    /// Writes the frontend-only manifest for the rebuilt MainV2 movie.
    /// </summary>
    /// <param name="manifestPath">The output manifest path.</param>
    private static void WriteFrontendManifest(string manifestPath)
    {
        const string manifest = """
        {
          "name": "Subtitle size frontend mod",
          "patches": [
            {
              "owner": "MainMenu",
              "exportName": "MainV2",
              "exportType": "GFxMovieInfo",
              "replacementPath": "MainV2-subtitle-size.gfx",
              "payloadMagic": "GFX"
            }
          ]
        }
        """;

        File.WriteAllText(manifestPath, manifest);
    }

    /// <summary>
    /// Runs an external process and throws when it exits unsuccessfully.
    /// </summary>
    /// <param name="fileName">The executable to run.</param>
    /// <param name="arguments">The argument list to pass through.</param>
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
        string stdout = process.StandardOutput.ReadToEnd();
        string stderr = process.StandardError.ReadToEnd();
        process.WaitForExit();

        if (process.ExitCode != 0)
        {
            string detail = string.IsNullOrWhiteSpace(stderr) ? stdout : stderr;
            throw new InvalidOperationException($"Command failed: {fileName} {argumentString}{Environment.NewLine}{detail}".TrimEnd());
        }
    }

    /// <summary>
    /// Quotes a command-line argument when FFDec requires embedded whitespace or quotes.
    /// </summary>
    /// <param name="value">The raw argument value.</param>
    /// <returns>A command-line safe argument string.</returns>
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
    /// Validates the full set of combined pause, HUD, and frontend inputs.
    /// </summary>
    /// <param name="paths">The resolved build paths.</param>
    private static void ValidateAllInputs(BuildPaths paths)
    {
        string[] requiredPaths =
        {
            paths.PauseXmlPath,
            paths.PauseSourceGfxPath,
            paths.PauseScriptsPath,
            paths.HudSourceGfxPath,
            paths.HudScriptsPath,
            paths.FrontendXmlPath,
            paths.FrontendSourceGfxPath,
            paths.FrontendScriptsPath,
            paths.FfdecPath
        };

        ValidateRequiredPaths(requiredPaths);
    }

    /// <summary>
    /// Validates only the inputs needed for the frontend main-menu audio build.
    /// </summary>
    /// <param name="paths">The resolved build paths.</param>
    private static void ValidateFrontendInputs(BuildPaths paths)
    {
        string[] requiredPaths =
        {
            paths.FrontendXmlPath,
            paths.FrontendScriptsPath,
            paths.FfdecPath
        };

        ValidateRequiredPaths(requiredPaths);
    }

    /// <summary>
    /// Validates the inputs needed for the narrowed frontend version-label build.
    /// </summary>
    /// <param name="paths">The resolved build paths.</param>
    private static void ValidateFrontendVersionLabelInputs(BuildPaths paths)
    {
        string[] requiredPaths =
        {
            paths.FrontendSourceGfxPath,
            paths.FrontendScriptsPath,
            paths.FfdecPath
        };

        ValidateRequiredPaths(requiredPaths);
    }

    /// <summary>
    /// Ensures each required file or directory exists before invoking FFDec.
    /// </summary>
    /// <param name="requiredPaths">The paths that must already exist.</param>
    private static void ValidateRequiredPaths(IEnumerable<string> requiredPaths)
    {
        foreach (string requiredPath in requiredPaths)
        {
            if (!File.Exists(requiredPath) && !Directory.Exists(requiredPath))
            {
                throw new InvalidOperationException($"Required path not found: {requiredPath}");
            }
        }
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
    /// Recursively copies a script directory into a writable working location.
    /// </summary>
    /// <param name="sourceDirectory">The extracted FFDec script directory.</param>
    /// <param name="destinationDirectory">The working copy destination.</param>
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
    /// Writes a text file using the current platform newline convention and creates parent folders as needed.
    /// </summary>
    /// <param name="path">The destination file path.</param>
    /// <param name="contents">The file contents to write.</param>
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

    /// <summary>
    /// Escapes a string for insertion into an ActionScript string literal.
    /// </summary>
    /// <param name="value">The raw value to escape.</param>
    /// <returns>The escaped ActionScript literal contents.</returns>
    private static string EscapeActionScriptString(string value)
    {
        return value
            .Replace("\\", "\\\\", StringComparison.Ordinal)
            .Replace("\"", "\\\"", StringComparison.Ordinal);
    }
}
