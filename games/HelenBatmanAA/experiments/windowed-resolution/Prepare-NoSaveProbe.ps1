param([Parameter(Mandatory=$true)][string]$OutputRoot)
$ErrorActionPreference = 'Stop'
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..\..\..'))
$output = [IO.Path]::GetFullPath($OutputRoot)
$allowed = [IO.Path]::GetFullPath((Join-Path $repo 'output')) + '\'
if (-not $output.StartsWith($allowed, [StringComparison]::OrdinalIgnoreCase)) { throw 'Probe output must be inside repository output.' }
if (Test-Path -LiteralPath $output) { throw 'Use a fresh, nonexistent probe output directory.' }
$sourcePath = Join-Path $repo 'HelenRuntime\BatmanGraphicsSessionService.cpp'
$source = [IO.File]::ReadAllText($sourcePath)
$needle = 'Backend.ApplySessionDraft(*Session->Baseline, attempted.Draft)'
if ([regex]::Matches($source, [regex]::Escape($needle)).Count -ne 1) { throw 'Direct Commit writer substitution is no longer exact.' }
$generated = '#include "NoSaveResolutionProbe.h"' + "`r`n" + $source.Replace($needle, 'NoSaveResolutionProbe::Apply(*Session->Baseline, attempted.Draft)')
New-Item -ItemType Directory -Path $output | Out-Null
[IO.File]::WriteAllText((Join-Path $output 'BatmanGraphicsSessionService.cpp'), $generated, [Text.UTF8Encoding]::new($false))
Write-Output "NO_SAVE_GENERATED_SOURCE: $output\BatmanGraphicsSessionService.cpp"
Write-Output ('SOURCE_SHA256: ' + (Get-FileHash -LiteralPath $sourcePath).Hash)
