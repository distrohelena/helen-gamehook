param(
    [Parameter(Mandatory=$true)][string]$SnapshotRoot,
    [string]$CompareTo
)
$ErrorActionPreference = 'Stop'
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..\..\..'))
$output = [IO.Path]::GetFullPath($SnapshotRoot)
if (-not $output.StartsWith(([IO.Path]::GetFullPath((Join-Path $repo 'output')) + '\'), [StringComparison]::OrdinalIgnoreCase)) { throw 'Snapshots must be inside repository output.' }
if (Test-Path -LiteralPath $output) { throw 'Snapshot must be new; previous evidence is never overwritten.' }
$roots = @(
    'C:\Users\Helena\Documents\Square Enix\Batman Arkham Asylum GOTY\BmGame\Config',
    'C:\Program Files (x86)\Steam\steamapps\common\Batman Arkham Asylum GOTY\BmGame\Config'
)
$records = @()
New-Item -ItemType Directory -Path $output | Out-Null
foreach ($root in $roots) {
    if (-not (Test-Path -LiteralPath $root -PathType Container)) { throw "Required INI root missing: $root" }
    foreach ($file in Get-ChildItem -LiteralPath $root -File -Filter '*.ini') {
        $name = ([string]$records.Count) + '-' + $file.Name
        $destination = Join-Path $output $name
        Copy-Item -LiteralPath $file.FullName -Destination $destination
        $records += [pscustomobject]@{ Path=$file.FullName; Copy=$name; Sha256=(Get-FileHash -LiteralPath $destination).Hash }
    }
}
[IO.File]::WriteAllText((Join-Path $output 'manifest.json'), (ConvertTo-Json -InputObject $records -Depth 4), [Text.UTF8Encoding]::new($false))
Write-Output "INI_SNAPSHOT: $output ($($records.Count) files)"
if ($CompareTo) {
    $before = Get-Content -LiteralPath (Join-Path $CompareTo 'manifest.json') -Raw | ConvertFrom-Json
    $changes = 0
    foreach ($record in $records) {
        $old = @($before | Where-Object Path -eq $record.Path)
        if ($old.Count -eq 0) { Write-Output "ADDED: $($record.Path)"; $changes++ }
        elseif ($old.Count -ne 1) { throw 'Duplicate path in baseline manifest.' }
        elseif ($old[0].Sha256 -ne $record.Sha256) { Write-Output "CHANGED: $($record.Path)"; $changes++ }
    }
    foreach ($old in $before) {
        if (@($records | Where-Object Path -eq $old.Path).Count -eq 0) { Write-Output "REMOVED: $($old.Path)"; $changes++ }
    }
    Write-Output "INI_DIFFERENCES: $changes"
}
