namespace BmGameGfxPatcher;

/// <summary>
/// Applies the direct MainMenu.MainV2 graphics-options patch to one Unreal frontend package.
/// </summary>
internal static class MainMenuGraphicsOptionsPackagePatcher
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
    /// Patches one frontend package so the main-menu movie is replaced with the generated graphics-options prototype payload.
    /// </summary>
    /// <param name="packagePath">Path to the source frontend package.</param>
    /// <param name="outputPath">Destination package path that receives the patched package.</param>
    /// <param name="prototypeGfxPath">Path to the generated graphics-options prototype MainV2 GFX file.</param>
    public static void PatchPackage(string packagePath, string outputPath, string prototypeGfxPath)
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
        byte[] patchedPayload = MainMenuGraphicsOptionsGfxPatcher.Patch(originalPayload, prototypeGfxPath);

        string tempDirectory = Path.Combine(Path.GetTempPath(), "batman-mainv2-graphics-options-" + Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(tempDirectory);

        try
        {
            string payloadPath = Path.Combine(tempDirectory, "MainV2-graphics-options.gfx");
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
            Name = "MainMenu MainV2 graphics-options patch",
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

        manifest.Initialize(Path.Combine(manifestDirectory, "generated-mainv2-graphics-options.manifest.jsonc"));
        return manifest;
    }
}
