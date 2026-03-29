using System.Text;
using System.Xml;

namespace SubtitleSizeModBuilder;

internal static class HudXmlFontPatcher
{
    private sealed record TextFieldPatch(int CharacterId, int FontHeight, int XMax, int YMax, int? XMin = null, int? YMin = null);

    private static readonly TextFieldPatch[] Patches =
    {
        // Prompt entry labels used by left/right and main prompts.
        new(82, 900, 7000, 1450, XMin: -80, YMin: -80),
        new(85, 900, 7600, 1450, XMin: -80, YMin: -80),

        // Subtitle body.
        new(171, 1200, 12260, 6200, XMin: -80, YMin: -80),

        // Objective / prompt text blocks commonly shown as gameplay tips.
        new(193, 900, 12000, 1900, XMin: -80, YMin: -80),
        new(196, 900, 18044, 2200, XMin: -80, YMin: -80),

        // Info text block on the HUD.
        new(274, 1100, 7380, 5200, XMin: -80, YMin: -80),

        // Target cursor title/label callouts.
        new(520, 900, 9200, 1450, XMin: -80, YMin: -80),
    };

    public static void Patch(string sourceXmlPath, string outputXmlPath)
    {
        var document = new XmlDocument();
        document.Load(sourceXmlPath);

        XmlNodeList? items = document.SelectNodes("//item[@type='DefineEditTextTag']");
        if (items is null)
        {
            throw new InvalidOperationException("Could not find any DefineEditTextTag nodes in the HUD XML.");
        }

        var byCharacterId = items
            .OfType<XmlElement>()
            .Where(static element => int.TryParse(element.GetAttribute("characterID"), out _))
            .ToDictionary(
                static element => int.Parse(element.GetAttribute("characterID")),
                static element => element);

        foreach (TextFieldPatch patch in Patches)
        {
            if (!byCharacterId.TryGetValue(patch.CharacterId, out XmlElement? item))
            {
                throw new InvalidOperationException($"Could not find HUD text field characterID={patch.CharacterId}.");
            }

            item.SetAttribute("fontHeight", patch.FontHeight.ToString());

            XmlElement? bounds = item
                .ChildNodes
                .OfType<XmlElement>()
                .FirstOrDefault(static child => child.Name == "bounds");

            if (bounds is null)
            {
                throw new InvalidOperationException($"HUD text field characterID={patch.CharacterId} is missing bounds.");
            }

            bounds.SetAttribute("Xmax", patch.XMax.ToString());
            bounds.SetAttribute("Ymax", patch.YMax.ToString());

            if (patch.XMin.HasValue)
            {
                bounds.SetAttribute("Xmin", patch.XMin.Value.ToString());
            }

            if (patch.YMin.HasValue)
            {
                bounds.SetAttribute("Ymin", patch.YMin.Value.ToString());
            }
        }

        Directory.CreateDirectory(Path.GetDirectoryName(outputXmlPath)!);
        var settings = new XmlWriterSettings
        {
            Encoding = new UTF8Encoding(encoderShouldEmitUTF8Identifier: false),
            Indent = true,
            NewLineHandling = NewLineHandling.Replace
        };

        using XmlWriter writer = XmlWriter.Create(outputXmlPath, settings);
        document.Save(writer);
    }
}
