using System.Text;
using System.Xml;

namespace SubtitleSizeModBuilder;

/// <summary>
/// Applies structural frontend MainV2 XML patches required by the graphics-options prototype build.
/// </summary>
internal static class GraphicsOptionsXmlPatcher
{
    /// <summary>
    /// Sprite id for the top-level options menu screen.
    /// </summary>
    private const int ScreenOptionsMenuSpriteId = 333;

    /// <summary>
    /// Sprite id for the existing Game Options PC screen used as the cloning source for sprite 600.
    /// </summary>
    private const int ScreenOptionsGamePcSpriteId = 356;

    /// <summary>
    /// Sprite id for the existing yes/no prompt used as a cloning source for sprite 601.
    /// </summary>
    private const int YesNoPromptSpriteId = 253;

    /// <summary>
    /// Sprite id reserved for the cloned graphics options screen.
    /// </summary>
    private const int ScreenOptionsGraphicsSpriteId = 600;

    /// <summary>
    /// Sprite id reserved for the prototype graphics exit prompt.
    /// </summary>
    private const int GraphicsExitPromptSpriteId = 601;

    /// <summary>
    /// Depth used by the top options-menu Game button after rebalance.
    /// </summary>
    private const int GameButtonDepth = 43;

    /// <summary>
    /// Depth used by the new options-menu Graphics button after rebalance.
    /// </summary>
    private const int GraphicsButtonDepth = 37;

    /// <summary>
    /// Depth used by the options-menu Audio button after rebalance.
    /// </summary>
    private const int AudioButtonDepth = 35;

    /// <summary>
    /// Depth used by the options-menu Controls button after rebalance.
    /// </summary>
    private const int ControlsButtonDepth = 33;

    /// <summary>
    /// Depth used by the options-menu Credits button after rebalance.
    /// </summary>
    private const int CreditsButtonDepth = 31;

    /// <summary>
    /// Temporary depth used to relocate legacy non-button depth-39 timeline tags before the new Game button is assigned to depth 39.
    /// </summary>
    private const int LegacyDepth39RelocationDepth = 41;

    /// <summary>
    /// Source depth used by the options-menu title text timeline.
    /// </summary>
    private const int OptionsMenuTitleDepth = 40;

    /// <summary>
    /// Target depth used to lift the options-menu title above the relocated legacy depth-39 overlay.
    /// </summary>
    private const int OptionsMenuRaisedTitleDepth = 42;

    /// <summary>
    /// Source depth used by the yes/no prompt "No" button timeline.
    /// </summary>
    private const int PromptNoButtonDepth = 13;

    /// <summary>
    /// Source depth used by the yes/no prompt "Yes" button timeline.
    /// </summary>
    private const int PromptYesButtonDepth = 15;

    /// <summary>
    /// Target depth used by the new graphics prompt "Apply" button timeline.
    /// </summary>
    private const int PromptApplyButtonDepth = 11;

    /// <summary>
    /// Character id for the shared list-row template used by option rows.
    /// </summary>
    private const int ListTemplateCharacterId = 290;

    /// <summary>
    /// Character id used by audio slider rows in the source screen.
    /// </summary>
    private const int AudioSliderTemplateCharacterId = 303;

    /// <summary>
    /// X translation used by full-page graphics rows so the list aligns with the broader Game Options column.
    /// </summary>
    private const int ListTemplateTranslateX = -781;

    /// <summary>
    /// Depth used by the inherited graphics header backing block.
    /// </summary>
    private const int GraphicsHeaderBackingDepth = 146;

    /// <summary>
    /// Depth used by the inherited graphics title text.
    /// </summary>
    private const int GraphicsHeaderTitleDepth = 147;

    /// <summary>
    /// Target Y translation for the graphics header backing block after moving it near the top of the graphics screen.
    /// </summary>
    private const int GraphicsHeaderBackingTranslateY = -4780;

    /// <summary>
    /// Target Y translation for the graphics title text after moving it near the top of the graphics screen.
    /// </summary>
    private const int GraphicsHeaderTitleTranslateY = -5436;

    /// <summary>
    /// Depth used by the retained translucent graphics background panel.
    /// </summary>
    private const int GraphicsPanelDepth = 3;

    /// <summary>
    /// Character id used by the retained translucent graphics background panel.
    /// </summary>
    private const int GraphicsPanelCharacterId = 307;

    /// <summary>
    /// Target Y translation for the retained translucent graphics background panel.
    /// </summary>
    private const int GraphicsPanelTranslateY = -4600;

    /// <summary>
    /// Target X scale for the retained translucent graphics background panel.
    /// </summary>
    private const string GraphicsPanelScaleX = "2.0688171";

    /// <summary>
    /// Target Y scale for the retained translucent graphics background panel after extending it to cover most of the screen height.
    /// </summary>
    private const string GraphicsPanelScaleY = "4.625";

    /// <summary>
    /// Source row depth cloned to create missing fullscreen graphics rows from the game screen.
    /// </summary>
    private const int GraphicsRowCloneSourceDepth = 58;

    /// <summary>
    /// XML attribute names that can carry character ids and therefore must be remapped when a collision is detected.
    /// </summary>
    private static readonly string[] IdAttributeNames =
    [
        "shapeId",
        "spriteId",
        "characterID",
        "characterId",
        "buttonId",
        "fontId",
        "bitmapId",
        "soundId"
    ];

    /// <summary>
    /// Target Y translation slots for the five options-menu buttons in visual order.
    /// </summary>
    private static readonly int[] OptionsMenuButtonTranslateY =
    [
        1776,
        2383,
        2990,
        3596,
        4203
    ];

    /// <summary>
    /// Graphics row depths ordered from top-to-bottom visual layout.
    /// </summary>
    private static readonly int[] GraphicsRowDepths =
    [
        141,
        133,
        125,
        117,
        109,
        101,
        93,
        85,
        77,
        69,
        61,
        53,
        45,
        37,
        29
    ];

    /// <summary>
    /// Graphics row instance names ordered top-to-bottom.
    /// </summary>
    private static readonly string[] GraphicsRowNames =
    [
        "GraphicsRow1",
        "GraphicsRow2",
        "GraphicsRow3",
        "GraphicsRow4",
        "GraphicsRow5",
        "GraphicsRow6",
        "GraphicsRow7",
        "GraphicsRow8",
        "GraphicsRow9",
        "GraphicsRow10",
        "GraphicsRow11",
        "GraphicsRow12",
        "GraphicsRow13",
        "GraphicsRow14",
        "GraphicsRow15"
    ];

    /// <summary>
    /// Graphics row Y coordinates ordered top-to-bottom across the full screen height instead of the original audio rectangle.
    /// </summary>
    private static readonly int[] GraphicsRowTranslateY =
    [
        -1380,
        -1020,
        -660,
        -300,
        60,
        420,
        780,
        1140,
        1500,
        1860,
        2220,
        2580,
        2940,
        3300,
        3660
    ];

    /// <summary>
    /// Legacy Game Options row depths that must be removed after row cloning so sprite 600 stays constrained to the fixed fifteen-row graphics layout.
    /// </summary>
    private static readonly int[] LegacyGameRowDepths =
    [
        58,
        67,
        76,
        94
    ];

    /// <summary>
    /// Patches a frontend XML file by adding graphics screen structures and rebalancing the options-menu button stack.
    /// </summary>
    /// <param name="inputXmlPath">Path to the extracted MainV2 XML source.</param>
    /// <param name="outputXmlPath">Path that receives the patched MainV2 XML.</param>
    public static void Patch(string inputXmlPath, string outputXmlPath)
    {
        var document = new XmlDocument
        {
            PreserveWhitespace = false
        };
        document.Load(inputXmlPath);

        XmlElement tags = GetSingleElement(document.DocumentElement, "tags");
        XmlElement optionsMenuSprite = FindSpriteById(tags, ScreenOptionsMenuSpriteId);
        XmlElement optionsGamePcSprite = FindSpriteById(tags, ScreenOptionsGamePcSpriteId);
        XmlElement yesNoPromptSprite = FindSpriteById(tags, YesNoPromptSpriteId);

        HashSet<int> reservedIds =
        [
            ScreenOptionsGraphicsSpriteId,
            GraphicsExitPromptSpriteId
        ];

        RemapExistingCharacterId(tags, ScreenOptionsGraphicsSpriteId, reservedIds);
        RemapExistingCharacterId(tags, GraphicsExitPromptSpriteId, reservedIds);

        PatchOptionsMenu(optionsMenuSprite);
        AppendGraphicsSpritesAndExports(tags, optionsGamePcSprite, yesNoPromptSprite);

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
    /// Rebalances the options-menu stack to five rows by cloning the Game button to depth 39 and converting depth 37 into Graphics.
    /// </summary>
    /// <param name="optionsMenuSprite">The options-menu sprite node.</param>
    private static void PatchOptionsMenu(XmlElement optionsMenuSprite)
    {
        XmlElement subTags = GetSingleElement(optionsMenuSprite, "subTags");
        List<XmlElement> originalChildren = subTags.ChildNodes.OfType<XmlElement>().ToList();

        CloneGameDepthTimelineToDepth39(subTags, originalChildren);
        RenameDepth37ButtonToGraphics(subTags);
        ApplyOptionsMenuButtonTranslateY(subTags);
    }

    /// <summary>
    /// Moves all legacy depth-39 timeline tags to depth 41 so depth 39 can be dedicated to the Game button.
    /// </summary>
    /// <param name="subTags">The options-menu timeline node collection.</param>
    /// <param name="originalChildren">Snapshot of original child nodes before cloning.</param>
    private static void RelocateLegacyDepth39Timeline(XmlElement subTags, IEnumerable<XmlElement> originalChildren)
    {
        foreach (XmlElement child in originalChildren)
        {
            if (!IsDepthTag(child, GameButtonDepth))
            {
                continue;
            }

            if (!IsTimelineDepthTagType(child))
            {
                continue;
            }

            SetAttribute(child, "depth", LegacyDepth39RelocationDepth.ToString());
        }
    }

    /// <summary>
    /// Clones every depth-37 timeline tag to depth 39 so the original Game behavior remains available above Graphics.
    /// </summary>
    /// <param name="subTags">The options-menu timeline node collection.</param>
    /// <param name="originalChildren">Snapshot of original child nodes before cloning.</param>
    private static void CloneGameDepthTimelineToDepth39(XmlElement subTags, IEnumerable<XmlElement> originalChildren)
    {
        foreach (XmlElement child in originalChildren)
        {
            if (!IsDepthTag(child, GraphicsButtonDepth))
            {
                continue;
            }

            if (!IsTimelineDepthTagType(child))
            {
                continue;
            }

            XmlElement clone = (XmlElement)child.CloneNode(deep: true);
            SetAttribute(clone, "depth", GameButtonDepth.ToString());

            if (GetAttribute(clone, "type") == "PlaceObject2Tag" && GetAttribute(clone, "characterId") == "117")
            {
                SetAttribute(clone, "name", "Game");
            }

            child.ParentNode!.InsertAfter(clone, child);
        }
    }

    /// <summary>
    /// Renames the depth-37 button placement to Graphics so script import can target the new options row.
    /// </summary>
    /// <param name="subTags">The options-menu timeline node collection.</param>
    private static void RenameDepth37ButtonToGraphics(XmlElement subTags)
    {
        foreach (XmlElement child in subTags.ChildNodes.OfType<XmlElement>())
        {
            if (GetAttribute(child, "type") != "PlaceObject2Tag")
            {
                continue;
            }

            if (!IsDepthTag(child, GraphicsButtonDepth))
            {
                continue;
            }

            if (GetAttribute(child, "characterId") != "117")
            {
                continue;
            }

            SetAttribute(child, "name", "Graphics");
        }
    }

    /// <summary>
    /// Forces every options-menu button depth timeline placement to the expected Y slot layout.
    /// </summary>
    /// <param name="subTags">The options-menu timeline node collection.</param>
    private static void ApplyOptionsMenuButtonTranslateY(XmlElement subTags)
    {
        Dictionary<int, int> translateYByDepth = new()
        {
            [GameButtonDepth] = OptionsMenuButtonTranslateY[0],
            [GraphicsButtonDepth] = OptionsMenuButtonTranslateY[1],
            [AudioButtonDepth] = OptionsMenuButtonTranslateY[2],
            [ControlsButtonDepth] = OptionsMenuButtonTranslateY[3],
            [CreditsButtonDepth] = OptionsMenuButtonTranslateY[4]
        };

        foreach (XmlElement child in subTags.ChildNodes.OfType<XmlElement>())
        {
            if (GetAttribute(child, "type") != "PlaceObject2Tag")
            {
                continue;
            }

            int depth = ParseRequiredDepth(child);
            if (!translateYByDepth.TryGetValue(depth, out int targetTranslateY))
            {
                continue;
            }

            XmlElement matrix = GetSingleElement(child, "matrix");
            SetTranslateY(matrix, targetTranslateY);
        }
    }

    /// <summary>
    /// Lifts the options-menu title timeline above the relocated legacy depth-39 overlay so the <c>$UI.Options</c> header remains visible.
    /// </summary>
    /// <param name="subTags">The options-menu timeline node collection.</param>
    private static void RaiseOptionsMenuTitleAboveRelocatedTimeline(XmlElement subTags)
    {
        foreach (XmlElement child in subTags.ChildNodes.OfType<XmlElement>())
        {
            if (!IsDepthTag(child, OptionsMenuTitleDepth))
            {
                continue;
            }

            if (!IsTimelineDepthTagType(child))
            {
                continue;
            }

            SetAttribute(child, "depth", OptionsMenuRaisedTitleDepth.ToString());
        }
    }

    /// <summary>
    /// Freezes the relocated vanilla Options header backing and title into their settled visible state so the chooser keeps the retail header bar.
    /// </summary>
    /// <param name="subTags">The options-menu timeline node collection.</param>
    private static void LockOptionsMenuHeaderVisible(XmlElement subTags)
    {
        LockHeaderDepthVisible(subTags, LegacyDepth39RelocationDepth);
        LockHeaderDepthVisible(subTags, OptionsMenuRaisedTitleDepth);
    }

    /// <summary>
    /// Appends cloned graphics screen structures and the corresponding export and class registration tags.
    /// </summary>
    /// <param name="tags">The frontend root tags node.</param>
    /// <param name="optionsGamePcSprite">The source Game Options sprite used for cloning.</param>
    /// <param name="yesNoPromptSprite">The source yes/no prompt sprite used for cloning.</param>
    private static void AppendGraphicsSpritesAndExports(XmlElement tags, XmlElement optionsGamePcSprite, XmlElement yesNoPromptSprite)
    {
        XmlElement graphicsSprite = CloneSpriteWithNewId(optionsGamePcSprite, ScreenOptionsGraphicsSpriteId);
        PatchGraphicsScreenSprite(graphicsSprite);
        XmlElement graphicsExitPromptSprite = CloneSpriteWithNewId(yesNoPromptSprite, GraphicsExitPromptSpriteId);
        PatchGraphicsExitPromptSprite(graphicsExitPromptSprite);

        tags.AppendChild(graphicsSprite);
        tags.AppendChild(CreateExportAssetsTag(tags, ScreenOptionsGraphicsSpriteId, "ScreenOptionsGraphics"));
        tags.AppendChild(graphicsExitPromptSprite);
        tags.AppendChild(CreateExportAssetsTag(tags, GraphicsExitPromptSpriteId, "GraphicsExitPrompt"));
        tags.AppendChild(CloneDoInitActionTagForSprite(tags, ScreenOptionsGamePcSpriteId, ScreenOptionsGraphicsSpriteId));
        tags.AppendChild(CloneDoInitActionTagForSprite(tags, YesNoPromptSpriteId, GraphicsExitPromptSpriteId));
    }

    /// <summary>
    /// Converts the cloned graphics screen timeline into a fixed fifteen-row list bound to dedicated graphics row instances.
    /// </summary>
    /// <param name="graphicsSprite">The cloned graphics screen sprite.</param>
    private static void PatchGraphicsScreenSprite(XmlElement graphicsSprite)
    {
        XmlElement subTags = GetSingleElement(graphicsSprite, "subTags");
        RemoveGameOnlyHelperWidgets(subTags);
        RemoveInnerPanelShellPlacements(subTags);
        List<XmlElement> originalChildren = subTags.ChildNodes.OfType<XmlElement>().ToList();
        EnsureGraphicsRowDepthTimeline(originalChildren);
        RemoveLegacyGameRowDepthTimelines(subTags);
        NormalizeGraphicsHeaderPlacements(subTags);
        NormalizeGraphicsPanelPlacement(subTags);
        NameGraphicsTitlePlacement(subTags);
        NormalizeGraphicsRowPlacements(subTags);
    }

    /// <summary>
    /// Removes Game Options helper widgets and helper-only frame scripts that must not remain on the cloned graphics screen.
    /// </summary>
    /// <param name="subTags">The graphics screen timeline node collection.</param>
    private static void RemoveGameOnlyHelperWidgets(XmlElement subTags)
    {
        RemoveNamedPlacement(subTags, "PadOnly");
        RemoveNamedPlacement(subTags, "BrightnessHelp");
        RemoveNamedPlacement(subTags, "CameraAssistHelp");
        RemoveActionTagsContainingToken(subTags, "PadOnly");
        RemoveActionTagsContainingToken(subTags, "BrightnessHelp");
        RemoveActionTagsContainingToken(subTags, "CameraAssistHelp");
    }

    /// <summary>
    /// Removes only the inherited inner panel shell placements so the cloned graphics screen keeps the outer shell art while losing the boxed center overlay.
    /// </summary>
    /// <param name="subTags">The graphics screen timeline node collection.</param>
    private static void RemoveInnerPanelShellPlacements(XmlElement subTags)
    {
        RemovePlacementAtDepth(subTags, 21);
        RemovePlacementAtDepth(subTags, 22);
        RemovePlacementAtDepth(subTags, 23);
        RemovePlacementAtDepth(subTags, 25);
    }

    /// <summary>
    /// Removes every placement tag at a specific depth while leaving other timeline tags and retained shell layers untouched.
    /// </summary>
    /// <param name="subTags">The sprite timeline node collection.</param>
    /// <param name="depth">The exact placement depth that must be removed.</param>
    private static void RemovePlacementAtDepth(XmlElement subTags, int depth)
    {
        List<XmlElement> matchingPlacements = subTags.ChildNodes
            .OfType<XmlElement>()
            .Where(node =>
                GetAttribute(node, "type").StartsWith("PlaceObject", StringComparison.Ordinal) &&
                ParseRequiredDepth(node) == depth)
            .ToList();

        foreach (XmlElement matchingPlacement in matchingPlacements)
        {
            matchingPlacement.ParentNode!.RemoveChild(matchingPlacement);
        }
    }

    /// <summary>
    /// Removes every named placement instance from a sprite timeline.
    /// </summary>
    /// <param name="subTags">The sprite timeline node collection.</param>
    /// <param name="instanceName">The placement instance name that must be removed.</param>
    private static void RemoveNamedPlacement(XmlElement subTags, string instanceName)
    {
        List<XmlElement> matchingPlacements = subTags.ChildNodes
            .OfType<XmlElement>()
            .Where(node =>
                GetAttribute(node, "type") == "PlaceObject2Tag" &&
                GetAttribute(node, "name") == instanceName)
            .ToList();

        foreach (XmlElement matchingPlacement in matchingPlacements)
        {
            matchingPlacement.ParentNode!.RemoveChild(matchingPlacement);
        }
    }

    /// <summary>
    /// Removes every DoAction tag whose action bytes decode to ActionScript containing a required token.
    /// </summary>
    /// <param name="subTags">The sprite timeline node collection.</param>
    /// <param name="token">The ActionScript token that marks a helper-only frame script.</param>
    private static void RemoveActionTagsContainingToken(XmlElement subTags, string token)
    {
        List<XmlElement> matchingActionTags = subTags.ChildNodes
            .OfType<XmlElement>()
            .Where(node =>
                GetAttribute(node, "type") == "DoActionTag" &&
                ActionBytesContainToken(node, token))
            .ToList();

        foreach (XmlElement matchingActionTag in matchingActionTags)
        {
            matchingActionTag.ParentNode!.RemoveChild(matchingActionTag);
        }
    }

    /// <summary>
    /// Determines whether a DoAction tag contains a specific ASCII token in its decoded action bytes payload.
    /// </summary>
    /// <param name="actionTag">The DoAction tag to inspect.</param>
    /// <param name="token">The ASCII token that must be present.</param>
    /// <returns><see langword="true" /> when the decoded action bytes contain the token; otherwise <see langword="false" />.</returns>
    private static bool ActionBytesContainToken(XmlElement actionTag, string token)
    {
        string? actionBytes = GetAttribute(actionTag, "actionBytes");
        if (string.IsNullOrWhiteSpace(actionBytes))
        {
            return false;
        }

        string actionScript = Encoding.ASCII.GetString(Convert.FromHexString(actionBytes));
        return actionScript.Contains(token, StringComparison.Ordinal);
    }

    /// <summary>
    /// Removes every legacy Game Options row timeline from the cloned graphics sprite once its depth-58 timeline has been used as the clone template.
    /// </summary>
    /// <param name="subTags">The graphics screen timeline node collection.</param>
    private static void RemoveLegacyGameRowDepthTimelines(XmlElement subTags)
    {
        foreach (int legacyDepth in LegacyGameRowDepths)
        {
            RemoveTimelineTagsAtDepth(subTags, legacyDepth);
        }
    }

    /// <summary>
    /// Removes every timeline tag that operates on a specific depth.
    /// </summary>
    /// <param name="subTags">The sprite timeline node collection.</param>
    /// <param name="depth">The depth whose timeline tags must be removed.</param>
    private static void RemoveTimelineTagsAtDepth(XmlElement subTags, int depth)
    {
        List<XmlElement> matchingTimelineTags = subTags.ChildNodes
            .OfType<XmlElement>()
            .Where(node =>
                IsDepthTag(node, depth) &&
                IsTimelineDepthTagType(node))
            .ToList();

        foreach (XmlElement matchingTimelineTag in matchingTimelineTags)
        {
            matchingTimelineTag.ParentNode!.RemoveChild(matchingTimelineTag);
        }
    }

    /// <summary>
    /// Clones the source row depth timeline onto every missing fixed row depth in a single pass.
    /// </summary>
    /// <param name="originalChildren">Snapshot of original child tags before cloning.</param>
    private static void EnsureGraphicsRowDepthTimeline(IEnumerable<XmlElement> originalChildren)
    {
        List<XmlElement> templateTimeline = originalChildren
            .Where(child =>
                IsDepthTag(child, GraphicsRowCloneSourceDepth) &&
                IsTimelineDepthTagType(child))
            .ToList();

        if (templateTimeline.Count == 0)
        {
            throw new InvalidOperationException($"Could not find template row timeline at depth {GraphicsRowCloneSourceDepth}.");
        }

        HashSet<int> existingDepths = originalChildren
            .Where(child =>
                GetAttribute(child, "type") == "PlaceObject2Tag" &&
                (GetAttribute(child, "characterId") == ListTemplateCharacterId.ToString() ||
                 GetAttribute(child, "characterId") == AudioSliderTemplateCharacterId.ToString()))
            .Select(ParseRequiredDepth)
            .ToHashSet();

        List<int> missingDepths = GraphicsRowDepths
            .Where(depth => !existingDepths.Contains(depth))
            .ToList();

        if (missingDepths.Count == 0)
        {
            return;
        }

        foreach (XmlElement templateTag in templateTimeline)
        {
            XmlElement insertionCursor = templateTag;
            foreach (int depth in missingDepths)
            {
                XmlElement clone = (XmlElement)templateTag.CloneNode(deep: true);
                SetAttribute(clone, "depth", depth.ToString());
                insertionCursor.ParentNode!.InsertAfter(clone, insertionCursor);
                insertionCursor = clone;
            }
        }
    }

    /// <summary>
    /// Rewrites each visible graphics row placement to use list-template character id, deterministic names, and target Y layout.
    /// </summary>
    /// <param name="subTags">The graphics screen timeline node collection.</param>
    private static void NormalizeGraphicsRowPlacements(XmlElement subTags)
    {
        if (GraphicsRowDepths.Length != GraphicsRowNames.Length || GraphicsRowDepths.Length != GraphicsRowTranslateY.Length)
        {
            throw new InvalidOperationException("Graphics row layout arrays are not aligned.");
        }

        for (int index = 0; index < GraphicsRowDepths.Length; index++)
        {
            int depth = GraphicsRowDepths[index];
            string rowName = GraphicsRowNames[index];
            int translateY = GraphicsRowTranslateY[index];
            XmlElement placement = FindInitialPlacementAtDepth(subTags, depth);

            string? currentCharacterIdText = GetAttribute(placement, "characterId");
            if (currentCharacterIdText is null)
            {
                throw new InvalidOperationException($"Expected characterId on graphics row depth {depth}.");
            }

            int currentCharacterId = int.Parse(currentCharacterIdText);
            if (currentCharacterId != ListTemplateCharacterId && currentCharacterId != AudioSliderTemplateCharacterId)
            {
                throw new InvalidOperationException($"Unexpected character id {currentCharacterId} at graphics row depth {depth}.");
            }

            SetAttribute(placement, "characterId", ListTemplateCharacterId.ToString());
            SetAttribute(placement, "name", rowName);
            XmlElement matrix = GetSingleElement(placement, "matrix");
            int sourceTranslateY = ReadRequiredTranslateY(matrix);
            int translateYDelta = translateY - sourceTranslateY;
            NormalizeTimelinePlacementsAtDepth(subTags, depth, ListTemplateTranslateX, translateY, translateYDelta);
            SetTranslateX(matrix, ListTemplateTranslateX);
            SetTranslateY(matrix, translateY);
        }
    }

    /// <summary>
    /// Moves only the inherited header shell and title text to the higher top-of-screen slot without disturbing the rest of the graphics screen.
    /// </summary>
    /// <param name="subTags">The graphics screen timeline node collection.</param>
    private static void NormalizeGraphicsHeaderPlacements(XmlElement subTags)
    {
        NormalizeTimelineTranslateYAtDepth(subTags, GraphicsHeaderBackingDepth, GraphicsHeaderBackingTranslateY);
        NormalizeTimelineTranslateYAtDepth(subTags, GraphicsHeaderTitleDepth, GraphicsHeaderTitleTranslateY);
    }

    /// <summary>
    /// Resizes only the retained translucent graphics background panel so the full graphics row stack fits inside it.
    /// </summary>
    /// <param name="subTags">The graphics screen timeline node collection.</param>
    private static void NormalizeGraphicsPanelPlacement(XmlElement subTags)
    {
        XmlElement placement = FindInitialPlacementAtDepth(subTags, GraphicsPanelDepth);
        if (GetAttribute(placement, "characterId") != GraphicsPanelCharacterId.ToString())
        {
            throw new InvalidOperationException(
                $"Expected retained graphics panel at depth {GraphicsPanelDepth} with character {GraphicsPanelCharacterId}.");
        }

        XmlElement matrix = GetSingleElement(placement, "matrix");
        int sourceTranslateY = ReadRequiredTranslateY(matrix);
        int translateYDelta = GraphicsPanelTranslateY - sourceTranslateY;

        foreach (XmlElement timelinePlacement in subTags.ChildNodes
                     .OfType<XmlElement>()
                     .Where(node =>
                         GetAttribute(node, "type") == "PlaceObject2Tag" &&
                         GetAttribute(node, "depth") == GraphicsPanelDepth.ToString()))
        {
            XmlElement? timelineMatrix = timelinePlacement.ChildNodes
                .OfType<XmlElement>()
                .SingleOrDefault(node => node.Name == "matrix");
            if (timelineMatrix is null)
            {
                continue;
            }

            int currentTranslateY = ReadRequiredTranslateY(timelineMatrix);
            SetTranslateY(timelineMatrix, currentTranslateY + translateYDelta);
            SetScaleX(timelineMatrix, GraphicsPanelScaleX);
            SetScaleY(timelineMatrix, GraphicsPanelScaleY);
        }
    }

    /// <summary>
    /// Names the cloned title edit-text placement so the injected graphics screen script can bind <c>this.Title</c>.
    /// Without the instance name, the inherited <c>$UI.OptionsGame</c> default text stays visible.
    /// </summary>
    /// <param name="subTags">The graphics screen timeline node collection.</param>
    private static void NameGraphicsTitlePlacement(XmlElement subTags)
    {
        XmlElement titlePlacement = FindInitialPlacementAtDepth(subTags, GraphicsHeaderTitleDepth);
        SetAttribute(titlePlacement, "name", "Title");
        SetAttribute(titlePlacement, "placeFlagHasName", "true");
    }

    /// <summary>
    /// Applies fixed row geometry to every placement matrix on a specific row depth so the full row cluster stays aligned across the timeline.
    /// </summary>
    /// <param name="subTags">The graphics screen timeline node collection.</param>
    /// <param name="depth">The row depth whose timeline placements should move together.</param>
    /// <param name="targetTranslateX">The X coordinate that every row placement at this depth should use.</param>
    /// <param name="targetTranslateY">The Y coordinate that every row placement at this depth should use.</param>
    /// <param name="translateYDelta">The signed Y delta that should be applied.</param>
    private static void NormalizeTimelinePlacementsAtDepth(
        XmlElement subTags,
        int depth,
        int targetTranslateX,
        int targetTranslateY,
        int translateYDelta)
    {
        foreach (XmlElement placement in subTags.ChildNodes
                     .OfType<XmlElement>()
                     .Where(node =>
                         GetAttribute(node, "type") == "PlaceObject2Tag" &&
                         GetAttribute(node, "depth") == depth.ToString()))
        {
            XmlElement? matrix = placement.ChildNodes
                .OfType<XmlElement>()
                .SingleOrDefault(node => node.Name == "matrix");
            if (matrix is null)
            {
                continue;
            }

            int currentTranslateY = ReadRequiredTranslateY(matrix);
            SetTranslateX(matrix, targetTranslateX);
            SetTranslateY(matrix, currentTranslateY + translateYDelta);
            if (GetAttribute(placement, "characterId") == "0")
            {
                SetTranslateY(matrix, targetTranslateY);
            }
        }
    }

    /// <summary>
    /// Applies a uniform Y offset to every placement matrix on a specific depth so animated header timelines stay visually aligned after repositioning.
    /// </summary>
    /// <param name="subTags">The sprite timeline node collection.</param>
    /// <param name="depth">The target depth whose placements should move together.</param>
    /// <param name="targetTranslateY">The target Y coordinate for the initial non-zero placement at this depth.</param>
    private static void NormalizeTimelineTranslateYAtDepth(XmlElement subTags, int depth, int targetTranslateY)
    {
        XmlElement placement = FindInitialPlacementAtDepth(subTags, depth);
        XmlElement matrix = GetSingleElement(placement, "matrix");
        int sourceTranslateY = ReadRequiredTranslateY(matrix);
        int translateYDelta = targetTranslateY - sourceTranslateY;

        foreach (XmlElement timelinePlacement in subTags.ChildNodes
                     .OfType<XmlElement>()
                     .Where(node =>
                         GetAttribute(node, "type") == "PlaceObject2Tag" &&
                         GetAttribute(node, "depth") == depth.ToString()))
        {
            XmlElement? timelineMatrix = timelinePlacement.ChildNodes
                .OfType<XmlElement>()
                .SingleOrDefault(node => node.Name == "matrix");
            if (timelineMatrix is null)
            {
                continue;
            }

            int currentTranslateY = ReadRequiredTranslateY(timelineMatrix);
            SetTranslateY(timelineMatrix, currentTranslateY + translateYDelta);
        }
    }

    /// <summary>
    /// Rewrites a relocated header depth to its final fully visible placement state and removes the remaining fade-out timeline tags.
    /// </summary>
    /// <param name="subTags">The options-menu timeline node collection.</param>
    /// <param name="depth">The relocated header depth to freeze.</param>
    private static void LockHeaderDepthVisible(XmlElement subTags, int depth)
    {
        XmlElement initialPlacement = FindInitialPlacementAtDepth(subTags, depth);
        XmlElement settledPlacement = FindLastFullyVisiblePlacementAtDepth(subTags, depth);
        CopyPlacementVisualState(initialPlacement, settledPlacement);

        List<XmlElement> animatedTimelineTags = subTags.ChildNodes
            .OfType<XmlElement>()
            .Where(node =>
                !ReferenceEquals(node, initialPlacement) &&
                GetAttribute(node, "depth") == depth.ToString() &&
                IsTimelineDepthTagType(node))
            .ToList();

        foreach (XmlElement animatedTimelineTag in animatedTimelineTags)
        {
            subTags.RemoveChild(animatedTimelineTag);
        }
    }

    /// <summary>
    /// Finds the last fully visible placement state on a depth so the settled retail header pose can be preserved statically.
    /// </summary>
    /// <param name="subTags">The timeline node collection.</param>
    /// <param name="depth">The depth whose visible pose should be captured.</param>
    /// <returns>The final fully opaque placement at the target depth.</returns>
    private static XmlElement FindLastFullyVisiblePlacementAtDepth(XmlElement subTags, int depth)
    {
        XmlElement? placement = subTags.ChildNodes
            .OfType<XmlElement>()
            .Where(node =>
                GetAttribute(node, "type") == "PlaceObject2Tag" &&
                GetAttribute(node, "depth") == depth.ToString())
            .LastOrDefault(node => ReadAlphaMultiplier(node) == 256);

        if (placement is null)
        {
            throw new InvalidOperationException($"Expected a fully visible placement at depth {depth}.");
        }

        return placement;
    }

    /// <summary>
    /// Copies matrix and color state from a settled source placement onto the initial visible placement that owns the character id.
    /// </summary>
    /// <param name="targetPlacement">The initial non-zero placement that should become the static header.</param>
    /// <param name="sourcePlacement">The source placement whose visual state should be copied.</param>
    private static void CopyPlacementVisualState(XmlElement targetPlacement, XmlElement sourcePlacement)
    {
        ReplacePlacementChild(targetPlacement, sourcePlacement, "matrix");
        ReplacePlacementChild(targetPlacement, sourcePlacement, "colorTransform");

        SetAttribute(
            targetPlacement,
            "placeFlagHasMatrix",
            sourcePlacement.ChildNodes.OfType<XmlElement>().Any(node => node.Name == "matrix") ? "true" : "false");

        SetAttribute(
            targetPlacement,
            "placeFlagHasColorTransform",
            sourcePlacement.ChildNodes.OfType<XmlElement>().Any(node => node.Name == "colorTransform") ? "true" : "false");
    }

    /// <summary>
    /// Replaces an optional child node on a placement element with the matching child cloned from another placement.
    /// </summary>
    /// <param name="targetPlacement">The placement whose child node should be replaced.</param>
    /// <param name="sourcePlacement">The placement supplying the replacement child node.</param>
    /// <param name="childName">The optional child element name.</param>
    private static void ReplacePlacementChild(XmlElement targetPlacement, XmlElement sourcePlacement, string childName)
    {
        XmlElement? existingChild = targetPlacement.ChildNodes
            .OfType<XmlElement>()
            .SingleOrDefault(node => node.Name == childName);

        if (existingChild is not null)
        {
            targetPlacement.RemoveChild(existingChild);
        }

        XmlElement? sourceChild = sourcePlacement.ChildNodes
            .OfType<XmlElement>()
            .SingleOrDefault(node => node.Name == childName);

        if (sourceChild is null)
        {
            return;
        }

        targetPlacement.AppendChild(sourceChild.CloneNode(deep: true));
    }

    /// <summary>
    /// Reads the alpha multiplier from an optional color transform, treating missing alpha state as fully visible.
    /// </summary>
    /// <param name="placement">The placement whose alpha multiplier should be inspected.</param>
    /// <returns>The alpha multiplier in SWF fixed-point integer form.</returns>
    private static int ReadAlphaMultiplier(XmlElement placement)
    {
        XmlElement? colorTransform = placement.ChildNodes
            .OfType<XmlElement>()
            .SingleOrDefault(node => node.Name == "colorTransform");

        string? alphaMultTerm = colorTransform is null ? null : GetAttribute(colorTransform, "alphaMultTerm");
        if (string.IsNullOrWhiteSpace(alphaMultTerm))
        {
            return 256;
        }

        return int.Parse(alphaMultTerm);
    }

    /// <summary>
    /// Finds the initial non-zero placement tag at a target depth.
    /// </summary>
    /// <param name="subTags">The timeline node collection.</param>
    /// <param name="depth">The target depth.</param>
    /// <returns>The initial placement element.</returns>
    private static XmlElement FindInitialPlacementAtDepth(XmlElement subTags, int depth)
    {
        return subTags.ChildNodes
            .OfType<XmlElement>()
            .First(node =>
                GetAttribute(node, "type") == "PlaceObject2Tag" &&
                GetAttribute(node, "depth") == depth.ToString() &&
                GetAttribute(node, "characterId") != "0");
    }

    /// <summary>
    /// Rebuilds the cloned prompt sprite to include a third interactive button at depth 11.
    /// </summary>
    /// <param name="graphicsExitPromptSprite">The cloned prompt sprite that will become id 601.</param>
    private static void PatchGraphicsExitPromptSprite(XmlElement graphicsExitPromptSprite)
    {
        XmlElement subTags = GetSingleElement(graphicsExitPromptSprite, "subTags");
        List<XmlElement> originalChildren = subTags.ChildNodes.OfType<XmlElement>().ToList();

        ClonePromptDepthTimeline(subTags, originalChildren, PromptNoButtonDepth, PromptApplyButtonDepth, "Apply");
        RenamePromptButtonInstance(subTags, PromptNoButtonDepth, "No");
        RenamePromptButtonInstance(subTags, PromptYesButtonDepth, "Yes");
        CenterApplyPromptButton(subTags);
    }

    /// <summary>
    /// Clones every timeline tag at a source depth to a target depth so prompt button behavior remains frame-correct.
    /// </summary>
    /// <param name="subTags">The prompt timeline node collection.</param>
    /// <param name="originalChildren">Snapshot of the original timeline nodes.</param>
    /// <param name="sourceDepth">The source button depth.</param>
    /// <param name="targetDepth">The target button depth for the clone.</param>
    /// <param name="targetInstanceName">The target instance name for cloned button placements.</param>
    private static void ClonePromptDepthTimeline(
        XmlElement subTags,
        IEnumerable<XmlElement> originalChildren,
        int sourceDepth,
        int targetDepth,
        string targetInstanceName)
    {
        foreach (XmlElement child in originalChildren)
        {
            if (!IsDepthTag(child, sourceDepth))
            {
                continue;
            }

            if (!IsTimelineDepthTagType(child))
            {
                continue;
            }

            XmlElement clone = (XmlElement)child.CloneNode(deep: true);
            SetAttribute(clone, "depth", targetDepth.ToString());

            if (GetAttribute(clone, "type") == "PlaceObject2Tag" && GetAttribute(clone, "characterId") == "117")
            {
                SetAttribute(clone, "name", targetInstanceName);
            }

            child.ParentNode!.InsertAfter(clone, child);
        }
    }

    /// <summary>
    /// Renames the initial prompt button placement instance at a specific depth.
    /// </summary>
    /// <param name="subTags">The prompt timeline node collection.</param>
    /// <param name="depth">The target depth to rename.</param>
    /// <param name="instanceName">The instance name to apply.</param>
    private static void RenamePromptButtonInstance(XmlElement subTags, int depth, string instanceName)
    {
        XmlElement initialPlacement = subTags.ChildNodes
            .OfType<XmlElement>()
            .First(node =>
                GetAttribute(node, "type") == "PlaceObject2Tag" &&
                GetAttribute(node, "depth") == depth.ToString() &&
                GetAttribute(node, "characterId") == "117");

        SetAttribute(initialPlacement, "name", instanceName);
    }

    /// <summary>
    /// Places the cloned Apply button midway between the original Yes and No button positions.
    /// </summary>
    /// <param name="subTags">The prompt timeline node collection.</param>
    private static void CenterApplyPromptButton(XmlElement subTags)
    {
        XmlElement noPlacement = FindInitialPromptButtonPlacement(subTags, PromptNoButtonDepth);
        XmlElement yesPlacement = FindInitialPromptButtonPlacement(subTags, PromptYesButtonDepth);
        XmlElement applyPlacement = FindInitialPromptButtonPlacement(subTags, PromptApplyButtonDepth);

        XmlElement noMatrix = GetSingleElement(noPlacement, "matrix");
        XmlElement yesMatrix = GetSingleElement(yesPlacement, "matrix");
        XmlElement applyMatrix = GetSingleElement(applyPlacement, "matrix");

        int noTranslateY = ReadRequiredTranslateY(noMatrix);
        int yesTranslateY = ReadRequiredTranslateY(yesMatrix);
        int centeredTranslateY = (noTranslateY + yesTranslateY) / 2;

        SetTranslateY(applyMatrix, centeredTranslateY);
    }

    /// <summary>
    /// Finds the initial prompt button placement by depth.
    /// </summary>
    /// <param name="subTags">The prompt timeline node collection.</param>
    /// <param name="depth">The target placement depth.</param>
    /// <returns>The matching prompt button placement.</returns>
    private static XmlElement FindInitialPromptButtonPlacement(XmlElement subTags, int depth)
    {
        return subTags.ChildNodes
            .OfType<XmlElement>()
            .First(node =>
                GetAttribute(node, "type") == "PlaceObject2Tag" &&
                GetAttribute(node, "depth") == depth.ToString() &&
                GetAttribute(node, "characterId") == "117");
    }

    /// <summary>
    /// Remaps all occurrences of a colliding character id to a new free id so required target ids remain available.
    /// </summary>
    /// <param name="tags">The frontend root tags node.</param>
    /// <param name="collidingId">The id that must be remapped away.</param>
    /// <param name="reservedIds">Ids that must not be used for remap targets.</param>
    private static void RemapExistingCharacterId(XmlElement tags, int collidingId, ISet<int> reservedIds)
    {
        if (!ContainsAnyIdValue(tags, collidingId))
        {
            return;
        }

        int remappedId = AllocateUnusedId(tags, reservedIds);
        ReplaceIdValue(tags, collidingId, remappedId);
        reservedIds.Add(remappedId);
    }

    /// <summary>
    /// Determines whether the tags tree contains at least one supported id attribute or export entry with the target value.
    /// </summary>
    /// <param name="tags">The frontend root tags node.</param>
    /// <param name="id">The id to search.</param>
    /// <returns><see langword="true" /> when at least one supported location uses the id; otherwise <see langword="false" />.</returns>
    private static bool ContainsAnyIdValue(XmlElement tags, int id)
    {
        string idText = id.ToString();
        foreach (XmlElement element in EnumerateElements(tags))
        {
            foreach (string attributeName in IdAttributeNames)
            {
                if (GetAttribute(element, attributeName) == idText)
                {
                    return true;
                }
            }
        }

        foreach (XmlElement exportIdElement in EnumerateExportedIdElements(tags))
        {
            if (exportIdElement.InnerText == idText)
            {
                return true;
            }
        }

        return false;
    }

    /// <summary>
    /// Finds a free id value that does not collide with existing ids or reserved ids.
    /// </summary>
    /// <param name="tags">The frontend root tags node.</param>
    /// <param name="reservedIds">Ids that are not allowed for allocation.</param>
    /// <returns>A newly allocated id.</returns>
    private static int AllocateUnusedId(XmlElement tags, ISet<int> reservedIds)
    {
        HashSet<int> usedIds = CollectUsedIds(tags);
        const int MaxSwfCharacterId = ushort.MaxValue;
        for (int candidate = 1; candidate <= MaxSwfCharacterId; candidate++)
        {
            if (!usedIds.Contains(candidate) && !reservedIds.Contains(candidate))
            {
                return candidate;
            }
        }

        throw new InvalidOperationException("Failed to allocate a free SWF character id in the UI16 range.");
    }

    /// <summary>
    /// Replaces all supported id attributes and export entries from one value to another.
    /// </summary>
    /// <param name="tags">The frontend root tags node.</param>
    /// <param name="sourceId">The id that must be replaced.</param>
    /// <param name="targetId">The new id value.</param>
    private static void ReplaceIdValue(XmlElement tags, int sourceId, int targetId)
    {
        string sourceText = sourceId.ToString();
        string targetText = targetId.ToString();

        foreach (XmlElement element in EnumerateElements(tags))
        {
            foreach (string attributeName in IdAttributeNames)
            {
                if (GetAttribute(element, attributeName) == sourceText)
                {
                    SetAttribute(element, attributeName, targetText);
                }
            }
        }

        foreach (XmlElement exportIdElement in EnumerateExportedIdElements(tags))
        {
            if (exportIdElement.InnerText == sourceText)
            {
                exportIdElement.InnerText = targetText;
            }
        }
    }

    /// <summary>
    /// Collects every integer id used in supported id attributes and export id entries.
    /// </summary>
    /// <param name="tags">The frontend root tags node.</param>
    /// <returns>A set containing all parsed id values.</returns>
    private static HashSet<int> CollectUsedIds(XmlElement tags)
    {
        HashSet<int> ids = [];

        foreach (XmlElement element in EnumerateElements(tags))
        {
            foreach (string attributeName in IdAttributeNames)
            {
                string? attributeValue = GetAttribute(element, attributeName);
                if (attributeValue is not null && int.TryParse(attributeValue, out int parsedId))
                {
                    ids.Add(parsedId);
                }
            }
        }

        foreach (XmlElement exportIdElement in EnumerateExportedIdElements(tags))
        {
            if (int.TryParse(exportIdElement.InnerText, out int parsedId))
            {
                ids.Add(parsedId);
            }
        }

        return ids;
    }

    /// <summary>
    /// Enumerates the root tags element and all descendant elements.
    /// </summary>
    /// <param name="root">The root element.</param>
    /// <returns>The flattened element sequence.</returns>
    private static IEnumerable<XmlElement> EnumerateElements(XmlElement root)
    {
        yield return root;

        foreach (XmlElement descendant in root.SelectNodes(".//*")!.OfType<XmlElement>())
        {
            yield return descendant;
        }
    }

    /// <summary>
    /// Enumerates every export id entry under ExportAssetsTag nodes.
    /// </summary>
    /// <param name="tags">The frontend root tags node.</param>
    /// <returns>The sequence of export id elements.</returns>
    private static IEnumerable<XmlElement> EnumerateExportedIdElements(XmlElement tags)
    {
        foreach (XmlElement exportAssetsTag in tags.ChildNodes.OfType<XmlElement>())
        {
            if (GetAttribute(exportAssetsTag, "type") != "ExportAssetsTag")
            {
                continue;
            }

            XmlElement exportIds = GetSingleElement(exportAssetsTag, "tags");
            foreach (XmlElement exportId in exportIds.ChildNodes.OfType<XmlElement>())
            {
                yield return exportId;
            }
        }
    }

    /// <summary>
    /// Reads and parses a required translateY matrix attribute.
    /// </summary>
    /// <param name="matrix">The matrix element to inspect.</param>
    /// <returns>The parsed translateY value.</returns>
    private static int ReadRequiredTranslateY(XmlElement matrix)
    {
        string? translateYText = GetAttribute(matrix, "translateY");
        if (translateYText is null)
        {
            throw new InvalidOperationException("Expected matrix translateY attribute.");
        }

        return int.Parse(translateYText);
    }

    /// <summary>
    /// Clones a sprite node and rewrites its sprite id.
    /// </summary>
    /// <param name="sourceSprite">The source sprite node.</param>
    /// <param name="targetSpriteId">The target sprite id for the clone.</param>
    /// <returns>The cloned sprite node.</returns>
    private static XmlElement CloneSpriteWithNewId(XmlElement sourceSprite, int targetSpriteId)
    {
        XmlElement clone = (XmlElement)sourceSprite.CloneNode(deep: true);
        SetAttribute(clone, "spriteId", targetSpriteId.ToString());
        return clone;
    }

    /// <summary>
    /// Creates an ExportAssets tag entry for a single sprite export.
    /// </summary>
    /// <param name="tags">The owning tags node used to create child elements.</param>
    /// <param name="spriteId">The sprite id to export.</param>
    /// <param name="exportName">The symbol name to export.</param>
    /// <returns>A new ExportAssetsTag node.</returns>
    private static XmlElement CreateExportAssetsTag(XmlElement tags, int spriteId, string exportName)
    {
        XmlDocument document = tags.OwnerDocument ?? throw new InvalidOperationException("Expected tags owner document.");

        XmlElement exportAssetsTag = document.CreateElement("item");
        SetAttribute(exportAssetsTag, "type", "ExportAssetsTag");
        SetAttribute(exportAssetsTag, "forceWriteAsLong", "true");

        XmlElement exportedTags = document.CreateElement("tags");
        XmlElement exportedTagItem = document.CreateElement("item");
        exportedTagItem.InnerText = spriteId.ToString();
        exportedTags.AppendChild(exportedTagItem);

        XmlElement exportedNames = document.CreateElement("names");
        XmlElement exportedNameItem = document.CreateElement("item");
        exportedNameItem.InnerText = exportName;
        exportedNames.AppendChild(exportedNameItem);

        exportAssetsTag.AppendChild(exportedTags);
        exportAssetsTag.AppendChild(exportedNames);
        return exportAssetsTag;
    }

    /// <summary>
    /// Clones a DoInitActionTag from one sprite id to another sprite id.
    /// </summary>
    /// <param name="tags">The frontend root tags node.</param>
    /// <param name="sourceSpriteId">The source sprite id used to locate the existing DoInitActionTag.</param>
    /// <param name="targetSpriteId">The target sprite id for the cloned DoInitActionTag.</param>
    /// <returns>The cloned DoInitActionTag node.</returns>
    private static XmlElement CloneDoInitActionTagForSprite(XmlElement tags, int sourceSpriteId, int targetSpriteId)
    {
        XmlElement sourceDoInitAction = tags.ChildNodes
            .OfType<XmlElement>()
            .Single(node =>
                GetAttribute(node, "type") == "DoInitActionTag" &&
                GetAttribute(node, "spriteId") == sourceSpriteId.ToString());

        XmlElement clone = (XmlElement)sourceDoInitAction.CloneNode(deep: true);
        SetAttribute(clone, "spriteId", targetSpriteId.ToString());
        return clone;
    }

    /// <summary>
    /// Finds a DefineSpriteTag by sprite id.
    /// </summary>
    /// <param name="tags">The frontend root tags node.</param>
    /// <param name="spriteId">The sprite id to locate.</param>
    /// <returns>The matching sprite node.</returns>
    private static XmlElement FindSpriteById(XmlElement tags, int spriteId)
    {
        return tags.ChildNodes
            .OfType<XmlElement>()
            .Single(node =>
                GetAttribute(node, "type") == "DefineSpriteTag" &&
                GetAttribute(node, "spriteId") == spriteId.ToString());
    }

    /// <summary>
    /// Reads a required child XML element by name.
    /// </summary>
    /// <param name="parent">The parent XML node.</param>
    /// <param name="name">The required child element name.</param>
    /// <returns>The matching child element.</returns>
    private static XmlElement GetSingleElement(XmlNode? parent, string name)
    {
        XmlElement? element = parent?.ChildNodes.OfType<XmlElement>().SingleOrDefault(node => node.Name == name);
        return element ?? throw new InvalidOperationException($"Expected child element '{name}'.");
    }

    /// <summary>
    /// Reads an optional attribute value.
    /// </summary>
    /// <param name="element">The element to inspect.</param>
    /// <param name="name">The attribute name.</param>
    /// <returns>The attribute value when present; otherwise <see langword="null" />.</returns>
    private static string? GetAttribute(XmlElement element, string name)
    {
        return element.HasAttribute(name) ? element.GetAttribute(name) : null;
    }

    /// <summary>
    /// Writes an attribute value.
    /// </summary>
    /// <param name="element">The element to update.</param>
    /// <param name="name">The attribute name.</param>
    /// <param name="value">The attribute value to set.</param>
    private static void SetAttribute(XmlElement element, string name, string value)
    {
        element.SetAttribute(name, value);
    }

    /// <summary>
    /// Determines whether an element is a supported timeline tag type that carries a depth attribute.
    /// </summary>
    /// <param name="element">The element to evaluate.</param>
    /// <returns><see langword="true" /> when the element is a PlaceObject2Tag or RemoveObject2Tag.</returns>
    private static bool IsTimelineDepthTagType(XmlElement element)
    {
        string? type = GetAttribute(element, "type");
        return type == "PlaceObject2Tag" || type == "RemoveObject2Tag";
    }

    /// <summary>
    /// Tests whether an element depth matches a specific integer value.
    /// </summary>
    /// <param name="element">The element to inspect.</param>
    /// <param name="depth">The depth value to compare.</param>
    /// <returns><see langword="true" /> when the element depth matches the requested value.</returns>
    private static bool IsDepthTag(XmlElement element, int depth)
    {
        string? depthText = GetAttribute(element, "depth");
        return depthText == depth.ToString();
    }

    /// <summary>
    /// Parses the required depth value from an element.
    /// </summary>
    /// <param name="element">The element containing a depth attribute.</param>
    /// <returns>The parsed depth integer.</returns>
    private static int ParseRequiredDepth(XmlElement element)
    {
        string? depthText = GetAttribute(element, "depth");
        if (depthText is null)
        {
            throw new InvalidOperationException("Expected depth attribute.");
        }

        return int.Parse(depthText);
    }

    /// <summary>
    /// Writes a matrix translateY value.
    /// </summary>
    /// <param name="matrix">The matrix element to modify.</param>
    /// <param name="translateY">The new Y translation.</param>
    private static void SetTranslateY(XmlElement matrix, int translateY)
    {
        matrix.SetAttribute("translateY", translateY.ToString());
    }

    /// <summary>
    /// Writes a matrix translateX value.
    /// </summary>
    /// <param name="matrix">The matrix element to modify.</param>
    /// <param name="translateX">The new X translation.</param>
    private static void SetTranslateX(XmlElement matrix, int translateX)
    {
        matrix.SetAttribute("translateX", translateX.ToString());
    }

    /// <summary>
    /// Writes a matrix scaleX value while forcing the matrix to stay scaled.
    /// </summary>
    /// <param name="matrix">The matrix element to modify.</param>
    /// <param name="scaleX">The new X scale.</param>
    private static void SetScaleX(XmlElement matrix, string scaleX)
    {
        matrix.SetAttribute("hasScale", "true");
        matrix.SetAttribute("scaleX", scaleX);
    }

    /// <summary>
    /// Writes a matrix scaleY value while forcing the matrix to stay scaled.
    /// </summary>
    /// <param name="matrix">The matrix element to modify.</param>
    /// <param name="scaleY">The new Y scale.</param>
    private static void SetScaleY(XmlElement matrix, string scaleY)
    {
        matrix.SetAttribute("hasScale", "true");
        matrix.SetAttribute("scaleY", scaleY);
    }
}
