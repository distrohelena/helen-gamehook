param()
$ErrorActionPreference = 'Stop'
$compilerRoot = 'C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\MSVC\14.44.35207'
$sdkRoot = 'C:\Program Files (x86)\Windows Kits\10'
$sdkVersion = '10.0.26100.0'
$outputRoot = Join-Path $env:TEMP ('BatmanDirectProbe-' + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $outputRoot | Out-Null
$arguments = @('/nologo', '/std:c++20', '/EHsc', '/MT', '/W4', '/WX', '/DUNICODE', '/D_UNICODE',
    "/I$compilerRoot\include", "/I$sdkRoot\Include\$sdkVersion\ucrt",
    "/I$sdkRoot\Include\$sdkVersion\shared", "/I$sdkRoot\Include\$sdkVersion\um",
    (Join-Path $PSScriptRoot 'FullscreenProbe.cpp'), (Join-Path $PSScriptRoot 'DispatchAdapter.cpp'), (Join-Path $PSScriptRoot 'ProbeTests.cpp'),
    "/Fo$outputRoot\", "/Fe$outputRoot\ProbeTests.exe", '/link',
    "/LIBPATH:$compilerRoot\lib\x86", "/LIBPATH:$sdkRoot\Lib\$sdkVersion\ucrt\x86",
    "/LIBPATH:$sdkRoot\Lib\$sdkVersion\um\x86", 'kernel32.lib')
& "$compilerRoot\bin\Hostx64\x86\cl.exe" @arguments
if ($LASTEXITCODE -ne 0) { throw "Probe compilation failed: $LASTEXITCODE" }
& (Join-Path $outputRoot 'ProbeTests.exe') (Join-Path $PSScriptRoot 'fixtures')
if ($LASTEXITCODE -ne 0) { throw "Probe tests failed: $LASTEXITCODE" }
Write-Output "Probe test executable: $outputRoot\ProbeTests.exe"
