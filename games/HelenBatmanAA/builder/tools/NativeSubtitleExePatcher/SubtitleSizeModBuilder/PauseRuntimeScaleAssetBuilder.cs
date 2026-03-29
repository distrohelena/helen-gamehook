using System.Diagnostics;

namespace SubtitleSizeModBuilder;

internal sealed record PauseRuntimeBuildPaths(
    string RootPath,
    string OutputDirectory,
    string TempDirectory,
    string PauseXmlPath,
    string PauseSourceGfxPath,
    string PauseScriptsPath,
    string HudSourceGfxPath,
    string HudScriptsPath,
    string FfdecPath,
    string PauseWorkingScriptsPath,
    string HudWorkingScriptsPath,
    string PausePatchedXmlPath,
    string PauseStructuralGfxPath,
    string PauseOutputGfxPath,
    string HudOutputGfxPath,
    string BmGameManifestPath)
{
    public static PauseRuntimeBuildPaths FromRoot(string root, string ffdecPath, string outputDirectory)
    {
        string fullRoot = Path.GetFullPath(root);
        string fullOutput = Path.GetFullPath(outputDirectory);
        string tempDirectory = Path.Combine(fullOutput, "_build");

        return new PauseRuntimeBuildPaths(
            RootPath: fullRoot,
            OutputDirectory: fullOutput,
            TempDirectory: tempDirectory,
            PauseXmlPath: Path.Combine(fullRoot, "extracted", "pause", "Pause.xml"),
            PauseSourceGfxPath: Path.Combine(fullRoot, "extracted", "pause", "Pause-extracted.gfx"),
            PauseScriptsPath: Path.Combine(fullRoot, "extracted", "pause", "pause-ffdec-export", "scripts"),
            HudSourceGfxPath: Path.Combine(fullRoot, "extracted", "hud", "HUD-extracted.gfx"),
            HudScriptsPath: Path.Combine(fullRoot, "extracted", "hud", "hud-ffdec-scripts", "scripts"),
            FfdecPath: Path.GetFullPath(ffdecPath),
            PauseWorkingScriptsPath: Path.Combine(tempDirectory, "pause-scripts"),
            HudWorkingScriptsPath: Path.Combine(tempDirectory, "hud-scripts"),
            PausePatchedXmlPath: Path.Combine(tempDirectory, "Pause-runtime-scale.xml"),
            PauseStructuralGfxPath: Path.Combine(tempDirectory, "Pause-runtime-scale-structural.gfx"),
            PauseOutputGfxPath: Path.Combine(fullOutput, "Pause-runtime-scale.gfx"),
            HudOutputGfxPath: Path.Combine(fullOutput, "HUD-runtime-scale.gfx"),
            BmGameManifestPath: Path.Combine(fullOutput, "pause-runtime-scale.manifest.jsonc"));
    }
}

internal static class PauseRuntimeScaleAssetBuilder
{
    public static void Build(PauseRuntimeBuildPaths paths)
    {
        ValidateInputs(paths);

        RecreateDirectory(paths.OutputDirectory);
        Directory.CreateDirectory(paths.TempDirectory);

        CopyDirectory(paths.PauseScriptsPath, paths.PauseWorkingScriptsPath);
        CopyDirectory(paths.HudScriptsPath, paths.HudWorkingScriptsPath);
        PatchPauseScripts(paths.PauseWorkingScriptsPath);
        PatchHudScripts(paths.HudWorkingScriptsPath);
        PauseXmlPatcher.Patch(paths.PauseXmlPath, paths.PausePatchedXmlPath);

        RunProcess(paths.FfdecPath, "-xml2swf", paths.PausePatchedXmlPath, paths.PauseStructuralGfxPath);
        RunProcess(paths.FfdecPath, "-importScript", paths.PauseStructuralGfxPath, paths.PauseOutputGfxPath, paths.PauseWorkingScriptsPath);
        RunProcess(paths.FfdecPath, "-importScript", paths.HudSourceGfxPath, paths.HudOutputGfxPath, paths.HudWorkingScriptsPath);
        WriteBmGameManifest(paths.BmGameManifestPath);
    }

    private static void PatchPauseScripts(string scriptsRoot)
    {
        WriteAllText(
            Path.Combine(scriptsRoot, "__Packages", "rs", "ui", "ListItem.as"),
            ScriptTemplates.PauseRuntimeScaleListItem);

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
        string hudFramePath = Path.Combine(scriptsRoot, "DefineSprite_987", "frame_1", "DoAction.as");
        string script = File.ReadAllText(hudFramePath);

        const string anchor = "var AdjustTheseMovies = Array(";
        if (!script.Contains(anchor, StringComparison.Ordinal))
        {
            throw new InvalidOperationException($"Could not find '{anchor}' in {hudFramePath}");
        }

        script = script.Replace(
            anchor,
            ScriptTemplates.HudRuntimeScaleHelpers + Environment.NewLine + anchor,
            StringComparison.Ordinal);

        const string initLine = "PromptManager.SetMode(false,true);";
        if (!script.Contains(initLine, StringComparison.Ordinal))
        {
            throw new InvalidOperationException($"Could not find '{initLine}' in {hudFramePath}");
        }

        script = script.Replace(
            initLine,
            initLine + Environment.NewLine + "ApplySavedSubtitleSizeCommand();",
            StringComparison.Ordinal);

        WriteAllText(hudFramePath, script);
    }

    private static void WriteBmGameManifest(string manifestPath)
    {
        const string manifest = """
        {
          "name": "Pause runtime subtitle scale",
          "patches": [
            {
              "owner": "PauseMenu",
              "exportName": "Pause",
              "exportType": "GFxMovieInfo",
              "replacementPath": "Pause-runtime-scale.gfx",
              "payloadMagic": "GFX"
            },
            {
              "owner": "GameHUD",
              "exportName": "HUD",
              "exportType": "GFxMovieInfo",
              "replacementPath": "HUD-runtime-scale.gfx",
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

    private static void ValidateInputs(PauseRuntimeBuildPaths paths)
    {
        string[] requiredPaths =
        {
            paths.PauseXmlPath,
            paths.PauseScriptsPath,
            paths.HudSourceGfxPath,
            paths.HudScriptsPath,
            paths.FfdecPath
        };

        foreach (string requiredPath in requiredPaths)
        {
            if (!File.Exists(requiredPath) && !Directory.Exists(requiredPath))
            {
                throw new FileNotFoundException($"Required input not found: {requiredPath}");
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

        foreach (string directory in Directory.GetDirectories(sourceDirectory, "*", SearchOption.AllDirectories))
        {
            string relative = Path.GetRelativePath(sourceDirectory, directory);
            Directory.CreateDirectory(Path.Combine(destinationDirectory, relative));
        }

        foreach (string file in Directory.GetFiles(sourceDirectory, "*", SearchOption.AllDirectories))
        {
            string relative = Path.GetRelativePath(sourceDirectory, file);
            string destination = Path.Combine(destinationDirectory, relative);
            Directory.CreateDirectory(Path.GetDirectoryName(destination)!);
            File.Copy(file, destination, overwrite: true);
        }
    }

    private static void WriteAllText(string path, string content)
    {
        Directory.CreateDirectory(Path.GetDirectoryName(path)!);
        File.WriteAllText(path, content);
    }
}
