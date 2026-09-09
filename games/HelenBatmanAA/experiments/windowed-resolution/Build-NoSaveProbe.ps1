param([Parameter(Mandatory=$true)][string]$OutputRoot, [switch]$SameSizeRefresh, [switch]$BloomReload)
$ErrorActionPreference = 'Stop'
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..\..\..'))
$output = [IO.Path]::GetFullPath($OutputRoot)
# Preparation validates location and requires a nonexistent directory; never reuse old generated sources.
& (Join-Path $PSScriptRoot 'Prepare-NoSaveProbe.ps1') -OutputRoot $output
$taskBuildPath = [Environment]::GetEnvironmentVariable('PATH', 'Process')
[Environment]::SetEnvironmentVariable('PATH', $null, 'Process')
[Environment]::SetEnvironmentVariable('Path', $taskBuildPath, 'Process')
$arguments = @("$repo\HelenProxyDInput8\HelenProxyDInput8.vcxproj", '/t:Build', '/p:Configuration=Release',
    '/p:Platform=Win32', '/p:PlatformToolset=v143', "/p:ForceImportBeforeCppTargets=$PSScriptRoot\NoSaveProbe.targets",
    "/p:NoSaveOutput=$output", '/m:1', '/nodeReuse:false', '/v:minimal', "/flp:logfile=$output\build.log;verbosity=normal")
if ($SameSizeRefresh) { $arguments += '/p:EnableSameSizeRefresh=true' }
if ($BloomReload) { $arguments += '/p:EnableBloomReload=true' }
& "$repo\games\HelenBatmanAA\scripts\Invoke-BatmanConsoleTool.ps1" -FilePath 'C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe' -Arguments $arguments
Get-FileHash -LiteralPath "$output\native\HelenGameHook.dll", "$output\native\dinput8.dll", "$output\native\HelenRuntime.lib" -Algorithm SHA256
