namespace SubtitleSizeModBuilder;

/// <summary>
/// Produces the selective ActionScript shell used to expose the retail graphics-options screen.
/// Four declaratively described settings are connected to the frontend carrier; every other row
/// remains a visible, callback-free placeholder so the stock screen layout and navigation remain stable.
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
           var InitializationIndex;
           var InitializationDeadline;
           var InitializationComplete;
           var InitializationFailed;
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
              this.InitializationIndex = 0;
              this.InitializationDeadline = undefined;
              this.InitializationComplete = false;
              this.InitializationFailed = false;
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
           function CreateSettings()
           {
              this.Settings = new Array(
                 {RowIndex:3,Name:"VSync",Values:new Array("Off","On"),ConfigValues:new Array(0,1),ReadRequest:4200,ReadResponseBase:4210,WriteRequestBase:4220,WriteAcknowledgementBase:4230,FailureResponse:4299,InitialIndex:-1,DraftIndex:-1},
                 {RowIndex:4,Name:"MSAA",Values:new Array("Off","2x","4x","8x","16x"),ConfigValues:new Array(0,1,2,3,5),ReadRequest:4300,ReadResponseBase:4310,WriteRequestBase:4320,WriteAcknowledgementBase:4330,FailureResponse:4399,InitialIndex:-1,DraftIndex:-1},
                 {RowIndex:13,Name:"PhysX",Values:new Array("Off","Normal","High"),ConfigValues:new Array(0,1,2),ReadRequest:4400,ReadResponseBase:4410,WriteRequestBase:4420,WriteAcknowledgementBase:4430,FailureResponse:4499,InitialIndex:-1,DraftIndex:-1},
                 {RowIndex:14,Name:"NVIDIA Stereo 3D",Values:new Array("Off","On"),ConfigValues:new Array(0,1),ReadRequest:4500,ReadResponseBase:4510,WriteRequestBase:4520,WriteAcknowledgementBase:4530,FailureResponse:4599,InitialIndex:-1,DraftIndex:-1}
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
              var setting = this.GetSettingForRow(rowIndex);
              if(setting == undefined)
              {
                 return -1;
              }
              return setting.DraftIndex;
           }
           function GetInitialIndex(rowIndex)
           {
              var setting = this.GetSettingForRow(rowIndex);
              if(setting == undefined)
              {
                 return -1;
              }
              return setting.InitialIndex;
           }
           function IsUnavailable(rowIndex)
           {
              return this.InitializationFailed;
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
           function BeginInitialization()
           {
              this.InitializationIndex = 0;
              this.InitializationDeadline = getTimer() + 10000;
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
              if(this.InitializationIndex >= this.Settings.length)
              {
                 this.CompleteInitialization();
                 return undefined;
              }
              this.CurrentPendingSetting = this.Settings[this.InitializationIndex];
              this.CurrentPendingCode = this.CurrentPendingSetting.ReadRequest;
              flash.external.ExternalInterface.call("FE_SetControlType",this.Settings[this.InitializationIndex].ReadRequest,"");
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
              if(getTimer() >= this.InitializationDeadline)
              {
                 this.FailInitialization();
                 return undefined;
              }
              var rawValue = int(flash.external.ExternalInterface.call("FE_GetControlType"));
              var setting = this.Settings[this.InitializationIndex];
              if(rawValue == setting.FailureResponse)
              {
                 this.FailInitialization();
                 return undefined;
              }
              if(rawValue >= setting.ReadResponseBase && rawValue < setting.ReadResponseBase + setting.Values.length)
              {
                 this.Settings[this.InitializationIndex].InitialIndex = rawValue - this.Settings[this.InitializationIndex].ReadResponseBase;
                 this.Settings[this.InitializationIndex].DraftIndex = this.Settings[this.InitializationIndex].InitialIndex;
                 this.InitializationIndex = this.InitializationIndex + 1;
                 this.SendInitializationRequest();
              }
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
              setting.DraftIndex = index;
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
              this.CurrentPendingOperation = "setting";
              this.CurrentPendingCode = this.CurrentPendingSetting.WriteRequestBase + this.CurrentPendingSetting.DraftIndex;
              this.CurrentPendingDeadline = getTimer() + 2000;
              flash.external.ExternalInterface.call("FE_SetControlType",this.CurrentPendingSetting.WriteRequestBase + this.CurrentPendingSetting.DraftIndex,"");
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
              if(getTimer() >= this.CurrentPendingDeadline)
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
              if(this.CurrentPendingOperation == "setting")
              {
                 if(rawValue == this.CurrentPendingSetting.FailureResponse)
                 {
                    this.BeginRollback();
                    return undefined;
                 }
                 if(rawValue == this.CurrentPendingSetting.WriteAcknowledgementBase + this.CurrentPendingSetting.DraftIndex)
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
              this.CurrentPendingOperation = "";
              this.CurrentPendingSetting = undefined;
              this.CurrentPendingCode = undefined;
              this.CurrentPendingDeadline = undefined;
              this.Screen.onEnterFrame = undefined;
           }
        }
        function CancelScreen()
        {
           this.GraphicsOptionsController.Destroy();
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
        this.onEnterFrame = function()
        {
           this.GraphicsOptionsController.Tick();
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
    /// Creates all fifteen row scripts in visual and navigation order, injecting four active
    /// declarative settings and retaining no-op placeholders for all unrelated settings.
    /// </summary>
    /// <param name="snapshot">The normalized user graphics snapshot retained by the builder contract.</param>
    /// <returns>The fifteen row clip-action scripts.</returns>
    public static string[] CreateRowClipActions(BatmanGraphicsIniBootstrapSnapshot snapshot)
    {
        ArgumentNullException.ThrowIfNull(snapshot);
        return
        [
            CreateRowClipAction("Fullscreen", "Not active", true),
            CreateRowClipAction("Resolution", "Not active", true),
            CreateActiveRowClipAction("VSync", 3, ["Off", "On"]),
            CreateActiveRowClipAction("MSAA", 4, ["Off", "2x", "4x", "8x", "16x"]),
            CreateRowClipAction("Detail Level", "Not active", true),
            CreateRowClipAction("Bloom", "Not active", true),
            CreateRowClipAction("Dynamic Shadows", "Not active", true),
            CreateRowClipAction("Motion Blur", "Not active", true),
            CreateRowClipAction("Distortion", "Not active", true),
            CreateRowClipAction("Fog Volumes", "Not active", true),
            CreateRowClipAction("Spherical Harmonic Lighting", "Not active", true),
            CreateRowClipAction("Ambient Occlusion", "Not active", true),
            CreateActiveRowClipAction("PhysX", 13, ["Off", "Normal", "High"]),
            CreateActiveRowClipAction("NVIDIA Stereo 3D", 14, ["Off", "On"]),
            CreateApplyRowClipAction()
        ];
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
              this.State = _parent.GraphicsOptionsController.GetDraftIndex(this.RowIndex);
              this.Initial = _parent.GraphicsOptionsController.GetInitialIndex(this.RowIndex);
              if(!_parent.GraphicsOptionsController.InitializationComplete)
              {
                 this.ItemText.text = _parent.GraphicsOptionsController.IsUnavailable(this.RowIndex) ? "Unavailable" : "Loading...";
                 this.LeftClicker._visible = false;
                 this.RightClicker._visible = false;
                 return undefined;
              }
              this.ItemText.text = this.Names[this.State];
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
              _parent.GraphicsOptionsController.ToggleSetting(this.RowIndex);
           };
           this.Increment = function()
           {
              _parent.GraphicsOptionsController.IncrementSetting(this.RowIndex);
           };
           this.Decrement = function()
           {
              _parent.GraphicsOptionsController.DecrementSetting(this.RowIndex);
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
              this.ItemText.text = _parent.GraphicsOptionsController.GetApplyStatusText();
              this.ItemText._alpha = _parent.GraphicsOptionsController.CanApply() ? 100 : 40;
              this.Label._alpha = _parent.GraphicsOptionsController.CanApply() ? 100 : 40;
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
              _parent.GraphicsOptionsController.ApplyChanges();
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
