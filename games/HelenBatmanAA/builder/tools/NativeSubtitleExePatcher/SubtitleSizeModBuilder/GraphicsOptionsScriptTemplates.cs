namespace SubtitleSizeModBuilder;

/// <summary>
/// Centralizes ActionScript templates injected by the graphics-options prototype builder.
/// </summary>
internal static class GraphicsOptionsScriptTemplates
{
    /// <summary>
    /// Registers the cloned graphics-options screen class against sprite 4096.
    /// </summary>
    public const string ScreenOptionsGraphicsRegistration = """
    Object.registerClass("ScreenOptionsGraphics",rs.ui.Screen);
    """;

    /// <summary>
    /// Registers the graphics exit prompt class against sprite 4097.
    /// </summary>
    public const string GraphicsExitPromptRegistration = """
    Object.registerClass("GraphicsExitPrompt",rs.ui.Screen);
    """;

    /// <summary>
    /// Replaces the options-menu frame script to include a dedicated Graphics button between Game and Audio.
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
    /// Replaces the options-menu cloned Graphics button behavior so it opens the new graphics screen.
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
    /// Initializes the graphics options screen as a render-only preview with fixed rows and no runtime callbacks.
    /// </summary>
    public const string GraphicsOptionsFrame1 = """
    class rs.ui.BatmanGraphicsOptionsController
    {
       var Screen;
       var RowOrder;
       function BatmanGraphicsOptionsController(screen)
       {
          this.Screen = screen;
          this.RowOrder = new Array();
       }
       function Init()
       {
          this.RowOrder = new Array("Fullscreen","Resolution","VSync","MSAA","DetailLevel","Bloom","DynamicShadows","MotionBlur","Distortion","FogVolumes","SphericalHarmonicLighting","AmbientOcclusion","PhysX","Stereo3D","ApplyChanges");
       }
       function BindFixedRows()
       {
          this.BindFixedRow(this.Screen.GraphicsRow1,"Fullscreen");
          this.BindFixedRow(this.Screen.GraphicsRow2,"Resolution");
          this.BindFixedRow(this.Screen.GraphicsRow3,"VSync");
          this.BindFixedRow(this.Screen.GraphicsRow4,"MSAA");
          this.BindFixedRow(this.Screen.GraphicsRow5,"DetailLevel");
          this.BindFixedRow(this.Screen.GraphicsRow6,"Bloom");
          this.BindFixedRow(this.Screen.GraphicsRow7,"DynamicShadows");
          this.BindFixedRow(this.Screen.GraphicsRow8,"MotionBlur");
          this.BindFixedRow(this.Screen.GraphicsRow9,"Distortion");
          this.BindFixedRow(this.Screen.GraphicsRow10,"FogVolumes");
          this.BindFixedRow(this.Screen.GraphicsRow11,"SphericalHarmonicLighting");
          this.BindFixedRow(this.Screen.GraphicsRow12,"AmbientOcclusion");
          this.BindFixedRow(this.Screen.GraphicsRow13,"PhysX");
          this.BindFixedRow(this.Screen.GraphicsRow14,"Stereo3D");
          this.BindFixedRow(this.Screen.GraphicsRow15,"ApplyChanges");
       }
       function BindFixedRow(rowClip,rowName)
       {
          if(rowClip == undefined)
          {
             throw "Missing required graphics row clip: " + rowName;
          }
          if(rowClip.BindGraphicsRow == undefined)
          {
             throw "Graphics row clip missing BindGraphicsRow: " + rowName;
          }
          rowClip.BindGraphicsRow(rowName);
       }
       function RefreshRowClip(rowClip,rowName)
       {
          if(rowClip == undefined)
          {
             throw "Missing required graphics row clip: " + rowName;
          }
          if(rowClip.Update == undefined)
          {
             throw "Graphics row clip missing Update: " + rowName;
          }
          rowClip.Update();
       }
       function RefreshAllRows()
       {
          this.RefreshRowClip(this.Screen.GraphicsRow1,"Fullscreen");
          this.RefreshRowClip(this.Screen.GraphicsRow2,"Resolution");
          this.RefreshRowClip(this.Screen.GraphicsRow3,"VSync");
          this.RefreshRowClip(this.Screen.GraphicsRow4,"MSAA");
          this.RefreshRowClip(this.Screen.GraphicsRow5,"DetailLevel");
          this.RefreshRowClip(this.Screen.GraphicsRow6,"Bloom");
          this.RefreshRowClip(this.Screen.GraphicsRow7,"DynamicShadows");
          this.RefreshRowClip(this.Screen.GraphicsRow8,"MotionBlur");
          this.RefreshRowClip(this.Screen.GraphicsRow9,"Distortion");
          this.RefreshRowClip(this.Screen.GraphicsRow10,"FogVolumes");
          this.RefreshRowClip(this.Screen.GraphicsRow11,"SphericalHarmonicLighting");
          this.RefreshRowClip(this.Screen.GraphicsRow12,"AmbientOcclusion");
          this.RefreshRowClip(this.Screen.GraphicsRow13,"PhysX");
          this.RefreshRowClip(this.Screen.GraphicsRow14,"Stereo3D");
          this.RefreshRowClip(this.Screen.GraphicsRow15,"ApplyChanges");
       }
       function GetRowLabel(rowName)
       {
          if(rowName == "VSync")
          {
             return "VSync";
          }
          if(rowName == "DetailLevel")
          {
             return "Detail Level";
          }
          if(rowName == "DynamicShadows")
          {
             return "Dynamic Shadows";
          }
          if(rowName == "MotionBlur")
          {
             return "Motion Blur";
          }
          if(rowName == "FogVolumes")
          {
             return "Fog Volumes";
          }
          if(rowName == "SphericalHarmonicLighting")
          {
             return "Spherical Harmonic Lighting";
          }
          if(rowName == "AmbientOcclusion")
          {
             return "Ambient Occlusion";
          }
          if(rowName == "Stereo3D")
          {
             return "NVIDIA Stereoscopic 3D";
          }
          if(rowName == "ApplyChanges")
          {
             return "Apply Changes";
          }
          return rowName;
       }
       function GetRowDisplayValue(rowName)
       {
          if(rowName == "Fullscreen")
          {
             return "Fullscreen";
          }
          if(rowName == "Resolution")
          {
             return "1920 x 1080";
          }
          if(rowName == "VSync")
          {
             return "Enabled";
          }
          if(rowName == "MSAA")
          {
             return "4x";
          }
          if(rowName == "DetailLevel")
          {
             return "High";
          }
          if(rowName == "Bloom" || rowName == "DynamicShadows" || rowName == "MotionBlur" || rowName == "Distortion" || rowName == "FogVolumes" || rowName == "SphericalHarmonicLighting" || rowName == "AmbientOcclusion")
          {
             return "Enabled";
          }
          if(rowName == "PhysX")
          {
             return "High";
          }
          if(rowName == "Stereo3D")
          {
             return "Disabled";
          }
          if(rowName == "ApplyChanges")
          {
             return "Preview Only";
          }
          return "Unavailable";
       }
       function GetRowValues(rowName)
       {
          return new Array(this.GetRowDisplayValue(rowName));
       }
       function GetRowState(rowName)
       {
          return 0;
       }
       function IsInteractiveRow(rowName)
       {
          return false;
       }
       function IsRowEnabled(rowName)
       {
          return true;
       }
    }
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
    this.GraphicsController = new rs.ui.BatmanGraphicsOptionsController(this);
    this.GraphicsController.Init();
    if(this.Title != undefined)
    {
       this.Title.text = "Graphics Options";
    }
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
    _rotation = -2;
    """;

    /// <summary>
    /// Settles the graphics options screen after the cloned timeline finishes loading and refreshes every fixed row.
    /// </summary>
    public const string GraphicsOptionsFrame15 = """
    this.GraphicsController.BindFixedRows();
    this.GraphicsController.RefreshAllRows();
    stop();
    """;

    /// <summary>
    /// Builds one reusable graphics-row clip action script for a fixed logical row name.
    /// </summary>
    /// <param name="rowName">Stable graphics row name bound to the clip.</param>
    /// <returns>ActionScript that binds one list-template row to the graphics controller.</returns>
    private static string CreateGraphicsRowClipAction(string rowName)
    {
        return $$"""
        onClipEvent(load){
           this.RowName = "{{rowName}}";
           this.BindGraphicsRow = function(rowName)
           {
              if(rowName == undefined)
              {
                 this._visible = false;
                 return undefined;
              }
              this._visible = true;
              this.RowName = rowName;
              this.LabelName = _parent.GraphicsController.GetRowLabel(rowName);
              this.Names = _parent.GraphicsController.GetRowValues(rowName);
              this.State = _parent.GraphicsController.GetRowState(rowName);
              this.Initial = this.State;
              this.Default = this.State;
              this.UpdateGraphicsRow();
           };
           this.IsEnabled = function()
           {
              return _parent.GraphicsController.IsRowEnabled(this.RowName);
           };
           this.UpdateGraphicsRow = function()
           {
              var _loc2_ = this.IsEnabled();
              var _loc3_ = this.Names[this.State];
              if(this.Label != undefined && this.Label.Label != undefined && this.Label.Label.Text != undefined)
              {
                 this.Label.Label.Text.text = this.LabelName;
              }
              else if(this.Label != undefined && this.Label.Text != undefined)
              {
                 this.Label.Text.text = this.LabelName;
              }
              else if(this.Label != undefined)
              {
                 this.Label.text = this.LabelName;
              }
              if(_loc3_ == undefined)
              {
                 _loc3_ = "";
              }
              this.ItemText.text = _loc3_;
              this.ItemText._alpha = _loc2_ ? 100 : 40;
              this.Label._alpha = _loc2_ ? 100 : 40;
              this.LeftClicker._visible = false;
              this.RightClicker._visible = false;
           };
           this.Update = function()
           {
              if(_parent.GraphicsController == undefined)
              {
                 return undefined;
              }
              this.LabelName = _parent.GraphicsController.GetRowLabel(this.RowName);
              this.Names = _parent.GraphicsController.GetRowValues(this.RowName);
              this.State = _parent.GraphicsController.GetRowState(this.RowName);
              this.UpdateGraphicsRow();
           };
           this.ShowPrompt = function()
           {
              var _loc2_ = _root.PromptManager;
              if(this._parent.BackScreen != "")
              {
                 _loc2_.SetPrompt(_loc2_.CI_B,"$UI.Cancel",this._parent.myListener.onPromptClick,100,100);
              }
           };
           this.RunAction = function(bMouse)
           {
              return undefined;
           };
           this.Increment = function()
           {
              return undefined;
           };
           this.Decrement = function()
           {
              return undefined;
           };
           if(_parent.GraphicsController != undefined)
           {
              _parent.GraphicsController.BindFixedRow(this,this.RowName);
           }
           else
           {
              this._visible = false;
           }
        }
        """;
    }

    /// <summary>
    /// Binds top graphics row interactions to Fullscreen.
    /// </summary>
    public static readonly string GraphicsRow1ClipAction = CreateGraphicsRowClipAction("Fullscreen");

    /// <summary>
    /// Binds second graphics row interactions to Resolution.
    /// </summary>
    public static readonly string GraphicsRow2ClipAction = CreateGraphicsRowClipAction("Resolution");

    /// <summary>
    /// Binds third graphics row interactions to VSync.
    /// </summary>
    public static readonly string GraphicsRow3ClipAction = CreateGraphicsRowClipAction("VSync");

    /// <summary>
    /// Binds fourth graphics row interactions to MSAA.
    /// </summary>
    public static readonly string GraphicsRow4ClipAction = CreateGraphicsRowClipAction("MSAA");

    /// <summary>
    /// Binds fifth graphics row interactions to DetailLevel.
    /// </summary>
    public static readonly string GraphicsRow5ClipAction = CreateGraphicsRowClipAction("DetailLevel");

    /// <summary>
    /// Binds sixth graphics row interactions to Bloom.
    /// </summary>
    public static readonly string GraphicsRow6ClipAction = CreateGraphicsRowClipAction("Bloom");

    /// <summary>
    /// Binds seventh graphics row interactions to DynamicShadows.
    /// </summary>
    public static readonly string GraphicsRow7ClipAction = CreateGraphicsRowClipAction("DynamicShadows");

    /// <summary>
    /// Binds eighth graphics row interactions to MotionBlur.
    /// </summary>
    public static readonly string GraphicsRow8ClipAction = CreateGraphicsRowClipAction("MotionBlur");

    /// <summary>
    /// Binds ninth graphics row interactions to Distortion.
    /// </summary>
    public static readonly string GraphicsRow9ClipAction = CreateGraphicsRowClipAction("Distortion");

    /// <summary>
    /// Binds tenth graphics row interactions to FogVolumes.
    /// </summary>
    public static readonly string GraphicsRow10ClipAction = CreateGraphicsRowClipAction("FogVolumes");

    /// <summary>
    /// Binds eleventh graphics row interactions to SphericalHarmonicLighting.
    /// </summary>
    public static readonly string GraphicsRow11ClipAction = CreateGraphicsRowClipAction("SphericalHarmonicLighting");

    /// <summary>
    /// Binds twelfth graphics row interactions to AmbientOcclusion.
    /// </summary>
    public static readonly string GraphicsRow12ClipAction = CreateGraphicsRowClipAction("AmbientOcclusion");

    /// <summary>
    /// Binds thirteenth graphics row interactions to PhysX.
    /// </summary>
    public static readonly string GraphicsRow13ClipAction = CreateGraphicsRowClipAction("PhysX");

    /// <summary>
    /// Binds fourteenth graphics row interactions to Stereo3D.
    /// </summary>
    public static readonly string GraphicsRow14ClipAction = CreateGraphicsRowClipAction("Stereo3D");

    /// <summary>
    /// Binds fifteenth graphics row interactions to ApplyChanges.
    /// </summary>
    public static readonly string GraphicsRow15ClipAction = CreateGraphicsRowClipAction("ApplyChanges");

    /// <summary>
    /// Initializes the three-button graphics exit prompt and wires cancel behavior.
    /// </summary>
    public const string GraphicsExitPromptFrame1 = """
    function CancelScreen()
    {
       Response("cancel");
    }
    function ConfigureUnsavedChanges(controller)
    {
       this.GraphicsController = controller;
       this.Message.Label.text = "Unsaved graphics changes detected.";
    }
    function ConfigureRestartRequired(controller)
    {
       this.GraphicsController = controller;
       this.Message.Label.text = "Some changes require a restart.";
    }
    function Response(option)
    {
       if(this.GraphicsController != undefined)
       {
          this.GraphicsController.OnExitPromptResponse(option);
       }
       this.gotoAndPlay("out");
    }
    this.BackScreen = "!";
    this.BackScreenIndex = 0;
    this.FocusIndex = 1;
    this.Flags = this.FLAG_NOROLLOVER;
    this.Init();
    this.AddItem(Yes,2,1,-1,-1);
    this.AddItem(Apply,0,2,-1,-1);
    this.AddItem(No,1,0,-1,-1);
    _rotation = -2.5;
    """;

    /// <summary>
    /// Wires the middle prompt button to discard pending graphics changes.
    /// </summary>
    public const string GraphicsExitPromptApplyButton = """
    onClipEvent(load){
       function RunAction()
       {
          _parent.Response("discard");
       }
       this.ButtonName = "Discard";
       Label.Text.text = this.ButtonName;
    }
    """;

    /// <summary>
    /// Wires the lower prompt button to cancel prompt dismissal.
    /// </summary>
    public const string GraphicsExitPromptNoButton = """
    onClipEvent(load){
       function RunAction()
       {
          _parent.Response("cancel");
       }
       this.ButtonName = "Cancel";
       Label.Text.text = this.ButtonName;
    }
    """;

    /// <summary>
    /// Wires the upper prompt button to apply and continue.
    /// </summary>
    public const string GraphicsExitPromptYesButton = """
    onClipEvent(load){
       function RunAction()
       {
          _parent.Response("apply");
       }
       this.ButtonName = "Apply";
       Label.Text.text = this.ButtonName;
    }
    """;
}
