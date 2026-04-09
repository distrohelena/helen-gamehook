namespace BmGameGfxPatcher;

/// <summary>
/// Applies the direct MainMenu.MainV2 version-label patch to one Unreal frontend package.
/// </summary>
internal static class MainMenuVersionLabelPackagePatcher
{
    /// <summary>
    /// Stores the export owner that contains the main-menu movie.
    /// </summary>
    private const string MovieOwner = "MainMenu";

    /// <summary>
    /// Stores the export object name for the main-menu movie.
    /// </summary>
    private const string MovieName = "MainV2";

    /// <summary>
    /// Stores the Unreal export type used by the main-menu movie.
    /// </summary>
    private const string MovieExportType = "GFxMovieInfo";

    /// <summary>
    /// Stores the embedded payload marker used by Scaleform movies in Unreal exports.
    /// </summary>
    private const string PayloadMagic = "GFX";

    /// <summary>
    /// Patches one frontend package so the main-menu version label shows the requested build label.
    /// </summary>
    /// <param name="packagePath">Path to the source frontend package.</param>
    /// <param name="outputPath">Destination package path that receives the patched package.</param>
    /// <param name="buildLabel">Build label to show on the main menu.</param>
    public static void PatchPackage(string packagePath, string outputPath, string buildLabel)
    {
        string fullPackagePath = Path.GetFullPath(packagePath);
        string fullOutputPath = Path.GetFullPath(outputPath);

        if (string.Equals(fullPackagePath, fullOutputPath, StringComparison.OrdinalIgnoreCase))
        {
            throw new InvalidOperationException("Refusing to patch in place. Write to a separate output path.");
        }

        UnrealPackage package = UnrealPackage.Load(fullPackagePath);
        ExportEntry exportEntry = package.FindExport(MovieOwner, MovieName, MovieExportType);
        ReadOnlyMemory<byte> originalObject = package.ReadObjectBytes(exportEntry);
        byte[] originalPayload = EmbeddedPayloadExtractor.ExtractPayloadBytes(originalObject.Span, PayloadMagic);
        byte[] patchedPayload = MainMenuVersionLabelGfxPatcher.Patch(originalPayload, buildLabel);

        string tempDirectory = Path.Combine(Path.GetTempPath(), "batman-mainv2-version-label-" + Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(tempDirectory);

        try
        {
            string payloadPath = Path.Combine(tempDirectory, "MainV2-version-label.gfx");
            File.WriteAllBytes(payloadPath, patchedPayload);

            PatchManifest manifest = CreatePatchManifest(originalObject.Span, payloadPath, tempDirectory);
            GfxPatchApplier.ApplyManifest(fullPackagePath, fullOutputPath, manifest, verifyOutput: true);
        }
        finally
        {
            if (Directory.Exists(tempDirectory))
            {
                Directory.Delete(tempDirectory, recursive: true);
            }
        }
    }

    /// <summary>
    /// Builds the temporary one-export manifest used to inject the patched MainV2 payload back into the package.
    /// </summary>
    /// <param name="originalObject">Original serialized GFxMovieInfo export bytes.</param>
    /// <param name="payloadPath">Path to the patched MainV2 payload file.</param>
    /// <param name="manifestDirectory">Temporary manifest directory used to resolve absolute paths.</param>
    /// <returns>Initialized patch manifest ready for application.</returns>
    private static PatchManifest CreatePatchManifest(
        ReadOnlySpan<byte> originalObject,
        string payloadPath,
        string manifestDirectory)
    {
        var manifest = new PatchManifest
        {
            Name = "MainMenu MainV2 version label patch",
            Patches =
            [
                new GfxPatchDefinition
                {
                    PatchMode = "gfx",
                    Owner = MovieOwner,
                    ExportName = MovieName,
                    ExportType = MovieExportType,
                    ReplacementPath = payloadPath,
                    PayloadMagic = PayloadMagic,
                    ExpectedOriginalLength = originalObject.Length,
                    ExpectedOriginalSha256 = Hashing.Sha256Hex(originalObject)
                }
            ]
        };

        manifest.Initialize(Path.Combine(manifestDirectory, "generated-mainv2-version-label.manifest.jsonc"));
        return manifest;
    }
}
