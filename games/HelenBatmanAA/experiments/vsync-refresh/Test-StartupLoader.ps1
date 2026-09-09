param([switch]$FailureCase)
$ErrorActionPreference = 'Stop'
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..\..\..'))
$compiler = 'C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\MSVC\14.44.35207'
$sdk = 'C:\Program Files (x86)\Windows Kits\10'
$version = '10.0.26100.0'
$output = Join-Path $repo ('output\startup-loader-' + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $output | Out-Null
Write-Output "LOADER_FIXTURE_ROOT: $output"
$tool = Join-Path $repo 'games\HelenBatmanAA\scripts\Invoke-BatmanConsoleTool.ps1'
$common = @('/nologo','/std:c++20','/EHsc','/MD','/W4','/WX','/DUNICODE','/D_UNICODE','/DNOMINMAX','/DWIN32_LEAN_AND_MEAN','/DHELEN_ENABLE_STARTUP_HOOKS',
    "/I$repo\include", "/I$compiler\include", "/I$sdk\Include\$version\ucrt", "/I$sdk\Include\$version\shared", "/I$sdk\Include\$version\um", "/Fo$output\")
if ($FailureCase) { $common += '/DFIXTURE_FAIL_INIT' }
$libs = @("/LIBPATH:$compiler\lib\x86", "/LIBPATH:$sdk\Lib\$version\ucrt\x86", "/LIBPATH:$sdk\Lib\$version\um\x86", 'kernel32.lib')
& $tool -FilePath "$compiler\bin\Hostx64\x86\cl.exe" -Arguments ($common + @('/LD', "$PSScriptRoot\LoaderFixtureRuntime.cpp", "$repo\HelenRuntime\StartupHookLifetime.cpp", "$repo\HelenRuntime\ProcessModulePin.cpp", "/Fe$output\FixtureRuntime.dll", '/link') + $libs)
& $tool -FilePath "$compiler\bin\Hostx64\x86\cl.exe" -Arguments ($common + @('/LD', "$repo\HelenProxyDInput8\HelenProxyDInput8.cpp", "/Fe$output\dinput8.dll", '/link', "$output\FixtureRuntime.lib", "/DEF:$repo\HelenProxyDInput8\HelenProxyDInput8.def") + $libs)
& $tool -FilePath "$compiler\bin\Hostx64\x86\cl.exe" -Arguments ($common + @("$PSScriptRoot\LoaderFixtureHost.cpp", "/Fe$output\StaticHost.exe", '/link', "$output\FixtureRuntime.lib", "$output\dinput8.lib") + $libs)
& $tool -FilePath "$compiler\bin\Hostx64\x86\cl.exe" -Arguments ($common + @('/DFIXTURE_DYNAMIC_HOST', "$PSScriptRoot\LoaderFixtureHost.cpp", "/Fe$output\DynamicHost.exe", '/link') + $libs)
& $tool -FilePath "$output\StaticHost.exe" -Arguments @()
& $tool -FilePath "$output\DynamicHost.exe" -Arguments @()
& $tool -FilePath "$output\DynamicHost.exe" -Arguments @('pin')
