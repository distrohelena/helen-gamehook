namespace SubtitleSizeModBuilder;

/// <summary>
/// Resolves the filesystem locations used by the graphics-options shell builder.
/// The shell keeps the selective frontend import path but reads the user's Batman INI
/// so the VSync row opens with the same state as the game configuration.
/// </summary>
/// <param name="RootPath">The absolute Batman builder workspace root containing extracted frontend assets.</param>
/// <param name="OutputDirectory">The absolute directory receiving the generated shell GFX asset.</param>
/// <param name="TempDirectory">The absolute staging directory for patched XML, structural GFX, and scripts.</param>
/// <param name="FrontendXmlPath">The absolute path to the extracted MainV2 FFDec XML source.</param>
/// <param name="FrontendSourceGfxPath">The absolute path to the extracted MainV2 GFX source used by the import pipeline.</param>
/// <param name="FrontendScriptsPath">The absolute path to the extracted MainV2 FFDec script tree.</param>
/// <param name="FfdecPath">The absolute path to the FFDec command-line executable.</param>
/// <param name="BatmanUserIniPath">The absolute path to the Batman user <c>BmEngine.ini</c> file used for VSync bootstrap.</param>
/// <param name="FrontendWorkingScriptsPath">The absolute path to the writable staged script tree for the shell build.</param>
/// <param name="FrontendPatchedXmlPath">The absolute path to the patched shell XML emitted before XML-to-SWF conversion.</param>
/// <param name="FrontendStructuralGfxPath">The absolute path to the structural shell GFX emitted by FFDec XML-to-SWF conversion.</param>
/// <param name="FrontendOutputGfxPath">The absolute path to the final imported graphics-options shell GFX asset.</param>
internal sealed record GraphicsOptionsShellBuildPaths(
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
    /// Resolves the shell build layout from a builder root, FFDec executable, output directory,
    /// and user INI path. Every supplied path is canonicalized before being stored so validation
    /// and the later FFDec staging steps operate on one unambiguous filesystem representation.
    /// The temporary artifacts are kept under <c>_build</c> below the requested output directory,
    /// while the final asset is emitted directly below that output directory.
    /// </summary>
    /// <param name="root">The Batman builder workspace root.</param>
    /// <param name="ffdecPath">The FFDec CLI executable path.</param>
    /// <param name="outputDirectory">The directory receiving generated shell output.</param>
    /// <param name="batmanUserIniPath">The Batman user <c>BmEngine.ini</c> path supplying the initial VSync value.</param>
    /// <returns>A shell build path set with every path fully resolved.</returns>
    public static GraphicsOptionsShellBuildPaths FromRoot(string root, string ffdecPath, string outputDirectory, string batmanUserIniPath)
    {
        string fullRoot = Path.GetFullPath(root);
        string fullOutput = Path.GetFullPath(outputDirectory);
        string tempDirectory = Path.Combine(fullOutput, "_build");

        return new GraphicsOptionsShellBuildPaths(
            RootPath: fullRoot,
            OutputDirectory: fullOutput,
            TempDirectory: tempDirectory,
            FrontendXmlPath: Path.Combine(fullRoot, "extracted", "frontend", "mainv2", "frontend-mainv2.xml"),
            FrontendSourceGfxPath: Path.Combine(fullRoot, "extracted", "frontend", "mainv2", "frontend-mainv2.gfx"),
            FrontendScriptsPath: Path.Combine(fullRoot, "extracted", "frontend", "mainv2", "frontend-mainv2-export", "scripts"),
            FfdecPath: Path.GetFullPath(ffdecPath),
            BatmanUserIniPath: Path.GetFullPath(batmanUserIniPath),
            FrontendWorkingScriptsPath: Path.Combine(tempDirectory, "frontend-scripts"),
            FrontendPatchedXmlPath: Path.Combine(tempDirectory, "MainV2-graphics-options-shell.xml"),
            FrontendStructuralGfxPath: Path.Combine(tempDirectory, "MainV2-graphics-options-shell-structural.gfx"),
            FrontendOutputGfxPath: Path.Combine(fullOutput, "MainV2-graphics-options.gfx"));
    }
}
