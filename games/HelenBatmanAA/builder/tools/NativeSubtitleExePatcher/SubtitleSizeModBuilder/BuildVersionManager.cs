using System.Globalization;

namespace SubtitleSizeModBuilder;

internal static class BuildVersionManager
{
    private const string InitialBuildLabel = "v1.1.1 HELENA ARKHAM FIX";

    public static string Resolve(string rootPath, string? requestedVersion)
    {
        if (!string.IsNullOrWhiteSpace(requestedVersion))
        {
            return requestedVersion.Trim();
        }

        string statePath = Path.Combine(rootPath, "generated", "subtitle-size-build-number.txt");
        Directory.CreateDirectory(Path.GetDirectoryName(statePath)!);

        int buildNumber = 1;
        if (File.Exists(statePath))
        {
            string raw = File.ReadAllText(statePath).Trim();
            if (int.TryParse(raw, NumberStyles.Integer, CultureInfo.InvariantCulture, out int existing) && existing >= 1)
            {
                buildNumber = existing + 1;
            }
        }

        File.WriteAllText(statePath, buildNumber.ToString(CultureInfo.InvariantCulture));
        return buildNumber == 1 ? InitialBuildLabel : $"v1.1.{buildNumber}";
    }
}
