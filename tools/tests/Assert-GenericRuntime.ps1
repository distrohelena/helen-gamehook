Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

<#
.SYNOPSIS
Fails when the generic runtime still depends on the removed per-game module architecture.
#>

# Returns the repository path resolved from this script location.
function Get-RepositoryRoot {
    return (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
}

# Adds a failure message when the provided condition is true.
function Add-Failure {
    param(
        [System.Collections.Generic.List[string]]$Failures,
        [bool]$Condition,
        [string]$Message
    )

    if ($Condition) {
        $Failures.Add($Message)
    }
}

# Validates that the generic runtime no longer references per-game module infrastructure.
function Assert-GenericRuntime {
    $repositoryRoot = Get-RepositoryRoot
    $failures = [System.Collections.Generic.List[string]]::new()

    $gameModuleHeader = Join-Path $repositoryRoot 'include\HelenHook\GameModule.h'
    Add-Failure -Failures $failures -Condition (Test-Path $gameModuleHeader) -Message 'Legacy header still exists: include/HelenHook/GameModule.h'

    $batmanProjectPath = Join-Path $repositoryRoot 'HelenBatmanAA\HelenBatmanAA.vcxproj'
    Add-Failure -Failures $failures -Condition (Test-Path $batmanProjectPath) -Message 'Legacy project still exists: HelenBatmanAA/HelenBatmanAA.vcxproj'

    $batmanSourcePath = Join-Path $repositoryRoot 'HelenBatmanAA\HelenBatmanAA.cpp'
    Add-Failure -Failures $failures -Condition (Test-Path $batmanSourcePath) -Message 'Legacy source still exists: HelenBatmanAA/HelenBatmanAA.cpp'

    $runtimeSourcePath = Join-Path $repositoryRoot 'HelenGameHook\HelenGameHook.cpp'
    $runtimeSource = Get-Content $runtimeSourcePath -Raw

    Add-Failure -Failures $failures -Condition ($runtimeSource -match '\bmodules_directory\b') -Message 'HelenGameHook.cpp still references modules_directory'
    Add-Failure -Failures $failures -Condition ($runtimeSource -match '\bGameModule\b') -Message 'HelenGameHook.cpp still references GameModule'
    Add-Failure -Failures $failures -Condition ($runtimeSource -match '\bHelenBatmanAA\b') -Message 'HelenGameHook.cpp still references HelenBatmanAA'
    Add-Failure -Failures $failures -Condition ($runtimeSource -match 'DLL_PROCESS_DETACH[\s\S]*?HelenShutdown\s*\(') -Message 'HelenGameHook.cpp still calls HelenShutdown() from DLL_PROCESS_DETACH'

    $runtimeProjectPath = Join-Path $repositoryRoot 'HelenGameHook\HelenGameHook.vcxproj'
    $runtimeProject = Get-Content $runtimeProjectPath -Raw
    Add-Failure -Failures $failures -Condition ($runtimeProject -match 'GameModule\.h') -Message 'HelenGameHook.vcxproj still references GameModule.h'

    $solutionPath = Join-Path $repositoryRoot 'HelenGameHook.sln'
    $solutionText = Get-Content $solutionPath -Raw
    Add-Failure -Failures $failures -Condition ($solutionText -match 'HelenBatmanAA') -Message 'HelenGameHook.sln still references HelenBatmanAA'

    if ($failures.Count -gt 0) {
        foreach ($failure in $failures) {
            Write-Host "[FAIL] $failure"
        }

        throw "Generic runtime architecture guard failed with $($failures.Count) violation(s)."
    }

    Write-Host 'Generic runtime architecture guard passed.'
}

Assert-GenericRuntime
