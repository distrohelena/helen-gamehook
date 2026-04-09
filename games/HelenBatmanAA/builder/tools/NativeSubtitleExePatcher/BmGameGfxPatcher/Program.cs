using System.Text.Json;

namespace BmGameGfxPatcher;

/// <summary>
/// Implements the command-line entry point for export inspection and patching operations.
/// </summary>
internal static class Program
{
    /// <summary>
    /// Stores the JSON serializer settings used for patch manifests.
    /// </summary>
    private static readonly JsonSerializerOptions ManifestJsonOptions = new()
    {
        AllowTrailingCommas = true,
        PropertyNameCaseInsensitive = true,
        ReadCommentHandling = JsonCommentHandling.Skip,
        WriteIndented = true
    };

    /// <summary>
    /// Dispatches one CLI command.
    /// </summary>
    /// <param name="args">Raw command-line arguments.</param>
    /// <returns>Process exit code.</returns>
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
                "list-exports" => RunListExports(tail),
                "describe-export" => RunDescribeExport(tail),
                "extract-gfx" => RunExtractGfx(tail),
                "describe-properties" => RunDescribeProperties(tail),
                "describe-function" => RunDescribeFunction(tail),
                "find-function-refs" => RunFindFunctionRefs(tail),
                "scale-font" => RunScaleFont(tail),
                "patch-mainv2-version-label" => RunPatchMainV2VersionLabel(tail),
                "patch-mainv2-audio-subtitle-size" => RunPatchMainV2AudioSubtitleSize(tail),
                "patch-mainv2-graphics-options" => RunPatchMainV2GraphicsOptions(tail),
                "patch" => RunPatch(tail),
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

    /// <summary>
    /// Lists exports in one Unreal package, optionally filtered by type.
    /// </summary>
    /// <param name="args">Command-line arguments for export listing.</param>
    /// <returns>Process exit code.</returns>
    private static int RunListExports(string[] args)
    {
        var options = new ArgumentReader(args);
        string packagePath = options.RequireValue("--package");
        string? typeFilter = options.GetValue("--type");
        options.ThrowIfAnyUnknown();

        UnrealPackage package = UnrealPackage.Load(packagePath);
        IEnumerable<ExportEntry> exports = package.Exports;

        if (!string.IsNullOrWhiteSpace(typeFilter))
        {
            exports = exports.Where(exportEntry => string.Equals(exportEntry.TypeName, typeFilter, StringComparison.Ordinal));
        }

        foreach (ExportEntry exportEntry in exports)
        {
            Console.WriteLine(
                $"{exportEntry.TableIndex,5}  {exportEntry.TypeName,-16}  {ValueOrDash(exportEntry.OwnerName),-16}  {exportEntry.ObjectName.Value,-24}  size={exportEntry.SerialDataSize,8}  offset={exportEntry.SerialDataOffset,8}");
        }

        return 0;
    }

    /// <summary>
    /// Prints metadata, payload offsets, and optional reference scans for one export object.
    /// </summary>
    /// <param name="args">Command-line arguments for export description.</param>
    /// <returns>Process exit code.</returns>
    private static int RunDescribeExport(string[] args)
    {
        var options = new ArgumentReader(args);
        string packagePath = options.RequireValue("--package");
        string owner = options.RequireValue("--owner");
        string exportName = options.RequireValue("--name");
        string exportType = options.GetValue("--type") ?? "GFxMovieInfo";
        string payloadMagic = options.GetValue("--payload-magic") ?? "GFX";
        bool scanRefs = options.GetFlag("--scan-refs");
        bool dumpHex = options.GetFlag("--dump-hex");
        int? maxBytes = options.GetOptionalInt32("--max-bytes");
        options.ThrowIfAnyUnknown();

        owner = owner == "-" ? string.Empty : owner;
        payloadMagic = payloadMagic == "-" ? string.Empty : payloadMagic;
        UnrealPackage package = UnrealPackage.Load(packagePath);
        ExportEntry exportEntry = package.FindExport(owner, exportName, exportType);
        ReadOnlyMemory<byte> objectBytes = package.ReadObjectBytes(exportEntry);

        int payloadOffset = string.IsNullOrEmpty(payloadMagic)
            ? -1
            : BinarySearch.FindUniqueAscii(objectBytes.Span, payloadMagic);
        string sha256 = Hashing.Sha256Hex(objectBytes.Span);

        Console.WriteLine($"Package: {Path.GetFullPath(packagePath)}");
        Console.WriteLine($"Index: {exportEntry.TableIndex}");
        Console.WriteLine($"Type: {exportEntry.TypeName}");
        Console.WriteLine($"Owner: {ValueOrDash(exportEntry.OwnerName)}");
        Console.WriteLine($"Name: {exportEntry.ObjectName.Value}");
        Console.WriteLine($"EntryOffset: {exportEntry.EntryOffset}");
        Console.WriteLine($"ObjectOffset: {exportEntry.SerialDataOffset}");
        Console.WriteLine($"ObjectSize: {exportEntry.SerialDataSize}");
        Console.WriteLine($"Sha256: {sha256}");
        Console.WriteLine($"PayloadMagic: {payloadMagic}");
        Console.WriteLine($"PayloadOffset: {payloadOffset}");

        if (scanRefs)
        {
            Console.WriteLine("References:");
            foreach (ReferenceOccurrence occurrence in package.ScanResolvedInt32References(objectBytes.Span))
            {
                Console.WriteLine(
                    $"  object+0x{occurrence.Offset:x4}  ref={occurrence.Reference,7}  {occurrence.ReferenceInfo.Kind}:{ValueOrDash(occurrence.ReferenceInfo.OwnerName)}.{occurrence.ReferenceInfo.ObjectName}");
            }
        }

        if (dumpHex)
        {
            Console.WriteLine("ObjectHex:");
            int bytesToDump = Math.Min(objectBytes.Length, maxBytes ?? objectBytes.Length);

            for (int offset = 0; offset < bytesToDump; offset += 16)
            {
                ReadOnlySpan<byte> chunk = objectBytes.Span.Slice(offset, Math.Min(16, bytesToDump - offset));
                Console.WriteLine($"  {offset:x4}: {Convert.ToHexString(chunk).ToLowerInvariant()}");
            }
        }

        return 0;
    }

    /// <summary>
    /// Extracts one embedded GFX payload from the requested export into a standalone file.
    /// </summary>
    /// <param name="args">Command-line arguments for payload extraction.</param>
    /// <returns>Process exit code.</returns>
    private static int RunExtractGfx(string[] args)
    {
        var options = new ArgumentReader(args);
        string packagePath = options.RequireValue("--package");
        string owner = options.RequireValue("--owner");
        string exportName = options.RequireValue("--name");
        string exportType = options.GetValue("--type") ?? "GFxMovieInfo";
        string payloadMagic = options.GetValue("--payload-magic") ?? "GFX";
        string outputPath = options.RequireValue("--output");
        options.ThrowIfAnyUnknown();

        owner = owner == "-" ? string.Empty : owner;
        EmbeddedPayloadExtractor.WritePayload(packagePath, owner, exportName, exportType, payloadMagic, outputPath);
        Console.WriteLine($"Extracted {exportType}:{ValueOrDash(owner)}.{exportName} payload into {Path.GetFullPath(outputPath)}");
        return 0;
    }

    /// <summary>
    /// Applies one manifest-driven patch set to a package and writes the patched output package.
    /// </summary>
    /// <param name="args">Command-line arguments for manifest patching.</param>
    /// <returns>Process exit code.</returns>
    private static int RunPatch(string[] args)
    {
        var options = new ArgumentReader(args);
        string packagePath = options.RequireValue("--package");
        string manifestPath = options.RequireValue("--manifest");
        string outputPath = options.RequireValue("--output");
        bool skipVerify = options.GetFlag("--skip-verify");
        options.ThrowIfAnyUnknown();

        string fullPackagePath = Path.GetFullPath(packagePath);
        string fullOutputPath = Path.GetFullPath(outputPath);

        if (string.Equals(fullPackagePath, fullOutputPath, StringComparison.OrdinalIgnoreCase))
        {
            throw new InvalidOperationException("Refusing to patch in place. Write to a separate output path.");
        }

        PatchManifest manifest = LoadManifest(manifestPath);
        PatchApplicationResult result = GfxPatchApplier.ApplyManifest(fullPackagePath, fullOutputPath, manifest, !skipVerify);

        Console.WriteLine($"Patched {result.AppliedPatches.Count} export(s) into {fullOutputPath}");

        foreach (AppliedPatch patch in result.AppliedPatches)
        {
            Console.WriteLine(
                $"  {patch.Export.TypeName}:{ValueOrDash(patch.Export.OwnerName)}.{patch.Export.ObjectName.Value} -> size={patch.PatchedObjectLength} offset={patch.PatchedObjectOffset}");
        }

        if (result.Verified)
        {
            Console.WriteLine("Verification succeeded.");
        }

        return 0;
    }

    /// <summary>
    /// Dumps serialized property tags for one export object starting at a requested byte offset.
    /// </summary>
    /// <param name="args">Command-line arguments for property inspection.</param>
    /// <returns>Process exit code.</returns>
    private static int RunDescribeProperties(string[] args)
    {
        var options = new ArgumentReader(args);
        string packagePath = options.RequireValue("--package");
        string owner = options.RequireValue("--owner");
        string exportName = options.RequireValue("--name");
        string exportType = options.RequireValue("--type");
        int startOffset = options.GetOptionalInt32("--start-offset") ?? 4;
        options.ThrowIfAnyUnknown();

        owner = owner == "-" ? string.Empty : owner;

        UnrealPackage package = UnrealPackage.Load(packagePath);
        ExportEntry exportEntry = package.FindExport(owner, exportName, exportType);
        ReadOnlyMemory<byte> objectBytes = package.ReadObjectBytes(exportEntry);
        IReadOnlyList<SerializedPropertyTag> properties = SerializedPropertyParser.ReadProperties(package, objectBytes.Span, startOffset);

        Console.WriteLine($"Package: {Path.GetFullPath(packagePath)}");
        Console.WriteLine($"Index: {exportEntry.TableIndex}");
        Console.WriteLine($"Type: {exportEntry.TypeName}");
        Console.WriteLine($"Owner: {ValueOrDash(exportEntry.OwnerName)}");
        Console.WriteLine($"Name: {exportEntry.ObjectName.Value}");
        Console.WriteLine($"ObjectOffset: {exportEntry.SerialDataOffset}");
        Console.WriteLine($"ObjectSize: {exportEntry.SerialDataSize}");
        Console.WriteLine($"PropertyStartOffset: {startOffset}");
        Console.WriteLine("Properties:");

        foreach (SerializedPropertyTag property in properties)
        {
            string preview = SerializedPropertyParser.FormatValuePreview(package, objectBytes.Span, property);
            Console.WriteLine(
                $"  object+0x{property.TagOffset:x4}  {property.Name,-24}  {property.TypeName,-16}  size={property.Size,5}  array={property.ArrayIndex,3}  payload=0x{property.PayloadOffset:x4}{preview}");
        }

        return 0;
    }

    /// <summary>
    /// Prints metadata and optional hex or reference scans for one Unreal function export.
    /// </summary>
    /// <param name="args">Command-line arguments for function inspection.</param>
    /// <returns>Process exit code.</returns>
    private static int RunDescribeFunction(string[] args)
    {
        var options = new ArgumentReader(args);
        string packagePath = options.RequireValue("--package");
        string owner = options.RequireValue("--owner");
        string exportName = options.RequireValue("--name");
        bool dumpScriptHex = options.GetFlag("--dump-script-hex");
        bool scanRefs = options.GetFlag("--scan-refs");
        options.ThrowIfAnyUnknown();

        UnrealPackage package = UnrealPackage.Load(packagePath);
        ExportEntry exportEntry = package.FindExport(owner, exportName, "Function");
        ReadOnlyMemory<byte> objectBytes = package.ReadObjectBytes(exportEntry);
        FunctionScriptLayout scriptLayout = FunctionExportObject.ReadScriptLayout(objectBytes.Span);

        Console.WriteLine($"Package: {Path.GetFullPath(packagePath)}");
        Console.WriteLine($"Index: {exportEntry.TableIndex}");
        Console.WriteLine($"Type: {exportEntry.TypeName}");
        Console.WriteLine($"Owner: {ValueOrDash(exportEntry.OwnerName)}");
        Console.WriteLine($"Name: {exportEntry.ObjectName.Value}");
        Console.WriteLine($"ObjectOffset: {exportEntry.SerialDataOffset}");
        Console.WriteLine($"ObjectSize: {exportEntry.SerialDataSize}");
        Console.WriteLine($"Sha256: {Hashing.Sha256Hex(objectBytes.Span)}");
        Console.WriteLine($"ScriptOffset: {scriptLayout.ScriptOffset}");
        Console.WriteLine($"ScriptSize: {scriptLayout.ScriptSize}");

        if (scanRefs)
        {
            Console.WriteLine("References:");
            foreach (ReferenceOccurrence occurrence in package.ScanResolvedInt32References(scriptLayout.ScriptBytes))
            {
                Console.WriteLine(
                    $"  script+0x{occurrence.Offset:x4}  ref={occurrence.Reference,7}  {occurrence.ReferenceInfo.Kind}:{ValueOrDash(occurrence.ReferenceInfo.OwnerName)}.{occurrence.ReferenceInfo.ObjectName}");
            }
        }

        if (dumpScriptHex)
        {
            Console.WriteLine("ScriptHex:");

            for (int offset = 0; offset < scriptLayout.ScriptBytes.Length; offset += 16)
            {
                ReadOnlySpan<byte> chunk = scriptLayout.ScriptBytes.AsSpan(offset, Math.Min(16, scriptLayout.ScriptBytes.Length - offset));
                Console.WriteLine($"  {offset:x4}: {Convert.ToHexString(chunk).ToLowerInvariant()}");
            }
        }

        return 0;
    }

    /// <summary>
    /// Scales one or more font exports and writes the patched package output.
    /// </summary>
    /// <param name="args">Command-line arguments for font scaling.</param>
    /// <returns>Process exit code.</returns>
    private static int RunScaleFont(string[] args)
    {
        var options = new ArgumentReader(args);
        string packagePath = options.RequireValue("--package");
        string outputPath = options.RequireValue("--output");
        string owner = options.GetValue("--owner") ?? "BmFonts";
        string namesValue = options.RequireValue("--names");
        double scale = options.GetOptionalDouble("--scale") ?? 2.0;
        int startOffset = options.GetOptionalInt32("--start-offset") ?? 4;
        options.ThrowIfAnyUnknown();

        string[] exportNames = namesValue
            .Split(',', StringSplitOptions.RemoveEmptyEntries | StringSplitOptions.TrimEntries);

        if (exportNames.Length == 0)
        {
            throw new InvalidOperationException("Provide at least one font export name in --names.");
        }

        if (scale <= 0)
        {
            throw new InvalidOperationException("--scale must be greater than zero.");
        }

        FontScaler.ScaleFonts(packagePath, outputPath, owner, exportNames, scale, startOffset);
        Console.WriteLine($"Scaled {exportNames.Length} font export(s) into {Path.GetFullPath(outputPath)}");
        return 0;
    }

    /// <summary>
    /// Patches the retail frontend MainV2 root script so the main-menu version label shows a custom build string.
    /// </summary>
    /// <param name="args">Command-line arguments for the version-label patch.</param>
    /// <returns>Process exit code.</returns>
    private static int RunPatchMainV2VersionLabel(string[] args)
    {
        var options = new ArgumentReader(args);
        string packagePath = options.RequireValue("--package");
        string outputPath = options.RequireValue("--output");
        string buildLabel = options.RequireValue("--label");
        options.ThrowIfAnyUnknown();

        MainMenuVersionLabelPackagePatcher.PatchPackage(packagePath, outputPath, buildLabel);
        Console.WriteLine($"Patched MainMenu.MainV2 version label into {Path.GetFullPath(outputPath)}");
        return 0;
    }

    /// <summary>
    /// Patches the retail frontend MainV2 audio screen so subtitle size becomes a dedicated fifth row.
    /// </summary>
    /// <param name="args">Command-line arguments for the audio subtitle-size patch.</param>
    /// <returns>Process exit code.</returns>
    private static int RunPatchMainV2AudioSubtitleSize(string[] args)
    {
        var options = new ArgumentReader(args);
        string packagePath = options.RequireValue("--package");
        string outputPath = options.RequireValue("--output");
        string prototypeGfxPath = options.RequireValue("--prototype-gfx");
        options.ThrowIfAnyUnknown();

        MainMenuAudioSubtitleSizePackagePatcher.PatchPackage(packagePath, outputPath, prototypeGfxPath);
        Console.WriteLine($"Patched MainMenu.MainV2 audio subtitle size into {Path.GetFullPath(outputPath)}");
        return 0;
    }

    /// <summary>
    /// Patches the retail frontend MainV2 movie so the graphics-options prototype payload is transplanted into the package.
    /// </summary>
    /// <param name="args">Command-line arguments for the graphics-options patch.</param>
    /// <returns>Process exit code.</returns>
    private static int RunPatchMainV2GraphicsOptions(string[] args)
    {
        var options = new ArgumentReader(args);
        string packagePath = options.RequireValue("--package");
        string outputPath = options.RequireValue("--output");
        string prototypeGfxPath = options.RequireValue("--prototype-gfx");
        options.ThrowIfAnyUnknown();

        MainMenuGraphicsOptionsPackagePatcher.PatchPackage(packagePath, outputPath, prototypeGfxPath);
        Console.WriteLine($"Patched MainMenu.MainV2 graphics-options movie into {Path.GetFullPath(outputPath)}");
        return 0;
    }

    /// <summary>
    /// Loads and initializes one patch manifest from disk.
    /// </summary>
    /// <param name="manifestPath">Manifest file path.</param>
    /// <returns>Initialized manifest.</returns>
    private static PatchManifest LoadManifest(string manifestPath)
    {
        string fullManifestPath = Path.GetFullPath(manifestPath);
        string json = File.ReadAllText(fullManifestPath);
        PatchManifest? manifest = JsonSerializer.Deserialize<PatchManifest>(json, ManifestJsonOptions);

        if (manifest is null)
        {
            throw new InvalidOperationException($"Manifest '{fullManifestPath}' could not be parsed.");
        }

        manifest.Initialize(fullManifestPath);
        return manifest;
    }

    /// <summary>
    /// Searches function exports for references that match the requested filters.
    /// </summary>
    /// <param name="args">Command-line arguments for function-reference scanning.</param>
    /// <returns>Process exit code.</returns>
    private static int RunFindFunctionRefs(string[] args)
    {
        var options = new ArgumentReader(args);
        string packagePath = options.RequireValue("--package");
        string? functionOwner = options.GetValue("--function-owner");
        string? refNameContains = options.GetValue("--ref-name-contains");
        string? refOwnerContains = options.GetValue("--ref-owner-contains");
        string? refTypeContains = options.GetValue("--ref-type-contains");
        string? refKind = options.GetValue("--ref-kind");
        bool showAllMatches = options.GetFlag("--show-all-matches");
        options.ThrowIfAnyUnknown();

        if (string.IsNullOrWhiteSpace(refNameContains) &&
            string.IsNullOrWhiteSpace(refOwnerContains) &&
            string.IsNullOrWhiteSpace(refTypeContains) &&
            string.IsNullOrWhiteSpace(refKind))
        {
            throw new InvalidOperationException(
                "Provide at least one ref filter: --ref-name-contains, --ref-owner-contains, --ref-type-contains, or --ref-kind.");
        }

        UnrealPackage package = UnrealPackage.Load(packagePath);
        IEnumerable<ExportEntry> functions = package.Exports.Where(exportEntry =>
            string.Equals(exportEntry.TypeName, "Function", StringComparison.Ordinal));

        if (!string.IsNullOrWhiteSpace(functionOwner))
        {
            functions = functions.Where(exportEntry =>
                string.Equals(exportEntry.OwnerName, functionOwner, StringComparison.Ordinal));
        }

        int hitCount = 0;

        foreach (ExportEntry exportEntry in functions)
        {
            ReadOnlyMemory<byte> objectBytes;
            FunctionScriptLayout scriptLayout;

            try
            {
                objectBytes = package.ReadObjectBytes(exportEntry);
                scriptLayout = FunctionExportObject.ReadScriptLayout(objectBytes.Span);
            }
            catch
            {
                continue;
            }

            ReferenceOccurrence[] matches = package.ScanResolvedInt32References(scriptLayout.ScriptBytes)
                .Where(occurrence => MatchesReferenceFilter(
                    occurrence.ReferenceInfo,
                    refNameContains,
                    refOwnerContains,
                    refTypeContains,
                    refKind))
                .ToArray();

            if (matches.Length == 0)
            {
                continue;
            }

            hitCount++;

            Console.WriteLine(
                $"{exportEntry.TableIndex,5}  {ValueOrDash(exportEntry.OwnerName),-24}  {exportEntry.ObjectName.Value,-28}  matches={matches.Length,3}  script={scriptLayout.ScriptSize,4}");

            if (showAllMatches)
            {
                foreach (ReferenceOccurrence occurrence in matches)
                {
                    Console.WriteLine(
                        $"       script+0x{occurrence.Offset:x4}  ref={occurrence.Reference,7}  {occurrence.ReferenceInfo.Kind}:{ValueOrDash(occurrence.ReferenceInfo.OwnerName)}.{occurrence.ReferenceInfo.ObjectName}");
                }
            }
        }

        Console.WriteLine($"Matched functions: {hitCount}");
        return 0;
    }

    /// <summary>
    /// Evaluates whether one resolved reference matches the requested filter set.
    /// </summary>
    /// <param name="referenceInfo">Reference metadata to evaluate.</param>
    /// <param name="refNameContains">Optional object-name substring filter.</param>
    /// <param name="refOwnerContains">Optional owner-name substring filter.</param>
    /// <param name="refTypeContains">Optional type-name substring filter.</param>
    /// <param name="refKind">Optional reference-kind filter.</param>
    /// <returns>True when the reference satisfies every requested filter.</returns>
    private static bool MatchesReferenceFilter(
        ReferenceInfo referenceInfo,
        string? refNameContains,
        string? refOwnerContains,
        string? refTypeContains,
        string? refKind)
    {
        if (!MatchesContains(referenceInfo.ObjectName, refNameContains))
        {
            return false;
        }

        if (!MatchesContains(referenceInfo.OwnerName, refOwnerContains))
        {
            return false;
        }

        if (!MatchesContains(referenceInfo.TypeName, refTypeContains))
        {
            return false;
        }

        if (!string.IsNullOrWhiteSpace(refKind) &&
            !string.Equals(referenceInfo.Kind, refKind, StringComparison.OrdinalIgnoreCase))
        {
            return false;
        }

        return true;
    }

    /// <summary>
    /// Evaluates one optional case-insensitive substring filter against one candidate string.
    /// </summary>
    /// <param name="candidate">Candidate string to test.</param>
    /// <param name="substring">Optional substring filter.</param>
    /// <returns>True when the filter is empty or the candidate contains it.</returns>
    private static bool MatchesContains(string? candidate, string? substring)
    {
        if (string.IsNullOrWhiteSpace(substring))
        {
            return true;
        }

        return !string.IsNullOrWhiteSpace(candidate) &&
               candidate.Contains(substring, StringComparison.OrdinalIgnoreCase);
    }

    /// <summary>
    /// Prints CLI usage text and reports success.
    /// </summary>
    /// <returns>Process exit code.</returns>
    private static int PrintHelpAndReturn()
    {
        PrintUsage();
        return 0;
    }

    /// <summary>
    /// Prints the supported CLI commands and their primary arguments.
    /// </summary>
    private static void PrintUsage()
    {
        Console.WriteLine("BmGameGfxPatcher");
        Console.WriteLine();
        Console.WriteLine("Commands:");
        Console.WriteLine("  list-exports --package <BmGame.u> [--type GFxMovieInfo]");
        Console.WriteLine("  describe-export --package <BmGame.u> --owner <Owner> --name <Name> [--type GFxMovieInfo] [--payload-magic GFX] [--scan-refs] [--dump-hex] [--max-bytes N]");
        Console.WriteLine("  extract-gfx --package <BmGame.u> --owner <Owner> --name <Name> --output <file> [--type GFxMovieInfo] [--payload-magic GFX]");
        Console.WriteLine("  describe-properties --package <package> --owner <Owner> --name <Name> --type <ExportType> [--start-offset 4]");
        Console.WriteLine("  describe-function --package <package> --owner <Owner> --name <FunctionName> [--scan-refs] [--dump-script-hex]");
        Console.WriteLine("  find-function-refs --package <package> [--function-owner <Owner>] (--ref-name-contains <Text> | --ref-owner-contains <Text> | --ref-type-contains <Text> | --ref-kind <Import|Export>) [--show-all-matches]");
        Console.WriteLine("  scale-font --package <package> --output <patched-package> --names <FontA,FontB> [--owner BmFonts] [--scale 2.0] [--start-offset 4]");
        Console.WriteLine("  patch-mainv2-version-label --package <Frontend.umap> --output <patched-package> --label <text>");
        Console.WriteLine("  patch-mainv2-audio-subtitle-size --package <Frontend.umap> --output <patched-package> --prototype-gfx <MainV2-subtitle-size.gfx>");
        Console.WriteLine("  patch-mainv2-graphics-options --package <Frontend.umap> --output <patched-package> --prototype-gfx <MainV2-graphics-options.gfx>");
        Console.WriteLine("  patch --package <package> --manifest <manifest.jsonc> --output <patched-package> [--skip-verify]");
    }

    /// <summary>
    /// Normalizes null or whitespace values for console output.
    /// </summary>
    /// <param name="value">Value to normalize.</param>
    /// <returns>The original value or a dash placeholder.</returns>
    private static string ValueOrDash(string? value)
    {
        return string.IsNullOrWhiteSpace(value) ? "-" : value;
    }
}
