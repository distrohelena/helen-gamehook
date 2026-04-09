using System.Text;

namespace BmGameGfxPatcher;

/// <summary>
/// Rewrites the retail MainMenu.MainV2 root DoAction bytecode so <c>PCVersionString</c> is
/// initialized directly in the embedded GFX payload.
/// </summary>
internal static class MainMenuVersionLabelGfxPatcher
{
    /// <summary>
    /// Stores the expected GFX file signature.
    /// </summary>
    private const string GfxMagic = "GFX";

    /// <summary>
    /// Stores the byte offset of the file-length field in the GFX header.
    /// </summary>
    private const int FileLengthOffset = 4;

    /// <summary>
    /// Stores the SWF tag code for DoAction.
    /// </summary>
    private const int DoActionTagCode = 12;

    /// <summary>
    /// Stores the action opcode for ConstantPool.
    /// </summary>
    private const byte ConstantPoolActionCode = 0x88;

    /// <summary>
    /// Stores the action opcode for Push.
    /// </summary>
    private const byte PushActionCode = 0x96;

    /// <summary>
    /// Stores the action opcode for DefineLocal.
    /// </summary>
    private const byte DefineLocalActionCode = 0x3C;

    /// <summary>
    /// Stores the action opcode for DefineLocal2.
    /// </summary>
    private const byte DefineLocal2ActionCode = 0x41;

    /// <summary>
    /// Stores the push value kind for 8-bit constant-pool references.
    /// </summary>
    private const byte PushConstant8Type = 0x08;

    /// <summary>
    /// Stores the push value kind for 16-bit constant-pool references.
    /// </summary>
    private const byte PushConstant16Type = 0x09;

    /// <summary>
    /// Stores the retail root variable name that drives the visible version label.
    /// </summary>
    private const string PcVersionStringName = "PCVersionString";

    /// <summary>
    /// Patches one MainV2 GFX payload so the retail root script defines an initialized
    /// <c>PCVersionString</c> local variable.
    /// </summary>
    /// <param name="gfxBytes">Original MainV2 GFX payload bytes.</param>
    /// <param name="buildLabel">Build label text to assign to <c>PCVersionString</c>.</param>
    /// <returns>Patched MainV2 GFX payload bytes.</returns>
    public static byte[] Patch(ReadOnlySpan<byte> gfxBytes, string buildLabel)
    {
        if (string.IsNullOrWhiteSpace(buildLabel))
        {
            throw new InvalidOperationException("Build label text must not be empty.");
        }

        if (buildLabel.IndexOf('\0') >= 0)
        {
            throw new InvalidOperationException("Build label text must not contain embedded null characters.");
        }

        ValidateGfxHeader(gfxBytes);
        int tagOffset = FindRootDoActionTagOffset(gfxBytes);
        ReadTagHeader(gfxBytes, tagOffset, out int tagCode, out int tagDataOffset, out int tagDataLength);
        byte[] patchedTagPayload = PatchRootDoActionPayload(gfxBytes[tagDataOffset..(tagDataOffset + tagDataLength)], buildLabel);
        byte[] patchedTagHeader = BuildLongTagHeader(tagCode, patchedTagPayload.Length);
        int suffixOffset = tagDataOffset + tagDataLength;
        byte[] patchedGfx = new byte[tagOffset + patchedTagHeader.Length + patchedTagPayload.Length + (gfxBytes.Length - suffixOffset)];

        gfxBytes[..tagOffset].CopyTo(patchedGfx);
        patchedTagHeader.CopyTo(patchedGfx.AsSpan(tagOffset));
        patchedTagPayload.CopyTo(patchedGfx.AsSpan(tagOffset + patchedTagHeader.Length));
        gfxBytes[suffixOffset..].CopyTo(patchedGfx.AsSpan(tagOffset + patchedTagHeader.Length + patchedTagPayload.Length));
        WriteInt32(patchedGfx, FileLengthOffset, patchedGfx.Length);
        return patchedGfx;
    }

    /// <summary>
    /// Verifies that the provided payload starts with a valid Batman GFX header.
    /// </summary>
    /// <param name="gfxBytes">Candidate GFX payload bytes.</param>
    private static void ValidateGfxHeader(ReadOnlySpan<byte> gfxBytes)
    {
        if (gfxBytes.Length < 8)
        {
            throw new InvalidOperationException("GFX payload is too short to contain a valid header.");
        }

        if (!gfxBytes[..3].SequenceEqual(Encoding.ASCII.GetBytes(GfxMagic)))
        {
            throw new InvalidOperationException("Embedded payload is not a GFX file.");
        }
    }

    /// <summary>
    /// Finds the unique top-level DoAction tag whose payload contains the retail
    /// <c>PCVersionString</c> declaration.
    /// </summary>
    /// <param name="gfxBytes">Complete GFX payload bytes.</param>
    /// <returns>Byte offset of the matching DoAction tag header.</returns>
    private static int FindRootDoActionTagOffset(ReadOnlySpan<byte> gfxBytes)
    {
        int offset = GetFirstTagOffset(gfxBytes);
        int matchOffset = -1;

        while (offset < gfxBytes.Length)
        {
            int tagOffset = offset;
            ReadTagHeader(gfxBytes, tagOffset, out int tagCode, out int tagDataOffset, out int tagDataLength);

            if (tagCode == 0)
            {
                break;
            }

            if (tagCode == DoActionTagCode &&
                ContainsAscii(gfxBytes[tagDataOffset..(tagDataOffset + tagDataLength)], PcVersionStringName))
            {
                if (matchOffset >= 0)
                {
                    throw new InvalidOperationException("Multiple DoAction tags referenced PCVersionString. The retail root patch target is not unique.");
                }

                matchOffset = tagOffset;
            }

            offset = tagDataOffset + tagDataLength;
        }

        if (matchOffset < 0)
        {
            throw new InvalidOperationException("Could not find the retail MainMenu.MainV2 root DoAction tag.");
        }

        return matchOffset;
    }

    /// <summary>
    /// Reads the first SWF tag offset after the GFX header, frame-size RECT, frame rate, and
    /// frame count fields.
    /// </summary>
    /// <param name="gfxBytes">Complete GFX payload bytes.</param>
    /// <returns>Byte offset of the first SWF tag header.</returns>
    private static int GetFirstTagOffset(ReadOnlySpan<byte> gfxBytes)
    {
        if (gfxBytes.Length < 9)
        {
            throw new InvalidOperationException("GFX payload is too short to contain the SWF frame header.");
        }

        int nBits = gfxBytes[8] >> 3;
        int rectBitLength = 5 + (4 * nBits);
        int rectByteLength = (rectBitLength + 7) / 8;
        int firstTagOffset = checked(8 + rectByteLength + 4);

        if (firstTagOffset > gfxBytes.Length)
        {
            throw new InvalidOperationException("GFX frame header pointed beyond the end of the payload.");
        }

        return firstTagOffset;
    }

    /// <summary>
    /// Reads one SWF tag header and resolves its tag code, payload offset, and payload length.
    /// </summary>
    /// <param name="gfxBytes">Complete GFX payload bytes.</param>
    /// <param name="tagOffset">Byte offset of the tag header.</param>
    /// <param name="tagCode">Resolved SWF tag code.</param>
    /// <param name="tagDataOffset">Byte offset of the tag payload.</param>
    /// <param name="tagDataLength">Byte length of the tag payload.</param>
    private static void ReadTagHeader(
        ReadOnlySpan<byte> gfxBytes,
        int tagOffset,
        out int tagCode,
        out int tagDataOffset,
        out int tagDataLength)
    {
        int recordHeader = ReadUInt16(gfxBytes, tagOffset);
        tagCode = recordHeader >> 6;
        int shortLength = recordHeader & 0x3F;
        tagDataOffset = tagOffset + 2;

        if (shortLength == 0x3F)
        {
            tagDataLength = ReadInt32(gfxBytes, tagDataOffset);
            tagDataOffset += 4;
        }
        else
        {
            tagDataLength = shortLength;
        }

        if (tagDataLength < 0 || tagDataOffset + tagDataLength > gfxBytes.Length)
        {
            throw new InvalidOperationException($"SWF tag {tagCode} at offset {tagOffset} points outside the GFX payload.");
        }
    }

    /// <summary>
    /// Patches the retail root DoAction payload by extending the constant pool and replacing the
    /// uninitialized retail declaration with an initialized declaration.
    /// </summary>
    /// <param name="tagPayload">Original root DoAction tag payload.</param>
    /// <param name="buildLabel">Build label text to assign to <c>PCVersionString</c>.</param>
    /// <returns>Patched root DoAction tag payload.</returns>
    private static byte[] PatchRootDoActionPayload(ReadOnlySpan<byte> tagPayload, string buildLabel)
    {
        if (tagPayload.IsEmpty || tagPayload[0] != ConstantPoolActionCode)
        {
            throw new InvalidOperationException("Retail MainV2 root DoAction did not start with the expected ConstantPool action.");
        }

        int constantPoolActionLength = checked(3 + ReadUInt16(tagPayload, 1));
        List<string> constantPool = ReadConstantPoolStrings(tagPayload[..constantPoolActionLength]);
        int pcVersionStringIndex = FindRequiredStringIndex(constantPool, PcVersionStringName);
        int buildLabelIndex = FindOptionalStringIndex(constantPool, buildLabel);
        byte[] patchedConstantPoolAction = tagPayload[..constantPoolActionLength].ToArray();

        if (buildLabelIndex < 0)
        {
            buildLabelIndex = constantPool.Count;
            constantPool.Add(buildLabel);
            patchedConstantPoolAction = BuildConstantPoolAction(constantPool);
        }

        byte[] retailDeclarationBytes = BuildRetailDeclarationBytes(pcVersionStringIndex);
        int retailDeclarationOffset = FindUniqueByteSequence(tagPayload, retailDeclarationBytes, "retail PCVersionString declaration");
        byte[] initializedDeclarationBytes = BuildInitializedDeclarationBytes(pcVersionStringIndex, buildLabelIndex);
        using var stream = new MemoryStream(capacity: checked(tagPayload.Length + patchedConstantPoolAction.Length - constantPoolActionLength + initializedDeclarationBytes.Length - retailDeclarationBytes.Length));

        stream.Write(patchedConstantPoolAction, 0, patchedConstantPoolAction.Length);
        stream.Write(tagPayload[constantPoolActionLength..retailDeclarationOffset]);
        stream.Write(initializedDeclarationBytes, 0, initializedDeclarationBytes.Length);
        stream.Write(tagPayload[(retailDeclarationOffset + retailDeclarationBytes.Length)..]);
        return stream.ToArray();
    }

    /// <summary>
    /// Reads the string entries stored in one ConstantPool action.
    /// </summary>
    /// <param name="constantPoolAction">Complete ConstantPool action bytes, including opcode and length.</param>
    /// <returns>Constant-pool strings in index order.</returns>
    private static List<string> ReadConstantPoolStrings(ReadOnlySpan<byte> constantPoolAction)
    {
        if (constantPoolAction.Length < 5 || constantPoolAction[0] != ConstantPoolActionCode)
        {
            throw new InvalidOperationException("Expected a ConstantPool action.");
        }

        int actionDataLength = ReadUInt16(constantPoolAction, 1);
        int stringCount = ReadUInt16(constantPoolAction, 3);
        int actionEndOffset = checked(3 + actionDataLength);
        int cursor = 5;
        var strings = new List<string>(stringCount);

        for (int stringIndex = 0; stringIndex < stringCount; stringIndex++)
        {
            int terminatorOffset = FindNullTerminator(constantPoolAction, cursor, actionEndOffset);
            strings.Add(Encoding.UTF8.GetString(constantPoolAction[cursor..terminatorOffset]));
            cursor = terminatorOffset + 1;
        }

        if (cursor != actionEndOffset)
        {
            throw new InvalidOperationException("ConstantPool action length did not match its serialized string payload.");
        }

        return strings;
    }

    /// <summary>
    /// Finds the required constant-pool string index and fails fast when the string is missing.
    /// </summary>
    /// <param name="constantPool">Constant-pool strings from the retail root DoAction payload.</param>
    /// <param name="text">String value to locate.</param>
    /// <returns>Zero-based constant-pool index of the requested string.</returns>
    private static int FindRequiredStringIndex(IReadOnlyList<string> constantPool, string text)
    {
        int index = FindOptionalStringIndex(constantPool, text);

        if (index < 0)
        {
            throw new InvalidOperationException($"Required ConstantPool string '{text}' was not found.");
        }

        return index;
    }

    /// <summary>
    /// Finds one optional constant-pool string index.
    /// </summary>
    /// <param name="constantPool">Constant-pool strings from the retail root DoAction payload.</param>
    /// <param name="text">String value to locate.</param>
    /// <returns>Zero-based constant-pool index, or <c>-1</c> when the string is absent.</returns>
    private static int FindOptionalStringIndex(IReadOnlyList<string> constantPool, string text)
    {
        for (int index = 0; index < constantPool.Count; index++)
        {
            if (string.Equals(constantPool[index], text, StringComparison.Ordinal))
            {
                return index;
            }
        }

        return -1;
    }

    /// <summary>
    /// Builds the retail declaration byte sequence that pushes <c>PCVersionString</c> and then
    /// defines it without an initializer.
    /// </summary>
    /// <param name="pcVersionStringIndex">Constant-pool index for <c>PCVersionString</c>.</param>
    /// <returns>Retail declaration byte sequence.</returns>
    private static byte[] BuildRetailDeclarationBytes(int pcVersionStringIndex)
    {
        using var stream = new MemoryStream(capacity: 8);
        WritePushAction(stream, [pcVersionStringIndex]);
        stream.WriteByte(DefineLocal2ActionCode);
        return stream.ToArray();
    }

    /// <summary>
    /// Builds the initialized declaration byte sequence that pushes <c>PCVersionString</c>, pushes
    /// the build label, and then defines the initialized local variable.
    /// </summary>
    /// <param name="pcVersionStringIndex">Constant-pool index for <c>PCVersionString</c>.</param>
    /// <param name="buildLabelIndex">Constant-pool index for the build label.</param>
    /// <returns>Initialized declaration byte sequence.</returns>
    private static byte[] BuildInitializedDeclarationBytes(int pcVersionStringIndex, int buildLabelIndex)
    {
        using var stream = new MemoryStream(capacity: 12);
        WritePushAction(stream, [pcVersionStringIndex, buildLabelIndex]);
        stream.WriteByte(DefineLocalActionCode);
        return stream.ToArray();
    }

    /// <summary>
    /// Builds one ConstantPool action from the provided string list.
    /// </summary>
    /// <param name="constantPool">Constant-pool strings to serialize in index order.</param>
    /// <returns>Serialized ConstantPool action bytes.</returns>
    private static byte[] BuildConstantPoolAction(IReadOnlyList<string> constantPool)
    {
        using var dataStream = new MemoryStream();
        using (var dataWriter = new BinaryWriter(dataStream, Encoding.UTF8, leaveOpen: true))
        {
            dataWriter.Write((ushort)constantPool.Count);

            foreach (string value in constantPool)
            {
                byte[] stringBytes = Encoding.UTF8.GetBytes(value);
                dataWriter.Write(stringBytes);
                dataWriter.Write((byte)0);
            }
        }

        if (dataStream.Length > ushort.MaxValue)
        {
            throw new InvalidOperationException("Patched ConstantPool action exceeded the SWF action length limit.");
        }

        using var actionStream = new MemoryStream(capacity: checked((int)dataStream.Length + 3));
        actionStream.WriteByte(ConstantPoolActionCode);
        WriteUInt16(actionStream, checked((ushort)dataStream.Length));
        dataStream.Position = 0;
        dataStream.CopyTo(actionStream);
        return actionStream.ToArray();
    }

    /// <summary>
    /// Writes one Push action that serializes the provided constant-pool references in order.
    /// </summary>
    /// <param name="stream">Destination stream that receives the Push action.</param>
    /// <param name="constantIndexes">Constant-pool indexes to serialize into the Push action.</param>
    private static void WritePushAction(Stream stream, IReadOnlyList<int> constantIndexes)
    {
        using var actionDataStream = new MemoryStream(capacity: checked(constantIndexes.Count * 3));

        foreach (int constantIndex in constantIndexes)
        {
            WriteConstantReference(actionDataStream, constantIndex);
        }

        if (actionDataStream.Length > ushort.MaxValue)
        {
            throw new InvalidOperationException("Push action length exceeded the SWF action limit.");
        }

        stream.WriteByte(PushActionCode);
        WriteUInt16(stream, checked((ushort)actionDataStream.Length));
        actionDataStream.Position = 0;
        actionDataStream.CopyTo(stream);
    }

    /// <summary>
    /// Writes one constant-pool reference in the encoding form required by its index size.
    /// </summary>
    /// <param name="stream">Destination stream that receives the encoded reference.</param>
    /// <param name="constantIndex">Constant-pool index to encode.</param>
    private static void WriteConstantReference(Stream stream, int constantIndex)
    {
        if (constantIndex < 0)
        {
            throw new InvalidOperationException("Constant-pool indexes must not be negative.");
        }

        if (constantIndex <= byte.MaxValue)
        {
            stream.WriteByte(PushConstant8Type);
            stream.WriteByte((byte)constantIndex);
            return;
        }

        if (constantIndex > ushort.MaxValue)
        {
            throw new InvalidOperationException("Constant-pool indexes above 65535 are not supported.");
        }

        stream.WriteByte(PushConstant16Type);
        WriteUInt16(stream, (ushort)constantIndex);
    }

    /// <summary>
    /// Builds a long-form SWF tag header for the provided tag code and payload length.
    /// </summary>
    /// <param name="tagCode">SWF tag code.</param>
    /// <param name="payloadLength">Tag payload length in bytes.</param>
    /// <returns>Serialized long-form SWF tag header bytes.</returns>
    private static byte[] BuildLongTagHeader(int tagCode, int payloadLength)
    {
        using var stream = new MemoryStream(capacity: 6);
        WriteUInt16(stream, checked((ushort)((tagCode << 6) | 0x3F)));
        WriteInt32(stream, payloadLength);
        return stream.ToArray();
    }

    /// <summary>
    /// Determines whether one byte span contains the provided ASCII text.
    /// </summary>
    /// <param name="buffer">Buffer to search.</param>
    /// <param name="text">ASCII text to locate.</param>
    /// <returns>True when the text was found.</returns>
    private static bool ContainsAscii(ReadOnlySpan<byte> buffer, string text)
    {
        byte[] needle = Encoding.ASCII.GetBytes(text);

        for (int offset = 0; offset <= buffer.Length - needle.Length; offset++)
        {
            if (buffer[offset..(offset + needle.Length)].SequenceEqual(needle))
            {
                return true;
            }
        }

        return false;
    }

    /// <summary>
    /// Finds one unique byte sequence within a larger byte span.
    /// </summary>
    /// <param name="buffer">Buffer to search.</param>
    /// <param name="pattern">Exact byte sequence to locate.</param>
    /// <param name="description">Human-readable description used in failure messages.</param>
    /// <returns>Byte offset of the unique match.</returns>
    private static int FindUniqueByteSequence(ReadOnlySpan<byte> buffer, ReadOnlySpan<byte> pattern, string description)
    {
        int matchOffset = -1;

        for (int offset = 0; offset <= buffer.Length - pattern.Length; offset++)
        {
            if (!buffer[offset..(offset + pattern.Length)].SequenceEqual(pattern))
            {
                continue;
            }

            if (matchOffset >= 0)
            {
                throw new InvalidOperationException($"The {description} was not unique inside the retail root DoAction payload.");
            }

            matchOffset = offset;
        }

        if (matchOffset < 0)
        {
            throw new InvalidOperationException($"Could not find the {description} inside the retail root DoAction payload.");
        }

        return matchOffset;
    }

    /// <summary>
    /// Finds the null terminator for one serialized ConstantPool string.
    /// </summary>
    /// <param name="buffer">ConstantPool action bytes.</param>
    /// <param name="startOffset">Offset where the current string starts.</param>
    /// <param name="endOffset">Exclusive end offset of the ConstantPool action payload.</param>
    /// <returns>Byte offset of the null terminator.</returns>
    private static int FindNullTerminator(ReadOnlySpan<byte> buffer, int startOffset, int endOffset)
    {
        for (int offset = startOffset; offset < endOffset; offset++)
        {
            if (buffer[offset] == 0)
            {
                return offset;
            }
        }

        throw new InvalidOperationException("ConstantPool string was not null terminated.");
    }

    /// <summary>
    /// Writes one 16-bit unsigned value to a writable stream.
    /// </summary>
    /// <param name="stream">Destination stream.</param>
    /// <param name="value">Unsigned 16-bit value to write.</param>
    private static void WriteUInt16(Stream stream, ushort value)
    {
        Span<byte> buffer = stackalloc byte[sizeof(ushort)];
        BitConverter.TryWriteBytes(buffer, value);
        stream.Write(buffer);
    }

    /// <summary>
    /// Writes one 32-bit signed value to a writable stream.
    /// </summary>
    /// <param name="stream">Destination stream.</param>
    /// <param name="value">Signed 32-bit value to write.</param>
    private static void WriteInt32(Stream stream, int value)
    {
        Span<byte> buffer = stackalloc byte[sizeof(int)];
        BitConverter.TryWriteBytes(buffer, value);
        stream.Write(buffer);
    }

    /// <summary>
    /// Writes one 32-bit signed value to a byte array at an absolute offset.
    /// </summary>
    /// <param name="buffer">Destination byte array.</param>
    /// <param name="offset">Byte offset where the value is written.</param>
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
    /// Reads one 16-bit unsigned value from a byte span.
    /// </summary>
    /// <param name="buffer">Source byte span.</param>
    /// <param name="offset">Byte offset of the value to read.</param>
    /// <returns>Unsigned 16-bit value at the requested offset.</returns>
    private static ushort ReadUInt16(ReadOnlySpan<byte> buffer, int offset)
    {
        if (offset < 0 || offset + sizeof(ushort) > buffer.Length)
        {
            throw new InvalidOperationException($"Cannot read UInt16 at offset {offset}.");
        }

        return BitConverter.ToUInt16(buffer[offset..(offset + sizeof(ushort))]);
    }

    /// <summary>
    /// Reads one 32-bit signed value from a byte span.
    /// </summary>
    /// <param name="buffer">Source byte span.</param>
    /// <param name="offset">Byte offset of the value to read.</param>
    /// <returns>Signed 32-bit value at the requested offset.</returns>
    private static int ReadInt32(ReadOnlySpan<byte> buffer, int offset)
    {
        if (offset < 0 || offset + sizeof(int) > buffer.Length)
        {
            throw new InvalidOperationException($"Cannot read Int32 at offset {offset}.");
        }

        return BitConverter.ToInt32(buffer[offset..(offset + sizeof(int))]);
    }
}
