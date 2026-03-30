using System.Diagnostics;

namespace SubtitleSizeModBuilder;

internal sealed record SubtitleRouteProbeBuildPaths(
    string RootPath,
    string OutputDirectory,
    string TempDirectory,
    string HudSourceGfxPath,
    string HudScriptsPath,
    string CharacterBioSourceGfxPath,
    string CharacterBioScriptsPath,
    string FfdecPath,
    string HudWorkingScriptsPath,
    string CharacterBioWorkingScriptsPath,
    string HudOutputGfxPath,
    string CharacterBioOutputGfxPath,
    string BmGameManifestPath)
{
    public static SubtitleRouteProbeBuildPaths FromRoot(string root, string ffdecPath, string outputDirectory)
    {
        string fullRoot = Path.GetFullPath(root);
        string fullOutput = Path.GetFullPath(outputDirectory);
        string tempDirectory = Path.Combine(fullOutput, "_build");

        return new SubtitleRouteProbeBuildPaths(
            RootPath: fullRoot,
            OutputDirectory: fullOutput,
            TempDirectory: tempDirectory,
            HudSourceGfxPath: Path.Combine(fullRoot, "extracted", "hud", "HUD-extracted.gfx"),
            HudScriptsPath: Path.Combine(fullRoot, "extracted", "hud", "hud-ffdec-scripts", "scripts"),
            CharacterBioSourceGfxPath: Path.Combine(fullRoot, "extracted", "frontend", "probe", "CharacterBio.gfx"),
            CharacterBioScriptsPath: Path.Combine(fullRoot, "extracted", "frontend", "probe", "CharacterBio-scripts", "scripts"),
            FfdecPath: Path.GetFullPath(ffdecPath),
            HudWorkingScriptsPath: Path.Combine(tempDirectory, "hud-scripts"),
            CharacterBioWorkingScriptsPath: Path.Combine(tempDirectory, "characterbio-scripts"),
            HudOutputGfxPath: Path.Combine(fullOutput, "HUD-subtitle-route-probe.gfx"),
            CharacterBioOutputGfxPath: Path.Combine(fullOutput, "CharacterBio-subtitle-route-probe.gfx"),
            BmGameManifestPath: Path.Combine(fullOutput, "subtitle-route-probe.manifest.jsonc"));
    }
}

internal static class SubtitleRouteProbeAssetBuilder
{
    private const string HudProbeSubtitleScript = """
    class rs.hud.Subtitle extends MovieClip
    {
       var Text;
       static var Justify = Array("left","center","right","justify");
       static var FixedSide = Array("left","center","right");
       var bAlignBottom = false;
       function Subtitle()
       {
          super();
          this.Text.Text.wordWrap = true;
          this.Text._visible = false;
       }
       function SetAlignBottom(bInState)
       {
          this.bAlignBottom = bInState;
       }
       function GetProbePrefix()
       {
          if(this._name == "Subtitles")
          {
             return "HUD: ";
          }
          if(this._name == "InfoText")
          {
             return "INFO: ";
          }
          return "";
       }
       function SetText(InText, InSize, InJustify)
       {
          if(InText == "")
          {
             this.Text._visible = false;
             return undefined;
          }
          this.Text._visible = true;
          this.Text.Text.text = this.GetProbePrefix() + InText;
          this.Text.Text.autoSize = rs.hud.Subtitle.FixedSide[1];
          var _loc2_ = new TextFormat();
          _loc2_.align = rs.hud.Subtitle.Justify[InJustify];
          _loc2_.size = InSize;
          this.Text.Text.setTextFormat(_loc2_);
          rs.misc.Utils.setTextShadow(this.Text.Text,1,1,15,30,1,0);
          if(this.bAlignBottom)
          {
             this.Text._y = - this.Text.Text._height;
          }
       }
       function GetJustificationName(Index)
       {
          return rs.hud.Subtitle.Justify[Index];
       }
    }
    """;

    public static void Build(SubtitleRouteProbeBuildPaths paths)
    {
        ValidateInputs(paths);
        RecreateDirectory(paths.OutputDirectory);
        Directory.CreateDirectory(paths.TempDirectory);

        CopyDirectory(paths.HudScriptsPath, paths.HudWorkingScriptsPath);
        CopyDirectory(paths.CharacterBioScriptsPath, paths.CharacterBioWorkingScriptsPath);

        WriteAllText(
            Path.Combine(paths.HudWorkingScriptsPath, "__Packages", "rs", "hud", "Subtitle.as"),
            HudProbeSubtitleScript);

        PatchCharacterBioScript(
            Path.Combine(paths.CharacterBioWorkingScriptsPath, "__Packages", "rs", "bio", "bio.as"));

        RunProcess(paths.FfdecPath, "-importScript", paths.HudSourceGfxPath, paths.HudOutputGfxPath, paths.HudWorkingScriptsPath);
        RunProcess(paths.FfdecPath, "-importScript", paths.CharacterBioSourceGfxPath, paths.CharacterBioOutputGfxPath, paths.CharacterBioWorkingScriptsPath);

        ValidateSwfOutput(paths.HudOutputGfxPath, "HUD");
        ValidateSwfOutput(paths.CharacterBioOutputGfxPath, "CharacterBio");
        WriteBmGameManifest(paths.BmGameManifestPath);
    }

    private static void PatchCharacterBioScript(string bioScriptPath)
    {
        string script = File.ReadAllText(bioScriptPath);

        script = script.Replace(
            "this.CurrentSubtitle = SubtitleText;",
            "this.CurrentSubtitle = SubtitleText == \"\" ? \"\" : \"BIO: \" + SubtitleText;",
            StringComparison.Ordinal);

        script = script.Replace(
            "this.BioScreen.BioStats.pages.tapes.Subtitles.text = SubtitleText;",
            "this.BioScreen.BioStats.pages.tapes.Subtitles.text = this.CurrentSubtitle;",
            StringComparison.Ordinal);

        WriteAllText(bioScriptPath, script);
    }

    private static void WriteBmGameManifest(string manifestPath)
    {
        const string manifest = """
        {
          "name": "Subtitle route probe",
          "patches": [
            {
              "owner": "GameHUD",
              "exportName": "HUD",
              "exportType": "GFxMovieInfo",
              "replacementPath": "HUD-subtitle-route-probe.gfx",
              "payloadMagic": "GFX"
            },
            {
              "owner": "CharacterBio",
              "exportName": "CharacterBio",
              "exportType": "GFxMovieInfo",
              "replacementPath": "CharacterBio-subtitle-route-probe.gfx",
              "payloadMagic": "GFX"
            }
          ]
        }
        """;

        File.WriteAllText(manifestPath, manifest);
    }

    private static void ValidateInputs(SubtitleRouteProbeBuildPaths paths)
    {
        string[] requiredPaths =
        {
            paths.HudSourceGfxPath,
            paths.HudScriptsPath,
            paths.CharacterBioSourceGfxPath,
            paths.CharacterBioScriptsPath,
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

        return "\"" + value.Replace("\"", "\\\"", StringComparison.Ordinal) + "\"";
    }

    private static void ValidateSwfOutput(string path, string label)
    {
        var file = new FileInfo(path);
        if (!file.Exists)
        {
            throw new InvalidOperationException($"Expected FFDec to create '{path}' for {label}, but it does not exist.");
        }

        if (file.Length < 1024)
        {
            throw new InvalidOperationException($"FFDec created an implausibly small {label} movie at '{path}' ({file.Length} bytes).");
        }
    }
}
