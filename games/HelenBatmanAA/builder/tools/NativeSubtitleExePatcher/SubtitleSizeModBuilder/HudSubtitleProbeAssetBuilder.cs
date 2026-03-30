using System.Diagnostics;

namespace SubtitleSizeModBuilder;

internal sealed record HudSubtitleProbeBuildPaths(
    string RootPath,
    string OutputDirectory,
    string TempDirectory,
    string HudSourceGfxPath,
    string HudScriptsPath,
    string FfdecPath,
    string HudWorkingScriptsPath,
    string HudOutputGfxPath,
    string BmGameManifestPath)
{
    public static HudSubtitleProbeBuildPaths FromRoot(string root, string ffdecPath, string outputDirectory)
    {
        string fullRoot = Path.GetFullPath(root);
        string fullOutput = Path.GetFullPath(outputDirectory);
        string tempDirectory = Path.Combine(fullOutput, "_build");

        return new HudSubtitleProbeBuildPaths(
            RootPath: fullRoot,
            OutputDirectory: fullOutput,
            TempDirectory: tempDirectory,
            HudSourceGfxPath: Path.Combine(fullRoot, "extracted", "hud", "HUD-extracted.gfx"),
            HudScriptsPath: Path.Combine(fullRoot, "extracted", "hud", "hud-ffdec-scripts", "scripts"),
            FfdecPath: Path.GetFullPath(ffdecPath),
            HudWorkingScriptsPath: Path.Combine(tempDirectory, "hud-scripts"),
            HudOutputGfxPath: Path.Combine(fullOutput, "HUD-subtitle-probe.gfx"),
            BmGameManifestPath: Path.Combine(fullOutput, "hud-subtitle-probe.manifest.jsonc"));
    }
}

internal static class HudSubtitleProbeAssetBuilder
{
    private const string ProbeSubtitleScript = """
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
             return "HUD:";
          }
          if(this._name == "InfoText")
          {
             return "INFO:";
          }
          return "TXT:";
       }
       function GetProbeSize()
       {
          if(this._name == "Subtitles")
          {
             return 64;
          }
          if(this._name == "InfoText")
          {
             return 44;
          }
          return 36;
       }
       function GetProbeScale()
       {
          if(this._name == "Subtitles")
          {
             return 220;
          }
          if(this._name == "InfoText")
          {
             return 180;
          }
          return 150;
       }
       function GetProbeColor()
       {
          if(this._name == "Subtitles")
          {
             return 16763904;
          }
          if(this._name == "InfoText")
          {
             return 65535;
          }
          return 16777215;
       }
       function SetText(InText, InSize, InJustify)
       {
          var _loc2_;
          if(InText == "")
          {
             this.Text._visible = false;
             return undefined;
          }
          this.Text._visible = true;
          this.Text.Text.text = this.GetProbePrefix() + " " + InText;
          this.Text.Text.autoSize = rs.hud.Subtitle.FixedSide[1];
          _loc2_ = new TextFormat();
          _loc2_.align = rs.hud.Subtitle.Justify[InJustify];
          _loc2_.size = this.GetProbeSize();
          _loc2_.color = this.GetProbeColor();
          this.Text.Text.setNewTextFormat(_loc2_);
          this.Text.Text.setTextFormat(_loc2_);
          this.Text.Text.textColor = this.GetProbeColor();
          this.Text._xscale = this.GetProbeScale();
          this.Text._yscale = this.GetProbeScale();
          rs.misc.Utils.setTextShadow(this.Text.Text,2,2,15,30,1,0);
          if(this.bAlignBottom)
          {
             this.Text._y = - this.Text._height;
          }
       }
       function GetJustificationName(Index)
       {
          return rs.hud.Subtitle.Justify[Index];
       }
    }
    """;

    public static void Build(HudSubtitleProbeBuildPaths paths)
    {
        ValidateInputs(paths);
        RecreateDirectory(paths.OutputDirectory);
        Directory.CreateDirectory(paths.TempDirectory);
        CopyDirectory(paths.HudScriptsPath, paths.HudWorkingScriptsPath);

        WriteAllText(
            Path.Combine(paths.HudWorkingScriptsPath, "__Packages", "rs", "hud", "Subtitle.as"),
            ProbeSubtitleScript);

        RunProcess(paths.FfdecPath, "-importScript", paths.HudSourceGfxPath, paths.HudOutputGfxPath, paths.HudWorkingScriptsPath);
        ValidateSwfOutput(paths.HudOutputGfxPath);
        WriteBmGameManifest(paths.BmGameManifestPath);
    }

    private static void WriteBmGameManifest(string manifestPath)
    {
        const string manifest = """
        {
          "name": "HUD subtitle probe",
          "patches": [
            {
              "owner": "GameHUD",
              "exportName": "HUD",
              "exportType": "GFxMovieInfo",
              "replacementPath": "HUD-subtitle-probe.gfx",
              "payloadMagic": "GFX"
            }
          ]
        }
        """;

        File.WriteAllText(manifestPath, manifest);
    }

    private static void ValidateInputs(HudSubtitleProbeBuildPaths paths)
    {
        string[] requiredPaths =
        {
            paths.HudSourceGfxPath,
            paths.HudScriptsPath,
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

    private static void ValidateSwfOutput(string path)
    {
        var file = new FileInfo(path);
        if (!file.Exists)
        {
            throw new InvalidOperationException($"Expected FFDec to create '{path}', but it does not exist.");
        }

        if (file.Length < 1024)
        {
            throw new InvalidOperationException($"FFDec created an implausibly small HUD probe movie at '{path}' ({file.Length} bytes).");
        }
    }
}
