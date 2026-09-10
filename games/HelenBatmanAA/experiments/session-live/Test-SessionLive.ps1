param([Parameter(Mandatory=$true)][string]$RuntimeLibraryPath,
    [ValidateSet('Delta','Overlay','Backend','Payload','Physx')][string]$Fixture = 'Delta')
$ErrorActionPreference = 'Stop'
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..\..\..'))
$compiler = 'C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\MSVC\14.44.35207'
$sdk = 'C:\Program Files (x86)\Windows Kits\10'
$version = '10.0.26100.0'
$output = Join-Path $repo ('output\session-' + $Fixture.ToLowerInvariant() + '-' + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $output | Out-Null
Write-Output "FIXTURE_ROOT: $output"
$tool = Join-Path $repo 'games\HelenBatmanAA\scripts\Invoke-BatmanConsoleTool.ps1'
$sources = @("$PSScriptRoot\Session${Fixture}Tests.cpp", "$PSScriptRoot\SessionGraphicsDelta.cpp")
if ($Fixture -eq 'Overlay') { $sources += @("$PSScriptRoot\SessionGraphicsOverlay.cpp", "$PSScriptRoot\SessionStagingFile.cpp", "$repo\HelenRuntime\FileWriteRoutingService.cpp") }
if ($Fixture -eq 'Backend') { $sources += "$PSScriptRoot\SessionGraphicsBackend.cpp" }
if ($Fixture -eq 'Payload') { $sources += "$PSScriptRoot\SessionSettingsPayload.cpp" }
if ($Fixture -eq 'Physx') { $sources += "$PSScriptRoot\SessionPhysxLevel.cpp" }
$arguments = @('/nologo','/std:c++20','/EHsc','/MD','/W4','/WX','/DUNICODE','/D_UNICODE','/DNOMINMAX',
    "/I$repo\include", "/I$compiler\include", "/I$sdk\Include\$version\ucrt", "/I$sdk\Include\$version\shared", "/I$sdk\Include\$version\um") + $sources + @(
    "/Fo$output\", "/Fe$output\Fixture.exe", '/link', '/LTCG', $RuntimeLibraryPath,
    "/LIBPATH:$compiler\lib\x86", "/LIBPATH:$sdk\Lib\$version\ucrt\x86", "/LIBPATH:$sdk\Lib\$version\um\x86",
    'kernel32.lib','user32.lib','bcrypt.lib','advapi32.lib','shell32.lib','ole32.lib')
& $tool -FilePath "$compiler\bin\Hostx64\x86\cl.exe" -Arguments $arguments
& $tool -FilePath "$output\Fixture.exe" -Arguments @($output)
