param([Parameter(Mandatory=$true)][string]$OutputRoot)
$ErrorActionPreference = 'Stop'
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..\..\..'))
$output = [IO.Path]::GetFullPath($OutputRoot)
$allowed = [IO.Path]::GetFullPath((Join-Path $repo 'output')) + '\'
if (-not $output.StartsWith($allowed,[StringComparison]::OrdinalIgnoreCase) -or (Test-Path -LiteralPath $output)) {
    throw 'Use a fresh nonexistent directory inside repository output.'
}
New-Item -ItemType Directory -Path $output | Out-Null
$sourceRoots = @('include','HelenRuntime','HelenGameHook','HelenProxyDInput8',
    'games\HelenBatmanAA\experiments\session-live','games\HelenBatmanAA\experiments\vsync-refresh',
    'games\HelenBatmanAA\experiments\windowed-resolution','games\HelenBatmanAA\experiments\bloom-reload')
$sourceFiles = foreach ($sourceRoot in $sourceRoots) {
    Get-ChildItem -LiteralPath (Join-Path $repo $sourceRoot) -File -Recurse |
        Where-Object { $_.Extension -in @('.h','.cpp','.targets','.vcxproj','.ps1') }
}
$sourceHashes = @($sourceFiles | Sort-Object FullName -Unique | ForEach-Object {
    [ordered]@{ path=$_.FullName; sha256=(Get-FileHash -LiteralPath $_.FullName).Hash }
})
$taskBuildPath = [Environment]::GetEnvironmentVariable('PATH','Process')
[Environment]::SetEnvironmentVariable('PATH',$null,'Process')
[Environment]::SetEnvironmentVariable('Path',$taskBuildPath,'Process')
$env:CL = '/MP4'
$arguments = @("$repo\HelenProxyDInput8\HelenProxyDInput8.vcxproj",'/t:Build','/p:Configuration=Release',
    '/p:Platform=Win32','/p:PlatformToolset=v143',"/p:ForceImportBeforeCppTargets=$PSScriptRoot\SessionLive.targets",
    "/p:SessionLiveOutput=$output",'/m:1','/nodeReuse:false','/v:minimal',"/flp:logfile=$output\build.log;verbosity=normal")
& "$repo\games\HelenBatmanAA\scripts\Invoke-BatmanConsoleTool.ps1" -FilePath 'C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe' -Arguments $arguments
foreach ($source in $sourceHashes) {
    if ((Get-FileHash -LiteralPath $source.path).Hash -ne $source.sha256) { throw "Source changed while compiling: $($source.path)" }
}
$manifest = [ordered]@{ mode='RoutingSessionLive'; sources=$sourceHashes; artifacts=@(
    Get-FileHash -LiteralPath "$output\native\HelenGameHook.dll","$output\native\dinput8.dll","$output\native\HelenRuntime.lib" |
        ForEach-Object { [ordered]@{ path=$_.Path; sha256=$_.Hash } }
) }
[IO.File]::WriteAllText((Join-Path $output 'source-manifest.json'),(ConvertTo-Json $manifest -Depth 8),[Text.UTF8Encoding]::new($false))
Get-FileHash -LiteralPath "$output\native\HelenGameHook.dll","$output\native\dinput8.dll","$output\native\HelenRuntime.lib" -Algorithm SHA256
