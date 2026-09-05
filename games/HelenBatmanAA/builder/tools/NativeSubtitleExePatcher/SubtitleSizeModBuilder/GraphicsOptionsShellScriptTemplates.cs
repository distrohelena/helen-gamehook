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
    /// requests live persisted values through the shared frontend carrier before enabling interaction.
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
           var InitializationDeadline;
           var InitializationComplete;
           var InitializationFailed;
           var ResolutionModes;
           var ResolutionInitialIndex;
           var ResolutionDraftIndex;
           var ResolutionCatalogCount;
           var ResolutionCatalogRequest;
           var ResolutionCatalogBase;
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
           var ResolutionInitializationStage;
           var ResolutionCatalogPersistedWidth;
           var ResolutionCatalogPersistedHeight;
           var ResolutionPersistedWidth;
           var ResolutionPersistedHeight;
           var ResolutionCatalogDeadline;
           var ResolutionCatalogIndex;
           var ResolutionPendingWidth;
           var ApplyQueue;
           var ApplyQueueIndex;
           var CurrentPendingSetting;
           var CurrentPendingOperation;
           var CurrentPendingCode;
           var CurrentPendingDeadline;
           var ApplySignalToggle;
           var RollbackSignalToggle;
           var ApplyInProgress;
           var InteractionBlocked;
           var UiStatus;
           var RollbackLocked;
           function BatmanGraphicsOptionsController(screen)
           {
              this.Screen = screen;
              this.Settings = new Array();
              this.ActiveSettingsByRow = new Object();
              this.DetailLeafRows = new Array(6,7,8,9,10,11,12);
              this.InitializationIndex = 0;
              this.InitializationDeadline = undefined;
              this.InitializationComplete = false;
              this.InitializationFailed = false;
              this.ResetResolutionCatalogState();
              this.ApplyQueue = new Array();
              this.ApplyQueueIndex = 0;
              this.CurrentPendingSetting = undefined;
              this.CurrentPendingOperation = "";
              this.CurrentPendingCode = undefined;
              this.CurrentPendingDeadline = undefined;
              this.ApplySignalToggle = 0;
              this.RollbackSignalToggle = 0;
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
              this.ResolutionCatalogCount = 0;
              this.ResolutionCatalogRequest = undefined;
              this.ResolutionCatalogBase = undefined;
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
              this.ResolutionInitializationStage = "settings";
              this.ResolutionCatalogPersistedWidth = undefined;
              this.ResolutionCatalogPersistedHeight = undefined;
              this.ResolutionPersistedWidth = undefined;
              this.ResolutionPersistedHeight = undefined;
              this.ResolutionCatalogDeadline = undefined;
              this.ResolutionCatalogIndex = 0;
              this.ResolutionPendingWidth = undefined;
           }
           function CreateSettings()
           {
              this.Settings = new Array(
                 {RowIndex:1,Name:"Fullscreen",Values:new Array("Windowed","Fullscreen"),ConfigValues:new Array(0,1),ReadRequest:4670,ReadResponseBase:4671,WriteRequestBase:4673,WriteAcknowledgementBase:4675,FailureResponse:4679,InitialIndex:-1,DraftIndex:-1},
                 {RowIndex:3,Name:"VSync",Values:new Array("Off","On"),ConfigValues:new Array(0,1),ReadRequest:4200,ReadResponseBase:4210,WriteRequestBase:4220,WriteAcknowledgementBase:4230,FailureResponse:4299,InitialIndex:-1,DraftIndex:-1},
                 {RowIndex:4,Name:"MSAA",Values:new Array("Off","2x","4x","8x","16x"),ConfigValues:new Array(0,1,2,3,5),ReadRequest:4300,ReadResponseBase:4310,WriteRequestBase:4320,WriteAcknowledgementBase:4330,FailureResponse:4399,InitialIndex:-1,DraftIndex:-1},
                 {RowIndex:6,Name:"Bloom",Values:new Array("Off","On"),ConfigValues:new Array(0,1),ReadRequest:4600,ReadResponseBase:4601,WriteRequestBase:4603,WriteAcknowledgementBase:4605,FailureResponse:4609,InitialIndex:-1,DraftIndex:-1},
                 {RowIndex:7,Name:"Dynamic Shadows",Values:new Array("Off","On"),ConfigValues:new Array(0,1),ReadRequest:4610,ReadResponseBase:4611,WriteRequestBase:4613,WriteAcknowledgementBase:4615,FailureResponse:4619,InitialIndex:-1,DraftIndex:-1},
                 {RowIndex:8,Name:"Motion Blur",Values:new Array("Off","On"),ConfigValues:new Array(0,1),ReadRequest:4620,ReadResponseBase:4621,WriteRequestBase:4623,WriteAcknowledgementBase:4625,FailureResponse:4629,InitialIndex:-1,DraftIndex:-1},
                 {RowIndex:9,Name:"Distortion",Values:new Array("Off","On"),ConfigValues:new Array(0,1),ReadRequest:4630,ReadResponseBase:4631,WriteRequestBase:4633,WriteAcknowledgementBase:4635,FailureResponse:4639,InitialIndex:-1,DraftIndex:-1},
                 {RowIndex:10,Name:"Fog Volumes",Values:new Array("Off","On"),ConfigValues:new Array(0,1),ReadRequest:4640,ReadResponseBase:4641,WriteRequestBase:4643,WriteAcknowledgementBase:4645,FailureResponse:4649,InitialIndex:-1,DraftIndex:-1},
                 {RowIndex:11,Name:"Spherical Harmonic Lighting",Values:new Array("Off","On"),ConfigValues:new Array(0,1),ReadRequest:4650,ReadResponseBase:4651,WriteRequestBase:4653,WriteAcknowledgementBase:4655,FailureResponse:4659,InitialIndex:-1,DraftIndex:-1},
                 {RowIndex:12,Name:"Ambient Occlusion",Values:new Array("Off","On"),ConfigValues:new Array(0,1),ReadRequest:4660,ReadResponseBase:4661,WriteRequestBase:4663,WriteAcknowledgementBase:4665,FailureResponse:4669,InitialIndex:-1,DraftIndex:-1},
                 {RowIndex:13,Name:"PhysX",Values:new Array("Off","Normal","High"),ConfigValues:new Array(0,1,2),ReadRequest:4400,ReadResponseBase:4410,WriteRequestBase:4420,WriteAcknowledgementBase:4430,FailureResponse:4499,InitialIndex:-1,DraftIndex:-1},
                 {RowIndex:14,Name:"Stereo 3D",Values:new Array("Off","On"),ConfigValues:new Array(0,1),ReadRequest:4500,ReadResponseBase:4510,WriteRequestBase:4520,WriteAcknowledgementBase:4530,FailureResponse:4599,InitialIndex:-1,DraftIndex:-1}
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
              if(!this.InitializationComplete || this.InitializationFailed || this.InteractionBlocked || this.RollbackLocked)
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
           function IsUnavailable(rowIndex)
           {
              if(!this.InitializationComplete)
              {
                 return false;
              }
              if(rowIndex == 2)
              {
                 return this.ResolutionModes.length == 0 || this.ResolutionDraftIndex < 0;
              }
              var setting = this.GetSettingForRow(rowIndex);
              return setting == undefined || setting.InitialIndex < 0;
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
              return setting != undefined && this.InitializationComplete && !this.InitializationFailed && !this.InteractionBlocked && !this.RollbackLocked && setting.InitialIndex >= 0 && setting.DraftIndex >= 0;
           }
           function CanApply()
           {
              return this.InitializationComplete && !this.InitializationFailed && !this.ApplyInProgress && !this.InteractionBlocked && !this.RollbackLocked && this.IsDirty();
           }
           function NormalizeTimerValue(value)
           {
              var normalizedValue = value % 4294967296;
              if(normalizedValue < 0)
              {
                 normalizedValue = normalizedValue + 4294967296;
              }
              return normalizedValue;
           }
           function IsDeadlineReached(deadline)
           {
              var elapsed = this.NormalizeTimerValue(getTimer()) - this.NormalizeTimerValue(deadline);
              if(elapsed < 0)
              {
                 elapsed = elapsed + 4294967296;
              }
              return elapsed < 2147483648;
           }
           function BeginInitialization()
           {
              this.InitializationIndex = 0;
              this.ResetResolutionCatalogState();
              this.ResolutionInitializationStage = "settings";
              this.InitializationDeadline = getTimer() + 10000;
              this.ResolutionCatalogDeadline = this.InitializationDeadline;
              this.InitializationComplete = false;
              this.InitializationFailed = false;
              this.ApplyInProgress = false;
              this.InteractionBlocked = true;
              this.UiStatus = "";
              this.CurrentPendingOperation = "initialization";
              this.Screen.BlockInput(true);
              this.RefreshRows();
              this.SendInitializationRequest();
           }
           function SendInitializationRequest()
           {
              if(this.InitializationIndex >= this.Settings.length && this.ResolutionInitializationStage == "settings")
              {
                 this.BeginResolutionCatalogInitialization();
                 return undefined;
              }
              if(this.ResolutionInitializationStage != "settings")
              {
                 this.PollInitializationCatalogProgress();
                 return undefined;
              }
              if(this.InitializationIndex >= this.Settings.length)
              {
                 this.CompleteInitialization();
                 return undefined;
              }
              this.CurrentPendingSetting = this.Settings[this.InitializationIndex];
              this.CurrentPendingCode = this.CurrentPendingSetting.ReadRequest;
              this.CurrentPendingDeadline = getTimer() + 2000;
              flash.external.ExternalInterface.call("FE_SetControlType",this.Settings[this.InitializationIndex].ReadRequest,"");
           }
           function BeginResolutionCatalogInitialization()
           {
              this.ResolutionCatalogRequest = 5200;
              this.ResolutionCatalogBase = 5200;
              this.ResolutionCatalogKind = 0;
              this.ResolutionInitializationStage = "windowed";
              this.ResolutionCatalogLoading = true;
              this.ResolutionCatalogFailure = false;
              this.ResolutionCatalogDeadline = getTimer() + 10000;
              this.SendResolutionCatalogRequest();
           }
           function PollInitializationCatalogProgress()
           {
              if(this.ResolutionInitializationStage == "windowed")
              {
                 this.ResolutionInitializationStage = "fullscreen";
                 this.ResolutionCatalogRequest = 5400;
                 this.ResolutionCatalogBase = 5400;
                 this.ResolutionCatalogKind = 1;
                 this.FullscreenResolutionModes = new Array();
                 this.ResolutionCatalogCount = 0;
                 this.ResolutionCatalogIndex = 0;
                 this.ResolutionPendingWidth = undefined;
                 this.ResolutionCatalogPersistedWidth = undefined;
                 this.ResolutionCatalogPersistedHeight = undefined;
                 this.ResolutionCatalogLoading = true;
                 this.ResolutionCatalogFailure = false;
                 this.ResolutionCatalogDeadline = getTimer() + 10000;
                 this.SendResolutionCatalogRequest();
                 return undefined;
              }
              if(this.ResolutionInitializationStage == "fullscreen")
              {
                 this.ResolutionInitializationStage = "desktop";
                 this.ResolutionCatalogRequest = 5600;
                 this.ResolutionCatalogBase = undefined;
                 this.ResolutionCatalogKind = -1;
                 this.ResolutionCatalogIndex = 0;
                 this.ResolutionCatalogLoading = true;
                 this.ResolutionCatalogFailure = false;
                 this.ResolutionCatalogDeadline = getTimer() + 10000;
                 this.SendResolutionCatalogRequest();
                 return undefined;
              }
              this.FinishResolutionInitialization();
           }
           function SendResolutionCatalogRequest()
           {
              this.CurrentPendingCode = this.ResolutionCatalogRequest;
              flash.external.ExternalInterface.call("FE_SetControlType",this.ResolutionCatalogRequest,"");
           }
           function CompleteInitialization()
           {
              this.InitializationComplete = true;
              this.InitializationFailed = false;
              this.InteractionBlocked = false;
              this.CurrentPendingOperation = "";
              this.CurrentPendingSetting = undefined;
              this.CurrentPendingCode = undefined;
              this.CurrentPendingDeadline = undefined;
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
              this.CurrentPendingOperation = "";
              this.CurrentPendingSetting = undefined;
              this.CurrentPendingCode = undefined;
              this.CurrentPendingDeadline = undefined;
              this.Screen.BlockInput(false);
              this.RefreshRows();
           }
           function Tick()
           {
              if(this.CurrentPendingOperation == "")
              {
                 return undefined;
              }
              if(this.CurrentPendingOperation == "initialization")
              {
                 this.PollInitialization();
                 return undefined;
              }
              this.PollTransaction();
           }
           function PollInitialization()
           {
              var deadline = this.ResolutionInitializationStage == "settings" ? this.CurrentPendingDeadline : this.ResolutionCatalogDeadline;
              if(this.IsDeadlineReached(deadline))
              {
                 if(this.ResolutionInitializationStage == "settings")
                 {
                    this.MarkSettingUnavailable(this.InitializationIndex);
                    return undefined;
                 }
                 this.HandleResolutionCatalogFailure();
                 return undefined;
              }
              var rawValue = int(flash.external.ExternalInterface.call("FE_GetControlType"));
              if(rawValue == this.CurrentPendingCode)
              {
                 return undefined;
              }
              if(this.ResolutionInitializationStage != "settings")
              {
                 this.PollResolutionCatalog(rawValue);
                 return undefined;
              }
              var setting = this.Settings[this.InitializationIndex];
              if(rawValue == setting.FailureResponse)
              {
                 this.MarkSettingUnavailable(this.InitializationIndex);
                 return undefined;
              }
              if(rawValue >= setting.ReadResponseBase && rawValue < setting.ReadResponseBase + setting.Values.length)
              {
                 this.Settings[this.InitializationIndex].InitialIndex = rawValue - this.Settings[this.InitializationIndex].ReadResponseBase;
                 this.Settings[this.InitializationIndex].DraftIndex = this.Settings[this.InitializationIndex].InitialIndex;
                 this.InitializationIndex = this.InitializationIndex + 1;
                 this.SendInitializationRequest();
                 return undefined;
              }
              if(rawValue != 0)
              {
                 this.MarkSettingUnavailable(this.InitializationIndex);
              }
           }
           function MarkSettingUnavailable(settingIndex)
           {
              if(settingIndex < 0 || settingIndex >= this.Settings.length)
              {
                 this.FailInitialization();
                 return undefined;
              }
              this.Settings[settingIndex].InitialIndex = -1;
              this.Settings[settingIndex].DraftIndex = -1;
              this.InitializationIndex = settingIndex + 1;
              this.SendInitializationRequest();
           }
           function PollResolutionCatalog(rawValue)
           {
              if(this.ResolutionInitializationStage == "desktop")
              {
                 this.PollDesktopActualMode(rawValue);
                 return undefined;
              }
              if(rawValue == 4899)
              {
                 this.HandleResolutionCatalogFailure();
                 return undefined;
              }
              var scalarValue = this.DecodeResolutionScalar(rawValue);
              if(scalarValue < 1)
              {
                 this.HandleResolutionCatalogFailure();
                 return undefined;
              }
              if(this.ResolutionCatalogRequest == this.ResolutionCatalogBase)
              {
                 if(scalarValue < 1 || scalarValue > 98)
                 {
                    this.HandleResolutionCatalogFailure();
                    return undefined;
                 }
                 this.ResolutionCatalogCount = scalarValue;
                 this.ResolutionCatalogIndex = 0;
                 this.ResolutionCatalogRequest = this.ResolutionCatalogBase + 1;
                 this.SendResolutionCatalogRequest();
                 return undefined;
              }
              if(this.ResolutionCatalogRequest >= this.ResolutionCatalogBase + 1 && this.ResolutionCatalogRequest < this.ResolutionCatalogBase + 1 + this.ResolutionCatalogCount * 2)
              {
                 if((this.ResolutionCatalogRequest - (this.ResolutionCatalogBase + 1)) % 2 == 0)
                 {
                    this.ResolutionPendingWidth = scalarValue;
                    this.ResolutionCatalogRequest = this.ResolutionCatalogRequest + 1;
                    this.SendResolutionCatalogRequest();
                    return undefined;
                 }
                 if(this.ResolutionPendingWidth == undefined)
                 {
                    this.HandleResolutionCatalogFailure();
                    return undefined;
                 }
                 var catalogModes = this.ResolutionCatalogKind == 0 ? this.WindowedResolutionModes : this.FullscreenResolutionModes;
                 var existingModeIndex = 0;
                 while(existingModeIndex < catalogModes.length)
                 {
                    if(catalogModes[existingModeIndex].Width == this.ResolutionPendingWidth && catalogModes[existingModeIndex].Height == scalarValue)
                    {
                       this.HandleResolutionCatalogFailure();
                       return undefined;
                    }
                    existingModeIndex = existingModeIndex + 1;
                 }
                 var widthValue = this.ResolutionPendingWidth;
                 var heightValue = scalarValue;
                 this.ResolutionModes = catalogModes;
                 this.ResolutionModes.push({Width:widthValue,Height:heightValue,Label:widthValue + " x " + heightValue});
                 this.ResolutionPendingWidth = undefined;
                 this.ResolutionCatalogIndex = this.ResolutionCatalogIndex + 1;
                 if(this.ResolutionCatalogIndex >= this.ResolutionCatalogCount)
                 {
                    this.ResolutionCatalogRequest = this.ResolutionCatalogBase + 197;
                }
                else
                {
                    this.ResolutionCatalogRequest = this.ResolutionCatalogBase + 1 + this.ResolutionCatalogIndex * 2;
                 }
                 this.SendResolutionCatalogRequest();
                 return undefined;
              }
              if(this.ResolutionCatalogRequest == this.ResolutionCatalogBase + 197)
              {
                 this.ResolutionCatalogPersistedWidth = scalarValue;
                 this.ResolutionCatalogRequest = this.ResolutionCatalogBase + 198;
                 this.SendResolutionCatalogRequest();
                 return undefined;
              }
              if(this.ResolutionCatalogRequest == this.ResolutionCatalogBase + 198)
              {
                 this.ResolutionCatalogPersistedHeight = scalarValue;
                 this.ResolutionCatalogLoading = false;
                 if(this.ResolutionCatalogKind == 0)
                 {
                    this.WindowedResolutionAvailable = true;
                 }
                 else
                 {
                    this.FullscreenResolutionAvailable = true;
                 }
                 this.CompleteResolutionCatalogLoad();
              }
           }
           function HandleResolutionCatalogFailure()
           {
              this.ResolutionCatalogFailure = true;
              this.ResolutionCatalogLoading = false;
              if(this.CurrentPendingOperation == "initialization")
              {
                 this.ResolutionCatalogRequest = undefined;
                 if(this.ResolutionCatalogKind == 0)
                 {
                    this.WindowedResolutionAvailable = false;
                    this.WindowedResolutionModes = new Array();
                 }
                 else if(this.ResolutionCatalogKind == 1)
                 {
                    this.FullscreenResolutionAvailable = false;
                    this.FullscreenResolutionModes = new Array();
                 }
                 else
                 {
                    this.DesktopActualWidth = undefined;
                    this.DesktopActualHeight = undefined;
                 }
                 this.PollInitializationCatalogProgress();
                 return undefined;
              }
              this.FailResolutionCatalogLoad();
           }
           function CompleteResolutionCatalogLoad()
           {
              this.ResolutionCatalogRequest = undefined;
              this.ResolutionCatalogFailure = false;
              this.ResolutionCatalogLoading = false;
              if(this.ResolutionPersistedWidth == undefined && this.ResolutionCatalogPersistedWidth != undefined)
              {
                 this.ResolutionPersistedWidth = this.ResolutionCatalogPersistedWidth;
                 this.ResolutionPersistedHeight = this.ResolutionCatalogPersistedHeight;
              }
              if(this.CurrentPendingOperation == "initialization")
              {
                 this.PollInitializationCatalogProgress();
              }
              else
              {
                 this.CurrentPendingOperation = "";
                 this.CurrentPendingCode = undefined;
                 this.InteractionBlocked = false;
                 this.RefreshRows();
              }
           }
           function FailResolutionCatalogLoad()
           {
              this.ResolutionCatalogRequest = undefined;
              this.ResolutionCatalogLoading = false;
              this.ResolutionCatalogFailure = true;
              this.CurrentPendingOperation = "";
              this.CurrentPendingCode = undefined;
              this.InteractionBlocked = false;
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
           function PollDesktopActualMode(rawValue)
           {
              if(rawValue == 4899)
              {
                 this.HandleResolutionCatalogFailure();
                 return undefined;
              }
              var scalarValue = this.DecodeResolutionScalar(rawValue);
              if(scalarValue < 1)
              {
                 this.HandleResolutionCatalogFailure();
                 return undefined;
              }
              if(this.ResolutionCatalogRequest == 5600)
              {
                 this.DesktopActualWidth = scalarValue;
                 this.ResolutionCatalogRequest = 5601;
                 this.SendResolutionCatalogRequest();
                 return undefined;
              }
              if(this.ResolutionCatalogRequest == 5601)
              {
                 this.DesktopActualHeight = scalarValue;
                 this.ResolutionCatalogRequest = undefined;
                 this.ResolutionCatalogLoading = false;
                 this.FinishResolutionInitialization();
              }
           }
           function FinishResolutionInitialization()
           {
              this.ResolutionInitializationStage = "ready";
              this.ResolutionCatalogRequest = undefined;
              this.ResolutionCatalogLoading = false;
              if(this.ResolutionPersistedWidth == undefined)
              {
                 this.ResolutionPersistedWidth = this.ResolutionCatalogPersistedWidth;
                 this.ResolutionPersistedHeight = this.ResolutionCatalogPersistedHeight;
              }
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
           function DecodeResolutionScalar(rawValue)
           {
              if(rawValue >= 0)
              {
                 return -1;
              }
              var magnitude = 0 - rawValue;
              var ordinal = Math.floor((magnitude - 1) / 32768);
              var scalarValue = magnitude - ordinal * 32768;
              var expectedOrdinal = this.GetResolutionRequestOrdinal(this.ResolutionCatalogRequest);
              if(ordinal != expectedOrdinal || scalarValue < 1 || scalarValue > 32767)
              {
                 return -1;
              }
              return scalarValue;
           }
           function GetResolutionRequestOrdinal(request)
           {
              if(request >= 4700 && request <= 4898) { return request - 4700; }
              if(request >= 5200 && request <= 5398) { return 199 + request - 5200; }
              if(request >= 5400 && request <= 5598) { return 398 + request - 5400; }
              if(request >= 5600 && request <= 5601) { return 597 + request - 5600; }
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
           function ApplyChanges()
           {
              if(!this.CanApply())
              {
                 return undefined;
              }
              this.ApplyQueue = new Array();
              var settingIndex = 0;
              while(settingIndex < this.Settings.length)
              {
                 if(this.Settings[settingIndex].DraftIndex != this.Settings[settingIndex].InitialIndex)
                 {
                    this.ApplyQueue.push(this.Settings[settingIndex]);
                 }
                 if(settingIndex == 0 && this.ResolutionDraftIndex >= 0 && (this.ResolutionDraftWidth != this.ResolutionInitialWidth || this.ResolutionDraftHeight != this.ResolutionInitialHeight))
                 {
                    this.ApplyQueue.push({IsResolution:true,SelectedIndex:this.ResolutionDraftIndex,SelectedWidth:this.ResolutionDraftWidth,SelectedHeight:this.ResolutionDraftHeight,SelectedKind:this.Settings[0].DraftIndex,WriteRequest:5000 + this.ResolutionDraftIndex,WriteAcknowledgement:5100 + this.ResolutionDraftIndex,FailureResponse:5199});
                 }
                 settingIndex = settingIndex + 1;
              }
              this.ApplyQueueIndex = 0;
              this.ApplyInProgress = true;
              this.InteractionBlocked = true;
              this.UiStatus = "Applying...";
              this.Screen.BlockInput(true);
              this.RefreshRows();
              this.BeginNextApplyStep();
           }
           function BeginNextApplyStep()
           {
              if(this.ApplyQueueIndex >= this.ApplyQueue.length)
              {
                 this.BeginCommit();
                 return undefined;
              }
              this.CurrentPendingSetting = this.ApplyQueue[this.ApplyQueueIndex];
              this.CurrentPendingOperation = this.CurrentPendingSetting.IsResolution ? "resolution" : "setting";
              if(this.CurrentPendingSetting.IsResolution)
              {
                 this.CurrentPendingCode = this.CurrentPendingSetting.WriteRequest;
              }
              else
              {
                 this.CurrentPendingCode = this.CurrentPendingSetting.WriteRequestBase + this.CurrentPendingSetting.DraftIndex;
              }
              this.CurrentPendingDeadline = getTimer() + 2000;
              flash.external.ExternalInterface.call("FE_SetControlType",this.CurrentPendingCode,"");
           }
           function BeginCommit()
           {
              this.CurrentPendingOperation = "commit";
              this.CurrentPendingSetting = undefined;
              this.ApplySignalToggle = this.ApplySignalToggle == 0 ? 1 : 0;
              this.CurrentPendingCode = 4990+this.ApplySignalToggle;
              this.CurrentPendingDeadline = getTimer() + 2000;
              flash.external.ExternalInterface.call("FE_SetControlType",4990+this.ApplySignalToggle,"");
           }
           function PollTransaction()
           {
              if(this.IsDeadlineReached(this.CurrentPendingDeadline))
              {
                 if(this.CurrentPendingOperation == "rollback")
                 {
                    this.FailRollback();
                    return undefined;
                 }
                 this.BeginRollback();
                 return undefined;
              }
              var rawValue = int(flash.external.ExternalInterface.call("FE_GetControlType"));
              if(this.CurrentPendingOperation == "setting" || this.CurrentPendingOperation == "resolution")
              {
                 if(rawValue == this.CurrentPendingSetting.FailureResponse || (this.CurrentPendingSetting.IsResolution && rawValue == 5199))
                 {
                    this.BeginRollback();
                    return undefined;
                 }
                 var expectedAcknowledgement = this.CurrentPendingSetting.IsResolution ? this.CurrentPendingSetting.WriteAcknowledgement : this.CurrentPendingSetting.WriteAcknowledgementBase + this.CurrentPendingSetting.DraftIndex;
                 if(rawValue == expectedAcknowledgement)
                 {
                    this.ApplyQueueIndex = this.ApplyQueueIndex + 1;
                    this.BeginNextApplyStep();
                 }
                 return undefined;
              }
              if(this.CurrentPendingOperation == "commit")
              {
                 if(rawValue == 4989)
                 {
                    this.BeginRollback();
                    return undefined;
                 }
                 if(rawValue == 4980+this.ApplySignalToggle)
                 {
                    this.CompleteCommit();
                 }
              }
              else if(this.CurrentPendingOperation == "rollback")
              {
                 if(rawValue == 4969)
                 {
                    this.FailRollback();
                    return undefined;
                 }
                 if(rawValue == 4960+this.RollbackSignalToggle)
                 {
                    this.CompleteRollback();
                 }
              }
           }
           function CompleteCommit()
           {
              this.CopyDraftToInitial();
              this.ApplyQueue = new Array();
              this.ApplyQueueIndex = 0;
              this.ApplyInProgress = false;
              this.InteractionBlocked = false;
              this.UiStatus = "";
              this.CurrentPendingOperation = "";
              this.CurrentPendingSetting = undefined;
              this.CurrentPendingCode = undefined;
              this.CurrentPendingDeadline = undefined;
              this.Screen.BlockInput(false);
              this.RefreshRows();
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
           function BeginRollback()
           {
              if(this.CurrentPendingOperation == "rollback")
              {
                 return undefined;
              }
              this.ApplyQueue = new Array();
              this.ApplyQueueIndex = 0;
              this.ApplyInProgress = true;
              this.InteractionBlocked = true;
              this.UiStatus = "Applying...";
              this.CurrentPendingOperation = "rollback";
              this.CurrentPendingSetting = undefined;
              this.RollbackSignalToggle = this.RollbackSignalToggle == 0 ? 1 : 0;
              this.CurrentPendingCode = 4970+this.RollbackSignalToggle;
              this.CurrentPendingDeadline = getTimer() + 2000;
              flash.external.ExternalInterface.call("FE_SetControlType",4970+this.RollbackSignalToggle,"");
           }
           function CompleteRollback()
           {
              this.Settings[0].DraftIndex = this.Settings[0].InitialIndex;
              this.SelectResolutionKind(this.Settings[0].InitialIndex);
              this.ResolutionDraftWidth = this.ResolutionInitialWidth;
              this.ResolutionDraftHeight = this.ResolutionInitialHeight;
              this.ResolutionDraftIndex = this.FindResolutionModeIndex(this.ResolutionModes,this.ResolutionInitialWidth,this.ResolutionInitialHeight);
              this.ApplyInProgress = false;
              this.InteractionBlocked = false;
              this.UiStatus = "Apply Failed";
              this.CurrentPendingOperation = "";
              this.CurrentPendingSetting = undefined;
              this.CurrentPendingCode = undefined;
              this.CurrentPendingDeadline = undefined;
              this.Screen.BlockInput(false);
              this.RefreshRows();
           }
           function FailRollback()
           {
              this.ApplyInProgress = false;
              this.InteractionBlocked = true;
              this.RollbackLocked = true;
              this.UiStatus = "Rollback Failed";
              this.CurrentPendingOperation = "";
              this.CurrentPendingSetting = undefined;
              this.CurrentPendingCode = undefined;
              this.CurrentPendingDeadline = undefined;
              this.Screen.BlockInput(false);
              this.RefreshRows();
           }
           function GetApplyStatusText()
           {
              return this.UiStatus;
           }
           function Destroy()
           {
              var settingIndex = 0;
              while(settingIndex < this.Settings.length)
              {
                 this.Settings[settingIndex].DraftIndex = this.Settings[settingIndex].InitialIndex;
                 settingIndex = settingIndex + 1;
              }
              this.ApplyQueue = new Array();
              this.ApplyQueueIndex = 0;
              this.ResetResolutionCatalogState();
              this.CurrentPendingOperation = "";
              this.CurrentPendingSetting = undefined;
              this.CurrentPendingCode = undefined;
              this.CurrentPendingDeadline = undefined;
              this.ApplyInProgress = false;
              this.InteractionBlocked = false;
              this.Screen.BlockInput(false);
           }
        }
        function CancelScreen()
        {
           if(this.GraphicsOptionsController != undefined)
           {
              this.GraphicsOptionsController.Destroy();
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
        this.Tick = function()
        {
           if(this.GraphicsOptionsController != undefined)
           {
              this.GraphicsOptionsController.Tick();
           }
        };
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
              if(!_parent.GraphicsOptionsController.InitializationComplete)
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
           this.RunAction = function()
           {
              if(_parent.GraphicsOptionsController != undefined)
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
           this.RunAction = function()
           {
              if(_parent.GraphicsOptionsController != undefined) { _parent.GraphicsOptionsController.ToggleResolution(); }
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
              if(!_parent.GraphicsOptionsController.InitializationComplete)
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
                 this.ItemText.text = _parent.GraphicsOptionsController.IsUnavailable(this.RowIndex) ? "Unavailable" : this.Names[this.State];
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
           this.RunAction = function()
           {
              if(_parent.GraphicsOptionsController != undefined)
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
