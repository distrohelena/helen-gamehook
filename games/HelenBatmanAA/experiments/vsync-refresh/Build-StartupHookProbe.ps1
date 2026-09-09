param([Parameter(Mandatory=$true)][string]$OutputRoot, [switch]$NormalControl)
$ErrorActionPreference = 'Stop'
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..\..\..'))
$output = [IO.Path]::GetFullPath($OutputRoot)
$allowed = [IO.Path]::GetFullPath((Join-Path $repo 'output')) + '\'
if (-not $output.StartsWith($allowed, [StringComparison]::OrdinalIgnoreCase)) { throw 'Output must be inside repository output.' }
if (Test-Path -LiteralPath $output) { throw 'Use a fresh nonexistent output directory.' }
New-Item -ItemType Directory -Path $output | Out-Null
$tool = Join-Path $repo 'games\HelenBatmanAA\scripts\Invoke-BatmanConsoleTool.ps1'
$buildArguments = @("$repo\HelenProxyDInput8\HelenProxyDInput8.vcxproj", '/t:Build', '/p:Configuration=Release', '/p:Platform=Win32', '/p:PlatformToolset=v143',
    "/p:ForceImportBeforeCppTargets=$PSScriptRoot\StartupHookProbe.targets", "/p:StartupHookOutput=$output", '/m:1', '/nodeReuse:false', '/v:minimal', "/flp:logfile=$output\build.log;verbosity=normal")
if ($NormalControl) { $buildArguments += '/p:EnableStartupHooks=false' }
# Normalize this child-build process's key spelling to match MSVC's Path entry.
# Preserve the value; never alter user or machine environment configuration.
$taskBuildPath = [Environment]::GetEnvironmentVariable('PATH', 'Process')
[Environment]::SetEnvironmentVariable('PATH', $null, 'Process')
[Environment]::SetEnvironmentVariable('Path', $taskBuildPath, 'Process')
& $tool -FilePath 'C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe' -Arguments $buildArguments
Get-FileHash -LiteralPath "$output\native\HelenGameHook.dll", "$output\native\dinput8.dll", "$output\native\HelenRuntime.lib" -Algorithm SHA256
