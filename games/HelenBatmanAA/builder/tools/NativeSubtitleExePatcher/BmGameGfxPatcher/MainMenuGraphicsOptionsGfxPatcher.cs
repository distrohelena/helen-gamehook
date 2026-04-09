using System.Text;

namespace BmGameGfxPatcher;

/// <summary>
/// Rewrites the retail MainMenu.MainV2 payload by transplanting the graphics-options
/// frontend assets from the prototype movie without replacing the full movie payload.
/// </summary>
internal static class MainMenuGraphicsOptionsGfxPatcher
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
    /// Stores the SWF tag code for ShowFrame.
    /// </summary>
    private const int ShowFrameTagCode = 1;

    /// <summary>
    /// Stores the SWF tag code for DefineSprite.
    /// </summary>
    private const int DefineSpriteTagCode = 39;

    /// <summary>
    /// Stores the SWF tag code for ExportAssets.
    /// </summary>
    private const int ExportAssetsTagCode = 56;

    /// <summary>
    /// Stores the SWF tag code for DoInitAction.
    /// </summary>
    private const int DoInitActionTagCode = 59;

    /// <summary>
    /// Stores the sprite id used by <c>ScreenOptionsMenu</c> inside MainMenu.MainV2.
    /// </summary>
    private const ushort ScreenOptionsMenuSpriteId = 333;

    /// <summary>
    /// Stores the source sprite id emitted by the prototype builder for <c>ScreenOptionsGraphics</c>.
    /// </summary>
    private const ushort PrototypeScreenOptionsGraphicsSpriteId = 600;

    /// <summary>
    /// Stores the source sprite id emitted by the prototype builder for <c>GraphicsExitPrompt</c>.
    /// </summary>
    private const ushort PrototypeGraphicsExitPromptSpriteId = 601;

    /// <summary>
    /// Stores the retail-safe sprite id used when transplanting <c>ScreenOptionsGraphics</c> into MainV2.
    /// </summary>
    private const ushort RetailScreenOptionsGraphicsSpriteId = 4096;

    /// <summary>
    /// Stores the retail-safe sprite id used when transplanting <c>GraphicsExitPrompt</c> into MainV2.
    /// </summary>
    private const ushort RetailGraphicsExitPromptSpriteId = 4097;

    /// <summary>
    /// Stores the SWF tag codes whose payload begins with a character-definition id so the patcher can
    /// prove the fixed retail destination ids are unused before it rewrites the retail payload.
    /// </summary>
    private static readonly int[] CharacterDefinitionTagCodes =
    [
        2,
        6,
        7,
        10,
        11,
        14,
        20,
        21,
        22,
        32,
        33,
        34,
        35,
        36,
        37,
        39,
        46,
        48,
        60,
        75,
        83,
        84,
        87,
        90,
        91
    ];

    /// <summary>
    /// Stores the token that uniquely identifies the <c>ScreenOptionsGraphics</c> export tag.
    /// </summary>
    private static readonly string[] ScreenOptionsGraphicsExportTokens =
    [
        "ScreenOptionsGraphics"
    ];

    /// <summary>
    /// Stores the token that uniquely identifies the <c>GraphicsExitPrompt</c> export tag.
    /// </summary>
    private static readonly string[] GraphicsExitPromptExportTokens =
    [
        "GraphicsExitPrompt"
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
        /// Stores the byte offset of the tag payload.
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
    /// Patches one retail MainV2 GFX payload by replacing only the options-menu sprite
    /// and appending the prototype graphics screen assets before the retail root ShowFrame.
    /// </summary>
    /// <param name="retailGfxBytes">Original retail MainV2 GFX payload bytes.</param>
    /// <param name="prototypeGfxPath">Path to the generated graphics-options prototype MainV2 GFX file.</param>
    /// <returns>Patched MainV2 GFX payload bytes.</returns>
    public static byte[] Patch(ReadOnlySpan<byte> retailGfxBytes, string prototypeGfxPath)
    {
        ValidateGfxHeader(retailGfxBytes, "retail MainMenu.MainV2 payload");
        byte[] prototypeGfxBytes = LoadPrototypeGfxBytes(prototypeGfxPath);
        ValidateGfxHeader(prototypeGfxBytes, "prototype MainMenu.MainV2 payload");

        SwfTag retailOptionsMenuTag = FindDefineSpriteTag(
            retailGfxBytes,
            ScreenOptionsMenuSpriteId,
            "retail ScreenOptionsMenu sprite");
        SwfTag retailRootShowFrameTag = FindUniqueTagByCode(
            retailGfxBytes,
            ShowFrameTagCode,
            "retail root ShowFrame tag");
        SwfTag prototypeOptionsMenuTag = FindDefineSpriteTag(
            prototypeGfxBytes,
            ScreenOptionsMenuSpriteId,
            "prototype ScreenOptionsMenu sprite");
        SwfTag prototypeGraphicsScreenTag = FindDefineSpriteTag(
            prototypeGfxBytes,
            PrototypeScreenOptionsGraphicsSpriteId,
            "prototype ScreenOptionsGraphics sprite");
        SwfTag prototypeGraphicsExitPromptTag = FindDefineSpriteTag(
            prototypeGfxBytes,
            PrototypeGraphicsExitPromptSpriteId,
            "prototype GraphicsExitPrompt sprite");
        SwfTag prototypeGraphicsScreenExportTag = FindUniqueTagByTokens(
            prototypeGfxBytes,
            ExportAssetsTagCode,
            ScreenOptionsGraphicsExportTokens,
            "prototype ScreenOptionsGraphics export tag");
        SwfTag prototypeGraphicsExitPromptExportTag = FindUniqueTagByTokens(
            prototypeGfxBytes,
            ExportAssetsTagCode,
            GraphicsExitPromptExportTokens,
            "prototype GraphicsExitPrompt export tag");
        SwfTag prototypeGraphicsScreenDoInitActionTag = FindDoInitActionTag(
            prototypeGfxBytes,
            PrototypeScreenOptionsGraphicsSpriteId,
            "prototype ScreenOptionsGraphics DoInitAction tag");
        SwfTag prototypeGraphicsExitPromptDoInitActionTag = FindDoInitActionTag(
            prototypeGfxBytes,
            PrototypeGraphicsExitPromptSpriteId,
            "prototype GraphicsExitPrompt DoInitAction tag");

        EnsureRetailDestinationSpriteIdIsUnused(
            retailGfxBytes,
            RetailScreenOptionsGraphicsSpriteId,
            "ScreenOptionsGraphics");
        EnsureRetailDestinationSpriteIdIsUnused(
            retailGfxBytes,
            RetailGraphicsExitPromptSpriteId,
            "GraphicsExitPrompt");
        EnsureMatchingTagCode(retailOptionsMenuTag, prototypeOptionsMenuTag, "ScreenOptionsMenu sprite");
        return RewritePatchedTags(
            retailGfxBytes,
            retailOptionsMenuTag,
            retailRootShowFrameTag,
            prototypeOptionsMenuTag,
            prototypeGraphicsScreenTag,
            prototypeGraphicsScreenExportTag,
            prototypeGraphicsExitPromptTag,
            prototypeGraphicsExitPromptExportTag,
            prototypeGraphicsScreenDoInitActionTag,
            prototypeGraphicsExitPromptDoInitActionTag,
            prototypeGfxBytes);
    }

    /// <summary>
    /// Loads the generated prototype GFX payload from disk and fails fast when the path is missing.
    /// </summary>
    /// <param name="prototypeGfxPath">Path to the generated graphics-options prototype MainV2 GFX file.</param>
    /// <returns>Prototype MainV2 GFX bytes.</returns>
    private static byte[] LoadPrototypeGfxBytes(string prototypeGfxPath)
    {
        if (string.IsNullOrWhiteSpace(prototypeGfxPath))
        {
            throw new InvalidOperationException("Provide --prototype-gfx with the generated MainV2 graphics-options payload path.");
        }

        string fullPrototypePath = Path.GetFullPath(prototypeGfxPath);
        if (!File.Exists(fullPrototypePath))
        {
            throw new InvalidOperationException($"Prototype MainV2 graphics-options payload was not found: {fullPrototypePath}");
        }

        return File.ReadAllBytes(fullPrototypePath);
    }

    /// <summary>
    /// Verifies that one byte span begins with a valid Batman GFX header.
    /// </summary>
    /// <param name="gfxBytes">Candidate GFX payload bytes.</param>
    /// <param name="description">Human-readable payload name used in failure messages.</param>
    private static void ValidateGfxHeader(ReadOnlySpan<byte> gfxBytes, string description)
    {
        if (gfxBytes.Length < 8)
        {
            throw new InvalidOperationException($"{description} is too short to contain a valid GFX header.");
        }

        if (!gfxBytes[..3].SequenceEqual(Encoding.ASCII.GetBytes(GfxMagic)))
        {
            throw new InvalidOperationException($"{description} is not a GFX file.");
        }
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
    /// Verifies that one reserved retail destination sprite id is unused before the transplant remaps prototype tags onto it.
    /// </summary>
    /// <param name="gfxBytes">Complete retail MainV2 GFX payload bytes.</param>
    /// <param name="spriteId">Reserved retail sprite id that must remain unused.</param>
    /// <param name="description">Human-readable asset name used in failure messages.</param>
    private static void EnsureRetailDestinationSpriteIdIsUnused(
        ReadOnlySpan<byte> gfxBytes,
        ushort spriteId,
        string description)
    {
        if (TryFindCharacterDefinitionTag(gfxBytes, spriteId, out SwfTag definitionTag))
        {
            throw new InvalidOperationException(
                $"Retail MainMenu.MainV2 already defines character id {spriteId} for {description} remapping " +
                $"via top-level SWF tag code {definitionTag.Code} at offset {definitionTag.Offset}.");
        }

        if (TryFindExportedCharacterReference(gfxBytes, spriteId, out string exportName))
        {
            throw new InvalidOperationException(
                $"Retail MainMenu.MainV2 already exports character id {spriteId} as '{exportName}', so {description} cannot reuse that fixed retail id.");
        }

        if (TryFindDoInitActionTag(gfxBytes, spriteId, out SwfTag doInitActionTag))
        {
            throw new InvalidOperationException(
                $"Retail MainMenu.MainV2 already binds DoInitAction sprite id {spriteId} at offset {doInitActionTag.Offset}, so {description} cannot reuse that fixed retail id.");
        }
    }

    /// <summary>
    /// Finds the first top-level character-definition tag that already owns one reserved character id.
    /// </summary>
    /// <param name="gfxBytes">Complete retail MainV2 GFX payload bytes.</param>
    /// <param name="characterId">Character id that must not already be defined.</param>
    /// <param name="tag">Resolved conflicting character-definition tag when one exists.</param>
    /// <returns>True when a conflicting top-level character-definition tag was found.</returns>
    private static bool TryFindCharacterDefinitionTag(
        ReadOnlySpan<byte> gfxBytes,
        ushort characterId,
        out SwfTag tag)
    {
        int offset = GetFirstTagOffset(gfxBytes);

        while (offset < gfxBytes.Length)
        {
            SwfTag currentTag = ReadTagHeader(gfxBytes, offset);

            if (currentTag.Code == 0)
            {
                break;
            }

            if (IsCharacterDefinitionTagCode(currentTag.Code) &&
                currentTag.DataLength >= sizeof(ushort) &&
                ReadUInt16(gfxBytes, currentTag.DataOffset) == characterId)
            {
                tag = currentTag;
                return true;
            }

            offset = currentTag.DataOffset + currentTag.DataLength;
        }

        tag = default;
        return false;
    }

    /// <summary>
    /// Determines whether one SWF tag code introduces a top-level character definition whose payload starts with the character id.
    /// </summary>
    /// <param name="tagCode">SWF tag code to classify.</param>
    /// <returns>True when the tag code encodes a top-level character definition.</returns>
    private static bool IsCharacterDefinitionTagCode(int tagCode)
    {
        return Array.IndexOf(CharacterDefinitionTagCodes, tagCode) >= 0;
    }

    /// <summary>
    /// Finds the first top-level ExportAssets entry that already references one reserved character id.
    /// </summary>
    /// <param name="gfxBytes">Complete retail MainV2 GFX payload bytes.</param>
    /// <param name="characterId">Character id that must not already be exported.</param>
    /// <param name="exportName">Exported linkage name that already claims the character id.</param>
    /// <returns>True when a conflicting ExportAssets entry was found.</returns>
    private static bool TryFindExportedCharacterReference(
        ReadOnlySpan<byte> gfxBytes,
        ushort characterId,
        out string exportName)
    {
        int offset = GetFirstTagOffset(gfxBytes);

        while (offset < gfxBytes.Length)
        {
            SwfTag tag = ReadTagHeader(gfxBytes, offset);

            if (tag.Code == 0)
            {
                break;
            }

            if (tag.Code == ExportAssetsTagCode)
            {
                ReadOnlySpan<byte> payload = gfxBytes[tag.DataOffset..(tag.DataOffset + tag.DataLength)];
                if (payload.Length < sizeof(ushort))
                {
                    throw new InvalidOperationException($"ExportAssets tag at offset {tag.Offset} was truncated before the export count.");
                }

                ushort exportCount = ReadUInt16(payload, 0);
                int exportOffset = sizeof(ushort);

                for (int index = 0; index < exportCount; index++)
                {
                    if (exportOffset + sizeof(ushort) > payload.Length)
                    {
                        throw new InvalidOperationException(
                            $"ExportAssets tag at offset {tag.Offset} was truncated before export entry {index} sprite id.");
                    }

                    ushort exportedCharacterId = ReadUInt16(payload, exportOffset);
                    exportOffset += sizeof(ushort);
                    string currentExportName = ReadNullTerminatedAsciiString(payload, ref exportOffset, $"ExportAssets tag at offset {tag.Offset} entry {index}");

                    if (exportedCharacterId == characterId)
                    {
                        exportName = currentExportName;
                        return true;
                    }
                }
            }

            offset = tag.DataOffset + tag.DataLength;
        }

        exportName = string.Empty;
        return false;
    }

    /// <summary>
    /// Finds the first top-level DoInitAction tag that already binds one reserved sprite id.
    /// </summary>
    /// <param name="gfxBytes">Complete retail MainV2 GFX payload bytes.</param>
    /// <param name="spriteId">Sprite id that must not already be bound.</param>
    /// <param name="tag">Resolved conflicting DoInitAction tag when one exists.</param>
    /// <returns>True when a conflicting DoInitAction tag was found.</returns>
    private static bool TryFindDoInitActionTag(
        ReadOnlySpan<byte> gfxBytes,
        ushort spriteId,
        out SwfTag tag)
    {
        int offset = GetFirstTagOffset(gfxBytes);

        while (offset < gfxBytes.Length)
        {
            SwfTag currentTag = ReadTagHeader(gfxBytes, offset);

            if (currentTag.Code == 0)
            {
                break;
            }

            if (currentTag.Code == DoInitActionTagCode &&
                currentTag.DataLength >= sizeof(ushort) &&
                ReadUInt16(gfxBytes, currentTag.DataOffset) == spriteId)
            {
                tag = currentTag;
                return true;
            }

            offset = currentTag.DataOffset + currentTag.DataLength;
        }

        tag = default;
        return false;
    }

    /// <summary>
    /// Reads one null-terminated ASCII string from a SWF payload and advances the supplied offset past the terminator.
    /// </summary>
    /// <param name="buffer">Payload buffer that stores the null-terminated ASCII string.</param>
    /// <param name="offset">Byte offset of the string start, updated to the first byte after the terminator.</param>
    /// <param name="description">Human-readable description used in failure messages.</param>
    /// <returns>Decoded ASCII string.</returns>
    private static string ReadNullTerminatedAsciiString(
        ReadOnlySpan<byte> buffer,
        ref int offset,
        string description)
    {
        int startOffset = offset;

        while (offset < buffer.Length && buffer[offset] != 0)
        {
            offset++;
        }

        if (offset >= buffer.Length)
        {
            throw new InvalidOperationException($"{description} was missing its string terminator.");
        }

        string value = Encoding.ASCII.GetString(buffer[startOffset..offset]);
        offset++;
        return value;
    }

    /// <summary>
    /// Finds one top-level tag by SWF tag code and requires it to be unique.
    /// </summary>
    /// <param name="gfxBytes">Complete GFX payload bytes.</param>
    /// <param name="tagCode">SWF tag code to locate.</param>
    /// <param name="description">Human-readable description used in failure messages.</param>
    /// <returns>Resolved top-level tag.</returns>
    private static SwfTag FindUniqueTagByCode(ReadOnlySpan<byte> gfxBytes, int tagCode, string description)
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

            if (tag.Code == tagCode)
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
            throw new InvalidOperationException($"Could not find the {description}.");
        }

        return match;
    }

    /// <summary>
    /// Finds the unique top-level ExportAssets tag whose payload contains every required ASCII token.
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
    /// Finds the unique top-level DoInitAction tag for one sprite id.
    /// </summary>
    /// <param name="gfxBytes">Complete GFX payload bytes.</param>
    /// <param name="spriteId">Sprite id encoded in the DoInitAction payload.</param>
    /// <param name="description">Human-readable description used in failure messages.</param>
    /// <returns>Resolved top-level DoInitAction tag.</returns>
    private static SwfTag FindDoInitActionTag(ReadOnlySpan<byte> gfxBytes, ushort spriteId, string description)
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

            if (tag.Code == DoInitActionTagCode &&
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
    /// Rewrites the retail payload by replacing the options-menu sprite and inserting the
    /// prototype graphics screen tags before the retail root ShowFrame tag.
    /// </summary>
    /// <param name="retailGfxBytes">Original retail MainV2 payload bytes.</param>
    /// <param name="retailOptionsMenuTag">Retail ScreenOptionsMenu sprite tag.</param>
    /// <param name="retailRootShowFrameTag">Retail root ShowFrame tag.</param>
    /// <param name="prototypeOptionsMenuTag">Prototype ScreenOptionsMenu sprite tag.</param>
    /// <param name="prototypeGraphicsScreenTag">Prototype ScreenOptionsGraphics sprite tag.</param>
    /// <param name="prototypeGraphicsScreenExportTag">Prototype ScreenOptionsGraphics export tag.</param>
    /// <param name="prototypeGraphicsExitPromptTag">Prototype GraphicsExitPrompt sprite tag.</param>
    /// <param name="prototypeGraphicsExitPromptExportTag">Prototype GraphicsExitPrompt export tag.</param>
    /// <param name="prototypeGraphicsScreenDoInitActionTag">Prototype ScreenOptionsGraphics DoInitAction tag.</param>
    /// <param name="prototypeGraphicsExitPromptDoInitActionTag">Prototype GraphicsExitPrompt DoInitAction tag.</param>
    /// <param name="prototypeGfxBytes">Complete prototype MainV2 payload bytes.</param>
    /// <returns>Patched MainV2 payload bytes.</returns>
    private static byte[] RewritePatchedTags(
        ReadOnlySpan<byte> retailGfxBytes,
        SwfTag retailOptionsMenuTag,
        SwfTag retailRootShowFrameTag,
        SwfTag prototypeOptionsMenuTag,
        SwfTag prototypeGraphicsScreenTag,
        SwfTag prototypeGraphicsScreenExportTag,
        SwfTag prototypeGraphicsExitPromptTag,
        SwfTag prototypeGraphicsExitPromptExportTag,
        SwfTag prototypeGraphicsScreenDoInitActionTag,
        SwfTag prototypeGraphicsExitPromptDoInitActionTag,
        ReadOnlySpan<byte> prototypeGfxBytes)
    {
        if (retailOptionsMenuTag.Offset >= retailRootShowFrameTag.Offset)
        {
            throw new InvalidOperationException("Expected the retail ScreenOptionsMenu sprite tag to appear before the retail root ShowFrame tag.");
        }

        byte[] replacementOptionsMenuTagBytes = SerializeTag(prototypeGfxBytes, prototypeOptionsMenuTag);
        byte[] graphicsScreenTagBytes = SerializeDefineSpriteTagWithSpriteId(
            prototypeGfxBytes,
            prototypeGraphicsScreenTag,
            RetailScreenOptionsGraphicsSpriteId,
            "prototype ScreenOptionsGraphics sprite");
        byte[] graphicsScreenExportTagBytes = SerializeExportAssetsTagWithRemappedId(
            prototypeGfxBytes,
            prototypeGraphicsScreenExportTag,
            PrototypeScreenOptionsGraphicsSpriteId,
            RetailScreenOptionsGraphicsSpriteId,
            "prototype ScreenOptionsGraphics export tag");
        byte[] graphicsExitPromptTagBytes = SerializeDefineSpriteTagWithSpriteId(
            prototypeGfxBytes,
            prototypeGraphicsExitPromptTag,
            RetailGraphicsExitPromptSpriteId,
            "prototype GraphicsExitPrompt sprite");
        byte[] graphicsExitPromptExportTagBytes = SerializeExportAssetsTagWithRemappedId(
            prototypeGfxBytes,
            prototypeGraphicsExitPromptExportTag,
            PrototypeGraphicsExitPromptSpriteId,
            RetailGraphicsExitPromptSpriteId,
            "prototype GraphicsExitPrompt export tag");
        byte[] graphicsScreenDoInitActionTagBytes = SerializeDoInitActionTagWithSpriteId(
            prototypeGfxBytes,
            prototypeGraphicsScreenDoInitActionTag,
            RetailScreenOptionsGraphicsSpriteId,
            "prototype ScreenOptionsGraphics DoInitAction tag");
        byte[] graphicsExitPromptDoInitActionTagBytes = SerializeDoInitActionTagWithSpriteId(
            prototypeGfxBytes,
            prototypeGraphicsExitPromptDoInitActionTag,
            RetailGraphicsExitPromptSpriteId,
            "prototype GraphicsExitPrompt DoInitAction tag");
        int retailOptionsMenuEndOffset = retailOptionsMenuTag.DataOffset + retailOptionsMenuTag.DataLength;
        int insertedTagBytesLength =
            graphicsScreenTagBytes.Length +
            graphicsScreenExportTagBytes.Length +
            graphicsExitPromptTagBytes.Length +
            graphicsExitPromptExportTagBytes.Length +
            graphicsScreenDoInitActionTagBytes.Length +
            graphicsExitPromptDoInitActionTagBytes.Length;
        int patchedLength = retailOptionsMenuTag.Offset +
            replacementOptionsMenuTagBytes.Length +
            (retailRootShowFrameTag.Offset - retailOptionsMenuEndOffset) +
            insertedTagBytesLength +
            (retailGfxBytes.Length - retailRootShowFrameTag.Offset);
        byte[] patchedGfx = new byte[patchedLength];
        int writeOffset = 0;

        retailGfxBytes[..retailOptionsMenuTag.Offset].CopyTo(patchedGfx);
        writeOffset += retailOptionsMenuTag.Offset;

        replacementOptionsMenuTagBytes.CopyTo(patchedGfx.AsSpan(writeOffset));
        writeOffset += replacementOptionsMenuTagBytes.Length;

        ReadOnlySpan<byte> middleBytes = retailGfxBytes[retailOptionsMenuEndOffset..retailRootShowFrameTag.Offset];
        middleBytes.CopyTo(patchedGfx.AsSpan(writeOffset));
        writeOffset += middleBytes.Length;

        graphicsScreenTagBytes.CopyTo(patchedGfx.AsSpan(writeOffset));
        writeOffset += graphicsScreenTagBytes.Length;
        graphicsScreenExportTagBytes.CopyTo(patchedGfx.AsSpan(writeOffset));
        writeOffset += graphicsScreenExportTagBytes.Length;
        graphicsExitPromptTagBytes.CopyTo(patchedGfx.AsSpan(writeOffset));
        writeOffset += graphicsExitPromptTagBytes.Length;
        graphicsExitPromptExportTagBytes.CopyTo(patchedGfx.AsSpan(writeOffset));
        writeOffset += graphicsExitPromptExportTagBytes.Length;
        graphicsScreenDoInitActionTagBytes.CopyTo(patchedGfx.AsSpan(writeOffset));
        writeOffset += graphicsScreenDoInitActionTagBytes.Length;
        graphicsExitPromptDoInitActionTagBytes.CopyTo(patchedGfx.AsSpan(writeOffset));
        writeOffset += graphicsExitPromptDoInitActionTagBytes.Length;

        retailGfxBytes[retailRootShowFrameTag.Offset..].CopyTo(patchedGfx.AsSpan(writeOffset));
        writeOffset += retailGfxBytes.Length - retailRootShowFrameTag.Offset;

        if (writeOffset != patchedGfx.Length)
        {
            throw new InvalidOperationException("Patched GFX reassembly wrote an unexpected number of bytes.");
        }

        WriteInt32(patchedGfx, FileLengthOffset, patchedGfx.Length);
        return patchedGfx;
    }

    /// <summary>
    /// Serializes one top-level prototype tag using a long-form SWF tag header.
    /// </summary>
    /// <param name="sourceGfxBytes">Complete prototype GFX payload bytes.</param>
    /// <param name="tag">Resolved prototype top-level tag.</param>
    /// <returns>Serialized tag header and payload bytes.</returns>
    private static byte[] SerializeTag(ReadOnlySpan<byte> sourceGfxBytes, SwfTag tag)
    {
        byte[] payload = sourceGfxBytes[tag.DataOffset..(tag.DataOffset + tag.DataLength)].ToArray();
        return SerializeTagPayload(tag.Code, payload);
    }

    /// <summary>
    /// Serializes one DefineSprite tag while remapping its leading sprite id to the retail-safe destination id.
    /// </summary>
    /// <param name="sourceGfxBytes">Complete prototype GFX payload bytes.</param>
    /// <param name="tag">Resolved prototype DefineSprite tag.</param>
    /// <param name="targetSpriteId">The retail-safe sprite id to encode.</param>
    /// <param name="description">Human-readable description used in failure messages.</param>
    /// <returns>Serialized tag header and payload bytes.</returns>
    private static byte[] SerializeDefineSpriteTagWithSpriteId(
        ReadOnlySpan<byte> sourceGfxBytes,
        SwfTag tag,
        ushort targetSpriteId,
        string description)
    {
        byte[] payload = sourceGfxBytes[tag.DataOffset..(tag.DataOffset + tag.DataLength)].ToArray();
        if (payload.Length < sizeof(ushort))
        {
            throw new InvalidOperationException($"{description} payload was too short to rewrite the sprite id.");
        }

        WriteUInt16(payload, 0, targetSpriteId);
        return SerializeTagPayload(tag.Code, payload);
    }

    /// <summary>
    /// Serializes one DoInitAction tag while remapping its leading sprite id to the retail-safe destination id.
    /// </summary>
    /// <param name="sourceGfxBytes">Complete prototype GFX payload bytes.</param>
    /// <param name="tag">Resolved prototype DoInitAction tag.</param>
    /// <param name="targetSpriteId">The retail-safe sprite id to encode.</param>
    /// <param name="description">Human-readable description used in failure messages.</param>
    /// <returns>Serialized tag header and payload bytes.</returns>
    private static byte[] SerializeDoInitActionTagWithSpriteId(
        ReadOnlySpan<byte> sourceGfxBytes,
        SwfTag tag,
        ushort targetSpriteId,
        string description)
    {
        byte[] payload = sourceGfxBytes[tag.DataOffset..(tag.DataOffset + tag.DataLength)].ToArray();
        if (payload.Length < sizeof(ushort))
        {
            throw new InvalidOperationException($"{description} payload was too short to rewrite the sprite id.");
        }

        WriteUInt16(payload, 0, targetSpriteId);
        return SerializeTagPayload(tag.Code, payload);
    }

    /// <summary>
    /// Serializes one ExportAssets tag while remapping one exported sprite id to the retail-safe destination id.
    /// </summary>
    /// <param name="sourceGfxBytes">Complete prototype GFX payload bytes.</param>
    /// <param name="tag">Resolved prototype ExportAssets tag.</param>
    /// <param name="sourceSpriteId">The prototype sprite id that should be remapped.</param>
    /// <param name="targetSpriteId">The retail-safe sprite id that should replace the prototype id.</param>
    /// <param name="description">Human-readable description used in failure messages.</param>
    /// <returns>Serialized tag header and payload bytes.</returns>
    private static byte[] SerializeExportAssetsTagWithRemappedId(
        ReadOnlySpan<byte> sourceGfxBytes,
        SwfTag tag,
        ushort sourceSpriteId,
        ushort targetSpriteId,
        string description)
    {
        byte[] payload = sourceGfxBytes[tag.DataOffset..(tag.DataOffset + tag.DataLength)].ToArray();
        if (payload.Length < sizeof(ushort))
        {
            throw new InvalidOperationException($"{description} payload was too short to read the export count.");
        }

        ushort exportCount = BitConverter.ToUInt16(payload, 0);
        int offset = sizeof(ushort);

        for (int index = 0; index < exportCount; index++)
        {
            if (offset + sizeof(ushort) > payload.Length)
            {
                throw new InvalidOperationException($"{description} export entry {index} was truncated before the sprite id.");
            }

            ushort exportedSpriteId = BitConverter.ToUInt16(payload, offset);
            if (exportedSpriteId == sourceSpriteId)
            {
                WriteUInt16(payload, offset, targetSpriteId);
            }

            offset += sizeof(ushort);
            while (offset < payload.Length && payload[offset] != 0)
            {
                offset++;
            }

            if (offset >= payload.Length)
            {
                throw new InvalidOperationException($"{description} export entry {index} was missing its string terminator.");
            }

            offset++;
        }

        return SerializeTagPayload(tag.Code, payload);
    }

    /// <summary>
    /// Serializes one top-level tag using a long-form SWF tag header.
    /// </summary>
    /// <param name="tagCode">SWF tag code.</param>
    /// <param name="payload">Serialized tag payload bytes.</param>
    /// <returns>Serialized tag header and payload bytes.</returns>
    private static byte[] SerializeTagPayload(int tagCode, byte[] payload)
    {
        byte[] header = BuildLongTagHeader(tagCode, payload.Length);
        byte[] serializedTag = new byte[header.Length + payload.Length];

        header.CopyTo(serializedTag, 0);
        payload.CopyTo(serializedTag, header.Length);
        return serializedTag;
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
    /// Writes one 16-bit unsigned value to a byte array at an absolute offset.
    /// </summary>
    /// <param name="buffer">Destination byte array.</param>
    /// <param name="offset">Byte offset where the value is written.</param>
    /// <param name="value">Unsigned 16-bit value to write.</param>
    private static void WriteUInt16(byte[] buffer, int offset, ushort value)
    {
        if (offset < 0 || offset + sizeof(ushort) > buffer.Length)
        {
            throw new InvalidOperationException($"Cannot write UInt16 at offset {offset}.");
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
