param(
    [Parameter(Mandatory = $true)][string]$PackParent,
    [Parameter(Mandatory = $true)][string]$BasePath,
    [Parameter(Mandatory = $true)][string]$TargetPath,
    [Parameter(Mandatory = $true)][string]$NativeDllPath,
    [Parameter(Mandatory = $true)][string]$RuntimeLibraryPath,
    [ValidateSet('none', 'engine-config')][string]$ExpectedRouteMode = 'none',
    [string]$ExpectedNativeDllSha256
)

$ErrorActionPreference = 'Stop'
$repoRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..\..'))
$compilerRoot = 'C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\MSVC\14.44.35207'
$sdkRoot = 'C:\Program Files (x86)\Windows Kits\10'
$sdkVersion = '10.0.26100.0'
$expectedHash = if ([string]::IsNullOrWhiteSpace($ExpectedNativeDllSha256)) {
    if ($ExpectedRouteMode -eq 'engine-config') {
        throw 'Routing verification requires the fresh source DLL SHA-256 explicitly.'
    }
    (Get-FileHash -LiteralPath $NativeDllPath -Algorithm SHA256).Hash.ToLowerInvariant()
} else {
    $ExpectedNativeDllSha256.ToLowerInvariant()
}
$verificationRoot = Join-Path ([IO.Path]::GetTempPath()) ('HelenDirectGraphicsVerify-' + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $verificationRoot | Out-Null
$compilerArguments = @('/nologo', '/std:c++20', '/EHsc', '/MD', '/W4', '/WX',
    "/I$repoRoot\include", "/I$compilerRoot\include", "/I$sdkRoot\Include\$sdkVersion\ucrt",
    "/I$sdkRoot\Include\$sdkVersion\shared", "/I$sdkRoot\Include\$sdkVersion\um",
    (Join-Path $repoRoot 'tests\HelenRuntime.Tests\BatmanDirectGraphicsPackageTests.cpp'),
    "/Fo$verificationRoot\", "/Fe$verificationRoot\VerifyPackage.exe", '/link', $RuntimeLibraryPath,
    "/LIBPATH:$compilerRoot\lib\x86", "/LIBPATH:$sdkRoot\Lib\$sdkVersion\ucrt\x86",
    "/LIBPATH:$sdkRoot\Lib\$sdkVersion\um\x86", 'kernel32.lib', 'bcrypt.lib', 'user32.lib', 'advapi32.lib')
& (Join-Path $PSScriptRoot 'Invoke-BatmanConsoleTool.ps1') -FilePath "$compilerRoot\bin\Hostx64\x86\cl.exe" -Arguments $compilerArguments
& (Join-Path $PSScriptRoot 'Invoke-BatmanConsoleTool.ps1') -FilePath (Join-Path $verificationRoot 'VerifyPackage.exe') -Arguments @(
    $PackParent, $BasePath, $TargetPath, $NativeDllPath, $expectedHash, $ExpectedRouteMode)
