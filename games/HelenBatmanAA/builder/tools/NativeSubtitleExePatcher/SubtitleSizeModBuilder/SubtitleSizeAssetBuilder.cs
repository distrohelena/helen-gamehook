using System.Diagnostics;

namespace SubtitleSizeModBuilder;

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

internal static class SubtitleSizeAssetBuilder
{
    public static void Build(BuildPaths paths)
    {
        ValidateInputs(paths);

        RecreateDirectory(paths.OutputDirectory);
        Directory.CreateDirectory(paths.TempDirectory);

        CopyDirectory(paths.PauseScriptsPath, paths.PauseWorkingScriptsPath);
        CopyDirectory(paths.HudScriptsPath, paths.HudWorkingScriptsPath);
        CopyDirectory(paths.FrontendScriptsPath, paths.FrontendWorkingScriptsPath);

        PatchPauseScripts(paths.PauseWorkingScriptsPath);
        PatchHudScripts(paths.HudWorkingScriptsPath);
        PatchFrontendScripts(paths.FrontendWorkingScriptsPath, paths.BuildVersion);
        PauseXmlPatcher.Patch(paths.PauseXmlPath, paths.PausePatchedXmlPath);

        RunProcess(paths.FfdecPath, "-xml2swf", paths.PausePatchedXmlPath, paths.PauseStructuralGfxPath);
        RunProcess(paths.FfdecPath, "-importScript", paths.PauseStructuralGfxPath, paths.PauseOutputGfxPath, paths.PauseWorkingScriptsPath);
        RunProcess(paths.FfdecPath, "-importScript", paths.HudSourceGfxPath, paths.HudOutputGfxPath, paths.HudWorkingScriptsPath);
        RunProcess(paths.FfdecPath, "-importScript", paths.FrontendSourceGfxPath, paths.FrontendOutputGfxPath, paths.FrontendWorkingScriptsPath);

        WriteBmGameManifest(paths.BmGameManifestPath);
        WriteFrontendManifest(paths.FrontendManifestPath);
    }

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

    private static void PatchHudScripts(string scriptsRoot)
    {
        WriteAllText(
            Path.Combine(scriptsRoot, "__Packages", "rs", "hud", "Subtitle.as"),
            ScriptTemplates.HudSubtitle);

        WriteAllText(
            Path.Combine(scriptsRoot, "DefineSprite_987", "frame_1", "DoAction.as"),
            ScriptTemplates.HudContentsFrame1);
    }

    private static void PatchFrontendScripts(string scriptsRoot, string buildVersion)
    {
        WriteAllText(
            Path.Combine(scriptsRoot, "DefineSprite_359_ScreenOptionsAudio", "frame_1", "DoAction.as"),
            ScriptTemplates.FrontendAudioFrame1);

        WriteAllText(
            Path.Combine(
                scriptsRoot,
                "DefineSprite_359_ScreenOptionsAudio",
                "frame_1",
                "PlaceObject2_290_List_Template_53",
                "CLIPACTIONRECORD onClipEvent(load).as"),
            ScriptTemplates.FrontendAudioSubtitleSizeClipAction);

        string rootScriptPath = Path.Combine(scriptsRoot, "frame_1", "DoAction.as");
        string rootScript = File.ReadAllText(rootScriptPath);
        const string marker = "var PCVersionString;";
        if (!rootScript.Contains(marker, StringComparison.Ordinal))
        {
            throw new InvalidOperationException($"Could not find '{marker}' in {rootScriptPath}");
        }

        string patched = rootScript.Replace(
            marker,
            $"var PCVersionString = \"{EscapeActionScriptString(buildVersion)}\";",
            StringComparison.Ordinal);

        WriteAllText(rootScriptPath, patched);
    }

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

    private static void ValidateInputs(BuildPaths paths)
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

        foreach (string requiredPath in requiredPaths)
        {
            if (!File.Exists(requiredPath) && !Directory.Exists(requiredPath))
            {
                throw new InvalidOperationException($"Required path not found: {requiredPath}");
            }
        }
    }

    private static void RecreateDirectory(string path)
    {
        if (Directory.Exists(path))
        {
            Directory.Delete(path, recursive: true);
        }

        Directory.CreateDirectory(path);
    }

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

    private static void WriteAllText(string path, string contents)
    {
        string? directory = Path.GetDirectoryName(path);
        if (!string.IsNullOrWhiteSpace(directory))
        {
            Directory.CreateDirectory(directory);
        }

        File.WriteAllText(path, contents.Replace("\n", Environment.NewLine));
    }

    private static string EscapeActionScriptString(string value)
    {
        return value
            .Replace("\\", "\\\\", StringComparison.Ordinal)
            .Replace("\"", "\\\"", StringComparison.Ordinal);
    }
}
