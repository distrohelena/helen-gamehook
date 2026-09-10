param([Parameter(Mandatory=$true)][string]$OutputRoot)
$ErrorActionPreference = 'Stop'
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..\..\..'))
$output = [IO.Path]::GetFullPath($OutputRoot)
if (-not $output.StartsWith(($repo + '\output\'), [StringComparison]::OrdinalIgnoreCase) -or (Test-Path -LiteralPath $output)) {
    throw 'Use a fresh output directory inside repository output.'
}
New-Item -ItemType Directory -Path $output | Out-Null
$tool = Join-Path $repo 'games\HelenBatmanAA\scripts\Invoke-BatmanConsoleTool.ps1'
# Preserve one PATH spelling when Windows inherited both variants.
$processPath = [Environment]::GetEnvironmentVariable('PATH', 'Process')
Remove-Item Env:Path -ErrorAction SilentlyContinue
Remove-Item Env:PATH -ErrorAction SilentlyContinue
$env:PATH = $processPath
$env:CL = '/MP4'
& $tool -FilePath 'C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe' -Arguments @(
    "$repo\tests\HelenRuntime.Tests\HelenRuntime.Tests.vcxproj", '/t:Build', '/p:Configuration=Release', '/p:Platform=Win32',
    '/p:PlatformToolset=v143', "/p:ForceImportBeforeCppTargets=$PSScriptRoot\OutputOnly.targets", "/p:SessionLiveOutput=$output",
    '/m:1', '/nodeReuse:false', '/v:minimal', "/flp:logfile=$output\build.log;verbosity=normal")
& $tool -FilePath "$output\native\tests\HelenRuntimeTests.exe" -Arguments @()
