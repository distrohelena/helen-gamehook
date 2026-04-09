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
    /// Initializes the graphics options screen with fixed row bindings across all fifteen graphics rows.
    /// </summary>
    public const string GraphicsOptionsFrame1 = """
    class rs.ui.BatmanGraphicsOptionsController
    {
       var Screen;
       var ExitPrompt;
       var RowOrder;
       var DraftState;
       var InitialState;
       var ExitPromptMode;
       function BatmanGraphicsOptionsController(screen)
       {
          this.Screen = screen;
          this.RowOrder = new Array();
          this.DraftState = {};
          this.InitialState = {};
          this.ExitPromptMode = "none";
       }
       function Init()
       {
          this.RowOrder = new Array("Fullscreen","Resolution","VSync","MSAA","DetailLevel","Bloom","DynamicShadows","MotionBlur","Distortion","FogVolumes","SphericalHarmonicLighting","AmbientOcclusion","PhysX","Stereo3D","ApplyChanges");
          this.LoadDraftValues();
          this.CaptureInitialState();
          this.BindFixedRows();
          this.RefreshAllRows();
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
       function GetTrackedDraftKeys()
       {
          return new Array("vsync","msaa","detailLevel","bloom","dynamicShadows","motionBlur","distortion","fogVolumes","sphericalHarmonicLighting","ambientOcclusion","physx","stereo");
       }
       function NormalizeInt(value, fallback)
       {
          if(value == undefined)
          {
             return fallback;
          }
          return int(value);
       }
       function ReadDraftInt(key, fallback)
       {
          return this.NormalizeInt(flash.external.ExternalInterface.call("Helen_GetInt",key),fallback);
       }
       function LoadDraftValues()
       {
          this.DraftState = {};
          this.DraftState.fullscreen = this.ReadDraftInt("fullscreen",0);
          this.DraftState.resolutionWidth = this.ReadDraftInt("resolutionWidth",0);
          this.DraftState.resolutionHeight = this.ReadDraftInt("resolutionHeight",0);
          this.DraftState.vsync = this.ReadDraftInt("vsync",0);
          this.DraftState.msaa = this.ReadDraftInt("msaa",0);
          this.DraftState.detailLevel = this.ReadDraftInt("detailLevel",1);
          this.DraftState.bloom = this.ReadDraftInt("bloom",0);
          this.DraftState.dynamicShadows = this.ReadDraftInt("dynamicShadows",0);
          this.DraftState.motionBlur = this.ReadDraftInt("motionBlur",0);
          this.DraftState.distortion = this.ReadDraftInt("distortion",0);
          this.DraftState.fogVolumes = this.ReadDraftInt("fogVolumes",0);
          this.DraftState.sphericalHarmonicLighting = this.ReadDraftInt("sphericalHarmonicLighting",0);
          this.DraftState.ambientOcclusion = this.ReadDraftInt("ambientOcclusion",0);
          this.DraftState.physx = this.ReadDraftInt("physx",0);
          this.DraftState.stereo = this.ReadDraftInt("stereo",0);
       }
       function CaptureInitialState()
       {
          var _loc2_ = this.GetTrackedDraftKeys();
          this.InitialState = {};
          var _loc3_ = 0;
          while(_loc3_ < _loc2_.length)
          {
             this.InitialState[_loc2_[_loc3_]] = this.DraftState[_loc2_[_loc3_]];
             _loc3_ = _loc3_ + 1;
          }
       }
       function RefreshFocusedRow()
       {
          this.RefreshAllRows();
          this.Screen.ReUpdate();
       }
       function GetDraftKey(rowName)
       {
          if(rowName == "VSync")
          {
             return "vsync";
          }
          if(rowName == "MSAA")
          {
             return "msaa";
          }
          if(rowName == "DetailLevel")
          {
             return "detailLevel";
          }
          if(rowName == "Bloom")
          {
             return "bloom";
          }
          if(rowName == "DynamicShadows")
          {
             return "dynamicShadows";
          }
          if(rowName == "MotionBlur")
          {
             return "motionBlur";
          }
          if(rowName == "Distortion")
          {
             return "distortion";
          }
          if(rowName == "FogVolumes")
          {
             return "fogVolumes";
          }
          if(rowName == "SphericalHarmonicLighting")
          {
             return "sphericalHarmonicLighting";
          }
          if(rowName == "AmbientOcclusion")
          {
             return "ambientOcclusion";
          }
          if(rowName == "PhysX")
          {
             return "physx";
          }
          if(rowName == "Stereo3D")
          {
             return "stereo";
          }
          return undefined;
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
       function GetRowValues(rowName)
       {
          if(rowName == "Fullscreen")
          {
             return new Array("Windowed","Fullscreen");
          }
          if(rowName == "Resolution")
          {
             return new Array(this.DraftState.resolutionWidth + " x " + this.DraftState.resolutionHeight);
          }
          if(rowName == "VSync")
          {
             return new Array("No","Yes");
          }
          if(rowName == "MSAA")
          {
             return new Array("Disabled","2x","4x","8x","8xQ","16x","16xQ");
          }
          if(rowName == "DetailLevel")
          {
             return new Array("Low","Medium","High","Very High","Custom");
          }
          if(rowName == "Bloom" || rowName == "DynamicShadows" || rowName == "MotionBlur" || rowName == "Distortion" || rowName == "FogVolumes" || rowName == "SphericalHarmonicLighting" || rowName == "AmbientOcclusion" || rowName == "Stereo3D")
          {
             return new Array("Disabled","Enabled");
          }
          if(rowName == "PhysX")
          {
             return new Array("Off","Normal","High");
          }
          if(rowName == "ApplyChanges")
          {
             return new Array("No Changes","Apply");
          }
          return new Array("Unavailable");
       }
       function IsApplyRow(rowName)
       {
          return rowName == "ApplyChanges";
       }
       function IsInteractiveRow(rowName)
       {
          return this.GetDraftKey(rowName) != undefined;
       }
       function IsRowEnabled(rowName)
       {
          if(rowName == "Fullscreen" || rowName == "Resolution")
          {
             return true;
          }
          if(this.IsApplyRow(rowName))
          {
             return this.HasUnsavedChanges();
          }
          if(this.IsInteractiveRow(rowName))
          {
             return true;
          }
          return false;
       }
       function GetRowState(rowName)
       {
          if(rowName == "Fullscreen")
          {
             return this.NormalizeInt(this.DraftState.fullscreen,0);
          }
          if(rowName == "Resolution")
          {
             return 0;
          }
          var _loc2_ = this.GetDraftKey(rowName);
          if(_loc2_ != undefined)
          {
             return this.NormalizeInt(this.DraftState[_loc2_],0);
          }
          if(rowName == "ApplyChanges")
          {
             return this.HasUnsavedChanges() ? 1 : 0;
          }
          return 0;
       }
       function SetDraftRowState(rowName, nextState)
       {
          var _loc2_ = this.GetDraftKey(rowName);
          if(_loc2_ == undefined)
          {
             return false;
          }
          if(!flash.external.ExternalInterface.call("Helen_SetInt",_loc2_,nextState))
          {
             return false;
          }
          this.LoadDraftValues();
          this.RefreshFocusedRow();
          return true;
       }
       function StepDraftRowState(rowName, delta)
       {
          var _loc2_ = this.GetRowValues(rowName);
          var _loc3_ = this.GetRowState(rowName);
          var _loc4_ = _loc3_ + delta;
          if(rowName == "DetailLevel" && _loc3_ == 4)
          {
             _loc4_ = delta > 0 ? 0 : 3;
          }
          if(_loc4_ < 0 || _loc4_ >= _loc2_.length)
          {
             return false;
          }
          if(rowName == "DetailLevel" && _loc4_ == 4)
          {
             return false;
          }
          return this.SetDraftRowState(rowName,_loc4_);
       }
       function CycleDraftRowState(rowName)
       {
          var _loc2_ = this.GetRowValues(rowName);
          var _loc3_ = this.GetRowState(rowName);
          var _loc4_ = _loc3_ + 1;
          if(rowName == "DetailLevel" && _loc3_ == 4)
          {
             _loc4_ = 0;
          }
          if(_loc4_ >= _loc2_.length)
          {
             _loc4_ = 0;
          }
          if(rowName == "DetailLevel" && _loc4_ == 4)
          {
             _loc4_ = 0;
          }
          return this.SetDraftRowState(rowName,_loc4_);
       }
       function PlayStepSound(forward)
       {
          flash.external.ExternalInterface.call("FE_PlaySoundFromString",forward ? "UI_FrontEndSFX.UI_Forward" : "UI_FrontEndSFX.UI_Back");
       }
       function HandleRowAction(rowName)
       {
          if(this.IsApplyRow(rowName))
          {
             this.ApplyChanges();
             return undefined;
          }
          if(this.IsInteractiveRow(rowName))
          {
             if(this.CycleDraftRowState(rowName))
             {
                this.PlayStepSound(true);
             }
          }
       }
       function IncrementRow(rowName)
       {
          if(this.IsInteractiveRow(rowName))
          {
             if(this.StepDraftRowState(rowName,1))
             {
                this.PlayStepSound(true);
             }
          }
       }
       function DecrementRow(rowName)
       {
          if(this.IsInteractiveRow(rowName))
          {
             if(this.StepDraftRowState(rowName,-1))
             {
                this.PlayStepSound(false);
             }
          }
       }
       function HasUnsavedChanges()
       {
          var _loc2_ = this.GetTrackedDraftKeys();
          var _loc3_ = 0;
          while(_loc3_ < _loc2_.length)
          {
             if(this.DraftState[_loc2_[_loc3_]] != this.InitialState[_loc2_[_loc3_]])
             {
                return true;
             }
             _loc3_ = _loc3_ + 1;
          }
          return false;
       }
       function EnsureExitPrompt()
       {
          if(this.ExitPrompt == undefined)
          {
             this.ExitPrompt = this.Screen.attachMovie("GraphicsExitPrompt","GraphicsExitPrompt",601);
          }
          return this.ExitPrompt;
       }
       function ShowUnsavedChangesPrompt()
       {
          this.EnsureExitPrompt();
          this.ExitPromptMode = "unsaved";
          this.ExitPrompt.ConfigureUnsavedChanges(this);
          this.ExitPrompt.gotoAndPlay("in");
       }
       function RequestUnsavedChangesPrompt()
       {
          this.ShowUnsavedChangesPrompt();
       }
       function ShowRestartRequiredPrompt()
       {
          this.EnsureExitPrompt();
          this.ExitPromptMode = "restart";
          this.ExitPrompt.ConfigureRestartRequired(this);
          this.ExitPrompt.gotoAndPlay("in");
       }
       function OnExitPromptResponse(response)
       {
          if(this.ExitPromptMode == "restart")
          {
             if(response == "apply" || response == "discard")
             {
                this.Screen.ReturnFromScreen();
                return undefined;
             }
             if(response == "cancel")
             {
                return undefined;
             }
          }
          if(response == "apply")
          {
             this.ApplyChanges();
          }
          else if(response == "discard")
          {
             this.Screen.ReturnFromScreen();
          }
          else if(response == "cancel")
          {
             return undefined;
          }
       }
       function ApplyChanges()
       {
          if(!flash.external.ExternalInterface.call("Helen_RunCommand","applyBatmanGraphicsDraft"))
          {
             return undefined;
          }
          this.LoadDraftValues();
          this.CaptureInitialState();
          this.RefreshFocusedRow();
          this.ShowRestartRequiredPrompt();
       }
    }
    function CancelScreen()
    {
       if(this.GraphicsController.HasUnsavedChanges())
       {
          this.GraphicsController.RequestUnsavedChangesPrompt();
          return undefined;
       }
       ReturnFromScreen();
    }
    flash.external.ExternalInterface.call("FE_SetActiveScreenName","Graphics Options");
    this.BackScreen = "OptionsMenu";
    this.BackScreenIndex = 1;
    this.FocusIndex = 0;
    this.Flags = this.FLAG_OPTIONS;
    this.Init();
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
           this.IsApplyRow = function()
           {
              return _parent.GraphicsController.IsApplyRow(this.RowName);
           };
           this.IsInteractiveRow = function()
           {
              return _parent.GraphicsController.IsInteractiveRow(this.RowName);
           };
           this.IsEnabled = function()
           {
              return _parent.GraphicsController.IsRowEnabled(this.RowName);
           };
           this.UpdateGraphicsRow = function()
           {
              var _loc2_ = this.IsEnabled();
              var _loc3_ = this.IsInteractiveRow();
              var _loc4_ = this.Names[this.State];
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
              if(_loc4_ == undefined)
              {
                 _loc4_ = "";
              }
              this.ItemText.text = _loc4_;
              this.ItemText._alpha = _loc2_ ? 100 : 40;
              this.Label._alpha = _loc2_ ? 100 : 40;
              if(this.IsApplyRow())
              {
                 this.LeftClicker._visible = false;
                 this.RightClicker._visible = false;
                 return undefined;
              }
              if(!_loc3_ || !_loc2_)
              {
                 this.LeftClicker._visible = false;
                 this.RightClicker._visible = false;
                 return undefined;
              }
              if(this.RowName == "DetailLevel" && this.State == 4)
              {
                 this.LeftClicker._visible = true;
                 this.RightClicker._visible = true;
                 return undefined;
              }
              this.LeftClicker._visible = this.State > 0;
              this.RightClicker._visible = this.State < this.Names.length - 1;
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
              if(this.IsApplyRow())
              {
                 if(this.IsEnabled())
                 {
                    _loc2_.SetPrompt(_loc2_.CI_Interact,"Apply",this._parent.myListener.onPromptClick,100,100);
                 }
                 return undefined;
              }
              if(this.IsInteractiveRow())
              {
                 _loc2_.SetPrompt(_loc2_.CI_Interact,"$UI.Cycle",this._parent.myListener.onPromptClick,100,100);
              }
           };
           this.RunAction = function(bMouse)
           {
              if(this.IsApplyRow())
              {
                 if(this.IsEnabled())
                 {
                    _parent.GraphicsController.HandleRowAction(this.RowName);
                 }
                 return undefined;
              }
              if(!this.IsInteractiveRow() || !this.IsEnabled())
              {
                 return undefined;
              }
              if(this.Names.length < 3 || !bMouse)
              {
                 _parent.GraphicsController.HandleRowAction(this.RowName);
              }
              else if(this._xmouse < 0)
              {
                 _parent.GraphicsController.DecrementRow(this.RowName);
              }
              else
              {
                 _parent.GraphicsController.IncrementRow(this.RowName);
              }
           };
           this.Increment = function()
           {
              _parent.GraphicsController.IncrementRow(this.RowName);
           };
           this.Decrement = function()
           {
              _parent.GraphicsController.DecrementRow(this.RowName);
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
