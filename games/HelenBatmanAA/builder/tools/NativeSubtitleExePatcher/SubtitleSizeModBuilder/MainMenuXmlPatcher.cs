using System.Text;
using System.Xml;

namespace SubtitleSizeModBuilder;

/// <summary>
/// Rebuilds the frontend main-menu audio screen XML so subtitle size becomes a real fifth row.
/// </summary>
internal static class MainMenuXmlPatcher
{
    /// <summary>
    /// The sprite id for the frontend audio options screen in MainV2.
    /// </summary>
    private const int ScreenOptionsAudioSpriteId = 359;

    /// <summary>
    /// The depth used by the vanilla subtitles list item row.
    /// </summary>
    private const int SubtitleRowDepth = 53;

    /// <summary>
    /// The depth reserved for the appended subtitle-size row.
    /// </summary>
    private const int SubtitleSizeRowDepth = 61;

    /// <summary>
    /// The Y position for the new subtitle-size row.
    /// </summary>
    private const int SubtitleSizeRowTranslateY = 5310;

    /// <summary>
    /// Writes a patched copy of the frontend XML with a cloned subtitle-size row appended at depth 61.
    /// </summary>
    /// <param name="inputXmlPath">Path to the extracted MainV2 XML.</param>
    /// <param name="outputXmlPath">Path that receives the patched XML.</param>
    public static void Patch(string inputXmlPath, string outputXmlPath)
    {
        var document = new XmlDocument
        {
            PreserveWhitespace = false
        };
        document.Load(inputXmlPath);

        XmlElement tags = GetSingleElement(document.DocumentElement, "tags");
        XmlElement screenOptionsAudio = FindSpriteById(tags, ScreenOptionsAudioSpriteId.ToString());
        PatchAudioScreen(screenOptionsAudio);

        Directory.CreateDirectory(Path.GetDirectoryName(outputXmlPath)!);

        var settings = new XmlWriterSettings
        {
            Encoding = new UTF8Encoding(encoderShouldEmitUTF8Identifier: false),
            Indent = true,
            NewLineHandling = NewLineHandling.Entitize
        };

        using var writer = XmlWriter.Create(outputXmlPath, settings);
        document.Save(writer);
    }

    /// <summary>
    /// Clones every subtitle-row tag to depth 61 and pins all depth-61 placements to the new row position.
    /// </summary>
    /// <param name="audioScreen">The frontend audio screen sprite node.</param>
    private static void PatchAudioScreen(XmlElement audioScreen)
    {
        XmlElement subTags = GetSingleElement(audioScreen, "subTags");
        List<XmlElement> originalChildren = subTags.ChildNodes.OfType<XmlElement>().ToList();
        XmlElement subtitlesInitial = FindInitialSubtitleRow(originalChildren);
        XmlElement subtitleSizeInitial = CloneSubtitleRowTag(subtitlesInitial);
        subtitlesInitial.ParentNode!.InsertAfter(subtitleSizeInitial, subtitlesInitial);

        foreach (XmlElement original in originalChildren)
        {
            string? type = GetAttribute(original, "type");
            string? depth = GetAttribute(original, "depth");

            if (depth == SubtitleRowDepth.ToString() && type is "PlaceObject2Tag" or "RemoveObject2Tag")
            {
                if (!ReferenceEquals(original, subtitlesInitial))
                {
                    XmlElement clone = CloneSubtitleRowTag(original);
                    original.ParentNode!.InsertAfter(clone, original);
                }

                continue;
            }
        }

        PinDepth61Placements(subTags);
    }

    /// <summary>
    /// Finds the original subtitles placement used as the cloning source for the appended row.
    /// </summary>
    /// <param name="children">The original sprite child tags.</param>
    /// <returns>The initial subtitles placement tag.</returns>
    private static XmlElement FindInitialSubtitleRow(IEnumerable<XmlElement> children)
    {
        return children.Single(node =>
            node.Name == "item" &&
            GetAttribute(node, "type") == "PlaceObject2Tag" &&
            GetAttribute(node, "depth") == SubtitleRowDepth.ToString() &&
            GetAttribute(node, "name") == "Subtitles");
    }

    /// <summary>
    /// Creates a depth-61 copy of a subtitle-row tag and renames the visible placement to SubtitleSize.
    /// </summary>
    /// <param name="original">The original subtitle-row tag.</param>
    /// <returns>A cloned tag ready for insertion as the fifth row.</returns>
    private static XmlElement CloneSubtitleRowTag(XmlElement original)
    {
        XmlElement clone = (XmlElement)original.CloneNode(deep: true);
        SetAttribute(clone, "depth", SubtitleSizeRowDepth.ToString());

        if (GetAttribute(clone, "type") == "PlaceObject2Tag")
        {
            SetAttribute(clone, "name", "SubtitleSize");

            if (clone["matrix"] is XmlElement matrix)
            {
                SetTranslateY(matrix, SubtitleSizeRowTranslateY);
            }
        }

        return clone;
    }

    /// <summary>
    /// Forces every depth-61 placement to remain pinned to the subtitle-size row position.
    /// </summary>
    /// <param name="subTags">The sprite child tag collection.</param>
    private static void PinDepth61Placements(XmlElement subTags)
    {
        foreach (XmlElement node in subTags.ChildNodes.OfType<XmlElement>())
        {
            if (GetAttribute(node, "type") != "PlaceObject2Tag" || GetAttribute(node, "depth") != SubtitleSizeRowDepth.ToString())
            {
                continue;
            }

            if (node["matrix"] is XmlElement matrix)
            {
                SetTranslateY(matrix, SubtitleSizeRowTranslateY);
            }
        }
    }

    /// <summary>
    /// Finds a sprite definition by its id.
    /// </summary>
    /// <param name="tags">The document tags node.</param>
    /// <param name="spriteId">The sprite id to locate.</param>
    /// <returns>The matching sprite element.</returns>
    private static XmlElement FindSpriteById(XmlElement tags, string spriteId)
    {
        return tags.ChildNodes
            .OfType<XmlElement>()
            .Single(node => GetAttribute(node, "type") == "DefineSpriteTag" && GetAttribute(node, "spriteId") == spriteId);
    }

    /// <summary>
    /// Reads a required child element and fails fast when the XML shape is unexpected.
    /// </summary>
    /// <param name="parent">The parent node.</param>
    /// <param name="name">The child element name.</param>
    /// <returns>The required child element.</returns>
    private static XmlElement GetSingleElement(XmlNode? parent, string name)
    {
        XmlElement? element = parent?.ChildNodes.OfType<XmlElement>().SingleOrDefault(node => node.Name == name);
        return element ?? throw new InvalidOperationException($"Expected child element '{name}'.");
    }

    /// <summary>
    /// Reads an optional XML attribute.
    /// </summary>
    /// <param name="element">The element to inspect.</param>
    /// <param name="name">The attribute name.</param>
    /// <returns>The attribute value when present; otherwise <see langword="null" />.</returns>
    private static string? GetAttribute(XmlElement element, string name)
    {
        return element.HasAttribute(name) ? element.GetAttribute(name) : null;
    }

    /// <summary>
    /// Sets an XML attribute value.
    /// </summary>
    /// <param name="element">The element to update.</param>
    /// <param name="name">The attribute name.</param>
    /// <param name="value">The value to write.</param>
    private static void SetAttribute(XmlElement element, string name, string value)
    {
        element.SetAttribute(name, value);
    }

    /// <summary>
    /// Pins a placement matrix to an exact Y position.
    /// </summary>
    /// <param name="matrix">The placement matrix to modify.</param>
    /// <param name="translateY">The target Y translation.</param>
    private static void SetTranslateY(XmlElement matrix, int translateY)
    {
        matrix.SetAttribute("translateY", translateY.ToString());
    }
}
