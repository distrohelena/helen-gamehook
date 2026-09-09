param()
$ErrorActionPreference = 'Stop'
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$compiler = 'C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\MSVC\14.44.35207'
$sdk = 'C:\Program Files (x86)\Windows Kits\10'
$version = '10.0.26100.0'
$output = Join-Path $repo ('output\d3d9-policy-' + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $output | Out-Null
Write-Output "FIXTURE_ROOT: $output"
$tool = Join-Path $repo 'games\HelenBatmanAA\scripts\Invoke-BatmanConsoleTool.ps1'
$arguments = @('/nologo','/std:c++20','/EHsc','/MD','/W4','/WX','/DNOMINMAX',
    "/I$repo\include", "/I$compiler\include", "/I$sdk\Include\$version\ucrt", "/I$sdk\Include\$version\shared", "/I$sdk\Include\$version\um",
    "$repo\tests\HelenRuntime.Tests\D3d9PresentationPolicyTests.cpp", "$repo\HelenRuntime\D3d9PresentationPolicy.cpp",
    "/Fo$output\", "/Fe$output\D3d9PresentationPolicyTests.exe", '/link',
    "/LIBPATH:$compiler\lib\x86", "/LIBPATH:$sdk\Lib\$version\ucrt\x86", "/LIBPATH:$sdk\Lib\$version\um\x86", 'kernel32.lib')
& $tool -FilePath "$compiler\bin\Hostx64\x86\cl.exe" -Arguments $arguments
& $tool -FilePath "$output\D3d9PresentationPolicyTests.exe" -Arguments @('run')
