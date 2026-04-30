namespace SubtitleSizeModBuilder;

/// <summary>
/// Loads Batman user graphics settings from <c>BmEngine.ini</c> and normalizes them to the frontend row state model.
/// </summary>
internal static class BatmanGraphicsIniBootstrapLoader
{
    /// <summary>
    /// Gets the default Batman user <c>BmEngine.ini</c> path under the current user's Documents folder.
    /// </summary>
    /// <returns>The absolute default user configuration path.</returns>
    public static string GetDefaultIniPath()
    {
        string documentsPath = Environment.GetFolderPath(Environment.SpecialFolder.MyDocuments);
        return Path.Combine(documentsPath, "Square Enix", "Batman Arkham Asylum GOTY", "BmGame", "Config", "BmEngine.ini");
    }

    /// <summary>
    /// Loads and normalizes the graphics settings from the specified user configuration file.
    /// </summary>
    /// <param name="iniPath">The absolute or relative path to the Batman user <c>BmEngine.ini</c> file.</param>
    /// <returns>The normalized bootstrap snapshot used by the frontend builder.</returns>
    public static BatmanGraphicsIniBootstrapSnapshot Load(string iniPath)
    {
        string fullIniPath = Path.GetFullPath(iniPath);
        if (!File.Exists(fullIniPath))
        {
            throw new InvalidOperationException($"Batman user INI not found: {fullIniPath}");
        }

        Dictionary<string, Dictionary<string, string>> sections = ParseSections(fullIniPath);

        int fullscreen = ReadRequiredBoolAsInt(sections, "SystemSettings", "Fullscreen");
        int resolutionWidth = ReadRequiredInt(sections, "SystemSettings", "ResX");
        int resolutionHeight = ReadRequiredInt(sections, "SystemSettings", "ResY");
        int vsync = ReadRequiredBoolAsInt(sections, "SystemSettings", "UseVsync");
        int msaa = NormalizeMsaa(ReadRequiredInt(sections, "SystemSettings", "MaxMultisamples"));
        int bloom = ReadRequiredBoolAsInt(sections, "SystemSettings", "Bloom");
        int dynamicShadows = ReadRequiredBoolAsInt(sections, "SystemSettings", "DynamicShadows");
        int motionBlur = ReadRequiredBoolAsInt(sections, "SystemSettings", "MotionBlur");
        int distortion = ReadRequiredBoolAsInt(sections, "SystemSettings", "Distortion");
        int fogVolumes = ReadRequiredBoolAsInt(sections, "SystemSettings", "FogVolumes");
        int sphericalHarmonicLighting = NormalizeSphericalHarmonicLighting(ReadRequiredBoolAsInt(sections, "SystemSettings", "DisableSphericalHarmonicLights"));
        int ambientOcclusion = ReadRequiredBoolAsInt(sections, "SystemSettings", "AmbientOcclusion");
        int stereo = ReadRequiredBoolAsInt(sections, "SystemSettings", "Stereo");
        int physx = ReadRequiredInt(sections, "Engine.Engine", "PhysXLevel");
        int detailLevel = ResolveDetailLevel(
            bloom,
            dynamicShadows,
            motionBlur,
            distortion,
            fogVolumes,
            sphericalHarmonicLighting,
            ambientOcclusion);

        return new BatmanGraphicsIniBootstrapSnapshot(
            fullscreen,
            resolutionWidth,
            resolutionHeight,
            vsync,
            msaa,
            detailLevel,
            bloom,
            dynamicShadows,
            motionBlur,
            distortion,
            fogVolumes,
            sphericalHarmonicLighting,
            ambientOcclusion,
            physx,
            stereo);
    }

    /// <summary>
    /// Parses an INI file into a section/key/value map while preserving exact key text.
    /// </summary>
    /// <param name="iniPath">The path to the INI file.</param>
    /// <returns>The parsed section map.</returns>
    private static Dictionary<string, Dictionary<string, string>> ParseSections(string iniPath)
    {
        var sections = new Dictionary<string, Dictionary<string, string>>(StringComparer.OrdinalIgnoreCase);
        Dictionary<string, string>? currentSection = null;

        foreach (string rawLine in File.ReadAllLines(iniPath))
        {
            string line = rawLine.Trim();
            if (line.Length == 0 || line.StartsWith(';') || line.StartsWith('#'))
            {
                continue;
            }

            if (line.StartsWith('[') && line.EndsWith(']'))
            {
                string sectionName = line[1..^1].Trim();
                if (sectionName.Length == 0)
                {
                    throw new InvalidOperationException($"Encountered an empty INI section name in '{iniPath}'.");
                }

                currentSection = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
                sections[sectionName] = currentSection;
                continue;
            }

            if (currentSection is null)
            {
                throw new InvalidOperationException($"Encountered an INI key before any section header in '{iniPath}': {line}");
            }

            int equalsIndex = line.IndexOf('=');
            if (equalsIndex <= 0)
            {
                throw new InvalidOperationException($"Encountered an invalid INI assignment in '{iniPath}': {line}");
            }

            string key = line[..equalsIndex].Trim();
            string value = line[(equalsIndex + 1)..].Trim();
            if (key.Length == 0)
            {
                throw new InvalidOperationException($"Encountered an empty INI key in '{iniPath}': {line}");
            }

            currentSection[key] = value;
        }

        return sections;
    }

    /// <summary>
    /// Reads a required integer from the parsed section map.
    /// </summary>
    /// <param name="sections">The parsed INI section map.</param>
    /// <param name="sectionName">The required section name.</param>
    /// <param name="keyName">The required key name.</param>
    /// <returns>The parsed integer value.</returns>
    private static int ReadRequiredInt(Dictionary<string, Dictionary<string, string>> sections, string sectionName, string keyName)
    {
        string rawValue = ReadRequiredValue(sections, sectionName, keyName);
        if (!int.TryParse(rawValue, out int value))
        {
            throw new InvalidOperationException($"INI value '{sectionName}.{keyName}' must be an integer but was '{rawValue}'.");
        }

        return value;
    }

    /// <summary>
    /// Reads a required boolean-like value from the parsed section map and normalizes it to <c>0</c> or <c>1</c>.
    /// </summary>
    /// <param name="sections">The parsed INI section map.</param>
    /// <param name="sectionName">The required section name.</param>
    /// <param name="keyName">The required key name.</param>
    /// <returns><c>1</c> when the value is enabled; otherwise <c>0</c>.</returns>
    private static int ReadRequiredBoolAsInt(Dictionary<string, Dictionary<string, string>> sections, string sectionName, string keyName)
    {
        string rawValue = ReadRequiredValue(sections, sectionName, keyName);

        if (rawValue.Equals("True", StringComparison.OrdinalIgnoreCase) || rawValue == "1")
        {
            return 1;
        }

        if (rawValue.Equals("False", StringComparison.OrdinalIgnoreCase) || rawValue == "0")
        {
            return 0;
        }

        throw new InvalidOperationException($"INI value '{sectionName}.{keyName}' must be a boolean-like value but was '{rawValue}'.");
    }

    /// <summary>
    /// Reads a required raw value from the parsed section map.
    /// </summary>
    /// <param name="sections">The parsed INI section map.</param>
    /// <param name="sectionName">The required section name.</param>
    /// <param name="keyName">The required key name.</param>
    /// <returns>The raw string value.</returns>
    private static string ReadRequiredValue(Dictionary<string, Dictionary<string, string>> sections, string sectionName, string keyName)
    {
        if (!sections.TryGetValue(sectionName, out Dictionary<string, string>? sectionValues))
        {
            throw new InvalidOperationException($"INI section '{sectionName}' was not found.");
        }

        if (!sectionValues.TryGetValue(keyName, out string? value))
        {
            throw new InvalidOperationException($"INI key '{sectionName}.{keyName}' was not found.");
        }

        return value;
    }

    /// <summary>
    /// Normalizes the raw Batman MSAA sample count to the frontend row index.
    /// </summary>
    /// <param name="rawMsaaSamples">The raw sample count from the INI.</param>
    /// <returns>The frontend MSAA index.</returns>
    private static int NormalizeMsaa(int rawMsaaSamples)
    {
        if (rawMsaaSamples <= 1)
        {
            return 0;
        }

        if (rawMsaaSamples == 2)
        {
            return 1;
        }

        if (rawMsaaSamples == 4)
        {
            return 2;
        }

        if (rawMsaaSamples == 8)
        {
            return 3;
        }

        throw new InvalidOperationException($"Unsupported MaxMultisamples value '{rawMsaaSamples}'.");
    }

    /// <summary>
    /// Converts Batman's disable flag into the frontend enable-state toggle for spherical harmonic lighting.
    /// </summary>
    /// <param name="disableFlag">The raw disable flag where <c>1</c> means disabled.</param>
    /// <returns>The normalized enable-state toggle where <c>1</c> means enabled.</returns>
    private static int NormalizeSphericalHarmonicLighting(int disableFlag)
    {
        return disableFlag == 0 ? 1 : 0;
    }

    /// <summary>
    /// Resolves the frontend detail-level preset from the normalized quality toggles.
    /// </summary>
    /// <param name="bloom">The normalized bloom toggle.</param>
    /// <param name="dynamicShadows">The normalized dynamic-shadows toggle.</param>
    /// <param name="motionBlur">The normalized motion-blur toggle.</param>
    /// <param name="distortion">The normalized distortion toggle.</param>
    /// <param name="fogVolumes">The normalized fog-volumes toggle.</param>
    /// <param name="sphericalHarmonicLighting">The normalized spherical-harmonic-lighting toggle.</param>
    /// <param name="ambientOcclusion">The normalized ambient-occlusion toggle.</param>
    /// <returns>The frontend preset index or <c>4</c> when the combination is custom.</returns>
    private static int ResolveDetailLevel(
        int bloom,
        int dynamicShadows,
        int motionBlur,
        int distortion,
        int fogVolumes,
        int sphericalHarmonicLighting,
        int ambientOcclusion)
    {
        if (bloom == 0 &&
            dynamicShadows == 0 &&
            motionBlur == 0 &&
            distortion == 0 &&
            fogVolumes == 0 &&
            sphericalHarmonicLighting == 0 &&
            ambientOcclusion == 0)
        {
            return 0;
        }

        if (bloom == 1 &&
            dynamicShadows == 1 &&
            motionBlur == 0 &&
            distortion == 0 &&
            fogVolumes == 0 &&
            sphericalHarmonicLighting == 0 &&
            ambientOcclusion == 0)
        {
            return 1;
        }

        if (bloom == 1 &&
            dynamicShadows == 1 &&
            motionBlur == 1 &&
            distortion == 1 &&
            fogVolumes == 1 &&
            sphericalHarmonicLighting == 1 &&
            ambientOcclusion == 0)
        {
            return 2;
        }

        if (bloom == 1 &&
            dynamicShadows == 1 &&
            motionBlur == 1 &&
            distortion == 1 &&
            fogVolumes == 1 &&
            sphericalHarmonicLighting == 1 &&
            ambientOcclusion == 1)
        {
            return 3;
        }

        return 4;
    }
}
