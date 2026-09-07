namespace SubtitleSizeModBuilder;

/// <summary>
/// Produces the selective ActionScript shell used to expose the retail graphics-options screen.
/// Twelve declaratively described settings are connected to the frontend carrier, while Detail Level
/// is a shell-local derived preset over seven quality leaves and the stock layout remains stable.
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
    /// Initializes the graphics screen and its declarative settings controller. The controller
    /// reads a native-owned snapshot synchronously before enabling interaction, retaining its session for Apply.
    /// </summary>
    /// <param name="snapshot">The normalized user graphics snapshot retained by the builder contract.</param>
    /// <returns>The generated screen-frame ActionScript.</returns>
    public static string CreateScreenFrame1(BatmanGraphicsIniBootstrapSnapshot snapshot)
    {
        ArgumentNullException.ThrowIfNull(snapshot);
        return """
        class rs.ui.BatmanGraphicsOptionsController
        {
           var Screen;
           var Settings;
           var ActiveSettingsByRow;
           var DetailLeafRows;
           var InitializationIndex;
           var InitializationComplete;
           var InitializationFailed;
           /** Diagnostic for the current Fullscreen read attempt; empty until that read fails. */
           var FullscreenReadFailure;
           var ResolutionModes;
           var ResolutionInitialIndex;
           var ResolutionDraftIndex;
           var ResolutionCatalogFailure;
           var ResolutionCatalogLoading;
           var WindowedResolutionModes;
           var FullscreenResolutionModes;
           var WindowedResolutionAvailable;
           var FullscreenResolutionAvailable;
           var DesktopActualWidth;
           var DesktopActualHeight;
           var ResolutionInitialWidth;
           var ResolutionInitialHeight;
           var ResolutionDraftWidth;
           var ResolutionDraftHeight;
           var WindowedRememberedWidth;
           var WindowedRememberedHeight;
           var FullscreenRememberedWidth;
           var FullscreenRememberedHeight;
           var ResolutionCatalogKind;
           var ResolutionPersistedWidth;
           var ResolutionPersistedHeight;
           var ApplyInProgress;
           var InteractionBlocked;
           var UiStatus;
           var RollbackLocked;
           /** Native ownership identity retained after the read phase closes. */
           var SessionId;
           /** Whether the complete captured baseline permits a native transaction. */
           var NativeCanApply;
           function BatmanGraphicsOptionsController(screen)
           {
              this.Screen = screen;
              this.Settings = new Array();
              this.ActiveSettingsByRow = new Object();
              this.DetailLeafRows = new Array(6,7,8,9,10,11,12);
              this.InitializationIndex = 0;
              this.InitializationComplete = false;
              this.InitializationFailed = false;
              this.FullscreenReadFailure = "";
              this.ResetResolutionCatalogState();
              this.ApplyInProgress = false;
              this.InteractionBlocked = true;
              this.UiStatus = "";
              this.RollbackLocked = false;
              this.CreateSettings();
           }
           function ResetResolutionCatalogState()
           {
              this.ResolutionModes = new Array();
              this.ResolutionInitialIndex = -1;
              this.ResolutionDraftIndex = -1;
              this.ResolutionCatalogFailure = false;
              this.ResolutionCatalogLoading = false;
              this.WindowedResolutionModes = new Array();
              this.FullscreenResolutionModes = new Array();
              this.WindowedResolutionAvailable = false;
              this.FullscreenResolutionAvailable = false;
              this.DesktopActualWidth = undefined;
              this.DesktopActualHeight = undefined;
              this.ResolutionInitialWidth = undefined;
              this.ResolutionInitialHeight = undefined;
              this.ResolutionDraftWidth = undefined;
              this.ResolutionDraftHeight = undefined;
              this.WindowedRememberedWidth = undefined;
              this.WindowedRememberedHeight = undefined;
              this.FullscreenRememberedWidth = undefined;
              this.FullscreenRememberedHeight = undefined;
              this.ResolutionCatalogKind = -1;
              this.ResolutionPersistedWidth = undefined;
              this.ResolutionPersistedHeight = undefined;
           }
           function CreateSettings()
           {
              this.Settings = new Array(
                 {RowIndex:1,Name:"Fullscreen",Values:new Array("Windowed","Fullscreen"),ConfigValues:new Array(0,1),InitialIndex:-1,DraftIndex:-1},
                 {RowIndex:3,Name:"VSync",Values:new Array("Off","On"),ConfigValues:new Array(0,1),InitialIndex:-1,DraftIndex:-1},
                 {RowIndex:4,Name:"MSAA",Values:new Array("Off","2x","4x","8x","16x"),ConfigValues:new Array(0,1,2,3,5),InitialIndex:-1,DraftIndex:-1},
                 {RowIndex:6,Name:"Bloom",Values:new Array("Off","On"),ConfigValues:new Array(0,1),InitialIndex:-1,DraftIndex:-1},
                 {RowIndex:7,Name:"Dynamic Shadows",Values:new Array("Off","On"),ConfigValues:new Array(0,1),InitialIndex:-1,DraftIndex:-1},
                 {RowIndex:8,Name:"Motion Blur",Values:new Array("Off","On"),ConfigValues:new Array(0,1),InitialIndex:-1,DraftIndex:-1},
                 {RowIndex:9,Name:"Distortion",Values:new Array("Off","On"),ConfigValues:new Array(0,1),InitialIndex:-1,DraftIndex:-1},
                 {RowIndex:10,Name:"Fog Volumes",Values:new Array("Off","On"),ConfigValues:new Array(0,1),InitialIndex:-1,DraftIndex:-1},
                 {RowIndex:11,Name:"Spherical Harmonic Lighting",Values:new Array("Off","On"),ConfigValues:new Array(0,1),InitialIndex:-1,DraftIndex:-1},
                 {RowIndex:12,Name:"Ambient Occlusion",Values:new Array("Off","On"),ConfigValues:new Array(0,1),InitialIndex:-1,DraftIndex:-1},
                 {RowIndex:13,Name:"PhysX",Values:new Array("Off","Normal","High"),ConfigValues:new Array(0,1,2),InitialIndex:-1,DraftIndex:-1},
                 {RowIndex:14,Name:"Stereo 3D",Values:new Array("Off","On"),ConfigValues:new Array(0,1),InitialIndex:-1,DraftIndex:-1}
              );
              this.ActiveSettingsByRow = new Object();
              var settingIndex = 0;
              while(settingIndex < this.Settings.length)
              {
                 this.ActiveSettingsByRow[this.Settings[settingIndex].RowIndex] = this.Settings[settingIndex];
                 settingIndex = settingIndex + 1;
              }
           }
           function GetSettingForRow(rowIndex)
           {
              return this.ActiveSettingsByRow[rowIndex];
           }
           function GetDraftIndex(rowIndex)
           {
              if(rowIndex == 2)
              {
                 return this.ResolutionDraftIndex;
              }
              var setting = this.GetSettingForRow(rowIndex);
              if(setting == undefined)
              {
                 return -1;
              }
              return setting.DraftIndex;
           }
           function GetInitialIndex(rowIndex)
           {
              if(rowIndex == 2)
              {
                 return this.ResolutionInitialIndex;
              }
              var setting = this.GetSettingForRow(rowIndex);
              if(setting == undefined)
              {
                 return -1;
              }
              return setting.InitialIndex;
           }
           function GetDetailLevelDraftIndex()
           {
              var leafIndex = 0;
              while(leafIndex < this.DetailLeafRows.length)
              {
                 var setting = this.GetSettingForRow(this.DetailLeafRows[leafIndex]);
                 if(setting == undefined || setting.DraftIndex < 0 || setting.DraftIndex > 1)
                 {
                    return 4;
                 }
                 leafIndex = leafIndex + 1;
              }
              var isLow = true;
              var isMedium = true;
              var isHigh = true;
              var isVeryHigh = true;
              leafIndex = 0;
              while(leafIndex < this.DetailLeafRows.length)
              {
                 var draftSetting = this.GetSettingForRow(this.DetailLeafRows[leafIndex]);
                 if(draftSetting.DraftIndex != 0)
                 {
                    isLow = false;
                 }
                 if((leafIndex < 2 && draftSetting.DraftIndex != 1) || (leafIndex >= 2 && draftSetting.DraftIndex != 0))
                 {
                    isMedium = false;
                 }
                 if((leafIndex < 6 && draftSetting.DraftIndex != 1) || (leafIndex == 6 && draftSetting.DraftIndex != 0))
                 {
                    isHigh = false;
                 }
                 if(draftSetting.DraftIndex != 1)
                 {
                    isVeryHigh = false;
                 }
                 leafIndex = leafIndex + 1;
              }
              if(isLow)
              {
                 return 0;
              }
              if(isMedium)
              {
                 return 1;
              }
              if(isHigh)
              {
                 return 2;
              }
              if(isVeryHigh)
              {
                 return 3;
              }
              return 4;
           }
           function GetDetailLevelInitialIndex()
           {
              var leafIndex = 0;
              while(leafIndex < this.DetailLeafRows.length)
              {
                 var setting = this.GetSettingForRow(this.DetailLeafRows[leafIndex]);
                 if(setting == undefined || setting.InitialIndex < 0 || setting.InitialIndex > 1)
                 {
                    return 4;
                 }
                 leafIndex = leafIndex + 1;
              }
              var isLow = true;
              var isMedium = true;
              var isHigh = true;
              var isVeryHigh = true;
              leafIndex = 0;
              while(leafIndex < this.DetailLeafRows.length)
              {
                 var initialSetting = this.GetSettingForRow(this.DetailLeafRows[leafIndex]);
                 if(initialSetting.InitialIndex != 0)
                 {
                    isLow = false;
                 }
                 if((leafIndex < 2 && initialSetting.InitialIndex != 1) || (leafIndex >= 2 && initialSetting.InitialIndex != 0))
                 {
                    isMedium = false;
                 }
                 if((leafIndex < 6 && initialSetting.InitialIndex != 1) || (leafIndex == 6 && initialSetting.InitialIndex != 0))
                 {
                    isHigh = false;
                 }
                 if(initialSetting.InitialIndex != 1)
                 {
                    isVeryHigh = false;
                 }
                 leafIndex = leafIndex + 1;
              }
              if(isLow)
              {
                 return 0;
              }
              if(isMedium)
              {
                 return 1;
              }
              if(isHigh)
              {
                 return 2;
              }
              if(isVeryHigh)
              {
                 return 3;
              }
              return 4;
           }
           function CanEditDetailLevel()
           {
              if(!this.AreSettingsLoaded() || this.InitializationFailed || this.InteractionBlocked || this.RollbackLocked)
              {
                 return false;
              }
              var leafIndex = 0;
              while(leafIndex < this.DetailLeafRows.length)
              {
                 var setting = this.GetSettingForRow(this.DetailLeafRows[leafIndex]);
                 if(setting == undefined || setting.InitialIndex < 0 || setting.DraftIndex < 0)
                 {
                    return false;
                 }
                 leafIndex = leafIndex + 1;
              }
              return true;
           }
           function SetDetailPreset(index,forward)
           {
              if((index != 0 && index != 1 && index != 2 && index != 3) || !this.CanEditDetailLevel())
              {
                 return undefined;
              }
              if(forward)
              {
                 flash.external.ExternalInterface.call("FE_PlaySoundFromString","UI_FrontEndSFX.UI_Forward");
              }
              else
              {
                 flash.external.ExternalInterface.call("FE_PlaySoundFromString","UI_FrontEndSFX.UI_Back");
              }
              var leafIndex = 0;
              while(leafIndex < this.DetailLeafRows.length)
              {
                 var targetIndex = 0;
                 if((index == 1 && leafIndex < 2) || (index >= 2 && leafIndex < 6) || (index == 3 && leafIndex == 6))
                 {
                    targetIndex = 1;
                 }
                 this.GetSettingForRow(this.DetailLeafRows[leafIndex]).DraftIndex = targetIndex;
                 leafIndex = leafIndex + 1;
              }
              this.RefreshRows();
           }
           function ToggleDetailPreset()
           {
              if(!this.CanEditDetailLevel())
              {
                 return undefined;
              }
              var currentIndex = this.GetDetailLevelDraftIndex();
              var targetIndex = 0;
              if(currentIndex >= 0 && currentIndex < 3)
              {
                 targetIndex = currentIndex + 1;
              }
              this.SetDetailPreset(targetIndex,true);
           }
           function IncrementDetailPreset()
           {
              if(!this.CanEditDetailLevel())
              {
                 return undefined;
              }
              var currentIndex = this.GetDetailLevelDraftIndex();
              if(currentIndex < 0 || currentIndex >= 3)
              {
                 return undefined;
              }
              this.SetDetailPreset(currentIndex + 1,true);
           }
           function DecrementDetailPreset()
           {
              if(!this.CanEditDetailLevel())
              {
                 return undefined;
              }
              var currentIndex = this.GetDetailLevelDraftIndex();
              var targetIndex = currentIndex - 1;
              if(currentIndex == 4)
              {
                 targetIndex = 3;
              }
              if(targetIndex < 0)
              {
                 return undefined;
              }
              this.SetDetailPreset(targetIndex,false);
           }
           /** Explains a failed Fullscreen read without replacing unavailable values with defaults. */
           function GetUnavailableLabel(rowIndex)
           {
              if(rowIndex == 1 && this.FullscreenReadFailure != "")
              {
                 return this.FullscreenReadFailure;
              }
              return "Unavailable";
           }
           function IsUnavailable(rowIndex)
           {
              if(rowIndex == 2)
              {
                 return this.InitializationComplete && (this.ResolutionModes.length == 0 || this.ResolutionDraftIndex < 0);
              }
              if(!this.AreSettingsLoaded())
              {
                 return false;
              }
              var setting = this.GetSettingForRow(rowIndex);
              return setting == undefined || setting.InitialIndex < 0;
           }
           /** Reports whether synchronous scalar transfer has finished, including individually unavailable values. */
           function AreSettingsLoaded()
           {
              return this.InitializationIndex >= this.Settings.length && !this.InitializationFailed;
           }
           function GetResolutionLabel()
           {
              if(this.ResolutionDraftIndex < 0 || this.ResolutionDraftIndex >= this.ResolutionModes.length)
              {
                 if(this.ResolutionDraftWidth != undefined && this.ResolutionDraftHeight != undefined)
                 {
                    return this.ResolutionDraftWidth + " x " + this.ResolutionDraftHeight;
                 }
                 return "Unavailable";
              }
              return this.ResolutionModes[this.ResolutionDraftIndex].Label;
           }
           function GetResolutionDraftIndex()
           {
              return this.ResolutionDraftIndex;
           }
           function GetResolutionInitialIndex()
           {
              return this.ResolutionInitialIndex;
           }
           function CanEditResolution()
           {
              var fullscreenSetting = this.GetSettingForRow(1);
              var catalogAvailable = fullscreenSetting != undefined && (fullscreenSetting.DraftIndex == 0 ? this.WindowedResolutionAvailable : this.FullscreenResolutionAvailable);
              return this.InitializationComplete && !this.InitializationFailed && !this.InteractionBlocked && !this.RollbackLocked && catalogAvailable && this.ResolutionDraftIndex >= 0 && this.ResolutionDraftIndex < this.ResolutionModes.length;
           }
           function CanIncrementResolution()
           {
              return this.CanEditResolution() && this.ResolutionDraftIndex < this.ResolutionModes.length - 1;
           }
           function CanDecrementResolution()
           {
              return this.CanEditResolution() && this.ResolutionDraftIndex > 0;
           }
           function SetResolutionDraftIndex(index,forward)
           {
              if(!this.CanEditResolution() || index < 0 || index >= this.ResolutionModes.length || index == this.ResolutionDraftIndex)
              {
                 return undefined;
              }
              if(forward)
              {
                 flash.external.ExternalInterface.call("FE_PlaySoundFromString","UI_FrontEndSFX.UI_Forward");
              }
              else
              {
                 flash.external.ExternalInterface.call("FE_PlaySoundFromString","UI_FrontEndSFX.UI_Back");
              }
              this.ResolutionDraftIndex = index;
              this.ResolutionDraftWidth = this.ResolutionModes[index].Width;
              this.ResolutionDraftHeight = this.ResolutionModes[index].Height;
              this.RefreshRows();
           }
           function RestoreResolutionInitial()
           {
              if(!this.CanEditResolution() || (this.ResolutionDraftWidth == this.ResolutionInitialWidth && this.ResolutionDraftHeight == this.ResolutionInitialHeight))
              {
                 return undefined;
              }
              this.ResolutionDraftIndex = this.FindResolutionModeIndex(this.ResolutionModes,this.ResolutionInitialWidth,this.ResolutionInitialHeight);
              this.ResolutionDraftWidth = this.ResolutionInitialWidth;
              this.ResolutionDraftHeight = this.ResolutionInitialHeight;
              this.RefreshRows();
           }
           function ToggleResolution()
           {
              if(!this.CanEditResolution())
              {
                 return undefined;
              }
              var nextIndex = this.ResolutionDraftIndex + 1;
              if(nextIndex >= this.ResolutionModes.length)
              {
                 nextIndex = 0;
              }
              this.SetResolutionDraftIndex(nextIndex,true);
           }
           function IncrementResolution()
           {
              if(!this.CanEditResolution() || this.ResolutionDraftIndex >= this.ResolutionModes.length - 1)
              {
                 return undefined;
              }
              this.SetResolutionDraftIndex(this.ResolutionDraftIndex + 1,true);
           }
           function DecrementResolution()
           {
              if(!this.CanEditResolution() || this.ResolutionDraftIndex <= 0)
              {
                 return undefined;
              }
              this.SetResolutionDraftIndex(this.ResolutionDraftIndex - 1,false);
           }
           function IsDirty()
           {
              if(!this.InitializationComplete || this.InitializationFailed)
              {
                 return false;
              }
              var settingIndex = 0;
              while(settingIndex < this.Settings.length)
              {
                 if(this.Settings[settingIndex].DraftIndex != this.Settings[settingIndex].InitialIndex)
                 {
                    return true;
                 }
                 settingIndex = settingIndex + 1;
              }
              if(this.ResolutionDraftWidth != this.ResolutionInitialWidth || this.ResolutionDraftHeight != this.ResolutionInitialHeight)
              {
                 return true;
              }
              return false;
           }
           function CanEdit(rowIndex)
           {
              var setting = this.GetSettingForRow(rowIndex);
              return setting != undefined && this.AreSettingsLoaded() && (rowIndex != 1 || this.InitializationComplete) && !this.InitializationFailed && !this.InteractionBlocked && !this.RollbackLocked && setting.InitialIndex >= 0 && setting.DraftIndex >= 0;
           }
           function CanApply()
           {
              return this.NativeCanApply && this.InitializationComplete && !this.InitializationFailed && !this.ApplyInProgress && !this.InteractionBlocked && !this.RollbackLocked && this.IsDirty();
           }
           /** Captures one native snapshot synchronously and ends only its read phase. */
           function BeginInitialization()
           {
              this.FullscreenReadFailure = "";
              this.InitializationIndex = 0;
              this.ResetResolutionCatalogState();
              this.InitializationComplete = false;
              this.InitializationFailed = false;
              this.ApplyInProgress = false;
              this.InteractionBlocked = true;
              this.RollbackLocked = false;
              this.NativeCanApply = false;
              this.UiStatus = "";
              this.Screen.BlockInput(true);
              this.SessionId = flash.external.ExternalInterface.call("Helen_Graphics_OpenV1");
              if(!this.IsUnsignedInteger(this.SessionId,4294967295) || this.SessionId == 0)
              {
                 this.SessionId = undefined;
                 this.FailInitialization();
                 return undefined;
              }
              var readEnded = false;
              var completeValues = true;
              try
              {
                 var settingIndex = 0;
                 while(settingIndex < this.Settings.length)
                 {
                    var setting = this.Settings[settingIndex];
                    var value = flash.external.ExternalInterface.call("Helen_Graphics_GetV1",this.SessionId,settingIndex);
                    var index = this.FindConfigValueIndex(setting,value);
                    setting.InitialIndex = index;
                    setting.DraftIndex = index;
                    if(index < 0) { completeValues = false; }
                    settingIndex = settingIndex + 1;
                 }
                 var widthValue = flash.external.ExternalInterface.call("Helen_Graphics_GetV1",this.SessionId,12);
                 var heightValue = flash.external.ExternalInterface.call("Helen_Graphics_GetV1",this.SessionId,13);
                 if(this.IsPositiveDimension(widthValue) && this.IsPositiveDimension(heightValue))
                 {
                    this.ResolutionPersistedWidth = widthValue;
                    this.ResolutionPersistedHeight = heightValue;
                 }
                 else { completeValues = false; }
                 widthValue = flash.external.ExternalInterface.call("Helen_Graphics_GetV1",this.SessionId,14);
                 heightValue = flash.external.ExternalInterface.call("Helen_Graphics_GetV1",this.SessionId,15);
                 if(this.IsPositiveDimension(widthValue) && this.IsPositiveDimension(heightValue))
                 {
                    this.DesktopActualWidth = widthValue;
                    this.DesktopActualHeight = heightValue;
                 }
                 var windowedModes = this.ReadDirectCatalog(0);
                 var fullscreenModes = this.ReadDirectCatalog(1);
                 this.WindowedResolutionAvailable = windowedModes != undefined;
                 this.FullscreenResolutionAvailable = fullscreenModes != undefined;
                 if(this.WindowedResolutionAvailable) { this.WindowedResolutionModes = windowedModes; }
                 if(this.FullscreenResolutionAvailable) { this.FullscreenResolutionModes = fullscreenModes; }
                 var canApply = flash.external.ExternalInterface.call("Helen_Graphics_GetV1",this.SessionId,16);
                 this.NativeCanApply = completeValues && canApply === 1;
              }
              catch(error)
              {
                 this.NativeCanApply = false;
                 this.UiStatus = "Read Failed";
              }
              finally
              {
                 try
                 {
                    readEnded = flash.external.ExternalInterface.call("Helen_Graphics_EndReadV1",this.SessionId) === true;
                 }
                 catch(endReadError)
                 {
                    readEnded = false;
                 }
              }
              if(!readEnded)
              {
                 this.NativeCanApply = false;
                 this.UiStatus = "Read Failed";
              }
              this.InitializationIndex = this.Settings.length;
              this.FinishResolutionInitialization();
           }
           /** Rejects coercion, fractional numbers and values outside the wire range. */
           function IsUnsignedInteger(value,maximum)
           {
              return typeof value == "number" && isFinite(value) && value >= 0 && value <= maximum && Math.floor(value) == value;
           }
           /** Accepts only positive signed engine dimensions. */
           function IsPositiveDimension(value)
           {
              return this.IsUnsignedInteger(value,2147483647) && value > 0;
           }
           /** Maps normalized config values to display indices without inventing defaults. */
           function FindConfigValueIndex(setting,value)
           {
              if(!this.IsUnsignedInteger(value,2147483647)) { return -1; }
              var index = 0;
              while(index < setting.ConfigValues.length)
              {
                 if(setting.ConfigValues[index] === value) { return index; }
                 index = index + 1;
              }
              return -1;
           }
           /** Reads the immutable native catalog in its original index order. */
           function ReadDirectCatalog(kind)
           {
              var count = flash.external.ExternalInterface.call("Helen_Graphics_ModeCountV1",this.SessionId,kind);
              if(!this.IsUnsignedInteger(count,98) || count == 0) { return undefined; }
              var modes = new Array();
              var index = 0;
              while(index < count)
              {
                 var widthValue = flash.external.ExternalInterface.call("Helen_Graphics_ModeWidthV1",this.SessionId,kind,index);
                 var heightValue = flash.external.ExternalInterface.call("Helen_Graphics_ModeHeightV1",this.SessionId,kind,index);
                 if(!this.IsPositiveDimension(widthValue) || !this.IsPositiveDimension(heightValue) || this.FindResolutionModeIndex(modes,widthValue,heightValue) >= 0)
                 {
                    return undefined;
                 }
                 modes.push({Width:widthValue,Height:heightValue,Label:widthValue + " x " + heightValue});
                 index = index + 1;
              }
              return modes;
           }
           function CompleteInitialization()
           {
              this.InitializationComplete = true;
              this.InitializationFailed = false;
              this.InteractionBlocked = false;
              this.Screen.BlockInput(false);
              this.RefreshRows();
           }
           function FailInitialization()
           {
              var settingIndex = 0;
              while(settingIndex < this.Settings.length)
              {
                 this.Settings[settingIndex].InitialIndex = -1;
                 this.Settings[settingIndex].DraftIndex = -1;
                 settingIndex = settingIndex + 1;
              }
              this.ResetResolutionCatalogState();
              this.InitializationComplete = false;
              this.InitializationFailed = true;
              this.ApplyInProgress = false;
              this.InteractionBlocked = true;
              this.Screen.BlockInput(false);
              this.RefreshRows();
           }
           function BeginResolutionCatalogLoad(targetIndex)
           {
              if(targetIndex != 0 && targetIndex != 1)
              {
                 return undefined;
              }
              this.SelectResolutionKind(targetIndex);
           }
           function FinishResolutionInitialization()
           {
              this.ResolutionCatalogLoading = false;
              this.ResolutionInitialWidth = this.ResolutionPersistedWidth;
              this.ResolutionInitialHeight = this.ResolutionPersistedHeight;
              this.ResolutionDraftWidth = this.ResolutionPersistedWidth;
              this.ResolutionDraftHeight = this.ResolutionPersistedHeight;
              if(this.Settings[0].InitialIndex >= 0)
              {
                 this.ResolutionModes = this.Settings[0].InitialIndex == 0 ? this.WindowedResolutionModes : this.FullscreenResolutionModes;
                 var initialModeIndex = this.FindResolutionModeIndex(this.ResolutionModes,this.ResolutionDraftWidth,this.ResolutionDraftHeight);
                 if(initialModeIndex >= 0)
                 {
                    this.ResolutionInitialIndex = initialModeIndex;
                    this.ResolutionDraftIndex = initialModeIndex;
                 }
                 else
                 {
                    this.ResolutionInitialIndex = -1;
                    this.ResolutionDraftIndex = -1;
                 }
              }
              if(this.Settings[0].InitialIndex == 0)
              {
                 this.WindowedRememberedWidth = this.ResolutionDraftWidth;
                 this.WindowedRememberedHeight = this.ResolutionDraftHeight;
              }
              else
              {
                 this.FullscreenRememberedWidth = this.ResolutionDraftWidth;
                 this.FullscreenRememberedHeight = this.ResolutionDraftHeight;
              }
              this.CompleteInitialization();
           }
           function SelectResolutionKind(targetIndex)
           {
              var previousModes = this.ResolutionModes;
              var previousIndex = this.ResolutionDraftIndex;
              var previousWidth = this.ResolutionDraftWidth;
              var previousHeight = this.ResolutionDraftHeight;
              var targetModes = targetIndex == 0 ? this.WindowedResolutionModes : this.FullscreenResolutionModes;
              var targetAvailable = targetIndex == 0 ? this.WindowedResolutionAvailable : this.FullscreenResolutionAvailable;
              if(!targetAvailable)
              {
                 this.ResolutionCatalogFailure = true;
                 this.RefreshRows();
                 return false;
              }
              if(this.ResolutionDraftWidth != undefined && this.ResolutionDraftHeight != undefined)
              {
                 if(targetIndex == 0)
                 {
                    this.FullscreenRememberedWidth = this.ResolutionDraftWidth;
                    this.FullscreenRememberedHeight = this.ResolutionDraftHeight;
                 }
                 else
                 {
                    this.WindowedRememberedWidth = this.ResolutionDraftWidth;
                    this.WindowedRememberedHeight = this.ResolutionDraftHeight;
                 }
              }
              this.ResolutionModes = targetModes;
              var rememberedWidth = targetIndex == 0 ? this.WindowedRememberedWidth : this.FullscreenRememberedWidth;
              var rememberedHeight = targetIndex == 0 ? this.WindowedRememberedHeight : this.FullscreenRememberedHeight;
              if(rememberedWidth == undefined)
              {
                 rememberedWidth = this.ResolutionDraftWidth;
                 rememberedHeight = this.ResolutionDraftHeight;
              }
              var targetModeIndex = this.FindResolutionModeIndex(targetModes,rememberedWidth,rememberedHeight);
              if(targetModeIndex < 0)
              {
                 targetModeIndex = this.FindResolutionModeIndex(targetModes,this.DesktopActualWidth,this.DesktopActualHeight);
                 if(targetModeIndex < 0)
                 {
                    this.ResolutionModes = previousModes;
                    this.ResolutionDraftIndex = previousIndex;
                    this.ResolutionDraftWidth = previousWidth;
                    this.ResolutionDraftHeight = previousHeight;
                    this.RefreshRows();
                    return false;
                 }
                 rememberedWidth = this.DesktopActualWidth;
                 rememberedHeight = this.DesktopActualHeight;
              }
              this.ResolutionDraftIndex = targetModeIndex;
              this.ResolutionDraftWidth = rememberedWidth;
              this.ResolutionDraftHeight = rememberedHeight;
              this.ResolutionCatalogFailure = false;
              this.RefreshRows();
              return true;
           }
           function FindResolutionModeIndex(modes,widthValue,heightValue)
           {
              if(widthValue == undefined || heightValue == undefined)
              {
                 return -1;
              }
              var modeIndex = 0;
              while(modeIndex < modes.length)
              {
                 if(modes[modeIndex].Width == widthValue && modes[modeIndex].Height == heightValue)
                 {
                    return modeIndex;
                 }
                 modeIndex = modeIndex + 1;
              }
              return -1;
           }
           function SetDraftIndex(rowIndex,index,forward)
           {
              var setting = this.GetSettingForRow(rowIndex);
              if(setting == undefined || !this.CanEdit(rowIndex) || index < 0 || index >= setting.Values.length || index == setting.DraftIndex)
              {
                 return undefined;
              }
              if(forward)
              {
                 flash.external.ExternalInterface.call("FE_PlaySoundFromString","UI_FrontEndSFX.UI_Forward");
              }
              else
              {
                 flash.external.ExternalInterface.call("FE_PlaySoundFromString","UI_FrontEndSFX.UI_Back");
              }
              var previousIndex = setting.DraftIndex;
              setting.DraftIndex = index;
              if(rowIndex == 1)
              {
                 if(!this.SelectResolutionKind(index))
                 {
                    setting.DraftIndex = previousIndex;
                    return undefined;
                 }
              }
              this.RefreshRows();
           }
           function ToggleSetting(rowIndex)
           {
              var setting = this.GetSettingForRow(rowIndex);
              if(setting == undefined || !this.CanEdit(rowIndex))
              {
                 return undefined;
              }
              var nextIndex = setting.DraftIndex + 1;
              if(nextIndex >= setting.Values.length)
              {
                 nextIndex = 0;
              }
              this.SetDraftIndex(rowIndex,nextIndex,true);
           }
           function IncrementSetting(rowIndex)
           {
              var setting = this.GetSettingForRow(rowIndex);
              if(setting == undefined || !this.CanEdit(rowIndex) || setting.DraftIndex >= setting.Values.length - 1)
              {
                 return undefined;
              }
              this.SetDraftIndex(rowIndex,setting.DraftIndex + 1,true);
           }
           function DecrementSetting(rowIndex)
           {
              var setting = this.GetSettingForRow(rowIndex);
              if(setting == undefined || !this.CanEdit(rowIndex) || setting.DraftIndex <= 0)
              {
                 return undefined;
              }
              this.SetDraftIndex(rowIndex,setting.DraftIndex - 1,false);
           }
           function RefreshRows()
           {
              if(this.Screen.GraphicsRow1 != undefined) { this.Screen.GraphicsRow1.Update(); }
              if(this.Screen.GraphicsRow2 != undefined) { this.Screen.GraphicsRow2.Update(); }
              if(this.Screen.GraphicsRow3 != undefined) { this.Screen.GraphicsRow3.Update(); }
              if(this.Screen.GraphicsRow4 != undefined) { this.Screen.GraphicsRow4.Update(); }
              if(this.Screen.GraphicsRow5 != undefined) { this.Screen.GraphicsRow5.Update(); }
              if(this.Screen.GraphicsRow6 != undefined) { this.Screen.GraphicsRow6.Update(); }
              if(this.Screen.GraphicsRow7 != undefined) { this.Screen.GraphicsRow7.Update(); }
              if(this.Screen.GraphicsRow8 != undefined) { this.Screen.GraphicsRow8.Update(); }
              if(this.Screen.GraphicsRow9 != undefined) { this.Screen.GraphicsRow9.Update(); }
              if(this.Screen.GraphicsRow10 != undefined) { this.Screen.GraphicsRow10.Update(); }
              if(this.Screen.GraphicsRow11 != undefined) { this.Screen.GraphicsRow11.Update(); }
              if(this.Screen.GraphicsRow12 != undefined) { this.Screen.GraphicsRow12.Update(); }
              if(this.Screen.GraphicsRow13 != undefined) { this.Screen.GraphicsRow13.Update(); }
              if(this.Screen.GraphicsRow14 != undefined) { this.Screen.GraphicsRow14.Update(); }
              if(this.Screen.GraphicsRow15 != undefined) { this.Screen.GraphicsRow15.Update(); }
              this.Screen.ReUpdate();
           }
           /** Stages normalized fields and consumes one synchronous native commit outcome. */
           function ApplyChanges()
           {
              if(!this.CanApply()) { return undefined; }
              this.ApplyInProgress = true;
              this.InteractionBlocked = true;
              this.UiStatus = "Applying...";
              this.Screen.BlockInput(true);
              this.RefreshRows();
              var transactionId = undefined;
              var commitInvoked = false;
              try
              {
                 transactionId = flash.external.ExternalInterface.call("Helen_Graphics_BeginApplyV1",this.SessionId);
                 if(!this.IsUnsignedInteger(transactionId,4294967295) || transactionId == 0)
                 {
                    transactionId = undefined;
                    this.UiStatus = "Apply Rejected";
                    return undefined;
                 }
                 var staged = true;
                 var settingIndex = 0;
                 while(staged && settingIndex < this.Settings.length)
                 {
                    var setting = this.Settings[settingIndex];
                    staged = flash.external.ExternalInterface.call("Helen_Graphics_SetFieldV1",this.SessionId,transactionId,settingIndex,setting.ConfigValues[setting.DraftIndex]) === true;
                    settingIndex = settingIndex + 1;
                 }
                 if(staged && (this.Settings[0].DraftIndex != this.Settings[0].InitialIndex || this.ResolutionDraftWidth != this.ResolutionInitialWidth || this.ResolutionDraftHeight != this.ResolutionInitialHeight))
                 {
                    staged = this.ResolutionDraftIndex >= 0 && flash.external.ExternalInterface.call("Helen_Graphics_SetResolutionV1",this.SessionId,transactionId,this.Settings[0].DraftIndex,this.ResolutionDraftIndex) === true;
                 }
                 if(!staged)
                 {
                    this.UiStatus = "Apply Rejected";
                    return undefined;
                 }
                 commitInvoked = true;
                 var outcome = flash.external.ExternalInterface.call("Helen_Graphics_CommitV1",this.SessionId,transactionId);
                 if(outcome === 0 || outcome === 3)
                 {
                    this.CopyDraftToInitial();
                    this.UiStatus = outcome === 3 ? "Applied - Cleanup Failed" : "";
                 }
                 else if(outcome === 1) { this.UiStatus = "Apply Failed"; }
                 else if(outcome === 4) { this.LockUncertainApply(); this.UiStatus = "Apply Partially Saved - Session Sync Failed"; }
                 else { this.LockUncertainApply(); }
              }
              catch(error)
              {
                 if(commitInvoked) { this.LockUncertainApply(); }
                 else { this.UiStatus = "Apply Rejected"; }
              }
              finally
              {
                 if(transactionId != undefined && !commitInvoked)
                 {
                    if(flash.external.ExternalInterface.call("Helen_Graphics_CancelApplyV1",this.SessionId,transactionId) !== true)
                    {
                       this.NativeCanApply = false;
                       this.UiStatus = "Cancel Failed";
                    }
                 }
                 this.ApplyInProgress = false;
                 this.InteractionBlocked = false;
                 this.Screen.BlockInput(false);
                 this.RefreshRows();
              }
           }
           /** Prevents retry when publication integrity has not been established. */
           function LockUncertainApply()
           {
              this.NativeCanApply = false;
              this.RollbackLocked = true;
              this.UiStatus = "Apply Integrity Uncertain";
           }
           function CopyDraftToInitial()
           {
              var settingIndex = 0;
              while(settingIndex < this.Settings.length)
              {
                 this.Settings[settingIndex].InitialIndex = this.Settings[settingIndex].DraftIndex;
                 settingIndex = settingIndex + 1;
              }
              this.ResolutionInitialIndex = this.ResolutionDraftIndex;
              this.ResolutionInitialWidth = this.ResolutionDraftWidth;
              this.ResolutionInitialHeight = this.ResolutionDraftHeight;
              this.ResolutionPersistedWidth = this.ResolutionDraftWidth;
              this.ResolutionPersistedHeight = this.ResolutionDraftHeight;
           }
           function GetApplyStatusText()
           {
              return this.UiStatus;
           }
           /** Closes native ownership before discarding the local draft or leaving the screen. */
           function Destroy()
           {
              if(this.ApplyInProgress) { return false; }
              if(this.SessionId != undefined)
              {
                 if(flash.external.ExternalInterface.call("Helen_Graphics_CloseV1",this.SessionId) !== true) { return false; }
                 this.SessionId = undefined;
              }
              this.NativeCanApply = false;
              this.InteractionBlocked = true;
              this.Screen.BlockInput(false);
              return true;
           }
        }
        function CancelScreen()
        {
           if(this.GraphicsOptionsController != undefined)
           {
              if(!this.GraphicsOptionsController.Destroy()) { return undefined; }
           }
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
        this.GraphicsOptionsController = new rs.ui.BatmanGraphicsOptionsController(this);
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
        this.GraphicsOptionsController.BeginInitialization();
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
    /// Creates all fifteen row scripts in visual and navigation order, injecting twelve transmitted
    /// settings plus the derived Detail Level controller while retaining fixed inactive rows.
    /// </summary>
    /// <param name="snapshot">The normalized user graphics snapshot retained by the builder contract.</param>
    /// <returns>The fifteen row clip-action scripts.</returns>
    public static string[] CreateRowClipActions(BatmanGraphicsIniBootstrapSnapshot snapshot)
    {
        ArgumentNullException.ThrowIfNull(snapshot);
        return
        [
            CreateActiveRowClipAction("Fullscreen", 1, ["Windowed", "Fullscreen"]),
            CreateResolutionRowClipAction(),
            CreateActiveRowClipAction("VSync", 3, ["Off", "On"]),
            CreateActiveRowClipAction("MSAA", 4, ["Off", "2x", "4x", "8x", "16x"]),
            CreateDetailLevelRowClipAction(),
            CreateActiveRowClipAction("Bloom", 6, ["Off", "On"]),
            CreateActiveRowClipAction("Dynamic Shadows", 7, ["Off", "On"]),
            CreateActiveRowClipAction("Motion Blur", 8, ["Off", "On"]),
            CreateActiveRowClipAction("Distortion", 9, ["Off", "On"]),
            CreateActiveRowClipAction("Fog Volumes", 10, ["Off", "On"]),
            CreateActiveRowClipAction("Spherical Harmonic Lighting", 11, ["Off", "On"]),
            CreateActiveRowClipAction("Ambient Occlusion", 12, ["Off", "On"]),
            CreateActiveRowClipAction("PhysX", 13, ["Off", "Normal", "High"]),
            CreateActiveRowClipAction("Stereo 3D", 14, ["Off", "On"]),
            CreateApplyRowClipAction()
        ];
    }

    /// <summary>
    /// Creates the derived Detail Level row. Its display is computed from the seven quality leaves,
    /// and all user actions delegate to the controller's preset methods without transport calls.
    /// </summary>
    /// <returns>An ActionScript load handler for the derived detail preset row.</returns>
    private static string CreateDetailLevelRowClipAction()
    {
        return $$"""
        onClipEvent(load){
           this.LabelName = "Detail Level";
           this.Names = new Array("Low","Medium","High","Very High","Custom");
           this.State = -1;
           this.Initial = -1;
           this.Default = -1;
           this.Update = function()
           {
              if(this.Label != undefined && this.Label.Label != undefined && this.Label.Label.Text != undefined)
              {
                 this.Label.Label.Text.text = "Detail Level";
              }
              else if(this.Label != undefined && this.Label.Text != undefined)
              {
                 this.Label.Text.text = "Detail Level";
              }
              else if(this.Label != undefined)
              {
                 this.Label.text = "Detail Level";
              }
              if(_parent.GraphicsOptionsController == undefined)
              {
                 if(this.ItemText != undefined)
                 {
                    this.ItemText.text = "Loading...";
                 }
                 if(this.LeftClicker != undefined)
                 {
                    this.LeftClicker._visible = false;
                 }
                 if(this.RightClicker != undefined)
                 {
                    this.RightClicker._visible = false;
                 }
                 return undefined;
              }
              this.State = _parent.GraphicsOptionsController.GetDetailLevelDraftIndex();
              this.Initial = _parent.GraphicsOptionsController.GetDetailLevelInitialIndex();
              if(!_parent.GraphicsOptionsController.AreSettingsLoaded())
              {
                 if(this.ItemText != undefined)
                 {
                    this.ItemText.text = _parent.GraphicsOptionsController.IsUnavailable(5) ? "Unavailable" : "Loading...";
                 }
                 if(this.LeftClicker != undefined)
                 {
                    this.LeftClicker._visible = false;
                 }
                 if(this.RightClicker != undefined)
                 {
                    this.RightClicker._visible = false;
                 }
                 return undefined;
              }
              if(this.ItemText != undefined)
              {
                 this.ItemText.text = this.Names[this.State];
              }
              if(this.LeftClicker != undefined)
              {
                 this.LeftClicker._visible = this.State > 0 && _parent.GraphicsOptionsController.CanEditDetailLevel();
              }
              if(this.RightClicker != undefined)
              {
                 this.RightClicker._visible = this.State < 3 && _parent.GraphicsOptionsController.CanEditDetailLevel();
              }
           };
           this.RunAction = function(bMouse)
           {
              if(bMouse)
              {
                 if(this._xmouse < 0) { this.Decrement(); }
                 else { this.Increment(); }
              }
              else if(_parent.GraphicsOptionsController != undefined)
              {
                 _parent.GraphicsOptionsController.ToggleDetailPreset();
              }
           };
           this.Increment = function()
           {
              if(_parent.GraphicsOptionsController != undefined)
              {
                 _parent.GraphicsOptionsController.IncrementDetailPreset();
              }
           };
           this.Decrement = function()
           {
              if(_parent.GraphicsOptionsController != undefined)
              {
                 _parent.GraphicsOptionsController.DecrementDetailPreset();
              }
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
           {{CreateEditableRowArrowAlignmentAction()}}
           this.Update();
        }
        """;
    }

    /// <summary>
    /// Creates the supported-resolution row. The row obtains its live catalog label and directional
    /// availability from the controller, while all changes remain in the shell draft until Apply.
    /// </summary>
    /// <returns>An ActionScript load handler for the resolution catalog row.</returns>
    private static string CreateResolutionRowClipAction()
    {
        return $$"""
        onClipEvent(load){
           this.LabelName = "Resolution";
           this.RowIndex = 2;
           this.Names = new Array();
           this.State = -1;
           this.Initial = -1;
           this.Default = -1;
           this.Update = function()
           {
              if(this.Label != undefined && this.Label.Label != undefined && this.Label.Label.Text != undefined)
              {
                 this.Label.Label.Text.text = "Resolution";
              }
              else if(this.Label != undefined && this.Label.Text != undefined)
              {
                 this.Label.Text.text = "Resolution";
              }
              else if(this.Label != undefined)
              {
                 this.Label.text = "Resolution";
              }
              if(_parent.GraphicsOptionsController == undefined)
              {
                 if(this.ItemText != undefined) { this.ItemText.text = "Loading..."; }
                 if(this.LeftClicker != undefined) { this.LeftClicker._visible = false; }
                 if(this.RightClicker != undefined) { this.RightClicker._visible = false; }
                 return undefined;
              }
              this.Names = new Array();
              var resolutionModeIndex = 0;
              while(resolutionModeIndex < _parent.GraphicsOptionsController.ResolutionModes.length)
              {
                 this.Names.push(_parent.GraphicsOptionsController.ResolutionModes[resolutionModeIndex].Label);
                 resolutionModeIndex = resolutionModeIndex + 1;
              }
              this.State = _parent.GraphicsOptionsController.GetResolutionDraftIndex();
              this.Initial = _parent.GraphicsOptionsController.GetResolutionInitialIndex();
              this.Default = this.Initial;
              if(!_parent.GraphicsOptionsController.InitializationComplete)
              {
                 if(this.ItemText != undefined) { this.ItemText.text = _parent.GraphicsOptionsController.IsUnavailable(2) ? "Unavailable" : "Loading..."; }
                 if(this.LeftClicker != undefined) { this.LeftClicker._visible = false; }
                 if(this.RightClicker != undefined) { this.RightClicker._visible = false; }
                 return undefined;
              }
              if(this.ItemText != undefined) { this.ItemText.text = _parent.GraphicsOptionsController.GetResolutionLabel(); }
              if(this.LeftClicker != undefined) { this.LeftClicker._visible = _parent.GraphicsOptionsController.CanDecrementResolution(); }
              if(this.RightClicker != undefined) { this.RightClicker._visible = _parent.GraphicsOptionsController.CanIncrementResolution(); }
           };
           this.RunAction = function(bMouse)
           {
              if(bMouse)
              {
                 if(this._xmouse < 0) { this.Decrement(); }
                 else { this.Increment(); }
              }
              else if(_parent.GraphicsOptionsController != undefined) { _parent.GraphicsOptionsController.ToggleResolution(); }
           };
           this.Increment = function()
           {
              if(_parent.GraphicsOptionsController != undefined) { _parent.GraphicsOptionsController.IncrementResolution(); }
           };
           this.Decrement = function()
           {
              if(_parent.GraphicsOptionsController != undefined) { _parent.GraphicsOptionsController.DecrementResolution(); }
           };
           this.HasChanged = function()
           {
              if(_parent.GraphicsOptionsController == undefined)
              {
                 return false;
              }
              return _parent.GraphicsOptionsController.ResolutionDraftWidth != _parent.GraphicsOptionsController.ResolutionInitialWidth || _parent.GraphicsOptionsController.ResolutionDraftHeight != _parent.GraphicsOptionsController.ResolutionInitialHeight;
           };
           this.IsDefault = function()
           {
              if(_parent.GraphicsOptionsController == undefined)
              {
                 return false;
              }
              return _parent.GraphicsOptionsController.ResolutionDraftWidth == _parent.GraphicsOptionsController.ResolutionInitialWidth && _parent.GraphicsOptionsController.ResolutionDraftHeight == _parent.GraphicsOptionsController.ResolutionInitialHeight;
           };
           this.RestoreInitialValue = function()
           {
              if(_parent.GraphicsOptionsController != undefined)
              {
                 _parent.GraphicsOptionsController.RestoreResolutionInitial();
                 this.Update();
              }
           };
           this.SetDefault = function()
           {
              if(_parent.GraphicsOptionsController != undefined)
              {
                 _parent.GraphicsOptionsController.RestoreResolutionInitial();
                 this.Update();
              }
           };
           this.ShowPrompt = function()
           {
           };
           this.Destroy = function()
           {
              while(this.Names.length > 0)
              {
                 this.Names.pop();
              }
           };
           this._visible = true;
           {{CreateEditableRowArrowAlignmentAction()}}
           this.Update();
        }
        """;
    }

    /// <summary>
    /// Returns the guarded geometry adjustment shared by editable graphics rows. Callers emit this
    /// snippet once from their load handler so repeated Update calls cannot accumulate positional drift.
    /// </summary>
    /// <returns>ActionScript that shifts the optional directional clickers away from centered value text.</returns>
    private static string CreateEditableRowArrowAlignmentAction()
    {
        return """
           if(this.LeftClicker != undefined && this.LeftClicker._x != undefined)
           {
              this.LeftClicker._x = this.LeftClicker._x - 12;
           }
           if(this.RightClicker != undefined && this.RightClicker._x != undefined)
           {
              this.RightClicker._x = this.RightClicker._x + 12;
           }
        """;
    }

    /// <summary>
    /// Creates an active setting row whose state and mutations are delegated to the declarative controller.
    /// </summary>
    /// <param name="label">The visible setting label.</param>
    /// <param name="rowIndex">The stable graphics row index used by the controller lookup.</param>
    /// <param name="values">The display values in their controller index order.</param>
    /// <returns>An ActionScript load handler for the active setting row.</returns>
    private static string CreateActiveRowClipAction(string label, int rowIndex, string[] values)
    {
        ArgumentNullException.ThrowIfNull(label);
        ArgumentNullException.ThrowIfNull(values);
        if (values.Length == 0)
        {
            throw new ArgumentException("An active graphics row must have at least one display value.", nameof(values));
        }

        string escapedLabel = EscapeActionScriptString(label);
        string escapedValues = string.Join(",", values.Select(value => $"\"{EscapeActionScriptString(value)}\""));

        return $$"""
        onClipEvent(load){
           this.LabelName = "{{escapedLabel}}";
           this.RowIndex = {{rowIndex}};
           this.Names = new Array({{escapedValues}});
           this.State = -1;
           this.Initial = -1;
           this.Default = -1;
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
              if(_parent.GraphicsOptionsController == undefined)
              {
                 if(this.ItemText != undefined)
                 {
                    this.ItemText.text = "Loading...";
                 }
                 if(this.LeftClicker != undefined)
                 {
                    this.LeftClicker._visible = false;
                 }
                 if(this.RightClicker != undefined)
                 {
                    this.RightClicker._visible = false;
                 }
                 return undefined;
              }
              this.State = _parent.GraphicsOptionsController.GetDraftIndex(this.RowIndex);
              this.Initial = _parent.GraphicsOptionsController.GetInitialIndex(this.RowIndex);
              if(this.Initial < 0 && !_parent.GraphicsOptionsController.AreSettingsLoaded())
              {
                 if(this.ItemText != undefined)
                 {
                    this.ItemText.text = _parent.GraphicsOptionsController.IsUnavailable(this.RowIndex) ? "Unavailable" : "Loading...";
                 }
                 if(this.LeftClicker != undefined)
                 {
                    this.LeftClicker._visible = false;
                 }
                 if(this.RightClicker != undefined)
                 {
                    this.RightClicker._visible = false;
                 }
                 return undefined;
              }
              if(this.ItemText != undefined)
              {
                 this.ItemText.text = _parent.GraphicsOptionsController.IsUnavailable(this.RowIndex) ? _parent.GraphicsOptionsController.GetUnavailableLabel(this.RowIndex) : this.Names[this.State];
              }
              if(this.LeftClicker != undefined)
              {
                 this.LeftClicker._visible = this.State > 0 && _parent.GraphicsOptionsController.CanEdit(this.RowIndex);
              }
              if(this.RightClicker != undefined)
              {
                 this.RightClicker._visible = this.State < this.Names.length - 1 && _parent.GraphicsOptionsController.CanEdit(this.RowIndex);
              }
           };
           this.RunAction = function(bMouse)
           {
              if(bMouse)
              {
                 if(this._xmouse < 0) { this.Decrement(); }
                 else { this.Increment(); }
              }
              else if(_parent.GraphicsOptionsController != undefined)
              {
                 _parent.GraphicsOptionsController.ToggleSetting(this.RowIndex);
              }
           };
           this.Increment = function()
           {
              if(_parent.GraphicsOptionsController != undefined)
              {
                 _parent.GraphicsOptionsController.IncrementSetting(this.RowIndex);
              }
           };
           this.Decrement = function()
           {
              if(_parent.GraphicsOptionsController != undefined)
              {
                 _parent.GraphicsOptionsController.DecrementSetting(this.RowIndex);
              }
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
           {{CreateEditableRowArrowAlignmentAction()}}
           this.Update();
        }
        """;
    }

    /// <summary>
    /// Creates the visible Apply Changes row whose activation dispatches through the settings
    /// controller while directional changes and prompt handling remain no-ops.
    /// </summary>
    /// <returns>An ActionScript load handler for row fifteen.</returns>
    private static string CreateApplyRowClipAction()
    {
        return """
        onClipEvent(load){
           this.LabelName = "Apply Changes";
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
              if(_parent.GraphicsOptionsController == undefined)
              {
                 if(this.ItemText != undefined)
                 {
                    this.ItemText.text = "";
                    this.ItemText._alpha = 40;
                 }
                 if(this.Label != undefined)
                 {
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
                 return undefined;
              }
              if(this.ItemText != undefined)
              {
                 this.ItemText.text = _parent.GraphicsOptionsController.GetApplyStatusText();
                 this.ItemText._alpha = _parent.GraphicsOptionsController.CanApply() ? 100 : 40;
              }
              if(this.Label != undefined)
              {
                 this.Label._alpha = _parent.GraphicsOptionsController.CanApply() ? 100 : 40;
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
              if(_parent.GraphicsOptionsController != undefined)
              {
                 _parent.GraphicsOptionsController.ApplyChanges();
              }
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
           this.LabelName = "{{escapedLabel}}";
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
