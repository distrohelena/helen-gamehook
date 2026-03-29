using System.Text;
using System.Xml;

namespace SubtitleSizeModBuilder;

internal static class PauseXmlPatcher
{
    private const int SubtitleRowDepth = 61;
    private const int SubtitlesRowDepth = 57;
    private const int VolumeSfxRowDepth = 49;
    private const int VolumeMusicRowDepth = 41;
    private const int VolumeDialogueRowDepth = 33;
    private const int SubtitleRowYOffset = 2795;

    private const int SubtitlesRowY = 3055;
    private const int VolumeSfxRowY = 3813;
    private const int VolumeMusicRowY = 4490;
    private const int VolumeDialogueRowY = 5170;
    private const int SubtitleSizeRowY = 5850;

    public static void Patch(string inputXmlPath, string outputXmlPath)
    {
        var document = new XmlDocument
        {
            PreserveWhitespace = false
        };
        document.Load(inputXmlPath);

        XmlElement tags = GetSingleElement(document.DocumentElement, "tags");
        XmlElement screenOptionsAudio = FindSpriteById(tags, "394");

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
            GetAttribute(node, "depth") == SubtitlesRowDepth.ToString() &&
            GetAttribute(node, "name") == "Subtitles");

        XmlElement subtitleSizeInitial = (XmlElement)subtitlesInitial.CloneNode(deep: true);
        SetAttribute(subtitleSizeInitial, "depth", SubtitleRowDepth.ToString());
        SetAttribute(subtitleSizeInitial, "name", "SubtitleSize");
        subtitlesInitial.ParentNode!.InsertAfter(subtitleSizeInitial, subtitlesInitial);

        foreach (XmlElement original in originalChildren)
        {
            if (ReferenceEquals(original, subtitlesInitial))
            {
                continue;
            }

            string? type = GetAttribute(original, "type");
            string? depth = GetAttribute(original, "depth");
            if (depth != SubtitlesRowDepth.ToString())
            {
                continue;
            }

            if (type is not ("PlaceObject2Tag" or "RemoveObject2Tag"))
            {
                continue;
            }

            XmlElement clone = (XmlElement)original.CloneNode(deep: true);
            SetAttribute(clone, "depth", SubtitleRowDepth.ToString());
            ShiftTranslateY(clone, SubtitleRowYOffset);
            original.ParentNode!.InsertAfter(clone, original);
        }

        SetAudioRowTranslateY(subTags, "VolumeSFX", VolumeSfxRowDepth, VolumeSfxRowY);
        SetAudioRowTranslateY(subTags, "VolumeMusic", VolumeMusicRowDepth, VolumeMusicRowY);
        SetAudioRowTranslateY(subTags, "VolumeDialogue", VolumeDialogueRowDepth, VolumeDialogueRowY);
        SetAudioRowTranslateY(subTags, "Subtitles", SubtitlesRowDepth, SubtitlesRowY);
        SetAudioRowTranslateY(subTags, "SubtitleSize", SubtitleRowDepth, SubtitleSizeRowY);
    }

    private static void SetAudioRowTranslateY(XmlElement subTags, string rowName, int depth, int translateY)
    {
        XmlElement row = subTags.ChildNodes
            .OfType<XmlElement>()
            .Single(node =>
                node.Name == "item" &&
                GetAttribute(node, "type") == "PlaceObject2Tag" &&
                GetAttribute(node, "name") == rowName &&
                GetAttribute(node, "depth") == depth.ToString());

        SetTranslateY(GetSingleElement(row, "matrix"), translateY);
    }

    private static void SetTranslateY(XmlElement matrix, int translateY)
    {
        matrix.SetAttribute("translateY", translateY.ToString());
    }

    private static void ShiftTranslateY(XmlElement element, int translateYDelta)
    {
        XmlElement? matrix = element.ChildNodes
            .OfType<XmlElement>()
            .SingleOrDefault(node => node.Name == "matrix");

        if (matrix is null)
        {
            return;
        }

        int currentTranslateY = int.Parse(matrix.GetAttribute("translateY"));
        SetTranslateY(matrix, currentTranslateY + translateYDelta);
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
}
