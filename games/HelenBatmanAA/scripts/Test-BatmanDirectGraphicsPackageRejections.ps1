param(
    [Parameter(Mandatory = $true)][string]$CandidateRoot,
    [Parameter(Mandatory = $true)][string]$RuntimeLibraryPath,
    [string]$RoutingCandidateRoot
)

$ErrorActionPreference = 'Stop'
$builderRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\builder'))
$testRoot = Join-Path ([IO.Path]::GetTempPath()) ('HelenDirectGraphicsRejections-' + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $testRoot | Out-Null
$cases = @('duplicate-hook', 'missing-export', 'wrong-signature', 'stale-target', 'legacy-command')
if (-not [string]::IsNullOrWhiteSpace($RoutingCandidateRoot)) {
    $cases += @('malformed-route', 'duplicate-route', 'wrong-target-route', 'stale-dll')
}
foreach ($caseName in $cases) {
    $sourceCandidateRoot = if ($caseName -in @('malformed-route', 'duplicate-route', 'wrong-target-route', 'stale-dll')) { $RoutingCandidateRoot } else { $CandidateRoot }
    $packParent = Join-Path $testRoot $caseName
    New-Item -ItemType Directory -Path $packParent | Out-Null
    Copy-Item -LiteralPath (Join-Path $sourceCandidateRoot 'packs\batman-aa-graphics-options') -Destination $packParent -Recurse
    $buildRoot = Join-Path $packParent 'batman-aa-graphics-options\builds\steam-goty-1.0'
    $jsonPath = Join-Path $buildRoot 'hooks.json'
    $value = Get-Content -LiteralPath $jsonPath -Raw | ConvertFrom-Json
    switch ($caseName) {
        'duplicate-hook' { $value.hooks = @($value.hooks[0], $value.hooks[0]) }
        'missing-export' { $value.hooks[0].blob.relocations[0].source.export = '@HelenProbeDispatch@24' }
        'wrong-signature' { $value.hooks[0].expectedBytes = '8B118B5205' }
        'stale-target' {
            $jsonPath = Join-Path $buildRoot 'files.json'
            $value = Get-Content -LiteralPath $jsonPath -Raw | ConvertFrom-Json
            $value.virtualFiles[0].source.target.sha256 = '0000000000000000000000000000000000000000000000000000000000000000'
        }
        'legacy-command' {
            $jsonPath = Join-Path $buildRoot 'commands.json'
            $value = @{ commands = @(@{ id = 'legacyApply'; name = 'Legacy Apply'; steps = @(@{ kind = 'apply-batman-graphics-config' }) }) }
        }
        'malformed-route' {
            $jsonPath = Join-Path $buildRoot 'build.json'
            $value = Get-Content -LiteralPath $jsonPath -Raw | ConvertFrom-Json
            $value.fileWriteRoutes[0].lifetime = 'persistent'
        }
        'duplicate-route' {
            $jsonPath = Join-Path $buildRoot 'build.json'
            $value = Get-Content -LiteralPath $jsonPath -Raw | ConvertFrom-Json
            $value.fileWriteRoutes = @($value.fileWriteRoutes[0], $value.fileWriteRoutes[0])
        }
        'wrong-target-route' {
            $jsonPath = Join-Path $buildRoot 'build.json'
            $value = Get-Content -LiteralPath $jsonPath -Raw | ConvertFrom-Json
            $value.fileWriteRoutes[0].path = 'Square Enix/Batman Arkham Asylum GOTY/BmGame/Config/Other.ini'
        }
    }
    [IO.File]::WriteAllText($jsonPath, (ConvertTo-Json -InputObject $value -Depth 12), [Text.UTF8Encoding]::new($false))
    $nativeDllPath = Join-Path $sourceCandidateRoot 'HelenGameHook.dll'
    $expectedNativeDllSha256 = (Get-FileHash -LiteralPath $nativeDllPath -Algorithm SHA256).Hash
    $expectedRouteMode = if ($sourceCandidateRoot -eq $RoutingCandidateRoot) { 'engine-config' } else { 'none' }
    if ($caseName -eq 'stale-dll') {
        $nativeDllPath = Join-Path $packParent 'stale.dll'
        $bytes = [IO.File]::ReadAllBytes((Join-Path $sourceCandidateRoot 'HelenGameHook.dll'))
        $bytes[0] = $bytes[0] -bxor 0xFF
        [IO.File]::WriteAllBytes($nativeDllPath, $bytes)
    }
    $rejected = $false
    try {
        & (Join-Path $PSScriptRoot 'Test-BatmanDirectGraphicsPackage.ps1') -PackParent $packParent -BasePath (Join-Path $builderRoot 'extracted\frontend-retail\Frontend.umap') -TargetPath (Join-Path $sourceCandidateRoot 'Frontend-direct-graphics.umap') -NativeDllPath $nativeDllPath -RuntimeLibraryPath $RuntimeLibraryPath -ExpectedRouteMode $expectedRouteMode -ExpectedNativeDllSha256 $expectedNativeDllSha256
    } catch {
        if ($_.Exception.Message -notmatch 'VerifyPackage.exe.*exited with code 1') { throw }
        $rejected = $true
    }
    if (-not $rejected) { throw "Native verifier accepted damaged candidate: $caseName" }
    Write-Output "REJECTED_AS_EXPECTED: $caseName"
}
Write-Output 'BATMAN_DIRECT_PACKAGE_REJECTIONS_PASS'
