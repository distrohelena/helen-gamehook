param([Parameter(Mandatory=$true)][string]$ArtifactRoot, [switch]$NormalControl)
$ErrorActionPreference = 'Stop'
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..\..\..'))
$artifacts = [IO.Path]::GetFullPath($ArtifactRoot)
$compiler = 'C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\MSVC\14.44.35207'
$sdk = 'C:\Program Files (x86)\Windows Kits\10'
$version = '10.0.26100.0'
$output = Join-Path $repo ('output\runtime-startup-' + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $output | Out-Null
Write-Output "REAL_RUNTIME_FIXTURE_ROOT: $output"
foreach ($name in @('HelenGameHook.dll', 'dinput8.dll')) {
    Copy-Item -LiteralPath (Join-Path $artifacts $name) -Destination (Join-Path $output $name)
    if ((Get-FileHash -LiteralPath (Join-Path $artifacts $name)).Hash -ne (Get-FileHash -LiteralPath (Join-Path $output $name)).Hash) { throw 'Artifact copy hash mismatch.' }
}
$tool = Join-Path $repo 'games\HelenBatmanAA\scripts\Invoke-BatmanConsoleTool.ps1'
$arguments = @('/nologo','/std:c++20','/EHsc','/MD','/W4','/WX','/DUNICODE','/D_UNICODE','/DNOMINMAX','/DWIN32_LEAN_AND_MEAN',
    "/I$repo\include", "/I$compiler\include", "/I$sdk\Include\$version\ucrt", "/I$sdk\Include\$version\shared", "/I$sdk\Include\$version\um",
    "$PSScriptRoot\RuntimeStartupHost.cpp", "/Fo$output\", "/Fe$output\RuntimeStartupHost.exe")
if (-not $NormalControl) { $arguments += '/DHELEN_ENABLE_STARTUP_HOOKS' }
$arguments += @('/link', "$artifacts\HelenGameHook.lib", "$artifacts\dinput8.lib",
    "/LIBPATH:$compiler\lib\x86", "/LIBPATH:$sdk\Lib\$version\ucrt\x86", "/LIBPATH:$sdk\Lib\$version\um\x86", 'kernel32.lib')
& $tool -FilePath "$compiler\bin\Hostx64\x86\cl.exe" -Arguments $arguments
& $tool -FilePath "$output\RuntimeStartupHost.exe" -Arguments @()
