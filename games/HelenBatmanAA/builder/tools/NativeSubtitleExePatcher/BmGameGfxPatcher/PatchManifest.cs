namespace BmGameGfxPatcher;

internal sealed class PatchManifest
{
    public string? Name { get; init; }

    public List<GfxPatchDefinition> Patches { get; init; } = [];

    public string? SourcePath { get; private set; }

    public string? SourceDirectory { get; private set; }

    public void Initialize(string sourcePath)
    {
        SourcePath = Path.GetFullPath(sourcePath);
        SourceDirectory = Path.GetDirectoryName(SourcePath) ?? Directory.GetCurrentDirectory();

        if (Patches.Count == 0)
        {
            throw new InvalidOperationException("Manifest contains no patches.");
        }

        var duplicateTargets = Patches
            .GroupBy(
                patch => $"{patch.ExportType}|{patch.Owner}|{patch.ExportName}",
                StringComparer.Ordinal)
            .Where(group => group.Count() > 1)
            .Select(group => group.Key)
            .ToArray();

        if (duplicateTargets.Length > 0)
        {
            throw new InvalidOperationException($"Manifest contains duplicate export targets: {string.Join(", ", duplicateTargets)}");
        }

        foreach (GfxPatchDefinition patch in Patches)
        {
            patch.Initialize(SourceDirectory);
        }
    }
}

internal sealed class GfxPatchDefinition
{
    public string PatchMode { get; init; } = "gfx";

    public string Owner { get; init; } = string.Empty;

    public string ExportName { get; init; } = string.Empty;

    public string ExportType { get; init; } = "GFxMovieInfo";

    public string? ReplacementPath { get; init; }

    public string PayloadMagic { get; init; } = "GFX";

    public string? ExpectedOriginalSha256 { get; init; }

    public int? ExpectedOriginalLength { get; init; }

    public string? OutputObjectPath { get; init; }

    public List<BytePatchOperation> BytePatches { get; init; } = [];

    public string? ResolvedReplacementPath { get; private set; }

    public string? ResolvedOutputObjectPath { get; private set; }

    public void Initialize(string manifestDirectory)
    {
        if (!string.Equals(PatchMode, "gfx", StringComparison.OrdinalIgnoreCase) &&
            !string.Equals(PatchMode, "raw", StringComparison.OrdinalIgnoreCase) &&
            !string.Equals(PatchMode, "bytepatch", StringComparison.OrdinalIgnoreCase))
        {
            throw new InvalidOperationException("Every patch must define 'patchMode' as either 'gfx', 'raw', or 'bytepatch'.");
        }

        if (string.IsNullOrWhiteSpace(Owner))
        {
            throw new InvalidOperationException("Every patch must define 'owner'.");
        }

        if (string.IsNullOrWhiteSpace(ExportName))
        {
            throw new InvalidOperationException("Every patch must define 'exportName'.");
        }

        if (string.IsNullOrWhiteSpace(ExportType))
        {
            throw new InvalidOperationException("Every patch must define 'exportType'.");
        }

        if ((string.Equals(PatchMode, "gfx", StringComparison.OrdinalIgnoreCase) ||
             string.Equals(PatchMode, "raw", StringComparison.OrdinalIgnoreCase)) &&
            string.IsNullOrWhiteSpace(ReplacementPath))
        {
            throw new InvalidOperationException("Every patch must define 'replacementPath'.");
        }

        if (string.Equals(PatchMode, "gfx", StringComparison.OrdinalIgnoreCase) &&
            string.IsNullOrWhiteSpace(PayloadMagic))
        {
            throw new InvalidOperationException("Every patch must define 'payloadMagic'.");
        }

        if (!string.IsNullOrWhiteSpace(ReplacementPath))
        {
            ResolvedReplacementPath = ResolvePath(manifestDirectory, ReplacementPath);

            if (!File.Exists(ResolvedReplacementPath))
            {
                throw new FileNotFoundException($"Replacement GFX file was not found: {ResolvedReplacementPath}");
            }
        }

        if (string.Equals(PatchMode, "bytepatch", StringComparison.OrdinalIgnoreCase))
        {
            if (BytePatches.Count == 0)
            {
                throw new InvalidOperationException("Bytepatch mode requires at least one 'bytePatches' entry.");
            }

            foreach (BytePatchOperation bytePatch in BytePatches)
            {
                bytePatch.Initialize();
            }
        }

        if (!string.IsNullOrWhiteSpace(OutputObjectPath))
        {
            ResolvedOutputObjectPath = ResolvePath(manifestDirectory, OutputObjectPath);
        }
    }

    private static string ResolvePath(string baseDirectory, string path)
    {
        return Path.GetFullPath(path, baseDirectory);
    }
}

internal sealed class BytePatchOperation
{
    public string Section { get; init; } = "object";

    public int Offset { get; init; }

    public string ExpectedHex { get; init; } = string.Empty;

    public string ReplacementHex { get; init; } = string.Empty;

    public byte[] ExpectedBytes { get; private set; } = [];

    public byte[] ReplacementBytes { get; private set; } = [];

    public void Initialize()
    {
        if (!string.Equals(Section, "object", StringComparison.OrdinalIgnoreCase) &&
            !string.Equals(Section, "script", StringComparison.OrdinalIgnoreCase))
        {
            throw new InvalidOperationException("Each byte patch must define 'section' as either 'object' or 'script'.");
        }

        if (Offset < 0)
        {
            throw new InvalidOperationException("Each byte patch must define a non-negative 'offset'.");
        }

        ExpectedBytes = HexEncoding.ParseHex(ExpectedHex);
        ReplacementBytes = HexEncoding.ParseHex(ReplacementHex);

        if (ExpectedBytes.Length == 0)
        {
            throw new InvalidOperationException("Each byte patch must define a non-empty 'expectedHex'.");
        }

        if (ReplacementBytes.Length != ExpectedBytes.Length)
        {
            throw new InvalidOperationException("Each byte patch must replace the same number of bytes it expects.");
        }
    }
}

internal sealed record AppliedPatch(
    GfxPatchDefinition Definition,
    ExportEntry Export,
    int PatchedObjectOffset,
    int PatchedObjectLength,
    string PatchedObjectSha256);

internal sealed record PatchApplicationResult(
    IReadOnlyList<AppliedPatch> AppliedPatches,
    bool Verified);
