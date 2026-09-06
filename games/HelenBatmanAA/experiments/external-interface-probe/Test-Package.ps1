param([Parameter(Mandatory = $true)][string]$PackageRoot)
$ErrorActionPreference = 'Stop'
$repoRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..\..\..'))
$compilerRoot = 'C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\MSVC\14.44.35207'
$sdkRoot = 'C:\Program Files (x86)\Windows Kits\10'
$sdkVersion = '10.0.26100.0'
$outputRoot = Join-Path $repoRoot 'output\batman-direct-probe\verify'
New-Item -ItemType Directory -Path $outputRoot -Force | Out-Null
$arguments = @('/nologo', '/std:c++20', '/EHsc', '/MD', '/W4', '/WX',
    "/I$repoRoot\include", "/I$compilerRoot\include", "/I$sdkRoot\Include\$sdkVersion\ucrt",
    "/I$sdkRoot\Include\$sdkVersion\shared", "/I$sdkRoot\Include\$sdkVersion\um",
    (Join-Path $PSScriptRoot 'VerifyPackage.cpp'), "/Fo$outputRoot\", "/Fe$outputRoot\VerifyPackage.exe", '/link',
    (Join-Path $repoRoot 'output\batman-direct-probe\bin\HelenRuntime.lib'),
    "/LIBPATH:$compilerRoot\lib\x86", "/LIBPATH:$sdkRoot\Lib\$sdkVersion\ucrt\x86",
    "/LIBPATH:$sdkRoot\Lib\$sdkVersion\um\x86", 'kernel32.lib', 'bcrypt.lib', 'user32.lib', 'advapi32.lib')
& "$compilerRoot\bin\Hostx64\x86\cl.exe" @arguments
if ($LASTEXITCODE -ne 0) { throw "Package verifier compilation failed: $LASTEXITCODE" }
$base = Join-Path $repoRoot 'games\HelenBatmanAA\builder\extracted\frontend-retail\Frontend.umap'
& (Join-Path $outputRoot 'VerifyPackage.exe') $PackageRoot $base
if ($LASTEXITCODE -ne 0) { throw "Native probe package validation failed: $LASTEXITCODE" }
