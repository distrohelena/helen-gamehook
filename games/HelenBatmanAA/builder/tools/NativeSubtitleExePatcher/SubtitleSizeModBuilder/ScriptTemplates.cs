namespace SubtitleSizeModBuilder;

/// <summary>
/// Centralizes the ActionScript source templates injected into rebuilt Gotham UI assets.
/// </summary>
internal static class ScriptTemplates
{
    /// <summary>
    /// Replaces the pause-menu ListItem with runtime subtitle-size storage support.
    /// </summary>
    public const string PauseRuntimeScaleListItem = """
    class rs.ui.ListItem extends MovieClip
    {
       var ItemText;
       var Label;
       var LabelName;
       var LeftClicker;
       var Names;
       var RightClicker;
       var GameVariable = "?name?";
       var State = 0;
       var Initial = 0;
       var Default = 0;
       function ListItem()
       {
          super();
       }
       function UsesSubtitleSizeStorage()
       {
          return this.GameVariable == "SubtitleSize";
       }
       function NormalizeIntState(InValue, DefaultValue)
       {
          if(InValue == undefined)
          {
              return DefaultValue;
          }
          return int(InValue);
       }
       function GetSubtitleDefaultState()
       {
          return 1;
       }
       function NormalizeStoredSubtitleState(InValue)
       {
          var _loc2_ = this.NormalizeIntState(InValue,this.GetSubtitleDefaultState());
          if(_loc2_ < 0 || _loc2_ > 2)
          {
             _loc2_ = this.GetSubtitleDefaultState();
          }
          return _loc2_;
       }
       function ReadSubtitleSizeState()
       {
          return this.NormalizeStoredSubtitleState(flash.external.ExternalInterface.call("Helen_GetInt","ui.subtitleSize"));
       }
       function IsBinarySubtitleToggle()
       {
          return this.GameVariable == "Subtitles" && this.Names.length == 2;
       }
       function NormalizeSubtitleToggleState(InValue)
       {
          if(InValue == undefined)
          {
             return 0;
          }
          return InValue != 0 ? 1 : 0;
       }
       function StoreSubtitleSizeState()
       {
          flash.external.ExternalInterface.call("Helen_SetInt","ui.subtitleSize",this.State);
       }
       function ApplySubtitleSizeRuntime()
       {
          flash.external.ExternalInterface.call("Helen_RunCommand","applySubtitleSize");
       }
       function WriteSubtitleSizeState()
       {
          this.StoreSubtitleSizeState();
          this.ApplySubtitleSizeRuntime();
       }
       function Init()
       {
          this.Names = new Array();
          this.GameVariable = arguments[0];
          if(this.UsesSubtitleSizeStorage())
          {
             this.LabelName = "Subtitle Size";
          }
          else
          {
             this.LabelName = "$UI." + arguments[0];
          }
          this.Label.text = this.LabelName;
          var _loc3_ = 1;
          while(_loc3_ < arguments.length)
          {
             this.Names.push(arguments[_loc3_]);
             _loc3_ = _loc3_ + 1;
          }
          var _loc4_;
          if(this.UsesSubtitleSizeStorage() && this.Names.length > 3)
          {
             while(this.Names.length > 3)
             {
                this.Names.pop();
             }
          }
          if(this.UsesSubtitleSizeStorage())
          {
             _loc4_ = this.ReadSubtitleSizeState();
          }
          else
          {
             _loc4_ = flash.external.ExternalInterface.call("FE_Get" + this.GameVariable);
             if(this.IsBinarySubtitleToggle())
             {
                _loc4_ = this.NormalizeSubtitleToggleState(_loc4_);
             }
          }
          if(_loc4_ == undefined)
          {
             _loc4_ = 0;
          }
          if(_loc4_ < 0 || _loc4_ >= this.Names.length)
          {
             _loc4_ = this.UsesSubtitleSizeStorage() ? this.GetSubtitleDefaultState() : 0;
          }
          this.State = _loc4_;
          this.Initial = _loc4_;
          this.UpdateLRMarkers();
          this.FetchDefault();
       }
       function Destroy()
       {
          while(this.Names.length)
          {
             this.Names.pop();
          }
       }
       function HasChanged()
       {
          return this.State != this.Initial;
       }
       function RestoreInitialValue()
       {
          if(this.HasChanged())
          {
             this.State = this.Initial;
             this.UpdateLRMarkers();
          }
       }
       function IsDefault()
       {
          return this.State == this.Default;
       }
       function FetchDefault()
       {
          var _loc2_;
           if(this.UsesSubtitleSizeStorage())
           {
              _loc2_ = this.GetSubtitleDefaultState();
           }
           else
           {
             _loc2_ = flash.external.ExternalInterface.call("FE_GetDefault" + this.GameVariable);
             if(this.IsBinarySubtitleToggle())
             {
                _loc2_ = this.NormalizeSubtitleToggleState(_loc2_);
             }
          }
          if(_loc2_ == undefined)
          {
             _loc2_ = 0;
          }
          this.Default = _loc2_;
       }
       function SetDefault()
       {
          this.State = this.Default;
          this.UpdateLRMarkers();
       }
       function onRollOver()
       {
          this._parent.DoSetFocus(this);
       }
       function onPress()
       {
          this._parent.DoPress(this,true);
       }
       function onRelease()
       {
          this._parent.DoRelease(this,true,true);
       }
       function onReleaseOutside()
       {
          this._parent.DoRelease(this,true,false);
       }
       function Update()
       {
          this.UpdateLRMarkers();
       }
       function ShowPrompt()
       {
          var _loc3_ = _root.PromptManager;
          if(this._parent.BackScreen != "")
          {
             _loc3_.SetPrompt(_loc3_.CI_B,"$UI.Cancel",this._parent.myListener.onPromptClick,100,100);
          }
          if(this.Names.length > 1)
          {
             _loc3_.SetPrompt(_loc3_.CI_Interact,"$UI.Cycle",this._parent.myListener.onPromptClick,100,100);
          }
       }
       function UpdateLRMarkersReal()
       {
          this.ItemText.text = this.Names[this.State];
          if(this.State > 0)
          {
             this.LeftClicker._visible = true;
          }
          else
          {
             this.LeftClicker._visible = false;
          }
          if(this.State < this.Names.length - 1)
          {
             this.RightClicker._visible = true;
          }
          else
          {
             this.RightClicker._visible = false;
          }
          if(this.UsesSubtitleSizeStorage())
          {
             this.WriteSubtitleSizeState();
          }
          else if(this.IsBinarySubtitleToggle())
          {
            if(this.State == 0)
            {
                flash.external.ExternalInterface.call("FE_SetSubtitles",0,this.Names[this.State]);
            }
            else
            {
                flash.external.ExternalInterface.call("FE_SetSubtitles",1,this.Names[this.State]);
                flash.external.ExternalInterface.call("Helen_RunCommand","applySubtitleSize");
            }
          }
          else
          {
             flash.external.ExternalInterface.call("FE_Set" + this.GameVariable,this.State,this.Names[this.State]);
          }
       }
       function UpdateLRMarkers()
       {
          this.UpdateLRMarkersReal();
       }
       function Increment()
       {
          if(this.State < this.Names.length - 1)
          {
             this.State += 1;
             this.UpdateLRMarkers();
             flash.external.ExternalInterface.call("FE_PlaySoundFromString","UI_FrontEndSFX.UI_Forward");
          }
       }
       function Decrement()
       {
          if(this.State > 0)
          {
             this.State -= 1;
             this.UpdateLRMarkers();
             flash.external.ExternalInterface.call("FE_PlaySoundFromString","UI_FrontEndSFX.UI_Back");
          }
       }
       function HasAction()
       {
          return false;
       }
       function Cycle()
       {
          if(this.Names.length == 1)
          {
             return undefined;
          }
          this.State += 1;
          if(this.State >= this.Names.length)
          {
             this.State = 0;
          }
          this.UpdateLRMarkers();
          flash.external.ExternalInterface.call("FE_PlaySoundFromString","UI_FrontEndSFX.UI_Forward");
       }
       function RunAction(bMouse)
       {
          if(this.Names.length < 3)
          {
             this.Cycle();
          }
          else if(bMouse)
          {
             if(this._xmouse < 0)
             {
                this.Decrement();
             }
             else
             {
                this.Increment();
             }
          }
          else
          {
             this.Cycle();
          }
       }
       function IsListButton()
       {
          return false;
       }
    }
    """;

    public const string HudRuntimeScaleHelpers = """
    function ApplySavedSubtitleSizeCommand()
    {
       flash.external.ExternalInterface.call("Helen_RunCommand","applySubtitleSize");
    }
    """;

    /// <summary>
    /// Reuses the runtime subtitle-size menu behavior for pause assets so all subtitle-size
    /// builds stay at three states (Small/Normal/Large).
    /// </summary>
    public const string PauseListItem = PauseRuntimeScaleListItem;

    public const string PauseAudioFrame1 = """
    function CancelScreen()
    {
       if(HaveOptionsChanged() == true)
       {
          flash.external.ExternalInterface.call("FE_TriggerOptionsSave");
       }
       ReturnFromScreen();
    }
    this.BackScreen = "Pause";
    this.BackScreenIndex = 3;
    this.FocusIndex = 0;
    this.Flags = this.FLAG_OPTIONS;
    this.Init();
    this.AddItem(Subtitles,4,1,-1,-1);
    this.AddItem(VolumeSFX,0,2,-1,-1);
    this.AddItem(VolumeMusic,1,3,-1,-1);
    this.AddItem(VolumeDialogue,2,4,-1,-1);
    this.AddItem(SubtitleSize,3,0,-1,-1);
    _rotation = -3;
    """;

    /// <summary>
    /// Stops the pause-menu audio screen on frame 12 after initialization.
    /// </summary>
    public const string PauseAudioFrame12 = """
    stop();
    """;

    /// <summary>
    /// Initializes the dedicated pause-menu subtitle-size row.
    /// </summary>
    public const string PauseAudioSubtitleSizeClipAction = """
    onClipEvent(load){
       this.Init("SubtitleSize","Small","Normal","Large");
    }
    """;

    /// <summary>
    /// Replaces the frontend ListItem so subtitle size is a dedicated row that can be disabled when subtitles are off.
    /// </summary>
    public const string FrontendListItem = """
    class rs.ui.ListItem extends MovieClip
    {
       var ItemText;
       var Label;
       var LabelName;
       var LeftClicker;
       var Names;
       var RightClicker;
       var GameVariable = "?name?";
       var State = 0;
       var Initial = 0;
       var Default = 0;
       function ListItem()
       {
          super();
       }
       function UsesSubtitleSizeStorage()
       {
          return this.GameVariable == "SubtitleSize";
       }
       function NormalizeBoolState(InValue, DefaultValue)
       {
          if(InValue == undefined)
          {
             return DefaultValue;
          }
          return InValue != 0 ? 1 : 0;
       }
       function GetSubtitleDefaultState()
       {
          return 1;
       }
       function GetSubtitleDefaultBits()
       {
          var _loc2_ = this.NormalizeBoolState(flash.external.ExternalInterface.call("FE_GetDefaultControlType"),0);
          var _loc3_ = this.NormalizeBoolState(flash.external.ExternalInterface.call("FE_GetDefaultSixAxis"),0);
          return {ControlType:_loc2_,SixAxis:_loc3_};
       }
       function GetSubtitleBitsState()
       {
          var _loc2_ = this.GetSubtitleDefaultBits();
          var _loc3_ = this.NormalizeBoolState(flash.external.ExternalInterface.call("FE_GetControlType"),_loc2_.ControlType);
          var _loc4_ = this.NormalizeBoolState(flash.external.ExternalInterface.call("FE_GetSixAxis"),_loc2_.SixAxis);
          if(_loc4_ != _loc2_.SixAxis)
          {
             return 2;
          }
          if(_loc3_ != _loc2_.ControlType)
          {
             return 0;
          }
          return 1;
       }
       function ReadSubtitleSizeState()
       {
          return this.GetSubtitleBitsState();
       }
       function WriteSubtitleSizeState()
       {
          var _loc2_ = this.GetSubtitleDefaultBits();
          switch(this.State)
          {
             case 0:
                _loc2_.ControlType = 1 - _loc2_.ControlType;
                break;
             case 2:
                _loc2_.SixAxis = 1 - _loc2_.SixAxis;
          }
          flash.external.ExternalInterface.call("FE_SetControlType",_loc2_.ControlType,_loc2_.ControlType != 0 ? "$UI.Mode1" : "$UI.Mode0");
          flash.external.ExternalInterface.call("FE_SetSixAxis",_loc2_.SixAxis,_loc2_.SixAxis != 0 ? "$UI.Yes" : "$UI.No");
       }
       function AreSubtitlesEnabled()
       {
          return this.NormalizeBoolState(flash.external.ExternalInterface.call("FE_GetSubtitles"),1) != 0;
       }
       function IsDisabled()
       {
          return this.UsesSubtitleSizeStorage() && !this.AreSubtitlesEnabled();
       }
       function Init()
       {
          this.Names = new Array();
          this.GameVariable = arguments[0];
          if(this.UsesSubtitleSizeStorage())
          {
             this.LabelName = "Subtitle Size";
          }
          else
          {
             this.LabelName = "$UI." + arguments[0];
          }
          this.Label.text = this.LabelName;
          var _loc3_ = 1;
          while(_loc3_ < arguments.length)
          {
             this.Names.push(arguments[_loc3_]);
             _loc3_ = _loc3_ + 1;
          }
          var _loc4_;
          if(this.UsesSubtitleSizeStorage())
          {
             _loc4_ = this.ReadSubtitleSizeState();
          }
          else
          {
             _loc4_ = flash.external.ExternalInterface.call("FE_Get" + this.GameVariable);
          }
          if(_loc4_ == undefined)
          {
             _loc4_ = this.UsesSubtitleSizeStorage() ? this.GetSubtitleDefaultState() : 0;
          }
          if(_loc4_ < 0 || _loc4_ >= this.Names.length)
          {
             _loc4_ = this.UsesSubtitleSizeStorage() ? this.GetSubtitleDefaultState() : 0;
          }
          this.State = _loc4_;
          this.Initial = _loc4_;
          this.UpdateLRMarkers();
          this.FetchDefault();
       }
       function Destroy()
       {
          while(this.Names.length)
          {
             this.Names.pop();
          }
       }
       function HasChanged()
       {
          return this.State != this.Initial;
       }
       function RestoreInitialValue()
       {
          if(this.HasChanged())
          {
             this.State = this.Initial;
             this.UpdateLRMarkers();
          }
       }
       function IsDefault()
       {
          return this.State == this.Default;
       }
       function FetchDefault()
       {
          var _loc2_;
          if(this.UsesSubtitleSizeStorage())
          {
             _loc2_ = this.GetSubtitleDefaultState();
          }
          else
          {
             _loc2_ = flash.external.ExternalInterface.call("FE_GetDefault" + this.GameVariable);
          }
          if(_loc2_ == undefined)
          {
             _loc2_ = 0;
          }
          this.Default = _loc2_;
       }
       function SetDefault()
       {
          this.State = this.Default;
          this.UpdateLRMarkers();
       }
       function onRollOver()
       {
          this._parent.DoSetFocus(this);
       }
       function onPress()
       {
          this._parent.DoPress(this,true);
       }
       function onRelease()
       {
          this._parent.DoRelease(this,true,true);
       }
       function onReleaseOutside()
       {
          this._parent.DoRelease(this,true,false);
       }
       function Update()
       {
          this.UpdateLRMarkers();
       }
       function ShowPrompt()
       {
          var _loc3_ = _root.PromptManager;
          if(this._parent.BackScreen != "")
          {
             _loc3_.SetPrompt(_loc3_.CI_B,"$UI.Cancel",this._parent.myListener.onPromptClick,100,100);
          }
          if(this.Names.length > 1 && !this.IsDisabled())
          {
             _loc3_.SetPrompt(_loc3_.CI_Interact,"$UI.Cycle",this._parent.myListener.onPromptClick,100,100);
          }
       }
       function UpdateLRMarkersReal()
       {
          this.ItemText.text = this.Names[this.State];
          var _loc2_ = this.IsDisabled();
          this.ItemText._alpha = _loc2_ ? 40 : 100;
          this.Label._alpha = _loc2_ ? 40 : 100;
          if(this.IsDisabled())
          {
             this.LeftClicker._visible = false;
             this.RightClicker._visible = false;
          }
          else
          {
             if(this.State > 0)
             {
                this.LeftClicker._visible = true;
             }
             else
             {
                this.LeftClicker._visible = false;
             }
             if(this.State < this.Names.length - 1)
             {
                this.RightClicker._visible = true;
             }
             else
             {
                this.RightClicker._visible = false;
             }
          }
          if(this.UsesSubtitleSizeStorage())
          {
             if(!_loc2_)
             {
                this.WriteSubtitleSizeState();
             }
          }
          else
          {
             flash.external.ExternalInterface.call("FE_Set" + this.GameVariable,this.State,this.Names[this.State]);
          }
       }
       function UpdateLRMarkers()
       {
          this.UpdateLRMarkersReal();
       }
       function Increment()
       {
          if(this.IsDisabled())
          {
             return undefined;
          }
          if(this.State < this.Names.length - 1)
          {
             this.State += 1;
             this.UpdateLRMarkers();
             flash.external.ExternalInterface.call("FE_PlaySoundFromString","UI_FrontEndSFX.UI_Forward");
          }
       }
       function Decrement()
       {
          if(this.IsDisabled())
          {
             return undefined;
          }
          if(this.State > 0)
          {
             this.State -= 1;
             this.UpdateLRMarkers();
             flash.external.ExternalInterface.call("FE_PlaySoundFromString","UI_FrontEndSFX.UI_Back");
          }
       }
       function HasAction()
       {
          return false;
       }
       function Cycle()
       {
          if(this.IsDisabled())
          {
             return undefined;
          }
          if(this.Names.length == 1)
          {
             return undefined;
          }
          this.State += 1;
          if(this.State >= this.Names.length)
          {
             this.State = 0;
          }
          this.UpdateLRMarkers();
          flash.external.ExternalInterface.call("FE_PlaySoundFromString","UI_FrontEndSFX.UI_Forward");
       }
       function RunAction(bMouse)
       {
          if(this.IsDisabled())
          {
             return undefined;
          }
          if(this.Names.length < 3)
          {
             this.Cycle();
          }
          else if(bMouse)
          {
             if(this._xmouse < 0)
             {
                this.Decrement();
             }
             else
             {
                this.Increment();
             }
          }
          else
          {
             this.Cycle();
          }
       }
       function IsListButton()
       {
          return false;
       }
    }
    """;

    /// <summary>
    /// Replaces the frontend audio screen frame script so SubtitleSize becomes a dedicated fifth row.
    /// </summary>
    public const string FrontendAudioFrame1 = """
    function CancelScreen()
    {
       if(HaveOptionsChanged() == true)
       {
          flash.external.ExternalInterface.call("FE_TriggerOptionsSave");
       }
       ReturnFromScreen();
    }
    flash.external.ExternalInterface.call("FE_SetActiveScreenName","Options Audio");
    this.BackScreen = "OptionsMenu";
    this.BackScreenIndex = 1;
    this.FocusIndex = 0;
    this.Flags = this.FLAG_OPTIONS;
    this.Init();
    _root.TriggerEvent("Options");
    this.AddItem(Subtitles,4,1,-1,-1);
    this.AddItem(VolumeSFX,0,2,-1,-1);
    this.AddItem(VolumeMusic,1,3,-1,-1);
    this.AddItem(VolumeDialogue,2,4,-1,-1);
    this.AddItem(SubtitleSize,3,0,-1,-1);
    _rotation = -2;
    """;

    /// <summary>
    /// Initializes the dedicated frontend subtitle-size row on the cloned depth-61 template.
    /// </summary>
    public const string FrontendAudioSubtitleSizeClipAction = """
    onClipEvent(load){
       this.Init("SubtitleSize","Small","Normal","Large");
    }
    """;

    /// <summary>
    /// Replaces the HUD subtitle clip so runtime scaling uses the forced size settings.
    /// </summary>
    public const string HudSubtitle = """
    class rs.hud.Subtitle extends MovieClip
    {
       var Text;
       var onEnterFrame;
       static var Justify = Array("left","center","right","justify");
       static var FixedSide = Array("left","center","right");
       var bAlignBottom = false;
       function Subtitle()
       {
          super();
          this.Text.Text.wordWrap = true;
          this.Text._visible = false;
          this.Text._xscale = 100;
          this.Text._yscale = 100;
       }
       function SetAlignBottom(bInState)
       {
          this.bAlignBottom = bInState;
       }
       function GetForcedFontSize()
       {
          if(this._name == "InfoText")
          {
             return 34;
          }
          return 48;
       }
       function GetForcedScalePercent()
       {
          if(this._name == "InfoText")
          {
             return 175;
          }
          return 225;
       }
       function ApplyForcedScale()
       {
          if(this.Text == undefined)
          {
             return undefined;
          }
          var _loc2_ = this.GetForcedScalePercent();
          this.Text._xscale = _loc2_;
          this.Text._yscale = _loc2_;
          if(this.bAlignBottom)
          {
             this.Text._y = - this.Text._height;
          }
       }
       function SetText(InText, InSize, InJustify)
       {
          if(InText == "")
          {
             this.Text._visible = false;
             return undefined;
          }
          this.Text._visible = true;
          this.Text.Text.text = InText;
          this.Text.Text.autoSize = rs.hud.Subtitle.FixedSide[1];
          var _loc2_ = new TextFormat();
          _loc2_.align = rs.hud.Subtitle.Justify[InJustify];
          _loc2_.size = this.GetForcedFontSize();
          this.Text.Text.setNewTextFormat(_loc2_);
          this.Text.Text.setTextFormat(_loc2_);
          rs.misc.Utils.setTextShadow(this.Text.Text,1,1,15,30,1,0);
          this.ApplyForcedScale();
       }
       function GetJustificationName(Index)
       {
          return rs.hud.Subtitle.Justify[Index];
       }
    }
    """;

    /// <summary>
    /// Replaces the HUD frame script so forced subtitle appearance updates run every frame.
    /// </summary>
    public const string HudContentsFrame1 = """
    function ApplyForcedSubtitleInstance(Instance, InText, InJustify, FontSize, ScalePercent, bAlignBottom)
    {
       if(Instance == undefined || Instance.Text == undefined || Instance.Text.Text == undefined)
       {
          return undefined;
       }
       if(InText == "")
       {
          Instance.Text._visible = false;
          return undefined;
       }
       Instance.Text._visible = true;
       Instance.Text.Text.text = InText;
       Instance.Text.Text.autoSize = rs.hud.Subtitle.FixedSide[1];
       Instance.Text.Text.wordWrap = true;
       var _loc2_ = new TextFormat();
       _loc2_.align = rs.hud.Subtitle.Justify[InJustify];
       _loc2_.size = FontSize;
       Instance.Text.Text.setNewTextFormat(_loc2_);
       Instance.Text.Text.setTextFormat(_loc2_);
       rs.misc.Utils.setTextShadow(Instance.Text.Text,1,1,15,30,1,0);
       Instance.Text._xscale = ScalePercent;
       Instance.Text._yscale = ScalePercent;
       if(bAlignBottom)
       {
          Instance.Text._y = - Instance.Text._height;
       }
       else
       {
          Instance.Text._y = 0;
       }
    }
    function InstallForcedSubtitleOverrides()
    {
       _root.InfoText = InfoText;
    }
    function ForceRuntimeSubtitleAppearance(Instance, FontSize, ScalePercent)
    {
       if(Instance == undefined || Instance.Text == undefined || Instance.Text.Text == undefined)
       {
          return undefined;
       }
       var _loc5_ = Instance.Text.Text;
       if(_loc5_.text == undefined || _loc5_.text == "")
       {
          return undefined;
       }
       var _loc4_ = _loc5_.getTextFormat();
       if(_loc4_ == undefined)
       {
          _loc4_ = new TextFormat();
       }
       _loc4_.size = FontSize;
       _loc5_.setNewTextFormat(_loc4_);
       _loc5_.setTextFormat(_loc4_);
       _loc5_.wordWrap = true;
       rs.misc.Utils.setTextShadow(_loc5_,1,1,15,30,1,0);
       Instance.Text._visible = true;
       Instance.Text._xscale = ScalePercent;
       Instance.Text._yscale = ScalePercent;
       if(Instance.bAlignBottom)
       {
          Instance.Text._y = - Instance.Text._height;
       }
       else
       {
          Instance.Text._y = 0;
       }
    }
    function TickForcedSubtitleAppearance()
    {
       this.ForceRuntimeSubtitleAppearance(_root.Subtitles,84,350);
       this.ForceRuntimeSubtitleAppearance(InfoText,52,240);
    }
    function SetMainPromptAtY(YPos)
    {
       MainPrompt._y = YPos;
    }
    function SetZoomMode(bOn, LookUpAngle)
    {
       ZoomOn = bOn;
       ZoomHeadAngle = LookUpAngle;
    }
    function TickZoomMode()
    {
       var _loc1_;
       if(ZoomOn)
       {
          _loc1_ = Math.random() * 1.5;
          HeadLeft.Marker.Mark._y = ZoomHeadAngle / 90 * 320 + _loc1_;
          if(ZoomGizmo.Mode == 3)
          {
             ZoomGizmo.gotoAndPlay("Intro");
             HeadLeft.gotoAndPlay("Intro");
          }
       }
       else if(!ZoomOn && ZoomGizmo.Mode == 1)
       {
          ZoomGizmo.gotoAndPlay("Outro");
          HeadLeft.gotoAndPlay("Outro");
       }
    }
    function ShowCritical()
    {
       CriticalStrike.gotoAndPlay("Intro");
    }
    function ShowRiddle(bShow, Riddle)
    {
       if(bShow)
       {
          FixedRiddle.gotoAndPlay("Intro");
          FixedRiddle.Riddle.text = Riddle;
          FixedRiddle.Riddle.autoSize = "left";
          FixedRiddle.Riddle.wordWrap = true;
          rs.misc.Utils.setTextShadow(FixedRiddle.Riddle,1,1,15,30,1,0);
       }
       else
       {
          FixedRiddle.gotoAndPlay("Outro");
       }
    }
    function ShowAnyTimePrompt(bVisible, inX, inY, inAnimName, bRestart, inDepth)
    {
       if(AnytimeGrappleIcon.bAttached == undefined)
       {
          AnytimeGrappleIcon.bAttached = true;
          PromptManager.GetPageButton(AnytimeGrappleIcon.PromptAttach,8,undefined,false);
       }
       AnytimeGrappleIcon._visible = bVisible;
       AnytimeGrappleIcon._x = inX * Stage.width;
       AnytimeGrappleIcon._y = inY * Stage.height;
       AnytimeGrappleIcon.rendererFloat = inDepth;
       if(AnytimeGrappleIcon.AnimName != inAnimName || bRestart)
       {
          AnytimeGrappleIcon.gotoAndPlay(inAnimName);
          AnytimeGrappleIcon.AnimName = inAnimName;
       }
    }
    function SetClockTimer(Text, bVisible, bWarning)
    {
       ClockTimer.Content.Label.text = Text;
       rs.misc.Utils.setTextShadow(ClockTimer.Content.Label,1,1,15,30,1,0);
       ClockTimer.bVisState = bVisible;
       if(bVisible && ClockTimer.bIn == false)
       {
          ClockTimer.gotoAndPlay("Intro");
       }
       else if(!bVisible && ClockTimer.bIn == true)
       {
          ClockTimer.gotoAndPlay("Outro");
       }
       ClockTimer.Warning._visible = bWarning;
    }
    function TestHUD()
    {
       var _loc3_ = 1 + int(Math.random() * 39);
       if(Math.random() > 0.6)
       {
          Combo.Set(_loc3_,_loc3_ > 10);
       }
       else
       {
          Combo.Set(0,false);
       }
       var _loc4_ = "On entering the Quincy Sharp Intensive Treatment Wing, you can be sure to receive the very best treatment as long as you are fully committed to reform and rehabilitation. The island Cell block is full of ex- patients who resisted treatment.";
       Subtitles.SetText(_loc4_,23,1);
       InfoText.SetText("On entering the Quincy Sharp Intensive Treatment Wing.",20,1);
       PromptManager.ClearPrompts();
       PromptManager.SetPrompt(1,"[Accept]",undefined,100,100);
       PromptManager.SetPrompt(2,"Cancel",undefined,100,100);
       PromptManager.SetPrompt(55,"Crypto-Tune",undefined,100,100);
       PromptManager.ReLayout();
       SetClockTimer("XX:XX",Math.random() < 0.5,Math.random() < 0.5);
       if(Math.random() > 0.75)
       {
          PromptManager.SetMainPrompt(4,"Y Blah!",undefined,100,100);
       }
       else if(Math.random() > 0.5)
       {
          PromptManager.SetMainPrompt(55,"X Double Blah!",undefined,100,100);
       }
       HPXPBar.ShowHP(true);
       HPXPBar.ShowXP(true);
       HPXPBar.SetCurrentHP(int(Math.random() * 100),TestHPMax);
       var _loc2_ = int(Math.random() * 500);
       var _loc5_ = int(Math.random() * _loc2_);
       HPXPBar.SetCurrentXP(_loc5_,500);
       HPXPBar.SetTargetXP(_loc2_,500);
       HPXPBar.ShowLevelUp(Math.random() < 0.5);
       _root.RiddleList.push("Riddle 1");
       _root.RiddleList.push("Riddle 2 should go into this one which should wrap cleanly.");
       _root.RiddleList.push("Riddle 3 - blah");
       _root.RoomName.SetText("REMI\'s ROOM " + Math.random());
       if(Math.random() < 0.5)
       {
          SetZoomMode(true,Math.random() * 60);
       }
       else
       {
          SetZoomMode(false,0);
       }
       XPMessage.AddItem("Message Title","Here\'s the message...",3);
       XPMessage.AddItem("Another Title","Here\'s another message...",5);
       ShowCritical();
    }
    var AdjustTheseMovies = Array({Movie:PromptManager,bScale:false},{Movie:GadgetSelect,bScale:false},{Movie:AnytimeGrappleIcon,bScale:false},{Movie:HPXPBar,bScale:false},{Movie:XPMessage,bScale:false},{Movie:CriticalStrike,bScale:false},{Movie:Combo,bScale:false},{Movie:HeadLeft,bScale:false},{Movie:ClockTimer,bScale:false},{Movie:_root.Credits,bScale:false});
    ZoomGizmo.gotoAndPlay("Out");
    HeadLeft.gotoAndPlay("Out");
    XPMessage.gotoAndPlay("Out");
    CriticalStrike.gotoAndPlay("Out");
    FixedRiddle.gotoAndPlay("Out");
    ClockTimer.gotoAndPlay("Out");
    AnytimeGrappleIcon._visible = false;
    var ZoomOn = false;
    var ZoomHeadAngle = 0;
    var TestHP = 10;
    var TestHPMax = 100;
    InstallForcedSubtitleOverrides();
    this.onEnterFrame = function()
    {
       this.TickForcedSubtitleAppearance();
    };
    PromptManager.SetMode(false,true);
    """;
}
