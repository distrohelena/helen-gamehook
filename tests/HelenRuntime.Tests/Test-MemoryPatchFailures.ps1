param([ValidateSet('all','write','remove','owned','inline','iat','fatal-checked')][string]$Case = 'all')
$ErrorActionPreference = 'Stop'
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$compiler = 'C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\MSVC\14.44.35207'
$sdk = 'C:\Program Files (x86)\Windows Kits\10'
$version = '10.0.26100.0'
$taskBuildPath = [Environment]::GetEnvironmentVariable('PATH', 'Process')
[Environment]::SetEnvironmentVariable('PATH', $null, 'Process')
[Environment]::SetEnvironmentVariable('Path', "$sdk\bin\$version\x86;$taskBuildPath", 'Process')
$output = Join-Path $repo ('output\memory-patch-' + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $output | Out-Null
Write-Output "FIXTURE_ROOT: $output"
$tool = Join-Path $repo 'games\HelenBatmanAA\scripts\Invoke-BatmanConsoleTool.ps1'
$sources = @((Join-Path $PSScriptRoot 'MemoryPatchFailureTests.cpp'), (Join-Path $repo 'HelenRuntime\Memory.cpp'), (Join-Path $repo 'HelenRuntime\Hook.cpp'))
if (Test-Path (Join-Path $repo 'HelenRuntime\MemoryPatch.cpp')) { $sources += Join-Path $repo 'HelenRuntime\MemoryPatch.cpp' }
$arguments = @('/nologo','/std:c++20','/EHsc','/MD','/W4','/WX','/DUNICODE','/D_UNICODE','/DNOMINMAX',
    "/I$repo\include", "/I$compiler\include", "/I$sdk\Include\$version\ucrt", "/I$sdk\Include\$version\shared", "/I$sdk\Include\$version\um",
    "/FI$PSScriptRoot\PatchFailureWin32.h") + $sources + @(
    "/Fo$output\", "/Fe$output\MemoryPatchFailureTests.exe", '/link', '/MANIFEST:EMBED', "/MANIFESTUAC:level='asInvoker' uiAccess='false'",
    "/LIBPATH:$compiler\lib\x86", "/LIBPATH:$sdk\Lib\$version\ucrt\x86", "/LIBPATH:$sdk\Lib\$version\um\x86", 'kernel32.lib')
& $tool -FilePath "$compiler\bin\Hostx64\x86\cl.exe" -Arguments $arguments
$cases = if ($Case -eq 'all') { @('write','remove','owned','inline','iat','fatal-checked') } else { @($Case) }
foreach ($scenario in $cases) {
    Write-Output "CASE: $scenario"
    & $tool -FilePath "$output\MemoryPatchFailureTests.exe" -Arguments @($scenario)
}
