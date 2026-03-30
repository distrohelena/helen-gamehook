using System.Diagnostics;

namespace SubtitleSizeModBuilder;

internal sealed record HudFontBuildPaths(
    string RootPath,
    string OutputDirectory,
    string TempDirectory,
    string HudXmlPath,
    string HudSourceGfxPath,
    string FfdecPath,
    string HudPatchedXmlPath,
    string HudOutputGfxPath,
    string BmGameManifestPath)
{
    public static HudFontBuildPaths FromRoot(string root, string ffdecPath, string outputDirectory)
    {
        string fullRoot = Path.GetFullPath(root);
        string fullOutput = Path.GetFullPath(outputDirectory);
        string tempDirectory = Path.Combine(fullOutput, "_build");

        return new HudFontBuildPaths(
            RootPath: fullRoot,
            OutputDirectory: fullOutput,
            TempDirectory: tempDirectory,
            HudXmlPath: Path.Combine(fullRoot, "extracted", "hud", "HUD.xml"),
            HudSourceGfxPath: Path.Combine(fullRoot, "extracted", "hud", "HUD-extracted.gfx"),
            FfdecPath: Path.GetFullPath(ffdecPath),
            HudPatchedXmlPath: Path.Combine(tempDirectory, "HUD-font-boost.xml"),
            HudOutputGfxPath: Path.Combine(fullOutput, "HUD-font-boost.gfx"),
            BmGameManifestPath: Path.Combine(fullOutput, "hud-font-boost.manifest.jsonc"));
    }
}

internal static class HudFontBoostAssetBuilder
{
    public static void Build(HudFontBuildPaths paths)
    {
        ValidateInputs(paths);
        RecreateDirectory(paths.OutputDirectory);
        Directory.CreateDirectory(paths.TempDirectory);

        HudXmlFontPatcher.Patch(paths.HudXmlPath, paths.HudPatchedXmlPath);
        RunProcess(paths.FfdecPath, "-xml2swf", paths.HudPatchedXmlPath, paths.HudOutputGfxPath);
        ValidateSwfOutput(paths.HudOutputGfxPath);
        WriteBmGameManifest(paths.BmGameManifestPath);
    }

    private static void WriteBmGameManifest(string manifestPath)
    {
        const string manifest = """
        {
          "name": "HUD font boost",
          "patches": [
            {
              "owner": "GameHUD",
              "exportName": "HUD",
              "exportType": "GFxMovieInfo",
              "replacementPath": "HUD-font-boost.gfx",
              "payloadMagic": "GFX"
            }
          ]
        }
        """;

        File.WriteAllText(manifestPath, manifest);
    }

    private static void ValidateInputs(HudFontBuildPaths paths)
    {
        string[] requiredPaths =
        {
            paths.HudXmlPath,
            paths.HudSourceGfxPath,
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

        string combined = string.Join(Environment.NewLine, new[] { stdout, stderr }.Where(static text => !string.IsNullOrWhiteSpace(text)));
        if (combined.Contains("SEVERE:", StringComparison.OrdinalIgnoreCase) ||
            combined.Contains("Exception", StringComparison.OrdinalIgnoreCase))
        {
            throw new InvalidOperationException($"Command reported an FFDec error: {fileName} {argumentString}{Environment.NewLine}{combined}".TrimEnd());
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

        return "\"" + value.Replace("\"", "\\\"", StringComparison.Ordinal) + "\"";
    }

    private static void ValidateSwfOutput(string path)
    {
        var file = new FileInfo(path);
        if (!file.Exists)
        {
            throw new InvalidOperationException($"Expected FFDec to create '{path}', but it does not exist.");
        }

        if (file.Length < 1024)
        {
            throw new InvalidOperationException($"FFDec created an implausibly small HUD movie at '{path}' ({file.Length} bytes).");
        }
    }
}
