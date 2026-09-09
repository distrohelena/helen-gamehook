param([switch]$DecisionBridge, [switch]$Scalar, [switch]$Concurrency, [switch]$Controller, [switch]$Optimized)
$ErrorActionPreference = 'Stop'
if (([int]$DecisionBridge.IsPresent + [int]$Scalar.IsPresent + [int]$Concurrency.IsPresent + [int]$Controller.IsPresent) -gt 1) { throw 'Select only one fixture.' }
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..\..\..'))
$compiler = 'C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\MSVC\14.44.35207'
$sdk = 'C:\Program Files (x86)\Windows Kits\10'
$version = '10.0.26100.0'
$output = Join-Path $repo ('output\vsync-activation-' + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $output | Out-Null
Write-Output "FIXTURE_ROOT: $output"
$tool = Join-Path $repo 'games\HelenBatmanAA\scripts\Invoke-BatmanConsoleTool.ps1'
$fixtureSource = if ($Controller) { 'RefreshActivationTests.cpp' } elseif ($DecisionBridge) { 'DecisionBridgeTests.cpp' } elseif ($Scalar) { 'ScalarTests.cpp' } elseif ($Concurrency) { 'RequestConcurrencyTests.cpp' } else { 'ActivationTests.cpp' }
$arguments = @('/nologo','/std:c++20','/EHsc','/MD','/W4','/WX','/DUNICODE','/D_UNICODE','/DNOMINMAX',
    "/I$compiler\include", "/I$sdk\Include\$version\ucrt", "/I$sdk\Include\$version\shared", "/I$sdk\Include\$version\um",
    (Join-Path $PSScriptRoot $fixtureSource), (Join-Path $PSScriptRoot 'ActivationMatcher.cpp'),
    (Join-Path $PSScriptRoot 'RefreshRequest.cpp'),
    (Join-Path $PSScriptRoot 'RefreshRequestScope.cpp'),
    (Join-Path $PSScriptRoot 'DecisionBridge.cpp'),
    (Join-Path $PSScriptRoot 'AnchoredResize.cpp'),
    (Join-Path $PSScriptRoot 'RefreshActivation.cpp'),
    (Join-Path $PSScriptRoot 'VsyncScalar.cpp'),
    "/Fo$output\", "/Fe$output\ActivationTests.exe", '/link',
    "/LIBPATH:$compiler\lib\x86", "/LIBPATH:$sdk\Lib\$version\ucrt\x86", "/LIBPATH:$sdk\Lib\$version\um\x86", 'kernel32.lib')
if ($Optimized) { $arguments = @('/O2', '/GL') + $arguments }
& $tool -FilePath "$compiler\bin\Hostx64\x86\cl.exe" -Arguments $arguments
& $tool -FilePath "$output\ActivationTests.exe" -Arguments @()
