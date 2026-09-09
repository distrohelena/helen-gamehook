param()
$ErrorActionPreference = 'Stop'
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..\..\..'))
$compiler = 'C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\MSVC\14.44.35207'
$sdk = 'C:\Program Files (x86)\Windows Kits\10'
$version = '10.0.26100.0'
$output = Join-Path $repo ('output\vsync-request-' + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $output | Out-Null
Write-Output "FIXTURE_ROOT: $output"
$tool = Join-Path $repo 'games\HelenBatmanAA\scripts\Invoke-BatmanConsoleTool.ps1'
$arguments = @('/nologo','/std:c++20','/EHsc','/MD','/W4','/WX','/DNOMINMAX',
    "/I$repo\include", "/I$compiler\include", "/I$sdk\Include\$version\ucrt", "/I$sdk\Include\$version\shared", "/I$sdk\Include\$version\um",
    "$PSScriptRoot\NoSaveVsyncRequestTests.cpp", "$PSScriptRoot\NoSaveVsyncRequest.cpp", "$repo\HelenRuntime\BatmanGraphicsDraftState.cpp",
    "/Fo$output\", "/Fe$output\NoSaveVsyncRequestTests.exe", '/link',
    "/LIBPATH:$compiler\lib\x86", "/LIBPATH:$sdk\Lib\$version\ucrt\x86", "/LIBPATH:$sdk\Lib\$version\um\x86", 'kernel32.lib')
& $tool -FilePath "$compiler\bin\Hostx64\x86\cl.exe" -Arguments $arguments
& $tool -FilePath "$output\NoSaveVsyncRequestTests.exe" -Arguments @('run')
