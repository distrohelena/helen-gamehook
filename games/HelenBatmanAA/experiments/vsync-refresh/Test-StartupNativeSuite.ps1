param([Parameter(Mandatory=$true)][string]$ArtifactRoot)
$ErrorActionPreference = 'Stop'
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..\..\..'))
$artifacts = [IO.Path]::GetFullPath($ArtifactRoot)
if (-not $artifacts.StartsWith((Join-Path $repo 'output') + '\', [StringComparison]::OrdinalIgnoreCase)) { throw 'Artifacts must be under repository output.' }
$output = Join-Path $artifacts ('tests-' + [Guid]::NewGuid().ToString('N'))
if (Test-Path -LiteralPath $output) { throw 'Native test output must be fresh.' }
New-Item -ItemType Directory -Path $output | Out-Null
Copy-Item -LiteralPath "$artifacts\HelenRuntime.lib" -Destination "$output\HelenRuntime.lib"
if ((Get-FileHash "$artifacts\HelenRuntime.lib").Hash -ne (Get-FileHash "$output\HelenRuntime.lib").Hash) { throw 'Library copy hash mismatch.' }
$tool = Join-Path $repo 'games\HelenBatmanAA\scripts\Invoke-BatmanConsoleTool.ps1'
$taskBuildPath = [Environment]::GetEnvironmentVariable('PATH', 'Process')
[Environment]::SetEnvironmentVariable('PATH', $null, 'Process')
[Environment]::SetEnvironmentVariable('Path', $taskBuildPath, 'Process')
$arguments = @("$repo\tests\HelenRuntime.Tests\HelenRuntime.Tests.vcxproj", '/t:Build', '/p:Configuration=Release', '/p:Platform=Win32', '/p:PlatformToolset=v143',
    '/p:BuildProjectReferences=false', "/p:ForceImportBeforeCppTargets=$PSScriptRoot\StartupNativeSuite.targets", "/p:StartupNativeOutput=$output", '/m:1', '/nodeReuse:false', '/v:minimal', "/flp:logfile=$output\build.log;verbosity=normal")
& $tool -FilePath 'C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe' -Arguments $arguments
& $tool -FilePath "$output\HelenRuntimeTests.exe" -Arguments @()
