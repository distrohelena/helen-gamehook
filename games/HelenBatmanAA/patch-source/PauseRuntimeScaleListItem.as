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

   function MapStoredSubtitleCodeToState(InValue)
   {
      switch(this.NormalizeIntState(InValue,4102))
      {
         case 4101:
            return 0;
         case 4103:
            return 2;
         default:
            return 1;
      }
   }

   function MapSubtitleStateToStoredCode(InValue)
   {
      switch(this.NormalizeIntState(InValue,this.GetSubtitleDefaultState()))
      {
         case 0:
            return 4101;
         case 2:
            return 4103;
         default:
            return 4102;
      }
   }

   function ReadSubtitleSizeState()
   {
      return this.MapStoredSubtitleCodeToState(flash.external.ExternalInterface.call("FE_GetControlType"));
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

   function WriteSubtitleSizeState()
   {
      flash.external.ExternalInterface.call("FE_SetControlType",this.MapSubtitleStateToStoredCode(this.State),"");
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
