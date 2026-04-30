namespace SubtitleSizeModBuilder;

/// <summary>
/// Resolves filesystem locations used by the graphics-options prototype builder.
/// </summary>
/// <param name="RootPath">Absolute path to the Batman builder workspace root.</param>
/// <param name="OutputDirectory">Absolute directory where generated graphics artifacts are written.</param>
/// <param name="TempDirectory">Absolute temporary working directory used during staging and XML patching.</param>
/// <param name="FrontendXmlPath">Absolute path to the extracted frontend MainV2 XML input.</param>
/// <param name="FrontendSourceGfxPath">Absolute path to the extracted frontend MainV2 GFX input.</param>
/// <param name="FrontendScriptsPath">Absolute path to the extracted frontend MainV2 FFDec script tree.</param>
/// <param name="FfdecPath">Absolute path to the FFDec CLI executable.</param>
/// <param name="BatmanUserIniPath">Absolute path to the Batman user <c>BmEngine.ini</c> file used for bootstrap defaults.</param>
/// <param name="FrontendWorkingScriptsPath">Absolute path to the writable staged frontend script tree.</param>
/// <param name="FrontendPatchedXmlPath">Absolute path where the patched MainV2 XML is emitted.</param>
/// <param name="FrontendStructuralGfxPath">Absolute path where XML-to-SWF structural output is emitted before script import.</param>
/// <param name="FrontendOutputGfxPath">Absolute path to the final rebuilt graphics-options MainV2 GFX output.</param>
internal sealed record GraphicsOptionsBuildPaths(
    string RootPath,
    string OutputDirectory,
    string TempDirectory,
    string FrontendXmlPath,
    string FrontendSourceGfxPath,
    string FrontendScriptsPath,
    string FfdecPath,
    string BatmanUserIniPath,
    string FrontendWorkingScriptsPath,
    string FrontendPatchedXmlPath,
    string FrontendStructuralGfxPath,
    string FrontendOutputGfxPath)
{
    /// <summary>
    /// Creates a fully-resolved graphics-options build layout from the requested workspace root and output paths.
    /// </summary>
    /// <param name="root">The Batman builder workspace root.</param>
    /// <param name="ffdecPath">The FFDec CLI executable path.</param>
    /// <param name="outputDirectory">The output directory for generated graphics artifacts.</param>
    /// <param name="batmanUserIniPath">The Batman user <c>BmEngine.ini</c> path used for bootstrap defaults.</param>
    /// <returns>The resolved graphics-options build path set.</returns>
    public static GraphicsOptionsBuildPaths FromRoot(string root, string ffdecPath, string outputDirectory, string batmanUserIniPath)
    {
        string fullRoot = Path.GetFullPath(root);
        string fullOutput = Path.GetFullPath(outputDirectory);
        string tempDirectory = Path.Combine(fullOutput, "_build");

        return new GraphicsOptionsBuildPaths(
            RootPath: fullRoot,
            OutputDirectory: fullOutput,
            TempDirectory: tempDirectory,
            FrontendXmlPath: Path.Combine(fullRoot, "extracted", "frontend", "mainv2", "frontend-mainv2.xml"),
            FrontendSourceGfxPath: Path.Combine(fullRoot, "extracted", "frontend", "mainv2", "frontend-mainv2.gfx"),
            FrontendScriptsPath: Path.Combine(fullRoot, "extracted", "frontend", "mainv2", "frontend-mainv2-export", "scripts"),
            FfdecPath: Path.GetFullPath(ffdecPath),
            BatmanUserIniPath: Path.GetFullPath(batmanUserIniPath),
            FrontendWorkingScriptsPath: Path.Combine(tempDirectory, "frontend-scripts"),
            FrontendPatchedXmlPath: Path.Combine(tempDirectory, "MainV2-graphics-options.xml"),
            FrontendStructuralGfxPath: Path.Combine(tempDirectory, "MainV2-graphics-options-structural.gfx"),
            FrontendOutputGfxPath: Path.Combine(fullOutput, "MainV2-graphics-options.gfx"));
    }
}
