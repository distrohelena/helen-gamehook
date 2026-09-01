namespace SubtitleSizeModBuilder;

/// <summary>
/// Resolves the filesystem locations used by the graphics-options shell builder.
/// The shell build deliberately has no user-INI input because every row is rendered
/// as a fixed, non-interactive placeholder and the generated asset only needs the
/// extracted frontend, FFDec, and a writable output directory.
/// </summary>
/// <param name="RootPath">The absolute Batman builder workspace root containing extracted frontend assets.</param>
/// <param name="OutputDirectory">The absolute directory receiving the generated shell GFX asset.</param>
/// <param name="TempDirectory">The absolute staging directory for patched XML, structural GFX, and scripts.</param>
/// <param name="FrontendXmlPath">The absolute path to the extracted MainV2 FFDec XML source.</param>
/// <param name="FrontendSourceGfxPath">The absolute path to the extracted MainV2 GFX source used by the import pipeline.</param>
/// <param name="FrontendScriptsPath">The absolute path to the extracted MainV2 FFDec script tree.</param>
/// <param name="FfdecPath">The absolute path to the FFDec command-line executable.</param>
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
    string FrontendWorkingScriptsPath,
    string FrontendPatchedXmlPath,
    string FrontendStructuralGfxPath,
    string FrontendOutputGfxPath)
{
    /// <summary>
    /// Resolves the shell build layout from a builder root, FFDec executable, and output directory.
    /// The temporary artifacts are kept under <c>_build</c> below the requested output directory,
    /// while the final asset is emitted directly below that output directory.
    /// </summary>
    /// <param name="root">The Batman builder workspace root.</param>
    /// <param name="ffdecPath">The FFDec CLI executable path.</param>
    /// <param name="outputDirectory">The directory receiving generated shell output.</param>
    /// <returns>A shell build path set with every path fully resolved.</returns>
    public static GraphicsOptionsShellBuildPaths FromRoot(string root, string ffdecPath, string outputDirectory)
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
            FrontendWorkingScriptsPath: Path.Combine(tempDirectory, "frontend-scripts"),
            FrontendPatchedXmlPath: Path.Combine(tempDirectory, "MainV2-graphics-options-shell.xml"),
            FrontendStructuralGfxPath: Path.Combine(tempDirectory, "MainV2-graphics-options-shell-structural.gfx"),
            FrontendOutputGfxPath: Path.Combine(fullOutput, "MainV2-graphics-options.gfx"));
    }
}
