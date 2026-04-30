using System.Security.Cryptography;

namespace BmGameGfxPatcher;

/// <summary>
/// Applies one manifest to a package by validating expected source object bytes, producing patched
/// objects, and updating export table offsets and sizes in either physical or logical storage.
/// </summary>
internal static class GfxPatchApplier
{
    /// <summary>
    /// Applies all manifest patches to the provided package and writes a patched package file.
    /// </summary>
    /// <param name="packagePath">Input package path to patch.</param>
    /// <param name="outputPath">Output package path that will receive the patched package.</param>
    /// <param name="manifest">Patch manifest with ordered export patch definitions.</param>
    /// <param name="verifyOutput">True to reopen and validate patched exports after writing output.</param>
    /// <returns>Patch result containing applied export metadata and verification status.</returns>
    public static PatchApplicationResult ApplyManifest(
        string packagePath,
        string outputPath,
        PatchManifest manifest,
        bool verifyOutput)
    {
        UnrealPackage package = UnrealPackage.Load(packagePath);
        List<AppliedPatch> appliedPatches = package.LogicalImage.UsesCompressedStorage
            ? ApplyManifestToCompressedPackage(package, outputPath, manifest)
            : ApplyManifestToUncompressedPackage(package, outputPath, manifest);

        if (!verifyOutput)
        {
            return new PatchApplicationResult(appliedPatches, Verified: false);
        }

        VerifyOutput(outputPath, appliedPatches);
        return new PatchApplicationResult(appliedPatches, Verified: true);
    }

    /// <summary>
    /// Applies manifest patches using the existing uncompressed append-and-retarget path.
    /// </summary>
    /// <param name="package">Loaded source package metadata and bytes.</param>
    /// <param name="outputPath">Destination package path.</param>
    /// <param name="manifest">Patch manifest to apply.</param>
    /// <returns>Applied patch metadata used for post-write verification.</returns>
    private static List<AppliedPatch> ApplyManifestToUncompressedPackage(
        UnrealPackage package,
        string outputPath,
        PatchManifest manifest)
    {
        string outputDirectory = Path.GetDirectoryName(outputPath) ?? Directory.GetCurrentDirectory();
        Directory.CreateDirectory(outputDirectory);
        File.Copy(package.FullPath, outputPath, overwrite: true);
        var appliedPatches = new List<AppliedPatch>(manifest.Patches.Count);

        using var stream = new FileStream(outputPath, FileMode.Open, FileAccess.ReadWrite, FileShare.None);

        foreach (GfxPatchDefinition definition in manifest.Patches)
        {
            AppliedPatch appliedPatch = ApplyPatchDefinitionToStorageStream(package, definition, stream);
            appliedPatches.Add(appliedPatch);
        }

        return appliedPatches;
    }

    /// <summary>
    /// Applies manifest patches against the logical package image, then rebuilds the physical
    /// compressed package using retail Batman chunk storage semantics.
    /// </summary>
    /// <param name="package">Loaded source package metadata and logical bytes.</param>
    /// <param name="outputPath">Destination package path.</param>
    /// <param name="manifest">Patch manifest to apply.</param>
    /// <returns>Applied patch metadata used for post-write verification.</returns>
    private static List<AppliedPatch> ApplyManifestToCompressedPackage(
        UnrealPackage package,
        string outputPath,
        PatchManifest manifest)
    {
        var appliedPatches = new List<AppliedPatch>(manifest.Patches.Count);
        using var logicalStream = new MemoryStream(capacity: checked(package.LogicalImage.Bytes.Length + (manifest.Patches.Count * 4096)));
        logicalStream.Write(package.LogicalImage.Bytes, 0, package.LogicalImage.Bytes.Length);

        foreach (GfxPatchDefinition definition in manifest.Patches)
        {
            AppliedPatch appliedPatch = ApplyPatchDefinitionToStorageStream(package, definition, logicalStream);
            appliedPatches.Add(appliedPatch);
        }

        byte[] patchedLogicalBytes = logicalStream.ToArray();
        CompressedPackageWriter.WritePackage(package, patchedLogicalBytes, outputPath);
        return appliedPatches;
    }

    /// <summary>
    /// Applies one patch definition to a writable package storage stream by appending the patched
    /// object and retargeting the export table size and offset fields.
    /// </summary>
    /// <param name="package">Loaded package used to resolve export metadata and original object bytes.</param>
    /// <param name="definition">Patch definition that describes replacement rules.</param>
    /// <param name="storageStream">Writable stream that receives patched object bytes and table edits.</param>
    /// <returns>Applied patch metadata describing the written object.</returns>
    private static AppliedPatch ApplyPatchDefinitionToStorageStream(
        UnrealPackage package,
        GfxPatchDefinition definition,
        Stream storageStream)
    {
        ExportEntry exportEntry = package.FindExport(definition.Owner, definition.ExportName, definition.ExportType);
        ReadOnlyMemory<byte> originalObject = package.ReadObjectBytes(exportEntry);

        ValidateOriginalObject(definition, originalObject.Span);
        byte[] replacementBytes = LoadReplacementBytes(definition);
        byte[] patchedObject = BuildPatchedObject(definition, originalObject.Span, replacementBytes);
        WriteOptionalPatchedObjectFile(definition, patchedObject);

        int patchedOffset = checked((int)storageStream.Length);
        storageStream.Seek(0, SeekOrigin.End);
        storageStream.Write(patchedObject, 0, patchedObject.Length);
        WriteInt32(storageStream, exportEntry.SerialDataSizeFieldOffset, patchedObject.Length);
        WriteInt32(storageStream, exportEntry.SerialDataOffsetFieldOffset, patchedOffset);

        return new AppliedPatch(
            definition,
            exportEntry,
            patchedOffset,
            patchedObject.Length,
            Hashing.Sha256Hex(patchedObject));
    }

    /// <summary>
    /// Validates length and hash preconditions declared in one patch definition.
    /// </summary>
    /// <param name="definition">Patch definition that declares expected source values.</param>
    /// <param name="originalObject">Serialized source object bytes from the package.</param>
    private static void ValidateOriginalObject(GfxPatchDefinition definition, ReadOnlySpan<byte> originalObject)
    {
        if (definition.ExpectedOriginalLength is int expectedLength && expectedLength != originalObject.Length)
        {
            throw new InvalidOperationException(
                $"Original object length mismatch for {definition.ExportType}:{definition.Owner}.{definition.ExportName}. Expected {expectedLength}, found {originalObject.Length}.");
        }

        string originalHash = Hashing.Sha256Hex(originalObject);
        if (!string.IsNullOrWhiteSpace(definition.ExpectedOriginalSha256) &&
            !string.Equals(definition.ExpectedOriginalSha256, originalHash, StringComparison.OrdinalIgnoreCase))
        {
            throw new InvalidOperationException(
                $"Original object hash mismatch for {definition.ExportType}:{definition.Owner}.{definition.ExportName}. Expected {definition.ExpectedOriginalSha256}, found {originalHash}.");
        }
    }

    /// <summary>
    /// Loads replacement bytes for one patch definition when replacement input is required.
    /// </summary>
    /// <param name="definition">Patch definition that may reference replacement bytes on disk.</param>
    /// <returns>Replacement byte payload, or an empty array when not required by patch mode.</returns>
    private static byte[] LoadReplacementBytes(GfxPatchDefinition definition)
    {
        return definition.ResolvedReplacementPath is null
            ? []
            : File.ReadAllBytes(definition.ResolvedReplacementPath);
    }

    /// <summary>
    /// Writes patched object bytes to the optional manifest output-object path.
    /// </summary>
    /// <param name="definition">Patch definition that may declare an output object path.</param>
    /// <param name="patchedObject">Patched serialized object bytes to write.</param>
    private static void WriteOptionalPatchedObjectFile(GfxPatchDefinition definition, byte[] patchedObject)
    {
        if (string.IsNullOrWhiteSpace(definition.ResolvedOutputObjectPath))
        {
            return;
        }

        string? objectDirectory = Path.GetDirectoryName(definition.ResolvedOutputObjectPath);
        if (!string.IsNullOrWhiteSpace(objectDirectory))
        {
            Directory.CreateDirectory(objectDirectory);
        }

        File.WriteAllBytes(definition.ResolvedOutputObjectPath, patchedObject);
    }

    /// <summary>
    /// Reopens the patched package and verifies each patched export offset, size, and payload hash.
    /// </summary>
    /// <param name="outputPath">Patched package path to reopen.</param>
    /// <param name="appliedPatches">Applied patch metadata expected in the output package.</param>
    private static void VerifyOutput(string outputPath, IReadOnlyList<AppliedPatch> appliedPatches)
    {
        UnrealPackage outputPackage = UnrealPackage.Load(outputPath);

        foreach (AppliedPatch appliedPatch in appliedPatches)
        {
            ExportEntry outputExport = outputPackage.FindExport(
                appliedPatch.Definition.Owner,
                appliedPatch.Definition.ExportName,
                appliedPatch.Definition.ExportType);

            if (outputExport.SerialDataOffset != appliedPatch.PatchedObjectOffset)
            {
                throw new InvalidOperationException(
                    $"Verification failed for {appliedPatch.Definition.ExportType}:{appliedPatch.Definition.Owner}.{appliedPatch.Definition.ExportName}. Offset mismatch.");
            }

            if (outputExport.SerialDataSize != appliedPatch.PatchedObjectLength)
            {
                throw new InvalidOperationException(
                    $"Verification failed for {appliedPatch.Definition.ExportType}:{appliedPatch.Definition.Owner}.{appliedPatch.Definition.ExportName}. Size mismatch.");
            }

            string outputHash = Hashing.Sha256Hex(outputPackage.ReadObjectBytes(outputExport).Span);
            if (!string.Equals(outputHash, appliedPatch.PatchedObjectSha256, StringComparison.Ordinal))
            {
                throw new InvalidOperationException(
                    $"Verification failed for {appliedPatch.Definition.ExportType}:{appliedPatch.Definition.Owner}.{appliedPatch.Definition.ExportName}. Hash mismatch.");
            }
        }
    }

    /// <summary>
    /// Builds patched object bytes for one patch definition and one source object.
    /// </summary>
    /// <param name="definition">Patch definition that describes patch mode and payload semantics.</param>
    /// <param name="originalObject">Original serialized object bytes.</param>
    /// <param name="replacementBytes">Replacement payload bytes used by <c>gfx</c> and <c>raw</c> modes.</param>
    /// <returns>Patched serialized object bytes.</returns>
    private static byte[] BuildPatchedObject(
        GfxPatchDefinition definition,
        ReadOnlySpan<byte> originalObject,
        ReadOnlySpan<byte> replacementBytes)
    {
        if (string.Equals(definition.PatchMode, "bytepatch", StringComparison.OrdinalIgnoreCase))
        {
            return ApplyBytePatches(definition, originalObject);
        }

        if (string.Equals(definition.PatchMode, "raw", StringComparison.OrdinalIgnoreCase))
        {
            return replacementBytes.ToArray();
        }

        int payloadOffset = BinarySearch.FindUniqueAscii(originalObject, definition.PayloadMagic);
        int originalGfxLength = ReadInt32(originalObject, payloadOffset + 4);
        int suffixOffset = checked(payloadOffset + originalGfxLength);

        if (suffixOffset < payloadOffset || suffixOffset > originalObject.Length)
        {
            throw new InvalidOperationException("Embedded GFX length points outside the exported object.");
        }

        ReadOnlySpan<byte> suffix = originalObject[suffixOffset..];
        byte[] patchedObject = new byte[payloadOffset + replacementBytes.Length + suffix.Length];

        originalObject[..payloadOffset].CopyTo(patchedObject);
        replacementBytes.CopyTo(patchedObject.AsSpan(payloadOffset));
        suffix.CopyTo(patchedObject.AsSpan(payloadOffset + replacementBytes.Length));

        int replacementGfxLength = ReadInt32(replacementBytes, 4);
        WriteInt32(patchedObject, payloadOffset - 12, checked(replacementGfxLength + 4));
        WriteInt32(patchedObject, payloadOffset - 4, replacementGfxLength);

        return patchedObject;
    }

    /// <summary>
    /// Applies byte-level replacement operations to a serialized object after validating expected bytes.
    /// </summary>
    /// <param name="definition">Patch definition that contains byte patch operations.</param>
    /// <param name="originalObject">Original serialized object bytes.</param>
    /// <returns>Patched serialized object bytes.</returns>
    private static byte[] ApplyBytePatches(GfxPatchDefinition definition, ReadOnlySpan<byte> originalObject)
    {
        byte[] patchedObject = originalObject.ToArray();

        foreach (BytePatchOperation bytePatch in definition.BytePatches)
        {
            int baseOffset = ResolvePatchSectionBaseOffset(definition, bytePatch, originalObject);
            int targetOffset = checked(baseOffset + bytePatch.Offset);

            if (targetOffset < 0 || targetOffset + bytePatch.ExpectedBytes.Length > patchedObject.Length)
            {
                throw new InvalidOperationException(
                    $"Byte patch for {definition.ExportType}:{definition.Owner}.{definition.ExportName} points outside the object.");
            }

            ReadOnlySpan<byte> currentBytes = patchedObject.AsSpan(targetOffset, bytePatch.ExpectedBytes.Length);
            if (!currentBytes.SequenceEqual(bytePatch.ExpectedBytes))
            {
                throw new InvalidOperationException(
                    $"Byte patch mismatch for {definition.ExportType}:{definition.Owner}.{definition.ExportName} at object offset {targetOffset}. " +
                    $"Expected {Convert.ToHexString(bytePatch.ExpectedBytes).ToLowerInvariant()}, found {Convert.ToHexString(currentBytes).ToLowerInvariant()}.");
            }

            bytePatch.ReplacementBytes.CopyTo(patchedObject.AsSpan(targetOffset, bytePatch.ReplacementBytes.Length));
        }

        return patchedObject;
    }

    /// <summary>
    /// Resolves the base offset used for one byte patch section selector.
    /// </summary>
    /// <param name="definition">Patch definition used to validate supported section semantics.</param>
    /// <param name="bytePatch">Byte patch operation with section and relative offset.</param>
    /// <param name="originalObject">Original serialized object bytes.</param>
    /// <returns>Section base offset inside the serialized object.</returns>
    private static int ResolvePatchSectionBaseOffset(
        GfxPatchDefinition definition,
        BytePatchOperation bytePatch,
        ReadOnlySpan<byte> originalObject)
    {
        if (string.Equals(bytePatch.Section, "object", StringComparison.OrdinalIgnoreCase))
        {
            return 0;
        }

        if (!string.Equals(definition.ExportType, "Function", StringComparison.Ordinal))
        {
            throw new InvalidOperationException(
                $"Script-relative byte patches are only supported for Function exports, not {definition.ExportType}.");
        }

        FunctionScriptLayout scriptLayout = FunctionExportObject.ReadScriptLayout(originalObject);
        return scriptLayout.ScriptOffset;
    }

    /// <summary>
    /// Writes one 32-bit signed value to a writable stream at an absolute byte offset.
    /// </summary>
    /// <param name="stream">Writable stream that supports seeking.</param>
    /// <param name="offset">Absolute byte offset where the value is written.</param>
    /// <param name="value">Signed 32-bit value to write.</param>
    private static void WriteInt32(Stream stream, int offset, int value)
    {
        Span<byte> buffer = stackalloc byte[sizeof(int)];
        BitConverter.TryWriteBytes(buffer, value);
        stream.Seek(offset, SeekOrigin.Begin);
        stream.Write(buffer);
    }

    /// <summary>
    /// Writes one 32-bit signed value to a byte array at an absolute offset.
    /// </summary>
    /// <param name="buffer">Destination byte array.</param>
    /// <param name="offset">Absolute byte offset where the value is written.</param>
    /// <param name="value">Signed 32-bit value to write.</param>
    private static void WriteInt32(byte[] buffer, int offset, int value)
    {
        if (offset < 0 || offset + sizeof(int) > buffer.Length)
        {
            throw new InvalidOperationException($"Cannot write Int32 at offset {offset}.");
        }

        BitConverter.GetBytes(value).CopyTo(buffer, offset);
    }

    /// <summary>
    /// Reads one 32-bit signed value from a span at the specified byte offset.
    /// </summary>
    /// <param name="buffer">Source byte span.</param>
    /// <param name="offset">Byte offset of the value to read.</param>
    /// <returns>Signed 32-bit value at the provided offset.</returns>
    private static int ReadInt32(ReadOnlySpan<byte> buffer, int offset)
    {
        if (offset < 0 || offset + sizeof(int) > buffer.Length)
        {
            throw new InvalidOperationException($"Cannot read Int32 at offset {offset}.");
        }

        return BitConverter.ToInt32(buffer[offset..(offset + sizeof(int))]);
    }
}
