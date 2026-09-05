param()
$ErrorActionPreference = 'Stop'
$repositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..\..')).Path
$compilerRoot = 'C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\MSVC\14.44.35207'
$sdkRoot = 'C:\Program Files (x86)\Windows Kits\10'
$sdkVersion = '10.0.26100.0'
$outputRoot = Join-Path $env:TEMP ('BatmanGraphicsCodec-' + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $outputRoot | Out-Null
$arguments = @('/nologo', '/std:c++20', '/EHsc', '/MT', '/W4', '/WX', '/DUNICODE', '/D_UNICODE', '/DNOMINMAX',
    "/I$repositoryRoot\HelenGameHook", "/I$compilerRoot\include", "/I$sdkRoot\Include\$sdkVersion\ucrt",
    "/I$sdkRoot\Include\$sdkVersion\shared", "/I$sdkRoot\Include\$sdkVersion\um",
    "$repositoryRoot\HelenGameHook\BatmanGraphicsPrimitiveCodec.cpp",
    "$repositoryRoot\tests\HelenRuntime.Tests\BatmanGraphicsPrimitiveCodecTests.cpp",
    "/Fo$outputRoot\", "/Fe$outputRoot\CodecTests.exe", '/link',
    "/LIBPATH:$compilerRoot\lib\x86", "/LIBPATH:$sdkRoot\Lib\$sdkVersion\ucrt\x86",
    "/LIBPATH:$sdkRoot\Lib\$sdkVersion\um\x86", 'kernel32.lib')
& "$compilerRoot\bin\Hostx64\x86\cl.exe" @arguments
if ($LASTEXITCODE -ne 0) { throw "Codec compilation failed: $LASTEXITCODE" }
& (Join-Path $outputRoot 'CodecTests.exe')
if ($LASTEXITCODE -ne 0) { throw "Codec tests failed: $LASTEXITCODE" }
Write-Output "Codec test executable: $outputRoot\CodecTests.exe"
