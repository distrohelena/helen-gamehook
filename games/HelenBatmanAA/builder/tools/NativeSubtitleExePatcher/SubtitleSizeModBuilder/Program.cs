namespace SubtitleSizeModBuilder;

internal static class Program
{
    private static int Main(string[] args)
    {
        if (args.Length == 0)
        {
            PrintUsage();
            return 1;
        }

        try
        {
            string command = args[0];
            string[] tail = args[1..];

            return command switch
            {
                "build-assets" => RunBuildAssets(tail),
                "build-pause-runtime-scale" => RunBuildPauseRuntimeScale(tail),
                "build-hud-font-boost" => RunBuildHudFontBoost(tail),
                "build-hud-subtitle-probe" => RunBuildHudSubtitleProbe(tail),
                "build-subtitle-route-probe" => RunBuildSubtitleRouteProbe(tail),
                "help" or "--help" or "-h" => PrintHelpAndReturn(),
                _ => throw new InvalidOperationException($"Unknown command '{command}'.")
            };
        }
        catch (Exception ex)
        {
            Console.Error.WriteLine(ex.Message);
            return 2;
        }
    }

    private static int RunBuildAssets(string[] args)
    {
        var options = new ArgumentReader(args);
        string root = Path.GetFullPath(options.RequireValue("--root"));
        string outputDirectory = Path.GetFullPath(options.GetValue("--output-dir") ?? Path.Combine(root, "generated", "subtitle-size"));
        string ffdecPath = Path.GetFullPath(options.GetValue("--ffdec") ?? Path.Combine(root, "extracted", "ffdec", "ffdec-cli.exe"));
        string buildVersion = BuildVersionManager.Resolve(root, options.GetValue("--build-version"));
        options.ThrowIfAnyUnknown();

        BuildPaths paths = BuildPaths.FromRoot(root, ffdecPath, outputDirectory, buildVersion);
        SubtitleSizeAssetBuilder.Build(paths);

        Console.WriteLine($"Build version:      {paths.BuildVersion}");
        Console.WriteLine($"Built Pause asset: {paths.PauseOutputGfxPath}");
        Console.WriteLine($"Built HUD asset:   {paths.HudOutputGfxPath}");
        Console.WriteLine($"Built Frontend:    {paths.FrontendOutputGfxPath}");
        Console.WriteLine($"BmGame manifest:   {paths.BmGameManifestPath}");
        Console.WriteLine($"Frontend manifest: {paths.FrontendManifestPath}");

        return 0;
    }

    private static int RunBuildPauseRuntimeScale(string[] args)
    {
        var options = new ArgumentReader(args);
        string root = Path.GetFullPath(options.RequireValue("--root"));
        string outputDirectory = Path.GetFullPath(options.GetValue("--output-dir") ?? Path.Combine(root, "generated", "pause-runtime-scale"));
        string ffdecPath = Path.GetFullPath(options.GetValue("--ffdec") ?? Path.Combine(root, "extracted", "ffdec", "ffdec-cli.exe"));
        options.ThrowIfAnyUnknown();

        PauseRuntimeBuildPaths paths = PauseRuntimeBuildPaths.FromRoot(root, ffdecPath, outputDirectory);
        PauseRuntimeScaleAssetBuilder.Build(paths);

        Console.WriteLine($"Built Pause asset:  {paths.PauseOutputGfxPath}");
        Console.WriteLine($"Built HUD asset:    {paths.HudOutputGfxPath}");
        Console.WriteLine($"BmGame manifest:    {paths.BmGameManifestPath}");
        return 0;
    }

    private static int RunBuildHudFontBoost(string[] args)
    {
        var options = new ArgumentReader(args);
        string root = Path.GetFullPath(options.RequireValue("--root"));
        string outputDirectory = Path.GetFullPath(options.GetValue("--output-dir") ?? Path.Combine(root, "generated", "hud-font-boost"));
        string ffdecPath = Path.GetFullPath(options.GetValue("--ffdec") ?? Path.Combine(root, "extracted", "ffdec", "ffdec-cli.exe"));
        options.ThrowIfAnyUnknown();

        HudFontBuildPaths paths = HudFontBuildPaths.FromRoot(root, ffdecPath, outputDirectory);
        HudFontBoostAssetBuilder.Build(paths);

        Console.WriteLine($"Built HUD asset:     {paths.HudOutputGfxPath}");
        Console.WriteLine($"BmGame manifest:     {paths.BmGameManifestPath}");

        return 0;
    }

    private static int RunBuildHudSubtitleProbe(string[] args)
    {
        var options = new ArgumentReader(args);
        string root = Path.GetFullPath(options.RequireValue("--root"));
        string outputDirectory = Path.GetFullPath(options.GetValue("--output-dir") ?? Path.Combine(root, "generated", "hud-subtitle-probe"));
        string ffdecPath = Path.GetFullPath(options.GetValue("--ffdec") ?? Path.Combine(root, "extracted", "ffdec", "ffdec-cli.exe"));
        options.ThrowIfAnyUnknown();

        HudSubtitleProbeBuildPaths paths = HudSubtitleProbeBuildPaths.FromRoot(root, ffdecPath, outputDirectory);
        HudSubtitleProbeAssetBuilder.Build(paths);

        Console.WriteLine($"Built HUD probe:      {paths.HudOutputGfxPath}");
        Console.WriteLine($"BmGame manifest:      {paths.BmGameManifestPath}");

        return 0;
    }

    private static int RunBuildSubtitleRouteProbe(string[] args)
    {
        var options = new ArgumentReader(args);
        string root = Path.GetFullPath(options.RequireValue("--root"));
        string outputDirectory = Path.GetFullPath(options.GetValue("--output-dir") ?? Path.Combine(root, "generated", "subtitle-route-probe"));
        string ffdecPath = Path.GetFullPath(options.GetValue("--ffdec") ?? Path.Combine(root, "extracted", "ffdec", "ffdec-cli.exe"));
        options.ThrowIfAnyUnknown();

        SubtitleRouteProbeBuildPaths paths = SubtitleRouteProbeBuildPaths.FromRoot(root, ffdecPath, outputDirectory);
        SubtitleRouteProbeAssetBuilder.Build(paths);

        Console.WriteLine($"Built HUD probe:      {paths.HudOutputGfxPath}");
        Console.WriteLine($"Built Bio probe:      {paths.CharacterBioOutputGfxPath}");
        Console.WriteLine($"BmGame manifest:      {paths.BmGameManifestPath}");

        return 0;
    }

    private static int PrintHelpAndReturn()
    {
        PrintUsage();
        return 0;
    }

    private static void PrintUsage()
    {
        Console.WriteLine("SubtitleSizeModBuilder");
        Console.WriteLine();
        Console.WriteLine("Commands:");
        Console.WriteLine("  build-assets --root <batman-builder-root> [--output-dir <generated\\subtitle-size>] [--ffdec <extracted\\ffdec\\ffdec-cli.exe>] [--build-version <label>]");
        Console.WriteLine("  build-pause-runtime-scale --root <batman-builder-root> [--output-dir <generated\\pause-runtime-scale>] [--ffdec <extracted\\ffdec\\ffdec-cli.exe>]");
        Console.WriteLine("  build-hud-font-boost --root <batman-builder-root> [--output-dir <generated\\hud-font-boost>] [--ffdec <extracted\\ffdec\\ffdec-cli.exe>]");
        Console.WriteLine("  build-hud-subtitle-probe --root <batman-builder-root> [--output-dir <generated\\hud-subtitle-probe>] [--ffdec <extracted\\ffdec\\ffdec-cli.exe>]");
        Console.WriteLine("  build-subtitle-route-probe --root <batman-builder-root> [--output-dir <generated\\subtitle-route-probe>] [--ffdec <extracted\\ffdec\\ffdec-cli.exe>]");
    }
}

internal sealed class ArgumentReader
{
    private readonly Dictionary<string, string?> values = new(StringComparer.Ordinal);
    private readonly HashSet<string> consumed = new(StringComparer.Ordinal);

    public ArgumentReader(string[] args)
    {
        for (int index = 0; index < args.Length; index++)
        {
            string key = args[index];

            if (!key.StartsWith("--", StringComparison.Ordinal))
            {
                throw new InvalidOperationException($"Unexpected argument '{key}'.");
            }

            string? value = null;
            if (index + 1 < args.Length && !args[index + 1].StartsWith("--", StringComparison.Ordinal))
            {
                value = args[index + 1];
                index++;
            }

            values[key] = value;
        }
    }

    public string RequireValue(string key)
    {
        if (!values.TryGetValue(key, out string? value) || string.IsNullOrWhiteSpace(value))
        {
            throw new InvalidOperationException($"Missing required option '{key}'.");
        }

        consumed.Add(key);
        return value;
    }

    public string? GetValue(string key)
    {
        if (!values.TryGetValue(key, out string? value))
        {
            return null;
        }

        consumed.Add(key);
        return value;
    }

    public void ThrowIfAnyUnknown()
    {
        string[] unknown = values.Keys.Where(key => !consumed.Contains(key)).OrderBy(key => key, StringComparer.Ordinal).ToArray();

        if (unknown.Length > 0)
        {
            throw new InvalidOperationException($"Unknown option(s): {string.Join(", ", unknown)}");
        }
    }
}
