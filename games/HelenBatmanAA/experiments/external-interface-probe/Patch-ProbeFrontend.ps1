param([Parameter(Mandatory = $true)][string]$ScriptRoot)
$ErrorActionPreference = 'Stop'
$framePath = Join-Path $ScriptRoot 'DefineSprite_600_ScreenOptionsGraphics\frame_1\DoAction.as'
$frame = [IO.File]::ReadAllText($framePath)
$needle = "this.Screen.BlockInput(true);`n      this.RefreshRows();`n      this.SendInitializationRequest();"
$frame = $frame.Replace("`r`n", "`n")
if ([regex]::Matches($frame, [regex]::Escape($needle)).Count -ne 1) { throw 'Unrecognized initialization source; refusing probe patch.' }
$replacement = @'
this.Screen.BlockInput(true);
              var directFullscreen = flash.external.ExternalInterface.call("Helen_ProbeGetFullscreen");
              if(typeof(directFullscreen) == "boolean")
              {
                 this.Settings[0].InitialIndex = directFullscreen ? 1 : 0;
                 this.Settings[0].DraftIndex = this.Settings[0].InitialIndex;
              }
              else
              {
                 this.Settings[0].InitialIndex = -1;
                 this.Settings[0].DraftIndex = -1;
                 this.FullscreenReadFailure = "Direct " + typeof(directFullscreen);
              }
              this.InitializationIndex = 1;
              this.RefreshRows();
              this.SendInitializationRequest();
'@
$frame = $frame.Replace($needle, $replacement)
$editGuard = '(rowIndex != 1 || this.InitializationComplete)'
if ([regex]::Matches($frame, [regex]::Escape($editGuard)).Count -ne 1) { throw 'Unrecognized Fullscreen edit guard.' }
$frame = $frame.Replace($editGuard, '(rowIndex != 1)')
[IO.File]::WriteAllText($framePath, $frame, [Text.UTF8Encoding]::new($false))
Write-Output 'PROBE_FRONTEND_PATCHED (Fullscreen read-only, first carrier request skipped)'
