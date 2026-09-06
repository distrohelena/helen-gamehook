param()
$ErrorActionPreference = 'Stop'
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..\..\..'))
$root = Join-Path $repo ('output\snapshot-probe-test-' + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $root | Out-Null
$snapshot = Join-Path $PSScriptRoot 'Snapshot-ProbeIni.ps1'
& $snapshot -SnapshotRoot "$root\before" | Out-Null
$unchanged = @(& $snapshot -SnapshotRoot "$root\after" -CompareTo "$root\before")
if ($unchanged -notcontains 'INI_DIFFERENCES: 0') { throw 'Identical source snapshots were reported changed.' }
# Modify only a test-owned manifest, never a source INI, to simulate one changed hash.
$records = Get-Content -LiteralPath "$root\before\manifest.json" -Raw | ConvertFrom-Json
$records[0].Sha256 = 'INTENTIONALLY_DIFFERENT_TEST_HASH'
[IO.File]::WriteAllText("$root\before\manifest.json", ($records | ConvertTo-Json -Depth 4), [Text.UTF8Encoding]::new($false))
$changed = @(& $snapshot -SnapshotRoot "$root\changed" -CompareTo "$root\before")
if ($changed -notcontains 'INI_DIFFERENCES: 1') { throw 'One changed hash was not isolated.' }
Write-Output 'SNAPSHOT_COMPARISON_PASS: unchanged=0, changed=1; source INIs never modified'
