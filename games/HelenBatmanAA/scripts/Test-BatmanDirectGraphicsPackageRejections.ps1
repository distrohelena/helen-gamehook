param(
    [Parameter(Mandatory = $true)][string]$CandidateRoot,
    [Parameter(Mandatory = $true)][string]$RuntimeLibraryPath
)

$ErrorActionPreference = 'Stop'
$builderRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\builder'))
$testRoot = Join-Path ([IO.Path]::GetTempPath()) ('HelenDirectGraphicsRejections-' + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $testRoot | Out-Null
foreach ($caseName in @('duplicate-hook', 'missing-export', 'wrong-signature', 'stale-target', 'legacy-command')) {
    $packParent = Join-Path $testRoot $caseName
    New-Item -ItemType Directory -Path $packParent | Out-Null
    Copy-Item -LiteralPath (Join-Path $CandidateRoot 'packs\batman-aa-graphics-options') -Destination $packParent -Recurse
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
    }
    [IO.File]::WriteAllText($jsonPath, (ConvertTo-Json -InputObject $value -Depth 12), [Text.UTF8Encoding]::new($false))
    $rejected = $false
    try {
        & (Join-Path $PSScriptRoot 'Test-BatmanDirectGraphicsPackage.ps1') -PackParent $packParent -BasePath (Join-Path $builderRoot 'extracted\frontend-retail\Frontend.umap') -TargetPath (Join-Path $CandidateRoot 'Frontend-direct-graphics.umap') -NativeDllPath (Join-Path $CandidateRoot 'HelenGameHook.dll') -RuntimeLibraryPath $RuntimeLibraryPath
    } catch {
        if ($_.Exception.Message -notmatch 'VerifyPackage.exe.*exited with code 1') { throw }
        $rejected = $true
    }
    if (-not $rejected) { throw "Native verifier accepted damaged candidate: $caseName" }
    Write-Output "REJECTED_AS_EXPECTED: $caseName"
}
Write-Output 'BATMAN_DIRECT_PACKAGE_REJECTIONS_PASS'
