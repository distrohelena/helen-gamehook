namespace SubtitleSizeModBuilder;

/// <summary>
/// Provides the static ActionScript shell used to display the graphics-options screen without
/// connecting the screen controls to any native settings service.
/// </summary>
public static class GraphicsOptionsShellScriptTemplates
{
    /// <summary>
    /// Registers the graphics-options screen with the stock screen implementation so the
    /// frontend can construct it through its normal screen routing table.
    /// </summary>
    public const string ScreenRegistration = """
    Object.registerClass("ScreenOptionsGraphics",rs.ui.Screen);
    """;

    /// <summary>
    /// Rebuilds the five-entry options chooser contract while retaining stock item ordering,
    /// focus links, and the small rotation used by the surrounding frontend artwork.
    /// </summary>
    public const string OptionsMenuFrame1 = """
    flash.external.ExternalInterface.call("FE_SetActiveScreenName","Options Menu");
    this.BackScreen = "Main*";
    this.BackScreenIndex = 5;
    this.State = 0;
    this.Init();
    this.AddItem(Game,4,1,-1,-1);
    this.AddItem(Graphics,0,2,-1,-1);
    this.AddItem(Audio,1,3,-1,-1);
    this.AddItem(Controls,2,4,-1,-1);
    this.AddItem(Credits,3,0,-1,-1);
    _rotation = -2;
    """;

    /// <summary>
    /// Gives the options chooser's graphics button its fixed caption and routes activation
    /// directly to the graphics screen while retaining the generic button update contract.
    /// </summary>
    public const string OptionsMenuGraphicsButtonClipAction = """
    onClipEvent(load){
       function RunAction()
       {
          _parent.GotoScreen("OptionsGraphics");
       }
       function Update()
       {
          Label.Text.text = this.ButtonName;
       }
       this.ButtonName = "Graphics Options";
       this.Update();
    }
    """;

    /// <summary>
    /// Initializes the graphics screen shell, including direct cancellation, stock navigation
    /// metadata, its title, and a closed fourteen-row focus loop with the spare row hidden.
    /// </summary>
    public const string ScreenFrame1 = """
    function CancelScreen()
    {
       ReturnFromScreen();
    }
    flash.external.ExternalInterface.call("FE_SetActiveScreenName","Graphics Options");
    this.BackScreen = "OptionsMenu";
    this.BackScreenIndex = 1;
    this.FocusIndex = 0;
    this.Flags = this.FLAG_OPTIONS;
    this.Init();
    _root.TriggerEvent("Options");
    if(this.Title != undefined)
    {
       this.Title.text = "Graphics Options";
    }
    this.AddItem(GraphicsRow1,13,1,-1,-1);
    this.AddItem(GraphicsRow2,0,2,-1,-1);
    this.AddItem(GraphicsRow3,1,3,-1,-1);
    this.AddItem(GraphicsRow4,2,4,-1,-1);
    this.AddItem(GraphicsRow5,3,5,-1,-1);
    this.AddItem(GraphicsRow6,4,6,-1,-1);
    this.AddItem(GraphicsRow7,5,7,-1,-1);
    this.AddItem(GraphicsRow8,6,8,-1,-1);
    this.AddItem(GraphicsRow9,7,9,-1,-1);
    this.AddItem(GraphicsRow10,8,10,-1,-1);
    this.AddItem(GraphicsRow11,9,11,-1,-1);
    this.AddItem(GraphicsRow12,10,12,-1,-1);
    this.AddItem(GraphicsRow13,11,13,-1,-1);
    this.AddItem(GraphicsRow14,12,0,-1,-1);
    GraphicsRow15._visible = false;
    _rotation = -2;
    """;

    /// <summary>
    /// Stops the graphics screen timeline after the shell has reached its settled frame.
    /// </summary>
    public const string ScreenFrame15 = """
    stop();
    """;

    /// <summary>
    /// Contains one generated clip action for each of the fourteen visible rows and one
    /// hidden spare-row action, in the same order as the screen navigation loop.
    /// </summary>
    public static readonly string[] RowClipActions =
    [
        CreateRowClipAction("Fullscreen", "Not active", true),
        CreateRowClipAction("Resolution", "Not active", true),
        CreateRowClipAction("VSync", "Not active", true),
        CreateRowClipAction("MSAA", "Not active", true),
        CreateRowClipAction("Detail Level", "Not active", true),
        CreateRowClipAction("Bloom", "Not active", true),
        CreateRowClipAction("Dynamic Shadows", "Not active", true),
        CreateRowClipAction("Motion Blur", "Not active", true),
        CreateRowClipAction("Distortion", "Not active", true),
        CreateRowClipAction("Fog Volumes", "Not active", true),
        CreateRowClipAction("Spherical Harmonic Lighting", "Not active", true),
        CreateRowClipAction("Ambient Occlusion", "Not active", true),
        CreateRowClipAction("PhysX", "Not active", true),
        CreateRowClipAction("Stereo 3D", "Not active", true),
        CreateRowClipAction(string.Empty, string.Empty, false)
    ];

    /// <summary>
    /// Creates a callback-free list-row clip action with fixed text, disabled directional
    /// clickers, no-op interaction methods, and the requested timeline visibility.
    /// </summary>
    /// <param name="label">The literal label written through the row's nested label text shape.</param>
    /// <param name="value">The literal value written through the row's value text shape.</param>
    /// <param name="visible">Whether the generated row clip should remain visible.</param>
    /// <returns>An ActionScript load handler that initializes the fixed row shell.</returns>
    public static string CreateRowClipAction(string label, string value, bool visible)
    {
        ArgumentNullException.ThrowIfNull(label);
        ArgumentNullException.ThrowIfNull(value);

        string escapedLabel = EscapeActionScriptString(label);
        string escapedValue = EscapeActionScriptString(value);
        string visibility = visible ? "true" : "false";

        return $$"""
        onClipEvent(load){
           this.Label.Label.Text.text = "{{escapedLabel}}";
           this.ItemText.text = "{{escapedValue}}";
           this.LeftClicker._visible = false;
           this.RightClicker._visible = false;
           this.RunAction = function()
           {
           };
           this.Increment = function()
           {
           };
           this.Decrement = function()
           {
           };
           this._visible = {{visibility}};
           this.Update = function()
           {
              this.Label.Label.Text.text = "{{escapedLabel}}";
              this.ItemText.text = "{{escapedValue}}";
              this.LeftClicker._visible = false;
              this.RightClicker._visible = false;
           };
           this.Update();
        }
        """;
    }

    /// <summary>
    /// Escapes an ActionScript string value by protecting backslashes before protecting quotes,
    /// preserving literal labels and values when they are inserted into generated source.
    /// </summary>
    /// <param name="value">The unescaped value to place inside an ActionScript string literal.</param>
    /// <returns>The value with backslashes and double quotes escaped for ActionScript.</returns>
    public static string EscapeActionScriptString(string value)
    {
        ArgumentNullException.ThrowIfNull(value);
        return value
            .Replace("\\", "\\\\", StringComparison.Ordinal)
            .Replace("\"", "\\\"", StringComparison.Ordinal);
    }
}
