param()

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'Rebuild-BatmanGraphicsOptionsExperiment.ps1') -FunctionsOnly

# Exercise the actual manifest generator: only catalog reads receive the faster cadence.
$manifest = New-GraphicsProtocolManifest
$observers = @($manifest.hooks.stateObservers)
if ($observers.Count -ne 16) { throw 'Expected the existing sixteen graphics observers.' }
foreach ($observer in $observers) {
    if ($observer.id -ceq 'graphicsObserverDisplayModeCatalog') {
        if ($observer.pollIntervalMs -ne 10) { throw 'Resolution catalog reads must use the approved 10 ms cadence.' }
        if ($observer.dynamicResponse.requests.Count -ne 599 -or $observer.failureResponseValue -ne 4899) {
            throw 'Faster reads must retain the complete existing catalog protocol.'
        }
    } elseif ($observer.pollIntervalMs -ne 50) {
        throw "Existing setting/write observer cadence changed: $($observer.id)"
    }
}
Write-Output 'CATALOG_POLLING_PASS'
