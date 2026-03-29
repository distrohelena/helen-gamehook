using System.Security.Cryptography;

namespace BmGameGfxPatcher;

internal static class GfxPatchApplier
{
    public static PatchApplicationResult ApplyManifest(
        string packagePath,
        string outputPath,
        PatchManifest manifest,
        bool verifyOutput)
    {
        UnrealPackage package = UnrealPackage.Load(packagePath);

        string outputDirectory = Path.GetDirectoryName(outputPath) ?? Directory.GetCurrentDirectory();
        Directory.CreateDirectory(outputDirectory);
        File.Copy(package.FullPath, outputPath, overwrite: true);

        var appliedPatches = new List<AppliedPatch>(manifest.Patches.Count);

        using (var stream = new FileStream(outputPath, FileMode.Open, FileAccess.ReadWrite, FileShare.None))
        {
            foreach (GfxPatchDefinition definition in manifest.Patches)
            {
                ExportEntry exportEntry = package.FindExport(definition.Owner, definition.ExportName, definition.ExportType);
                ReadOnlyMemory<byte> originalObject = package.ReadObjectBytes(exportEntry);

                if (definition.ExpectedOriginalLength is int expectedLength && expectedLength != originalObject.Length)
                {
                    throw new InvalidOperationException(
                        $"Original object length mismatch for {definition.ExportType}:{definition.Owner}.{definition.ExportName}. Expected {expectedLength}, found {originalObject.Length}.");
                }

                string originalHash = Hashing.Sha256Hex(originalObject.Span);
                if (!string.IsNullOrWhiteSpace(definition.ExpectedOriginalSha256) &&
                    !string.Equals(definition.ExpectedOriginalSha256, originalHash, StringComparison.OrdinalIgnoreCase))
                {
                    throw new InvalidOperationException(
                        $"Original object hash mismatch for {definition.ExportType}:{definition.Owner}.{definition.ExportName}. Expected {definition.ExpectedOriginalSha256}, found {originalHash}.");
                }

                byte[] replacementBytes = definition.ResolvedReplacementPath is null
                    ? []
                    : File.ReadAllBytes(definition.ResolvedReplacementPath);
                byte[] patchedObject = BuildPatchedObject(definition, originalObject.Span, replacementBytes);

                if (!string.IsNullOrWhiteSpace(definition.ResolvedOutputObjectPath))
                {
                    string? objectDirectory = Path.GetDirectoryName(definition.ResolvedOutputObjectPath);
                    if (!string.IsNullOrWhiteSpace(objectDirectory))
                    {
                        Directory.CreateDirectory(objectDirectory);
                    }

                    File.WriteAllBytes(definition.ResolvedOutputObjectPath, patchedObject);
                }

                int patchedOffset = checked((int)stream.Length);
                stream.Seek(0, SeekOrigin.End);
                stream.Write(patchedObject, 0, patchedObject.Length);

                WriteInt32(stream, exportEntry.SerialDataSizeFieldOffset, patchedObject.Length);
                WriteInt32(stream, exportEntry.SerialDataOffsetFieldOffset, patchedOffset);

                appliedPatches.Add(
                    new AppliedPatch(
                        definition,
                        exportEntry,
                        patchedOffset,
                        patchedObject.Length,
                        Hashing.Sha256Hex(patchedObject)));
            }
        }

        if (!verifyOutput)
        {
            return new PatchApplicationResult(appliedPatches, Verified: false);
        }

        VerifyOutput(outputPath, appliedPatches);
        return new PatchApplicationResult(appliedPatches, Verified: true);
    }

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

    private static void WriteInt32(FileStream stream, int offset, int value)
    {
        Span<byte> buffer = stackalloc byte[sizeof(int)];
        BitConverter.TryWriteBytes(buffer, value);
        stream.Seek(offset, SeekOrigin.Begin);
        stream.Write(buffer);
    }

    private static void WriteInt32(byte[] buffer, int offset, int value)
    {
        if (offset < 0 || offset + sizeof(int) > buffer.Length)
        {
            throw new InvalidOperationException($"Cannot write Int32 at offset {offset}.");
        }

        BitConverter.GetBytes(value).CopyTo(buffer, offset);
    }

    private static int ReadInt32(ReadOnlySpan<byte> buffer, int offset)
    {
        if (offset < 0 || offset + sizeof(int) > buffer.Length)
        {
            throw new InvalidOperationException($"Cannot read Int32 at offset {offset}.");
        }

        return BitConverter.ToInt32(buffer[offset..(offset + sizeof(int))]);
    }
}
