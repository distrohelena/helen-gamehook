param([Parameter(Mandatory=$true)][string]$RuntimeLibraryPath, [switch]$FailureCases, [switch]$InitialSurfaceHooks, [switch]$PresentationOverride, [switch]$PresentationOnly)
$ErrorActionPreference = 'Stop'
if ($PresentationOverride -and ($FailureCases -or $InitialSurfaceHooks)) { throw 'Run presentation override separately from other fixture modes.' }
if ($PresentationOnly -and ($FailureCases -or $InitialSurfaceHooks -or $PresentationOverride)) { throw 'Run pack-free presentation separately.' }
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$compiler = 'C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\MSVC\14.44.35207'
$sdk = 'C:\Program Files (x86)\Windows Kits\10'
$version = '10.0.26100.0'
$output = Join-Path $repo ('output\d3d9-reset-fixture-' + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $output | Out-Null
Write-Output "FIXTURE_ROOT: $output"
Write-Output ('RUNTIME_LIBRARY_SHA256: ' + (Get-FileHash -LiteralPath $RuntimeLibraryPath -Algorithm SHA256).Hash)
$tool = Join-Path $repo 'games\HelenBatmanAA\scripts\Invoke-BatmanConsoleTool.ps1'
$fixtureSource = if ($FailureCases) { 'HelenRuntime.Tests\D3d9ReplacementFailureTests.cpp' } else { 'HelenRuntime.Tests\D3d9ReplacementResetTests.cpp' }
$arguments = @('/nologo','/std:c++20','/EHsc','/MD','/W4','/WX','/DUNICODE','/D_UNICODE','/DNOMINMAX',
    "/I$repo\include", "/I$compiler\include", "/I$sdk\Include\$version\ucrt", "/I$sdk\Include\$version\shared", "/I$sdk\Include\$version\um", "/I$sdk\Include\$version\winrt",
    (Join-Path $PSScriptRoot $fixtureSource), "/Fo$output\", "/Fe$output\D3d9ReplacementResetTests.exe", '/link', $RuntimeLibraryPath,
    "/LIBPATH:$compiler\lib\x86", "/LIBPATH:$sdk\Lib\$version\ucrt\x86", "/LIBPATH:$sdk\Lib\$version\um\x86",
    'kernel32.lib','user32.lib','bcrypt.lib','advapi32.lib','shell32.lib','ole32.lib','d3d9.lib')
& $tool -FilePath "$compiler\bin\Hostx64\x86\cl.exe" -Arguments $arguments
$fixtureArguments = @("$output\fixtures")
if ($InitialSurfaceHooks) { $fixtureArguments += 'initial-surface-hooks' }
if ($PresentationOverride) { $fixtureArguments += 'presentation-override' }
if ($PresentationOnly) { $fixtureArguments += 'presentation-only' }
& $tool -FilePath "$output\D3d9ReplacementResetTests.exe" -Arguments $fixtureArguments
