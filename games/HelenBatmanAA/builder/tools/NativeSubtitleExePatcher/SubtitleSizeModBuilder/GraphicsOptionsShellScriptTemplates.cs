using System.Globalization;

namespace SubtitleSizeModBuilder;

/// <summary>
/// Produces the selective ActionScript shell used to expose the retail graphics-options screen.
/// Only VSync and Apply are connected to frontend behavior; every other row remains a visible,
/// callback-free placeholder so the stock screen layout and navigation remain stable.
/// </summary>
internal static class GraphicsOptionsShellScriptTemplates
{
    /// <summary>
    /// Registers the graphics-options screen with the stock screen implementation so the frontend
    /// can construct it through its normal screen-routing table.
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
    /// Gives the options chooser's graphics button its fixed caption and routes activation directly
    /// to the graphics screen while retaining the generic button update contract.
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
    /// Initializes the graphics screen and its focused VSync controller using the normalized INI
    /// snapshot. The fifteen rows form one closed navigation loop, including the visible Apply row.
    /// </summary>
    /// <param name="snapshot">The normalized user graphics snapshot supplying the initial VSync state.</param>
    /// <returns>The generated screen-frame ActionScript.</returns>
    public static string CreateScreenFrame1(BatmanGraphicsIniBootstrapSnapshot snapshot)
    {
        ArgumentNullException.ThrowIfNull(snapshot);
        string initialVsync = snapshot.Vsync.ToString(CultureInfo.InvariantCulture);

        return $$"""
        class rs.ui.BatmanGraphicsVsyncController
        {
           var Screen;
           var InitialVsync;
           var DraftVsync;
           var ApplySignalToggle;
           var ApplyInProgress;
           var ApplyWasDispatched;
           var ApplyTimerId;
           function BatmanGraphicsVsyncController(screen, initialVsync)
           {
              this.Screen = screen;
              this.InitialVsync = this.NormalizeVsync(initialVsync);
              this.DraftVsync = this.InitialVsync;
              this.ApplySignalToggle = 0;
              this.ApplyInProgress = false;
              this.ApplyWasDispatched = false;
              this.ApplyTimerId = undefined;
           }
           function NormalizeVsync(value)
           {
              if(value == 0)
              {
                 return 0;
              }
              return 1;
           }
           function IsDirty()
           {
              return this.DraftVsync != this.InitialVsync;
           }
           function CanApply()
           {
              return !this.ApplyInProgress && (this.IsDirty() || this.ApplyWasDispatched);
           }
           function SetVsync(value, forward)
           {
              if(this.ApplyInProgress)
              {
                 return undefined;
              }
              var normalized = this.NormalizeVsync(value);
              if(forward == true || (forward == undefined && normalized > this.DraftVsync))
              {
                 flash.external.ExternalInterface.call("FE_PlaySoundFromString","UI_FrontEndSFX.UI_Forward");
              }
              else if(forward == false || (forward == undefined && normalized < this.DraftVsync))
              {
                 flash.external.ExternalInterface.call("FE_PlaySoundFromString","UI_FrontEndSFX.UI_Back");
              }
              this.DraftVsync = normalized;
              this.RefreshRows();
           }
           function ToggleVsync()
           {
              this.SetVsync(this.DraftVsync == 0 ? 1 : 0);
           }
           function IncrementVsync()
           {
              this.SetVsync(this.DraftVsync == 0 ? 1 : 0,true);
           }
           function DecrementVsync()
           {
              this.SetVsync(this.DraftVsync == 0 ? 1 : 0,false);
           }
           function RefreshRows()
           {
              if(this.Screen.GraphicsRow3 != undefined)
              {
                 this.Screen.GraphicsRow3.State = this.DraftVsync;
                 this.Screen.GraphicsRow3.Update();
              }
              if(this.Screen.GraphicsRow15 != undefined)
              {
                 this.Screen.GraphicsRow15.Update();
              }
              this.Screen.ReUpdate();
           }
           function ApplyChanges()
           {
              if(!this.CanApply())
              {
                 return undefined;
              }
              this.ApplyInProgress = true;
              this.Screen.BlockInput(true);
              this.RefreshRows();
              flash.external.ExternalInterface.call("FE_SetControlType",4210+this.DraftVsync,"");
              this.ApplyTimerId = setInterval(this,"CompleteApply",100);
           }
           function CompleteApply()
           {
              if(this.ApplyTimerId != undefined)
              {
                 clearInterval(this.ApplyTimerId);
                 this.ApplyTimerId = undefined;
              }
              this.ApplySignalToggle = this.ApplySignalToggle == 0 ? 1 : 0;
              flash.external.ExternalInterface.call("FE_SetControlType",4990+this.ApplySignalToggle,"");
              this.ApplyWasDispatched = true;
              this.ApplyInProgress = false;
              this.Screen.BlockInput(false);
              this.RefreshRows();
           }
           function Destroy()
           {
              if(this.ApplyTimerId != undefined)
              {
                 clearInterval(this.ApplyTimerId);
                 this.ApplyTimerId = undefined;
              }
           }
        }
        function CancelScreen()
        {
           this.GraphicsVsyncController.Destroy();
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
        this.GraphicsVsyncController = new rs.ui.BatmanGraphicsVsyncController(this,{{initialVsync}});
        this.AddItem(GraphicsRow1,14,1,-1,-1);
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
        this.AddItem(GraphicsRow14,12,14,-1,-1);
        this.AddItem(GraphicsRow15,13,0,-1,-1);
        GraphicsRow15._visible = true;
        _rotation = -2;
        """;
    }

    /// <summary>
    /// Stops the graphics screen timeline after the shell has reached its settled frame.
    /// </summary>
    public const string ScreenFrame15 = """
    stop();
    """;

    /// <summary>
    /// Creates all fifteen row scripts in visual and navigation order, injecting the INI VSync
    /// state only into row three and retaining no-op placeholders for all unrelated settings.
    /// </summary>
    /// <param name="snapshot">The normalized user graphics snapshot supplying initial VSync.</param>
    /// <returns>The fifteen row clip-action scripts.</returns>
    public static string[] CreateRowClipActions(BatmanGraphicsIniBootstrapSnapshot snapshot)
    {
        ArgumentNullException.ThrowIfNull(snapshot);
        return
        [
            CreateRowClipAction("Fullscreen", "Not active", true),
            CreateRowClipAction("Resolution", "Not active", true),
            CreateVsyncRowClipAction(snapshot.Vsync),
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
            CreateApplyRowClipAction()
        ];
    }

    /// <summary>
    /// Creates the focused VSync row, including its two visible values and controller-backed actions.
    /// </summary>
    /// <param name="initialVsync">The normalized VSync state read from the user INI.</param>
    /// <returns>An ActionScript load handler for row three.</returns>
    private static string CreateVsyncRowClipAction(int initialVsync)
    {
        string normalizedVsync = initialVsync == 0 ? "0" : "1";
        return $$"""
        onClipEvent(load){
           this.Names = new Array("Off","On");
           this.State = {{normalizedVsync}};
           this.Initial = {{normalizedVsync}};
           this.Default = 0;
           this.Update = function()
           {
              if(_parent.GraphicsVsyncController != undefined)
              {
                 this.State = _parent.GraphicsVsyncController.DraftVsync;
              }
              if(this.Label != undefined && this.Label.Label != undefined && this.Label.Label.Text != undefined)
              {
                 this.Label.Label.Text.text = "VSync";
              }
              else if(this.Label != undefined && this.Label.Text != undefined)
              {
                 this.Label.Text.text = "VSync";
              }
              else if(this.Label != undefined)
              {
                 this.Label.text = "VSync";
              }
              this.ItemText.text = this.Names[this.State];
              if(this.LeftClicker != undefined)
              {
                 this.LeftClicker._visible = this.State > 0;
              }
              if(this.RightClicker != undefined)
              {
                 this.RightClicker._visible = this.State < this.Names.length - 1;
              }
           };
           this.RunAction = function()
           {
              _parent.GraphicsVsyncController.ToggleVsync();
           };
           this.Increment = function()
           {
              _parent.GraphicsVsyncController.IncrementVsync();
           };
           this.Decrement = function()
           {
              _parent.GraphicsVsyncController.DecrementVsync();
           };
           this.ShowPrompt = function()
           {
           };
           this.Destroy = function()
           {
              if(this.Names != undefined)
              {
                 while(this.Names.length > 0)
                 {
                    this.Names.pop();
                 }
              }
           };
           this._visible = true;
           this.Update();
        }
        """;
    }

    /// <summary>
    /// Creates the visible Apply Changes row whose activation dispatches through the focused
    /// controller while directional changes and prompt handling remain no-ops.
    /// </summary>
    /// <returns>An ActionScript load handler for row fifteen.</returns>
    private static string CreateApplyRowClipAction()
    {
        return """
        onClipEvent(load){
           this.Names = new Array("");
           this.State = 0;
           this.Initial = 0;
           this.Default = 0;
           this.Update = function()
           {
              if(this.Label != undefined && this.Label.Label != undefined && this.Label.Label.Text != undefined)
              {
                 this.Label.Label.Text.text = "Apply Changes";
              }
              else if(this.Label != undefined && this.Label.Text != undefined)
              {
                 this.Label.Text.text = "Apply Changes";
              }
              else if(this.Label != undefined)
              {
                 this.Label.text = "Apply Changes";
              }
              this.ItemText.text = "";
              if(_parent.GraphicsVsyncController != undefined)
              {
                 this.ItemText._alpha = _parent.GraphicsVsyncController.CanApply() ? 100 : 40;
                 this.Label._alpha = _parent.GraphicsVsyncController.CanApply() ? 100 : 40;
              }
              else
              {
                 this.ItemText._alpha = 40;
                 this.Label._alpha = 40;
              }
              if(this.LeftClicker != undefined)
              {
                 this.LeftClicker._visible = false;
              }
              if(this.RightClicker != undefined)
              {
                 this.RightClicker._visible = false;
              }
              this._visible = true;
           };
           this.RunAction = function()
           {
              _parent.GraphicsVsyncController.ApplyChanges();
           };
           this.Increment = function()
           {
           };
           this.Decrement = function()
           {
           };
           this.ShowPrompt = function()
           {
           };
           this.Destroy = function()
           {
              if(this.Names != undefined)
              {
                 while(this.Names.length > 0)
                 {
                    this.Names.pop();
                 }
              }
           };
           this._visible = true;
           this.Update();
        }
        """;
    }

    /// <summary>
    /// Creates a callback-free list-row clip action with fixed text, disabled directional clickers,
    /// no-op interaction methods, and the requested timeline visibility.
    /// </summary>
    /// <param name="label">The literal label written through the row's nested label text shape.</param>
    /// <param name="value">The literal value written through the row's value text shape.</param>
    /// <param name="visible">Whether the generated row clip remains visible.</param>
    /// <returns>An ActionScript load handler that initializes the fixed row shell.</returns>
    private static string CreateRowClipAction(string label, string value, bool visible)
    {
        ArgumentNullException.ThrowIfNull(label);
        ArgumentNullException.ThrowIfNull(value);

        string escapedLabel = EscapeActionScriptString(label);
        string escapedValue = EscapeActionScriptString(value);
        string visibility = visible ? "true" : "false";

        return $$"""
        onClipEvent(load){
           this.Names = new Array("{{escapedValue}}");
           this.State = 0;
           this.Initial = 0;
           this.Default = 0;
           this.Update = function()
           {
              if(this.Label != undefined && this.Label.Label != undefined && this.Label.Label.Text != undefined)
              {
                 this.Label.Label.Text.text = "{{escapedLabel}}";
              }
              else if(this.Label != undefined && this.Label.Text != undefined)
              {
                 this.Label.Text.text = "{{escapedLabel}}";
              }
              else if(this.Label != undefined)
              {
                 this.Label.text = "{{escapedLabel}}";
              }
              if(this.ItemText != undefined)
              {
                 this.ItemText.text = "{{escapedValue}}";
              }
              if(this.LeftClicker != undefined)
              {
                 this.LeftClicker._visible = false;
              }
              if(this.RightClicker != undefined)
              {
                 this.RightClicker._visible = false;
              }
           };
           this.RunAction = function()
           {
           };
           this.Increment = function()
           {
           };
           this.Decrement = function()
           {
           };
           this.ShowPrompt = function()
           {
           };
           this.Destroy = function()
           {
              if(this.Names != undefined)
              {
                 while(this.Names.length > 0)
                 {
                    this.Names.pop();
                 }
              }
           };
           this._visible = {{visibility}};
           this.Update();
        }
        """;
    }

    /// <summary>
    /// Rejects control characters that cannot be represented safely by the ActionScript string
    /// literal format, while allowing the three controls with explicit ActionScript escapes.
    /// </summary>
    /// <param name="value">The value whose characters must be validated.</param>
    /// <exception cref="ArgumentException">Thrown when an unsupported control character is found.</exception>
    public static void ValidateActionScriptString(string value)
    {
        ArgumentNullException.ThrowIfNull(value);

        foreach (char character in value)
        {
            if (character < '\u0020' && character != '\r' && character != '\n' && character != '\t')
            {
                throw new ArgumentException(
                    $"ActionScript string contains unsupported control character U+{(int)character:X4}.",
                    nameof(value));
            }
        }
    }

    /// <summary>
    /// Escapes an ActionScript string value by protecting backslashes before protecting quotes,
    /// converting supported control characters to ActionScript escapes, and rejecting unsupported controls.
    /// </summary>
    /// <param name="value">The unescaped value to place inside an ActionScript string literal.</param>
    /// <returns>The value with special characters escaped for ActionScript.</returns>
    public static string EscapeActionScriptString(string value)
    {
        ValidateActionScriptString(value);
        return value
            .Replace("\\", "\\\\", StringComparison.Ordinal)
            .Replace("\"", "\\\"", StringComparison.Ordinal)
            .Replace("\r", "\\r", StringComparison.Ordinal)
            .Replace("\n", "\\n", StringComparison.Ordinal)
            .Replace("\t", "\\t", StringComparison.Ordinal);
    }
}
