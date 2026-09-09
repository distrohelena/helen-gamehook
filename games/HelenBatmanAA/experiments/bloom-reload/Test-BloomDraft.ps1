param([switch]$Overlay, [switch]$Binding)
$ErrorActionPreference = 'Stop'
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..\..\..'))
$compiler = 'C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\MSVC\14.44.35207'
$sdk = 'C:\Program Files (x86)\Windows Kits\10'
$version = '10.0.26100.0'
$output = Join-Path $repo ('output\bloom-draft-' + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $output | Out-Null
Write-Output "FIXTURE_ROOT: $output"
$tool = Join-Path $repo 'games\HelenBatmanAA\scripts\Invoke-BatmanConsoleTool.ps1'
$arguments = @('/nologo','/std:c++20','/EHsc','/MD','/W4','/WX','/O2','/DUNICODE','/D_UNICODE','/DNOMINMAX',
    "/I$repo\include", "/I$compiler\include", "/I$sdk\Include\$version\ucrt", "/I$sdk\Include\$version\shared", "/I$sdk\Include\$version\um",
    "/Fo$output\", "/Fe$output\BloomDraftTests.exe", '/link',
    "/LIBPATH:$compiler\lib\x86", "/LIBPATH:$sdk\Lib\$version\ucrt\x86", "/LIBPATH:$sdk\Lib\$version\um\x86", 'kernel32.lib')
$sources = if ($Binding) { @("$PSScriptRoot\BloomIniBindingTests.cpp", "$PSScriptRoot\BloomIniBinding.cpp") } elseif ($Overlay) { @("$PSScriptRoot\BloomOverlayEditTests.cpp", "$PSScriptRoot\BloomOverlayEdit.cpp") } else {
    @("$PSScriptRoot\BloomDraftTests.cpp", "$PSScriptRoot\BloomDraft.cpp", "$repo\HelenRuntime\BatmanGraphicsDraftState.cpp")
}
& $tool -FilePath "$compiler\bin\Hostx64\x86\cl.exe" -Arguments ($sources + $arguments)
& $tool -FilePath "$output\BloomDraftTests.exe" -Arguments @("$output\fixtures")
