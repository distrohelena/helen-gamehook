using System.Text;
using System.Xml;

namespace SubtitleSizeModBuilder;

internal static class MainMenuXmlPatcher
{
    private const int ScreenOptionsAudioSpriteId = 359;
    private const int SubtitleSizeRowDepth = 61;
    private const int SubtitleRowYOffset = 900;
    private const int VolumeRowYOffset = 844;

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

    private static void PatchAudioScreen(XmlElement audioScreen)
    {
        XmlElement subTags = GetSingleElement(audioScreen, "subTags");
        List<XmlElement> originalChildren = subTags.ChildNodes.OfType<XmlElement>().ToList();

        XmlElement subtitlesInitial = originalChildren.Single(node =>
            node.Name == "item" &&
            GetAttribute(node, "type") == "PlaceObject2Tag" &&
            GetAttribute(node, "depth") == "53" &&
            GetAttribute(node, "name") == "Subtitles");

        XmlElement subtitleSizeInitial = (XmlElement)subtitlesInitial.CloneNode(deep: true);
        SetAttribute(subtitleSizeInitial, "depth", SubtitleSizeRowDepth.ToString());
        SetAttribute(subtitleSizeInitial, "name", "SubtitleSize");
        AdjustTranslateY(GetSingleElement(subtitleSizeInitial, "matrix"), SubtitleRowYOffset);
        subtitlesInitial.ParentNode!.InsertAfter(subtitleSizeInitial, subtitlesInitial);

        foreach (XmlElement original in originalChildren)
        {
            string? type = GetAttribute(original, "type");
            string? depth = GetAttribute(original, "depth");

            if (depth == "53" && type is "PlaceObject2Tag" or "RemoveObject2Tag")
            {
                if (!ReferenceEquals(original, subtitlesInitial))
                {
                    XmlElement clone = (XmlElement)original.CloneNode(deep: true);
                    SetAttribute(clone, "depth", SubtitleSizeRowDepth.ToString());

                    if (type == "PlaceObject2Tag" && clone["matrix"] is XmlElement subtitleMatrix)
                    {
                        AdjustTranslateY(subtitleMatrix, SubtitleRowYOffset);
                    }

                    original.ParentNode!.InsertAfter(clone, original);
                }

                continue;
            }

            if (depth is not ("29" or "37" or "45"))
            {
                continue;
            }

            if (type == "PlaceObject2Tag" && original["matrix"] is XmlElement matrix)
            {
                AdjustTranslateY(matrix, VolumeRowYOffset);
            }
        }
    }

    private static XmlElement FindSpriteById(XmlElement tags, string spriteId)
    {
        return tags.ChildNodes
            .OfType<XmlElement>()
            .Single(node => GetAttribute(node, "type") == "DefineSpriteTag" && GetAttribute(node, "spriteId") == spriteId);
    }

    private static XmlElement GetSingleElement(XmlNode? parent, string name)
    {
        XmlElement? element = parent?.ChildNodes.OfType<XmlElement>().SingleOrDefault(node => node.Name == name);
        return element ?? throw new InvalidOperationException($"Expected child element '{name}'.");
    }

    private static string? GetAttribute(XmlElement element, string name)
    {
        return element.HasAttribute(name) ? element.GetAttribute(name) : null;
    }

    private static void SetAttribute(XmlElement element, string name, string value)
    {
        element.SetAttribute(name, value);
    }

    private static void AdjustTranslateY(XmlElement matrix, int delta)
    {
        int current = int.Parse(matrix.GetAttribute("translateY"));
        matrix.SetAttribute("translateY", (current + delta).ToString());
    }
}
