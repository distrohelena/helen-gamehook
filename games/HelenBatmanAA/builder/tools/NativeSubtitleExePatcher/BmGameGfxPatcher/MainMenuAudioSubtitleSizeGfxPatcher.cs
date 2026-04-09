using System.Text;

namespace BmGameGfxPatcher;

/// <summary>
/// Rewrites the retail MainMenu.MainV2 payload by transplanting the prototype audio-screen sprite and
/// subtitle-size-aware <c>rs.ui.ListItem</c> class-definition tag.
/// </summary>
internal static class MainMenuAudioSubtitleSizeGfxPatcher
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
    /// Stores the SWF tag code for DefineSprite.
    /// </summary>
    private const int DefineSpriteTagCode = 39;

    /// <summary>
    /// Stores the SWF tag code for DoInitAction.
    /// </summary>
    private const int DoInitActionTagCode = 59;

    /// <summary>
    /// Stores the ScreenOptionsAudio sprite id used by MainMenu.MainV2.
    /// </summary>
    private const ushort ScreenOptionsAudioSpriteId = 359;

    /// <summary>
    /// Stores the retail ListItem token set used to identify the vanilla class-definition tag.
    /// </summary>
    private static readonly string[] RetailListItemTokens =
    [
        "GameVariable",
        "LeftClicker",
        "ItemText",
        "FE_GetDefault"
    ];

    /// <summary>
    /// Stores the prototype ListItem token set used to identify the subtitle-size-aware class-definition tag.
    /// </summary>
    private static readonly string[] PrototypeListItemTokens =
    [
        "UsesSubtitleSizeStorage",
        "AreSubtitlesEnabled",
        "FE_GetSubtitles"
    ];

    /// <summary>
    /// Represents one top-level SWF tag resolved inside a GFX payload.
    /// </summary>
    private readonly struct SwfTag
    {
        /// <summary>
        /// Stores the byte offset of the tag header inside the full payload.
        /// </summary>
        public readonly int Offset;

        /// <summary>
        /// Stores the SWF tag code.
        /// </summary>
        public readonly int Code;

        /// <summary>
        /// Stores the byte offset of the payload body.
        /// </summary>
        public readonly int DataOffset;

        /// <summary>
        /// Stores the serialized payload length in bytes.
        /// </summary>
        public readonly int DataLength;

        /// <summary>
        /// Initializes one resolved SWF tag descriptor.
        /// </summary>
        /// <param name="offset">Byte offset of the tag header.</param>
        /// <param name="code">SWF tag code.</param>
        /// <param name="dataOffset">Byte offset of the tag payload.</param>
        /// <param name="dataLength">Byte length of the tag payload.</param>
        public SwfTag(int offset, int code, int dataOffset, int dataLength)
        {
            Offset = offset;
            Code = code;
            DataOffset = dataOffset;
            DataLength = dataLength;
        }
    }

    /// <summary>
    /// Patches one MainV2 GFX payload by transplanting the prototype audio-screen sprite and ListItem class-definition tag.
    /// </summary>
    /// <param name="retailGfxBytes">Original retail MainV2 GFX payload bytes.</param>
    /// <param name="prototypeGfxPath">Path to the prototype MainV2 subtitle-size GFX used as the transplant source.</param>
    /// <returns>Patched MainV2 GFX payload bytes.</returns>
    public static byte[] Patch(ReadOnlySpan<byte> retailGfxBytes, string prototypeGfxPath)
    {
        ValidateGfxHeader(retailGfxBytes);
        byte[] prototypeGfxBytes = LoadPrototypeGfxBytes(prototypeGfxPath);
        ValidateGfxHeader(prototypeGfxBytes);

        SwfTag retailAudioSpriteTag = FindDefineSpriteTag(retailGfxBytes, ScreenOptionsAudioSpriteId, "retail ScreenOptionsAudio sprite");
        SwfTag prototypeAudioSpriteTag = FindDefineSpriteTag(prototypeGfxBytes, ScreenOptionsAudioSpriteId, "prototype ScreenOptionsAudio sprite");
        SwfTag retailListItemTag = FindUniqueTagByTokens(retailGfxBytes, DoInitActionTagCode, RetailListItemTokens, "retail rs.ui.ListItem class-definition");
        SwfTag prototypeListItemTag = FindUniqueTagByTokens(prototypeGfxBytes, DoInitActionTagCode, PrototypeListItemTokens, "prototype rs.ui.ListItem class-definition");

        EnsureMatchingTagCode(retailAudioSpriteTag, prototypeAudioSpriteTag, "ScreenOptionsAudio sprite");
        EnsureMatchingTagCode(retailListItemTag, prototypeListItemTag, "rs.ui.ListItem class-definition");
        return RewritePatchedTags(
            retailGfxBytes,
            retailAudioSpriteTag,
            prototypeAudioSpriteTag,
            retailListItemTag,
            prototypeListItemTag,
            prototypeGfxBytes);
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
    /// Loads the prototype MainV2 subtitle-size payload from the nearest checkout that contains the generated reference asset.
    /// </summary>
    /// <param name="prototypeGfxPath">Path to the prototype MainV2 subtitle-size GFX used as the transplant source.</param>
    /// <returns>Prototype MainV2 payload bytes.</returns>
    private static byte[] LoadPrototypeGfxBytes(string prototypeGfxPath)
    {
        string prototypePath = ResolvePrototypeGfxPath(prototypeGfxPath);
        return File.ReadAllBytes(prototypePath);
    }

    /// <summary>
    /// Resolves the explicit prototype GFX path and fails fast when the file is missing.
    /// </summary>
    /// <param name="prototypeGfxPath">Explicit path to the prototype MainV2 subtitle-size GFX.</param>
    /// <returns>Absolute path to the generated prototype MainV2 subtitle-size payload.</returns>
    private static string ResolvePrototypeGfxPath(string prototypeGfxPath)
    {
        if (string.IsNullOrWhiteSpace(prototypeGfxPath))
        {
            throw new InvalidOperationException("Provide --prototype-gfx with the generated MainV2 subtitle-size payload path.");
        }

        string explicitPath = Path.GetFullPath(prototypeGfxPath);
        if (!File.Exists(explicitPath))
        {
            throw new InvalidOperationException($"Prototype MainV2 subtitle-size payload was not found: {explicitPath}");
        }

        return explicitPath;
    }

    /// <summary>
    /// Finds the unique top-level DefineSprite tag for one sprite id.
    /// </summary>
    /// <param name="gfxBytes">Complete GFX payload bytes.</param>
    /// <param name="spriteId">Sprite id to locate.</param>
    /// <param name="description">Human-readable description used in failure messages.</param>
    /// <returns>Resolved top-level DefineSprite tag.</returns>
    private static SwfTag FindDefineSpriteTag(ReadOnlySpan<byte> gfxBytes, ushort spriteId, string description)
    {
        int offset = GetFirstTagOffset(gfxBytes);
        bool found = false;
        SwfTag match = default;

        while (offset < gfxBytes.Length)
        {
            SwfTag tag = ReadTagHeader(gfxBytes, offset);

            if (tag.Code == 0)
            {
                break;
            }

            if (tag.Code == DefineSpriteTagCode &&
                tag.DataLength >= sizeof(ushort) &&
                ReadUInt16(gfxBytes, tag.DataOffset) == spriteId)
            {
                if (found)
                {
                    throw new InvalidOperationException($"Multiple {description} tags were found.");
                }

                match = tag;
                found = true;
            }

            offset = tag.DataOffset + tag.DataLength;
        }

        if (!found)
        {
            throw new InvalidOperationException($"Could not find the {description} tag.");
        }

        return match;
    }

    /// <summary>
    /// Finds the unique top-level tag with the requested code whose payload contains every required ASCII token.
    /// </summary>
    /// <param name="gfxBytes">Complete GFX payload bytes.</param>
    /// <param name="tagCode">SWF tag code to match.</param>
    /// <param name="requiredTokens">ASCII tokens that must all appear in the payload.</param>
    /// <param name="description">Human-readable description used in failure messages.</param>
    /// <returns>Resolved top-level tag.</returns>
    private static SwfTag FindUniqueTagByTokens(
        ReadOnlySpan<byte> gfxBytes,
        int tagCode,
        IReadOnlyList<string> requiredTokens,
        string description)
    {
        int offset = GetFirstTagOffset(gfxBytes);
        bool found = false;
        SwfTag match = default;

        while (offset < gfxBytes.Length)
        {
            SwfTag tag = ReadTagHeader(gfxBytes, offset);

            if (tag.Code == 0)
            {
                break;
            }

            if (tag.Code == tagCode &&
                ContainsAllAsciiTokens(gfxBytes[tag.DataOffset..(tag.DataOffset + tag.DataLength)], requiredTokens))
            {
                if (found)
                {
                    throw new InvalidOperationException($"Multiple {description} tags were found.");
                }

                match = tag;
                found = true;
            }

            offset = tag.DataOffset + tag.DataLength;
        }

        if (!found)
        {
            throw new InvalidOperationException($"Could not find the {description} tag.");
        }

        return match;
    }

    /// <summary>
    /// Verifies that one retail tag and its prototype replacement share the same SWF tag code.
    /// </summary>
    /// <param name="retailTag">Resolved retail tag.</param>
    /// <param name="prototypeTag">Resolved prototype tag.</param>
    /// <param name="description">Human-readable description used in failure messages.</param>
    private static void EnsureMatchingTagCode(SwfTag retailTag, SwfTag prototypeTag, string description)
    {
        if (retailTag.Code != prototypeTag.Code)
        {
            throw new InvalidOperationException(
                $"The retail and prototype {description} tags used different SWF codes ({retailTag.Code} vs {prototypeTag.Code}).");
        }
    }

    /// <summary>
    /// Rewrites the retail payload by replacing the targeted top-level tags with the prototype payload bytes.
    /// </summary>
    /// <param name="retailGfxBytes">Original retail MainV2 payload bytes.</param>
    /// <param name="retailAudioSpriteTag">Retail ScreenOptionsAudio sprite tag.</param>
    /// <param name="prototypeAudioSpriteTag">Prototype ScreenOptionsAudio sprite tag.</param>
    /// <param name="retailListItemTag">Retail ListItem class-definition tag.</param>
    /// <param name="prototypeListItemTag">Prototype ListItem class-definition tag.</param>
    /// <param name="prototypeGfxBytes">Complete prototype MainV2 payload bytes.</param>
    /// <returns>Patched MainV2 payload bytes.</returns>
    private static byte[] RewritePatchedTags(
        ReadOnlySpan<byte> retailGfxBytes,
        SwfTag retailAudioSpriteTag,
        SwfTag prototypeAudioSpriteTag,
        SwfTag retailListItemTag,
        SwfTag prototypeListItemTag,
        ReadOnlySpan<byte> prototypeGfxBytes)
    {
        if (retailAudioSpriteTag.Offset >= retailListItemTag.Offset)
        {
            throw new InvalidOperationException("Expected the ScreenOptionsAudio sprite tag to appear before the ListItem class-definition tag.");
        }

        byte[] prototypeAudioPayload = prototypeGfxBytes[prototypeAudioSpriteTag.DataOffset..(prototypeAudioSpriteTag.DataOffset + prototypeAudioSpriteTag.DataLength)].ToArray();
        byte[] prototypeListItemPayload = prototypeGfxBytes[prototypeListItemTag.DataOffset..(prototypeListItemTag.DataOffset + prototypeListItemTag.DataLength)].ToArray();
        byte[] audioHeader = BuildLongTagHeader(retailAudioSpriteTag.Code, prototypeAudioPayload.Length);
        byte[] listItemHeader = BuildLongTagHeader(retailListItemTag.Code, prototypeListItemPayload.Length);
        int retailAudioEndOffset = retailAudioSpriteTag.DataOffset + retailAudioSpriteTag.DataLength;
        int retailListItemEndOffset = retailListItemTag.DataOffset + retailListItemTag.DataLength;
        int patchedLength = retailAudioSpriteTag.Offset +
            audioHeader.Length +
            prototypeAudioPayload.Length +
            (retailListItemTag.Offset - retailAudioEndOffset) +
            listItemHeader.Length +
            prototypeListItemPayload.Length +
            (retailGfxBytes.Length - retailListItemEndOffset);
        byte[] patchedGfx = new byte[patchedLength];
        int writeOffset = 0;

        retailGfxBytes[..retailAudioSpriteTag.Offset].CopyTo(patchedGfx);
        writeOffset += retailAudioSpriteTag.Offset;
        audioHeader.CopyTo(patchedGfx.AsSpan(writeOffset));
        writeOffset += audioHeader.Length;
        prototypeAudioPayload.CopyTo(patchedGfx.AsSpan(writeOffset));
        writeOffset += prototypeAudioPayload.Length;

        ReadOnlySpan<byte> middleBytes = retailGfxBytes[retailAudioEndOffset..retailListItemTag.Offset];
        middleBytes.CopyTo(patchedGfx.AsSpan(writeOffset));
        writeOffset += middleBytes.Length;

        listItemHeader.CopyTo(patchedGfx.AsSpan(writeOffset));
        writeOffset += listItemHeader.Length;
        prototypeListItemPayload.CopyTo(patchedGfx.AsSpan(writeOffset));
        writeOffset += prototypeListItemPayload.Length;

        ReadOnlySpan<byte> suffixBytes = retailGfxBytes[retailListItemEndOffset..];
        suffixBytes.CopyTo(patchedGfx.AsSpan(writeOffset));
        writeOffset += suffixBytes.Length;

        if (writeOffset != patchedGfx.Length)
        {
            throw new InvalidOperationException("Patched GFX reassembly wrote an unexpected number of bytes.");
        }

        WriteInt32(patchedGfx, FileLengthOffset, patchedGfx.Length);
        return patchedGfx;
    }

    /// <summary>
    /// Reads the first SWF tag offset after the GFX header, frame-size RECT, frame rate, and frame count fields.
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
    /// <returns>Resolved SWF tag descriptor.</returns>
    private static SwfTag ReadTagHeader(ReadOnlySpan<byte> gfxBytes, int tagOffset)
    {
        int recordHeader = ReadUInt16(gfxBytes, tagOffset);
        int tagCode = recordHeader >> 6;
        int shortLength = recordHeader & 0x3F;
        int tagDataOffset = tagOffset + 2;
        int tagDataLength;

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

        return new SwfTag(tagOffset, tagCode, tagDataOffset, tagDataLength);
    }

    /// <summary>
    /// Determines whether one payload contains every required ASCII token.
    /// </summary>
    /// <param name="buffer">Buffer to search.</param>
    /// <param name="requiredTokens">ASCII tokens that must all appear.</param>
    /// <returns>True when every token was found.</returns>
    private static bool ContainsAllAsciiTokens(ReadOnlySpan<byte> buffer, IReadOnlyList<string> requiredTokens)
    {
        foreach (string token in requiredTokens)
        {
            if (!ContainsAscii(buffer, token))
            {
                return false;
            }
        }

        return true;
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
