param(
    [string]$RetailFrontendPackagePath,
    [string]$BatmanRoot,
    [string]$Configuration = 'Debug',
    [string]$BuilderRoot,
    [string]$BatmanUserIniPath,
    [switch]$MutationTestsOnly
)

$ErrorActionPreference = 'Stop'
$env:MSBUILDDISABLENODEREUSE = '1'
$ExpectedGraphicsShellSha256 = '6CF058DA55867BE38F4A7861C877EB2D4CFD98E4510848D924B2322FCBDF65A5'
. (Join-Path $PSScriptRoot 'BatmanBuilderWorkspaceHelpers.ps1')

function Assert-ContainsOrdinal {
    param([string]$Text, [string]$Token, [string]$Context)
    if ($Text.IndexOf($Token, [StringComparison]::Ordinal) -lt 0) { throw "$Context is missing '$Token'." }
}

function Assert-NotContainsOrdinal {
    param([string]$Text, [string]$Token, [string]$Context)
    if ($Text.IndexOf($Token, [StringComparison]::Ordinal) -ge 0) { throw "$Context contains forbidden '$Token'." }
}

function Assert-FullSha256 {
    <# Reject abbreviated or non-hex evidence so provenance logs always carry the complete digest. #>
    param([Parameter(Mandatory = $true)] [string]$Hash, [Parameter(Mandatory = $true)] [string]$Context)
    if ($Hash -notmatch '\A[0-9A-Fa-f]{64}\z') { throw "$Context must be a full 64-character SHA-256 digest, found '$Hash'." }
}

function Assert-ExpectedSha256 {
    <# Require both a complete digest and the independently reviewed artifact identity. #>
    param(
        [Parameter(Mandatory = $true)] [string]$Hash,
        [Parameter(Mandatory = $true)] [string]$Expected,
        [Parameter(Mandatory = $true)] [string]$Context
    )

    Assert-FullSha256 -Hash $Hash -Context $Context
    if ($Hash -cne $Expected) { throw "$Context drifted. Expected '$Expected', found '$Hash'." }
}

function Get-ActionFunctionBody {
    param([string]$Text, [string]$Assignment)
    $match = [regex]::Match($Text, [regex]::Escape($Assignment) + '\s*=\s*function\s*\(\s*\)\s*\{(?<body>.*?)\}', [Text.RegularExpressions.RegexOptions]::Singleline)
    if (-not $match.Success) { throw "Missing expected row action $Assignment." }
    return $match.Groups['body'].Value.Trim()
}

function Get-VsyncSliceRowActionBody {
    param([string]$Text, [string]$Assignment)
    return Get-ActionFunctionBody -Text $Text -Assignment $Assignment
}

function Assert-NoOpVsyncSliceRowActions {
    param([string]$Text, [string]$Context)
    foreach ($action in @('RunAction', 'Increment', 'Decrement', 'ShowPrompt')) {
        if ((Get-VsyncSliceRowActionBody -Text $Text -Assignment "this.$action") -ne '') { throw "$Context action $action must be a no-op." }
    }
}

function Assert-VsyncSliceRowContract {
    param([string]$Text, [int]$Index, [string]$Context)

    $labels = @('Fullscreen', 'Resolution', 'VSync', 'MSAA', 'Detail Level', 'Bloom', 'Dynamic Shadows', 'Motion Blur', 'Distortion', 'Fog Volumes', 'Spherical Harmonic Lighting', 'Ambient Occlusion', 'PhysX', 'Stereo 3D', 'Apply Changes')
    if ($Index -in @(2, 3, 12, 13)) {
        $activeRows = @{
            2 = [pscustomobject]@{ RowIndex = 3; Values = 'this.Names = new Array("Off","On");' }
            3 = [pscustomobject]@{ RowIndex = 4; Values = 'this.Names = new Array("Off","2x","4x","8x","16x");' }
            12 = [pscustomobject]@{ RowIndex = 13; Values = 'this.Names = new Array("Off","Normal","High");' }
            13 = [pscustomobject]@{ RowIndex = 14; Values = 'this.Names = new Array("Off","On");' }
        }
        $activeRow = $activeRows[$Index]
        Assert-ContainsOrdinal $Text $activeRow.Values "$Context names"
        Assert-ContainsOrdinal $Text "this.RowIndex = $($activeRow.RowIndex);" "$Context row index"
        Assert-ContainsOrdinal $Text 'this.State = _parent.GraphicsOptionsController.GetDraftIndex(this.RowIndex);' "$Context initial state"
        Assert-ContainsOrdinal $Text '_parent.GraphicsOptionsController.ToggleSetting(this.RowIndex);' "$Context RunAction"
        Assert-ContainsOrdinal $Text '_parent.GraphicsOptionsController.IncrementSetting(this.RowIndex);' "$Context Increment"
        Assert-ContainsOrdinal $Text '_parent.GraphicsOptionsController.DecrementSetting(this.RowIndex);' "$Context Decrement"
        Assert-ContainsOrdinal $Text $labels[$Index] "$Context label"
        Assert-ContainsOrdinal $Text 'this._visible = true;' "$Context visibility"
        return
    }

    if ($Index -eq 14) {
        Assert-ContainsOrdinal $Text 'this.Names = new Array("");' "$Context names"
        Assert-ContainsOrdinal $Text 'Apply Changes' "$Context label"
        Assert-ContainsOrdinal $Text 'this.ItemText.text = "";' "$Context value"
        Assert-ContainsOrdinal $Text 'this._visible = true;' "$Context visibility"
        Assert-ContainsOrdinal $Text '_parent.GraphicsOptionsController.ApplyChanges();' "$Context RunAction"
        foreach ($action in @('Increment', 'Decrement')) {
            if ((Get-VsyncSliceRowActionBody -Text $Text -Assignment "this.$action") -ne '') { throw "$Context action $action must be a no-op." }
        }
        return
    }

    Assert-ContainsOrdinal $Text 'this.Names = new Array("Not active");' "$Context names"
    Assert-ContainsOrdinal $Text 'if(this.ItemText != undefined)' "$Context ItemText guard"
    Assert-ContainsOrdinal $Text 'this.ItemText.text = "Not active";' "$Context visible value"
    Assert-ContainsOrdinal $Text $labels[$Index] "$Context label"
    Assert-ContainsOrdinal $Text 'this._visible = true;' "$Context visibility"
    Assert-NoOpVsyncSliceRowActions -Text $Text -Context $Context
}

function Assert-RowShellContract {
    param([string]$ScreenDirectory)
    $depths = @('141', '133', '125', '117', '109', '101', '93', '85', '77', '69', '61', '53', '45', '37', '29')
    for ($index = 0; $index -lt $depths.Count; $index++) {
        $rowPath = Join-Path $ScreenDirectory "frame_1\PlaceObject2_290_List_Template_$($depths[$index])\CLIPACTIONRECORD onClipEvent(load).as"
        if (-not (Test-Path -LiteralPath $rowPath)) { throw "Missing known row script $rowPath." }
        $rowText = Get-Content -LiteralPath $rowPath -Raw
        if ($index -lt 15) {
            if ($rowText -notmatch 'this\.(?:Label\.)?Label\.Text\.text\s*=\s*"' -and $rowText -notmatch 'this\.Label\.Text\.text\s*=\s*"') { throw "row $($index + 1) is missing its fixed label assignment." }
            Assert-VsyncSliceRowContract -Text $rowText -Index $index -Context "row $($index + 1)"
        } else {
            throw 'Unexpected graphics row index.'
        }
    }
}

function Invoke-ExternalProcess {
    param([string]$FilePath, [string[]]$Arguments)
    $previousErrorActionPreference = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try { $output = @(& $FilePath @Arguments 2>&1) } finally { $ErrorActionPreference = $previousErrorActionPreference }
    [pscustomobject]@{ ExitCode = $LASTEXITCODE; Output = $output }
}

function New-VsyncIniFixture {
    param([string]$SourcePath, [string]$DestinationPath, [bool]$Enabled)
    $iniText = Get-Content -LiteralPath $SourcePath -Raw
    $useVsyncPattern = '(?m)^UseVsync=(?:True|False|0|1)\r?$'
    $matches = [regex]::Matches($iniText, $useVsyncPattern)
    if ($matches.Count -ne 1) { throw "Expected exactly one exact UseVsync assignment in valid INI fixture source, found $($matches.Count)." }
    $replacement = if ($Enabled) { 'UseVsync=True' } else { 'UseVsync=False' }
    $fixtureText = [regex]::Replace($iniText, $useVsyncPattern, $replacement, 1)
    [IO.File]::WriteAllText($DestinationPath, $fixtureText, [Text.UTF8Encoding]::new($false))
}

function New-GraphicsOptionsIniFixture {
    <# Create an isolated four-setting bootstrap fixture without changing the user's INI. #>
    param(
        [Parameter(Mandatory = $true)] [string]$SourcePath,
        [Parameter(Mandatory = $true)] [string]$DestinationPath,
        [Parameter(Mandatory = $true)] [bool]$Vsync,
        [Parameter(Mandatory = $true)] [int]$MsaaSamples,
        [Parameter(Mandatory = $true)] [int]$PhysxLevel,
        [Parameter(Mandatory = $true)] [bool]$Stereo
    )

    $fixtureText = Get-Content -LiteralPath $SourcePath -Raw
    $assignments = @(
        [pscustomobject]@{ Name = 'UseVsync'; Pattern = '(?m)^UseVsync=(?:True|False|0|1)\r?$'; Value = if ($Vsync) { 'True' } else { 'False' } },
        [pscustomobject]@{ Name = 'MaxMultisamples'; Pattern = '(?m)^MaxMultisamples=-?\d+\r?$'; Value = [string]$MsaaSamples },
        [pscustomobject]@{ Name = 'PhysXLevel'; Pattern = '(?m)^PhysXLevel=-?\d+\r?$'; Value = [string]$PhysxLevel },
        [pscustomobject]@{ Name = 'Stereo'; Pattern = '(?m)^Stereo=(?:True|False|0|1)\r?$'; Value = if ($Stereo) { 'True' } else { 'False' } }
    )
    foreach ($assignment in $assignments) {
        $matches = [regex]::Matches($fixtureText, $assignment.Pattern)
        if ($matches.Count -ne 1) { throw "Expected exactly one $($assignment.Name) assignment in isolated INI fixture source, found $($matches.Count)." }
        $fixtureText = [regex]::Replace($fixtureText, $assignment.Pattern, "$($assignment.Name)=$($assignment.Value)", 1)
    }
    [IO.File]::WriteAllText($DestinationPath, $fixtureText, [Text.UTF8Encoding]::new($false))
}

function New-MalformedIniFixture {
    param([string]$SourcePath, [string]$DestinationPath)
    $iniText = Get-Content -LiteralPath $SourcePath -Raw
    $useVsyncPattern = '(?m)^UseVsync=(?:True|False|0|1)\r?$'
    $matches = [regex]::Matches($iniText, $useVsyncPattern)
    if ($matches.Count -ne 1) { throw "Expected exactly one exact UseVsync assignment in valid INI fixture source, found $($matches.Count)." }
    $fixtureText = [regex]::Replace($iniText, $useVsyncPattern, 'UseVsync=Malformed', 1)
    [IO.File]::WriteAllText($DestinationPath, $fixtureText, [Text.UTF8Encoding]::new($false))
}

function Invoke-ShellBuild {
    param([string]$BuilderProject, [string]$Configuration, [string]$BuilderRoot, [string]$OutputDirectory, [string]$FfdecPath, [string]$IniPath)
    $result = Invoke-ExternalProcess -FilePath 'dotnet' -Arguments @('run', '--no-build', '--project', $BuilderProject, '-c', $Configuration, '--', 'build-main-menu-graphics-shell', '--root', $BuilderRoot, '--output-dir', $OutputDirectory, '--ffdec', $FfdecPath, '--ini', $IniPath)
    if ($result.ExitCode -ne 0) { throw "Shell build failed for INI '$IniPath': $($result.Output -join [Environment]::NewLine)" }
}

function Assert-ProductionGraphicsIniSnapshot {
    <# Read one fixture through the builder's production parser and compare its normalized Group 1 tuple. #>
    param(
        [Parameter(Mandatory = $true)] [string]$BuilderProject,
        [Parameter(Mandatory = $true)] [string]$Configuration,
        [Parameter(Mandatory = $true)] [string]$IniPath,
        [Parameter(Mandatory = $true)] [int[]]$ExpectedValues,
        [Parameter(Mandatory = $true)] [string]$Context
    )

    if ($ExpectedValues.Count -ne 4) { throw "$Context expected exactly four normalized Group 1 values." }
    $result = Invoke-ExternalProcess -FilePath 'dotnet' -Arguments @('run', '--no-build', '--project', $BuilderProject, '-c', $Configuration, '--', 'inspect-batman-graphics-ini', '--ini', $IniPath)
    if ($result.ExitCode -ne 0) { throw "$Context production INI inspection failed: $($result.Output -join [Environment]::NewLine)" }
    $jsonLine = @($result.Output | ForEach-Object { [string]$_ } | Where-Object { -not [string]::IsNullOrWhiteSpace($_) } | Select-Object -Last 1)
    if ($jsonLine.Count -ne 1) { throw "$Context production INI inspection did not emit exactly one normalized JSON object." }
    try { $snapshot = $jsonLine[0] | ConvertFrom-Json } catch { throw "$Context production INI inspection emitted invalid JSON: $jsonLine" }
    $actualProperties = @($snapshot.PSObject.Properties.Name)
    if (($actualProperties -join '|') -cne 'vsync|msaa|physx|stereo') { throw "$Context normalized snapshot properties drifted: $($actualProperties -join ', ')." }
    $actualValues = @([int]$snapshot.vsync, [int]$snapshot.msaa, [int]$snapshot.physx, [int]$snapshot.stereo)
    for ($index = 0; $index -lt $ExpectedValues.Count; $index++) {
        if ($actualValues[$index] -ne $ExpectedValues[$index]) { throw "$Context normalized Group 1 value $($index + 1) drifted. Expected $($ExpectedValues[$index]), found $($actualValues[$index])." }
    }
}

function Invoke-RetailGraphicsPatch {
    <# Patch the verified retail frontend into an isolated output package. #>
    param(
        [Parameter(Mandatory = $true)] [string]$PatcherProject,
        [Parameter(Mandatory = $true)] [string]$Configuration,
        [Parameter(Mandatory = $true)] [string]$RetailPackagePath,
        [Parameter(Mandatory = $true)] [string]$ManifestPath,
        [Parameter(Mandatory = $true)] [string]$OutputPath
    )

    $result = Invoke-ExternalProcess -FilePath 'dotnet' -Arguments @('run', '--no-build', '--project', $PatcherProject, '-c', $Configuration, '--', 'patch', '--package', $RetailPackagePath, '--manifest', $ManifestPath, '--output', $OutputPath)
    if ($result.ExitCode -ne 0) { throw "Retail graphics patch failed for '$OutputPath': $($result.Output -join [Environment]::NewLine)" }
    if (-not (Test-Path -LiteralPath $OutputPath -PathType Leaf)) { throw "Retail graphics patch did not create '$OutputPath'." }
}

function Invoke-GraphicsHgdeltaBuild {
    <# Build one isolated HGDL delta from the verified retail base to one target package. #>
    param(
        [Parameter(Mandatory = $true)] [string]$BuildHgdeltaPath,
        [Parameter(Mandatory = $true)] [string]$RetailPackagePath,
        [Parameter(Mandatory = $true)] [string]$TargetPackagePath,
        [Parameter(Mandatory = $true)] [string]$OutputPath
    )

    $result = & $BuildHgdeltaPath -BaseFile $RetailPackagePath -TargetFile $TargetPackagePath -OutputFile $OutputPath -ChunkSize 65536
    if ($LASTEXITCODE -ne 0 -or -not (Test-Path -LiteralPath $OutputPath -PathType Leaf)) {
        throw "Graphics HGDL build failed for '$OutputPath': $($result -join [Environment]::NewLine)"
    }
}

function Reconstruct-RetailHgdeltaTarget {
    <# Independently reconstruct an HGDL target from the verified retail base for the retail round-trip proof. #>
    param(
        [Parameter(Mandatory = $true)] [string]$BasePath,
        [Parameter(Mandatory = $true)] [string]$DeltaPath
    )

    $base = [IO.File]::ReadAllBytes($BasePath)
    $delta = [IO.File]::ReadAllBytes($DeltaPath)
    if ($delta.Length -lt 116 -or [Text.Encoding]::ASCII.GetString($delta, 0, 4) -cne 'HGDL') { throw "Invalid HGDL retail delta: $DeltaPath" }
    $major = [BitConverter]::ToUInt32($delta, 4)
    $minor = [BitConverter]::ToUInt32($delta, 8)
    $chunkSize = [BitConverter]::ToUInt32($delta, 12)
    $baseSize = [BitConverter]::ToUInt64($delta, 16)
    $targetSize64 = [BitConverter]::ToUInt64($delta, 24)
    if ($major -ne 1 -or $minor -ne 0 -or $chunkSize -ne 65536) { throw "Retail HGDL header drifted: version=$major.$minor chunkSize=$chunkSize." }
    if ($baseSize -ne [uint64]$base.Length -or $targetSize64 -gt [uint64][int]::MaxValue) { throw 'Retail HGDL size header does not match the verified base or verifier limits.' }
    $baseHash = ([BitConverter]::ToString($delta[32..63]) -replace '-', '').ToLowerInvariant()
    $targetHash = ([BitConverter]::ToString($delta[64..95]) -replace '-', '').ToLowerInvariant()
    $actualBaseHash = (Get-FileHash -LiteralPath $BasePath -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($baseHash -cne $actualBaseHash) { throw "Retail HGDL base hash mismatch: header=$baseHash actual=$actualBaseHash." }
    $chunkCount = [BitConverter]::ToUInt32($delta, 96)
    $chunkTableOffset = [BitConverter]::ToUInt64($delta, 100)
    $payloadOffset = [BitConverter]::ToUInt64($delta, 108)
    $tableEnd = $chunkTableOffset + ([uint64]$chunkCount * 20)
    if ($chunkTableOffset -lt 116 -or $tableEnd -gt [uint64]$delta.Length -or $payloadOffset -lt $tableEnd -or $payloadOffset -gt [uint64]$delta.Length) { throw 'Retail HGDL table or payload bounds are invalid.' }

    $targetSize = [int]$targetSize64
    $result = [IO.MemoryStream]::new($targetSize)
    [uint64]$written = 0
    try {
        for ($index = 0; $index -lt $chunkCount; $index++) {
            $entry = [int]($chunkTableOffset + ([uint64]$index * 20))
            $kind = [BitConverter]::ToUInt32($delta, $entry)
            $chunkLength64 = [BitConverter]::ToUInt32($delta, $entry + 4)
            $payloadRelativeOffset = [BitConverter]::ToUInt64($delta, $entry + 8)
            $payloadLength = [BitConverter]::ToUInt32($delta, $entry + 16)
            if ($chunkLength64 -eq 0 -or $chunkLength64 -gt [uint64]$chunkSize -or $written + $chunkLength64 -gt $targetSize64) { throw "Retail HGDL chunk $index has an invalid reconstructed size." }
            $chunkLength = [int]$chunkLength64
            if ($kind -eq 0) {
                if ($payloadRelativeOffset -ne 0 -or $payloadLength -ne 0) { throw "Retail HGDL base chunk $index contains payload metadata." }
                $baseOffset = [uint64]$index * [uint64]$chunkSize
                if ($baseOffset + $chunkLength64 -gt [uint64]$base.Length) { throw "Retail HGDL base chunk $index exceeds base bounds." }
                $result.Write($base, [int]$baseOffset, $chunkLength)
            } elseif ($kind -eq 1) {
                if ($payloadLength -ne $chunkLength64 -or $payloadOffset + $payloadRelativeOffset + $chunkLength64 -gt [uint64]$delta.Length) { throw "Retail HGDL replacement chunk $index exceeds payload bounds." }
                $result.Write($delta, [int]($payloadOffset + $payloadRelativeOffset), $chunkLength)
            } else {
                throw "Retail HGDL contains unsupported chunk kind $kind at index $index."
            }
            $written += $chunkLength64
        }
        if ($written -ne $targetSize64 -or $result.Length -ne $targetSize) { throw "Retail HGDL reconstruction size mismatch: expected $targetSize64, wrote $written." }
        [byte[]]$reconstructed = $result.ToArray()
        $actualTargetHash = (Get-FileHash -InputStream ([IO.MemoryStream]::new($reconstructed)) -Algorithm SHA256).Hash.ToLowerInvariant()
        if ($targetHash -cne $actualTargetHash) { throw "Retail HGDL target hash mismatch: header=$targetHash actual=$actualTargetHash." }
        return ,$reconstructed
    } finally {
        $result.Dispose()
    }
}

function Get-RetailExportFileSnapshot {
    <# Capture deterministic hashes for exported retail scripts and binary assets. #>
    param([Parameter(Mandatory = $true)] [string]$Root)
    if (-not (Test-Path -LiteralPath $Root -PathType Container)) { throw "Retail export root was not created: $Root" }
    $prefix = [IO.Path]::GetFullPath($Root).TrimEnd('\') + '\'
    return @(
        Get-ChildItem -LiteralPath $Root -Recurse -File | ForEach-Object {
            [pscustomobject]@{
                RelativePath = $_.FullName.Substring($prefix.Length).Replace('/', '\')
                Length = [int64]$_.Length
                Sha256 = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash
            }
        } | Sort-Object RelativePath
    )
}

function Assert-RetailProtectedXmlMutationTests {
    <# Prove that an unrelated protected root-tag reorder or content mutation is rejected. #>
    $mutationRoot = Join-Path ([IO.Path]::GetTempPath()) ('HelenBatmanRetailXmlMutation-' + [Guid]::NewGuid().ToString('N'))
    New-Item -ItemType Directory -Force -Path $mutationRoot | Out-Null
    $basePath = Join-Path $mutationRoot 'base.xml'
    $reorderedPath = Join-Path $mutationRoot 'reordered.xml'
    $changedPath = Join-Path $mutationRoot 'changed.xml'
    $retailShapeXmlPath = Join-Path $mutationRoot 'retail-shape.xml'
    $reconstructedShapeXmlPath = Join-Path $mutationRoot 'reconstructed-shape.xml'
    $retailShapeSvgPath = Join-Path $mutationRoot '600.svg'
    $reconstructedShapeSvgPath = Join-Path $mutationRoot '1017.svg'
    $baseXml = @'
<swf><tags><item type="DefineTextTag" id="1"><text>first</text></item><item type="DefineShapeTag" id="2"><shape>second</shape></item></tags></swf>
'@
    $reorderedXml = @'
<swf><tags><item type="DefineShapeTag" id="2"><shape>second</shape></item><item type="DefineTextTag" id="1"><text>first</text></item></tags></swf>
'@
    $changedXml = @'
<swf><tags><item type="DefineTextTag" id="1"><text>first</text></item><item type="DefineShapeTag" id="2"><shape>changed</shape></item></tags></swf>
'@
    $retailShapeXml = @'
<swf><tags><item type="DefineShapeTag" forceWriteAsLong="true" shapeId="600"><records><item type="StraightEdgeRecord" deltaX="12" /></records></item></tags></swf>
'@
    $reconstructedShapeXml = @'
<swf><tags><item type="DefineShapeTag" forceWriteAsLong="true" shapeId="1017"><records><item type="StraightEdgeRecord" deltaX="12" /></records></item></tags></swf>
'@
    $retailShapeSvg = @'
<svg xmlns="http://www.w3.org/2000/svg"><pattern id="PatternID_600_1" /><path d="M0 0 L12 12" style="fill:url(#PatternID_600_1)" /></svg>
'@
    $reconstructedShapeSvg = @'
<svg xmlns="http://www.w3.org/2000/svg"><pattern id="PatternID_1017_1" /><path d="M0 0 L12 12" style="fill:url(#PatternID_1017_1)" /></svg>
'@
    [IO.File]::WriteAllText($basePath, $baseXml, [Text.UTF8Encoding]::new($false))
    [IO.File]::WriteAllText($reorderedPath, $reorderedXml, [Text.UTF8Encoding]::new($false))
    [IO.File]::WriteAllText($changedPath, $changedXml, [Text.UTF8Encoding]::new($false))
    [IO.File]::WriteAllText($retailShapeXmlPath, $retailShapeXml, [Text.UTF8Encoding]::new($false))
    [IO.File]::WriteAllText($reconstructedShapeXmlPath, $reconstructedShapeXml, [Text.UTF8Encoding]::new($false))
    [IO.File]::WriteAllText($retailShapeSvgPath, $retailShapeSvg, [Text.UTF8Encoding]::new($false))
    [IO.File]::WriteAllText($reconstructedShapeSvgPath, $reconstructedShapeSvg, [Text.UTF8Encoding]::new($false))
    try {
        $reorderError = $null
        try { Assert-RetailProtectedXmlMatches -RetailXmlPath $basePath -ReconstructedXmlPath $reorderedPath } catch { $reorderError = $_.Exception.Message }
        if ([string]::IsNullOrWhiteSpace($reorderError) -or $reorderError -notmatch 'root index/order') { throw 'Retail XML mutation test did not reject unrelated protected root-tag reorder.' }
        $changeError = $null
        try { Assert-RetailProtectedXmlMatches -RetailXmlPath $basePath -ReconstructedXmlPath $changedPath } catch { $changeError = $_.Exception.Message }
        if ([string]::IsNullOrWhiteSpace($changeError) -or $changeError -notmatch 'fingerprint') { throw 'Retail XML mutation test did not reject unrelated protected root-tag content change.' }
        Assert-RetailShapeRemap -RetailSvgPath $retailShapeSvgPath -ReconstructedSvgPath $reconstructedShapeSvgPath -RetailXmlPath $retailShapeXmlPath -ReconstructedXmlPath $reconstructedShapeXmlPath
        Write-Output 'Retail XML mutation tests: unrelated reorder/change rejected; shape ID-only remap accepted.'
    } finally {
        if (Test-Path -LiteralPath $mutationRoot) {
            Remove-Item -LiteralPath $mutationRoot -Recurse -Force
        }
    }
}

function Get-RetailXmlProtectedFingerprint {
    <# Hash every unchanged root SWF tag with its original index; only documented inserted/rewritten nodes are excluded. #>
    param([Parameter(Mandatory = $true)] [string]$XmlPath)
    [xml]$document = Get-Content -LiteralPath $XmlPath -Raw
    $items = @($document.SelectNodes('/swf/tags/item'))
    $fingerprints = [Collections.Generic.List[object]]::new()
    for ($rootIndex = 0; $rootIndex -lt $items.Count; $rootIndex++) {
        $item = $items[$rootIndex]
        $type = $item.GetAttribute('type')
        $spriteId = $item.GetAttribute('spriteId')
        $shapeId = $item.GetAttribute('shapeId')
        $itemText = $item.OuterXml
        $exportNames = @($item.SelectNodes('./names/item') | ForEach-Object { $_.InnerText })
        $exportTags = @($item.SelectNodes('./tags/item') | ForEach-Object { $_.InnerText })
        if ($type -eq 'DefineSpriteTag' -and $spriteId -in @('333', '600')) { continue }
        if ($type -eq 'ExportAssetsTag' -and $exportNames.Count -eq 1 -and $exportNames[0] -ceq 'ScreenOptionsGraphics' -and $exportTags.Count -eq 1 -and $exportTags[0] -ceq '600') { continue }
        if ($type -eq 'DoInitActionTag' -and $spriteId -eq '600') { continue }
        if ($type -eq 'DefineShapeTag' -and $shapeId -in @('600', '1017')) { continue }
        $fingerprints.Add([pscustomobject]@{
                RootIndex = $rootIndex
                Type = $type
                Sha256 = (Get-FileHash -InputStream ([IO.MemoryStream]::new([Text.Encoding]::UTF8.GetBytes($itemText))) -Algorithm SHA256).Hash
            })
    }
    return $fingerprints.ToArray()
}

function Assert-RetailProtectedXmlMatches {
    <# Compare ordered protected root-tag fingerprints so unrelated reorder, insertion, or content drift fails. #>
    param(
        [Parameter(Mandatory = $true)] [string]$RetailXmlPath,
        [Parameter(Mandatory = $true)] [string]$ReconstructedXmlPath
    )

    $retailFingerprint = @(Get-RetailXmlProtectedFingerprint -XmlPath $RetailXmlPath)
    $reconstructedFingerprint = @(Get-RetailXmlProtectedFingerprint -XmlPath $ReconstructedXmlPath)
    if ($retailFingerprint.Count -ne $reconstructedFingerprint.Count) { throw "Verified retail XML protected root-tag count changed: retail=$($retailFingerprint.Count), reconstructed=$($reconstructedFingerprint.Count)." }
    for ($index = 0; $index -lt $retailFingerprint.Count; $index++) {
        $retailItem = $retailFingerprint[$index]
        $reconstructedItem = $reconstructedFingerprint[$index]
        if ($retailItem.RootIndex -ne $reconstructedItem.RootIndex -or $retailItem.Type -cne $reconstructedItem.Type) {
            throw "Verified retail XML protected root index/order changed at fingerprint index ${index}: retailRootIndex=$($retailItem.RootIndex), reconstructedRootIndex=$($reconstructedItem.RootIndex), retailType=$($retailItem.Type), reconstructedType=$($reconstructedItem.Type)."
        }
        if ($retailItem.Sha256 -cne $reconstructedItem.Sha256) {
            throw "Verified retail XML protected fingerprint changed at root index $($retailItem.RootIndex), fingerprint index $index outside the documented shell remaps."
        }
    }
}

function Assert-RetailShapeRemap {
    <# Prove the sole reserved-shape remap changes only the documented identifier, not geometry or styles. #>
    param(
        [Parameter(Mandatory = $true)] [string]$RetailSvgPath,
        [Parameter(Mandatory = $true)] [string]$ReconstructedSvgPath,
        [Parameter(Mandatory = $true)] [string]$RetailXmlPath,
        [Parameter(Mandatory = $true)] [string]$ReconstructedXmlPath
    )

    $retailXml = [xml](Get-Content -LiteralPath $RetailXmlPath -Raw)
    $reconstructedXml = [xml](Get-Content -LiteralPath $ReconstructedXmlPath -Raw)
    $retailShapeTags = @($retailXml.SelectNodes("/swf/tags/item[@type='DefineShapeTag' and @shapeId='600']"))
    $reconstructedShapeTags = @($reconstructedXml.SelectNodes("/swf/tags/item[@type='DefineShapeTag' and @shapeId='1017']"))
    if ($retailShapeTags.Count -ne 1 -or $reconstructedShapeTags.Count -ne 1) { throw "Expected exactly one XML shape remap 600->1017, found retail=$($retailShapeTags.Count), reconstructed=$($reconstructedShapeTags.Count)." }
    $retailRootItems = @($retailXml.SelectNodes('/swf/tags/item'))
    $reconstructedRootItems = @($reconstructedXml.SelectNodes('/swf/tags/item'))
    $retailShapeIndex = -1
    for ($rootIndex = 0; $rootIndex -lt $retailRootItems.Count; $rootIndex++) {
        if ([object]::ReferenceEquals($retailRootItems[$rootIndex], $retailShapeTags[0])) { $retailShapeIndex = $rootIndex; break }
    }
    $reconstructedShapeIndex = -1
    for ($rootIndex = 0; $rootIndex -lt $reconstructedRootItems.Count; $rootIndex++) {
        if ([object]::ReferenceEquals($reconstructedRootItems[$rootIndex], $reconstructedShapeTags[0])) { $reconstructedShapeIndex = $rootIndex; break }
    }
    if ($retailShapeIndex -ne $reconstructedShapeIndex) { throw "XML shape remap moved root index: retail=$retailShapeIndex, reconstructed=$reconstructedShapeIndex." }
    $retailShapeXml = $retailShapeTags[0].OuterXml -replace 'shapeId="600"', 'shapeId="TASK6_SHAPE_ID"'
    $reconstructedShapeXml = $reconstructedShapeTags[0].OuterXml -replace 'shapeId="1017"', 'shapeId="TASK6_SHAPE_ID"'
    if ($retailShapeXml -cne $reconstructedShapeXml) { throw 'XML shape remap 600->1017 changed protected shape geometry or records.' }

    $retailSvgText = [IO.File]::ReadAllText($RetailSvgPath)
    $reconstructedSvgText = [IO.File]::ReadAllText($ReconstructedSvgPath)
    [xml]$retailSvg = $retailSvgText
    [xml]$reconstructedSvg = $reconstructedSvgText
    $retailIds = @($retailSvg.SelectNodes('//@id') | ForEach-Object { $_.Value })
    $reconstructedIds = @($reconstructedSvg.SelectNodes('//@id') | ForEach-Object { $_.Value })
    if ($retailIds.Count -eq 0 -or $retailIds.Count -ne $reconstructedIds.Count) { throw "SVG shape remap identifier count drifted: retail=$($retailIds.Count), reconstructed=$($reconstructedIds.Count)." }
    $retailNormalizedSvg = [xml]$retailSvgText
    $reconstructedNormalizedSvg = [xml]$reconstructedSvgText
    for ($idIndex = 0; $idIndex -lt $retailIds.Count; $idIndex++) {
        $placeholder = "TASK6_SHAPE_ID_$idIndex"
        foreach ($attribute in @($retailNormalizedSvg.SelectNodes('//@*'))) { $attribute.Value = $attribute.Value.Replace($retailIds[$idIndex], $placeholder) }
        foreach ($attribute in @($reconstructedNormalizedSvg.SelectNodes('//@*'))) { $attribute.Value = $attribute.Value.Replace($reconstructedIds[$idIndex], $placeholder) }
    }
    $retailNormalizedText = $retailNormalizedSvg.OuterXml
    $reconstructedNormalizedText = $reconstructedNormalizedSvg.OuterXml
    $retailNormalizedHash = (Get-FileHash -InputStream ([IO.MemoryStream]::new([Text.Encoding]::UTF8.GetBytes($retailNormalizedText))) -Algorithm SHA256).Hash
    $reconstructedNormalizedHash = (Get-FileHash -InputStream ([IO.MemoryStream]::new([Text.Encoding]::UTF8.GetBytes($reconstructedNormalizedText))) -Algorithm SHA256).Hash
    if ($retailNormalizedHash -cne $reconstructedNormalizedHash) { throw 'SVG shape remap 600->1017 changed geometry, style, path, or embedded image data.' }
    $retailSvgHash = (Get-FileHash -LiteralPath $RetailSvgPath -Algorithm SHA256).Hash
    $reconstructedSvgHash = (Get-FileHash -LiteralPath $ReconstructedSvgPath -Algorithm SHA256).Hash
    Write-Output "Shape remap proof: shapes\600.svg -> shapes\1017.svg; XMLRootIndex=$retailShapeIndex; rawRetail=$retailSvgHash rawReconstructed=$reconstructedSvgHash normalized=$retailNormalizedHash"
}

function Assert-RetailExportPreservation {
    <# Require verified-retail XML/scripts/assets to remain byte-identical outside the explicit shell allowlist. #>
    param(
        [Parameter(Mandatory = $true)] [string]$RetailXmlPath,
        [Parameter(Mandatory = $true)] [string]$ReconstructedXmlPath,
        [Parameter(Mandatory = $true)] [string]$RetailExportRoot,
        [Parameter(Mandatory = $true)] [string]$ReconstructedExportRoot
    )

    $intentionalScripts = @(
        'scripts\ScreenOptionsGraphics.as',
        'scripts\ScreenOptionsAudio_2.as',
        'scripts\DefineSprite_333_ScreenOptionsMenu\frame_1\DoAction_2.as',
        'scripts\DefineSprite_333_ScreenOptionsMenu\frame_1\PlaceObject2_117_GenericButton_37\CLIPACTIONRECORD onClipEvent(load).as',
        'scripts\DefineSprite_333_ScreenOptionsMenu\frame_1\PlaceObject2_117_GenericButton_43\CLIPACTIONRECORD on(construct).as',
        'scripts\DefineSprite_333_ScreenOptionsMenu\frame_1\PlaceObject2_117_GenericButton_43\CLIPACTIONRECORD onClipEvent(load).as',
        'scripts\DefineSprite_600_ScreenOptionsGraphics\frame_1\DoAction.as',
        'scripts\DefineSprite_600_ScreenOptionsGraphics\frame_15\DoAction.as',
        'scripts\DefineSprite_600_ScreenOptionsGraphics\frame_1\PlaceObject2_290_List_Template_141\CLIPACTIONRECORD onClipEvent(load).as',
        'scripts\DefineSprite_600_ScreenOptionsGraphics\frame_1\PlaceObject2_290_List_Template_133\CLIPACTIONRECORD onClipEvent(load).as',
        'scripts\DefineSprite_600_ScreenOptionsGraphics\frame_1\PlaceObject2_290_List_Template_125\CLIPACTIONRECORD onClipEvent(load).as',
        'scripts\DefineSprite_600_ScreenOptionsGraphics\frame_1\PlaceObject2_290_List_Template_117\CLIPACTIONRECORD onClipEvent(load).as',
        'scripts\DefineSprite_600_ScreenOptionsGraphics\frame_1\PlaceObject2_290_List_Template_109\CLIPACTIONRECORD onClipEvent(load).as',
        'scripts\DefineSprite_600_ScreenOptionsGraphics\frame_1\PlaceObject2_290_List_Template_101\CLIPACTIONRECORD onClipEvent(load).as',
        'scripts\DefineSprite_600_ScreenOptionsGraphics\frame_1\PlaceObject2_290_List_Template_93\CLIPACTIONRECORD onClipEvent(load).as',
        'scripts\DefineSprite_600_ScreenOptionsGraphics\frame_1\PlaceObject2_290_List_Template_85\CLIPACTIONRECORD onClipEvent(load).as',
        'scripts\DefineSprite_600_ScreenOptionsGraphics\frame_1\PlaceObject2_290_List_Template_77\CLIPACTIONRECORD onClipEvent(load).as',
        'scripts\DefineSprite_600_ScreenOptionsGraphics\frame_1\PlaceObject2_290_List_Template_69\CLIPACTIONRECORD onClipEvent(load).as',
        'scripts\DefineSprite_600_ScreenOptionsGraphics\frame_1\PlaceObject2_290_List_Template_61\CLIPACTIONRECORD onClipEvent(load).as',
        'scripts\DefineSprite_600_ScreenOptionsGraphics\frame_1\PlaceObject2_290_List_Template_53\CLIPACTIONRECORD onClipEvent(load).as',
        'scripts\DefineSprite_600_ScreenOptionsGraphics\frame_1\PlaceObject2_290_List_Template_45\CLIPACTIONRECORD onClipEvent(load).as',
        'scripts\DefineSprite_600_ScreenOptionsGraphics\frame_1\PlaceObject2_290_List_Template_37\CLIPACTIONRECORD onClipEvent(load).as',
        'scripts\DefineSprite_600_ScreenOptionsGraphics\frame_1\PlaceObject2_290_List_Template_29\CLIPACTIONRECORD onClipEvent(load).as'
    )
    $intentionalSprite333Assets = @(
        'sprites\DefineSprite_333_ScreenOptionsMenu\7.png',
        'sprites\DefineSprite_333_ScreenOptionsMenu\8.png',
        'sprites\DefineSprite_333_ScreenOptionsMenu\11.png',
        'sprites\DefineSprite_333_ScreenOptionsMenu\15.png',
        'sprites\DefineSprite_333_ScreenOptionsMenu\16.png',
        'sprites\DefineSprite_333_ScreenOptionsMenu\18.png',
        'sprites\DefineSprite_333_ScreenOptionsMenu\19.png',
        'sprites\DefineSprite_333_ScreenOptionsMenu\20.png',
        'sprites\DefineSprite_333_ScreenOptionsMenu\22.png',
        'sprites\DefineSprite_333_ScreenOptionsMenu\23.png',
        'sprites\DefineSprite_333_ScreenOptionsMenu\24.png'
    )
    $retailFiles = @(Get-RetailExportFileSnapshot -Root $RetailExportRoot)
    $reconstructedFiles = @(Get-RetailExportFileSnapshot -Root $ReconstructedExportRoot)
    $shapeRemap = [ordered]@{ 'shapes\600.svg' = 'shapes\1017.svg' }
    $mappedTargetPaths = @($shapeRemap.Values)
    foreach ($sourcePath in $shapeRemap.Keys) {
        $targetPath = $shapeRemap[$sourcePath]
        $retailShape = @($retailFiles | Where-Object RelativePath -ceq $sourcePath)
        $reconstructedSourceShape = @($reconstructedFiles | Where-Object RelativePath -ceq $sourcePath)
        $retailTargetShape = @($retailFiles | Where-Object RelativePath -ceq $targetPath)
        $reconstructedShape = @($reconstructedFiles | Where-Object RelativePath -ceq $targetPath)
        if ($retailShape.Count -ne 1 -or $reconstructedSourceShape.Count -ne 0 -or $retailTargetShape.Count -ne 0 -or $reconstructedShape.Count -ne 1) {
            throw "Explicit shape remap $sourcePath -> $targetPath did not have retail-only source and reconstructed-only target files."
        }
        Assert-RetailShapeRemap -RetailSvgPath (Join-Path $RetailExportRoot $sourcePath) -ReconstructedSvgPath (Join-Path $ReconstructedExportRoot $targetPath) -RetailXmlPath $RetailXmlPath -ReconstructedXmlPath $ReconstructedXmlPath
    }
    $allPaths = @($retailFiles.RelativePath + $reconstructedFiles.RelativePath | Sort-Object -Unique)
    foreach ($relativePath in $allPaths) {
        $retail = $retailFiles | Where-Object RelativePath -ceq $relativePath | Select-Object -First 1
        $reconstructed = $reconstructedFiles | Where-Object RelativePath -ceq $relativePath | Select-Object -First 1
        $intentional = $intentionalScripts -contains $relativePath -or $intentionalSprite333Assets -contains $relativePath -or $relativePath -match '^scripts\\DefineSprite_600_ScreenOptionsGraphics(?:\\|$)' -or $relativePath -match '^sprites\\DefineSprite_600_ScreenOptionsGraphics(?:\\|$)' -or $mappedTargetPaths -contains $relativePath -or $shapeRemap.Keys -contains $relativePath
        if ($intentional) { continue }
        if ($null -eq $retail -or $null -eq $reconstructed -or $retail.Length -ne $reconstructed.Length -or $retail.Sha256 -cne $reconstructed.Sha256) {
            throw "Verified retail asset/script changed outside the explicit shell allowlist: $relativePath"
        }
    }
    Assert-RetailProtectedXmlMatches -RetailXmlPath $RetailXmlPath -ReconstructedXmlPath $ReconstructedXmlPath
    Write-Output 'Retail XML preservation: ordered protected root indices and fingerprints match.'
}

function Assert-ShellLiveHandshakeExport {
    param([string]$GfxPath, [string]$ExportRoot, [string]$FfdecPath, [string]$Context)
    $export = Invoke-ExternalProcess -FilePath $FfdecPath -Arguments @('-export', 'script', $ExportRoot, $GfxPath)
    if ($export.ExitCode -ne 0) { throw "FFDec failed to export $Context shell scripts: $($export.Output -join [Environment]::NewLine)" }
    $screenDirectory = @(Get-ChildItem -LiteralPath (Join-Path $ExportRoot 'scripts') -Recurse -Directory | Where-Object { $_.Name -eq 'DefineSprite_600_ScreenOptionsGraphics' })
    if ($screenDirectory.Count -ne 1) { throw "$Context export did not contain exactly one graphics screen directory." }
    $screenText = Get-Content -LiteralPath (Join-Path $screenDirectory[0].FullName 'frame_1\DoAction.as') -Raw
    Assert-ContainsOrdinal -Text $screenText -Token 'this.GraphicsOptionsController = new rs.ui.BatmanGraphicsOptionsController(this);' -Context "$Context controller initialization"
    Assert-ContainsOrdinal -Text $screenText -Token 'flash.external.ExternalInterface.call("FE_SetControlType",this.Settings[this.InitializationIndex].ReadRequest,"");' -Context "$Context live-state request"
    Assert-ContainsOrdinal -Text $screenText -Token 'flash.external.ExternalInterface.call("FE_GetControlType")' -Context "$Context live-state response poll"
    $rowPath = Join-Path $screenDirectory[0].FullName 'frame_1\PlaceObject2_290_List_Template_125\CLIPACTIONRECORD onClipEvent(load).as'
    $rowText = Get-Content -LiteralPath $rowPath -Raw
    Assert-ContainsOrdinal -Text $rowText -Token 'this.State = _parent.GraphicsOptionsController.GetDraftIndex(this.RowIndex);' -Context "$Context live VSync row state"
    Assert-ContainsOrdinal -Text $rowText -Token '"Unavailable" : "Loading..."' -Context "$Context explicit unresolved state"
}

function Assert-BuilderRejectsIni {
    param([string]$BuilderProject, [string]$Configuration, [string]$BuilderRoot, [string]$OutputDirectory, [string]$FfdecPath, [string]$IniPath, [string]$ExpectedDiagnostic, [string]$Context)
    $result = Invoke-ExternalProcess -FilePath 'dotnet' -Arguments @('run', '--no-build', '--project', $BuilderProject, '-c', $Configuration, '--', 'build-main-menu-graphics-shell', '--root', $BuilderRoot, '--output-dir', $OutputDirectory, '--ffdec', $FfdecPath, '--ini', $IniPath)
    if ($result.ExitCode -eq 0) { throw "$Context unexpectedly accepted INI '$IniPath'." }
    Assert-ContainsOrdinal -Text ($result.Output -join [Environment]::NewLine) -Token $ExpectedDiagnostic -Context "$Context diagnostic"
    $outputPath = Join-Path $OutputDirectory 'MainV2-graphics-options.gfx'
    if (Test-Path -LiteralPath $outputPath) { throw "$Context accepted output before rejecting INI '$IniPath': $outputPath" }
}

function Assert-CurrentGraphicsSourceProvenance {
    <# Prove the exact production shell call graph and keep legacy builders outside that graph. #>
    param(
        [Parameter(Mandatory = $true)] [string]$RebuildPath,
        [Parameter(Mandatory = $true)] [string]$ShellTemplatePath,
        [Parameter(Mandatory = $true)] [string]$ShellBuilderPath,
        [Parameter(Mandatory = $true)] [string]$BuilderProgramPath,
        [Parameter(Mandatory = $true)] [string]$XmlPatcherPath
    )

    foreach ($path in @($RebuildPath, $ShellTemplatePath, $ShellBuilderPath, $BuilderProgramPath, $XmlPatcherPath)) {
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw "Current graphics source was not found: $path" }
    }
    $rebuildText = Get-Content -LiteralPath $RebuildPath -Raw
    $shellTemplateText = Get-Content -LiteralPath $ShellTemplatePath -Raw
    $shellBuilderText = Get-Content -LiteralPath $ShellBuilderPath -Raw
    $builderProgramText = Get-Content -LiteralPath $BuilderProgramPath -Raw
    $xmlPatcherText = Get-Content -LiteralPath $XmlPatcherPath -Raw
    $legacyTemplatePath = Join-Path (Split-Path -Parent $ShellTemplatePath) 'GraphicsOptionsScriptTemplates.cs'
    if (-not (Test-Path -LiteralPath $legacyTemplatePath -PathType Leaf)) { throw "Legacy graphics template source was not found: $legacyTemplatePath" }
    $legacyTemplateText = Get-Content -LiteralPath $legacyTemplatePath -Raw
    $legacySourceText = $legacyTemplateText + $shellBuilderText + $xmlPatcherText
    $shellBuilderShellMatch = [regex]::Match($shellBuilderText, '(?s)private static void PatchFrontendShellScripts\(.*?(?=\r?\n\s*/// <summary>)')
    $buildShellMatch = [regex]::Match($shellBuilderText, '(?s)public static void BuildShell\(.*?(?=\r?\n\s*/// <summary>)')
    $programDispatchMatch = [regex]::Match($builderProgramText, '(?s)"build-main-menu-graphics-shell"\s*=>\s*RunBuildMainMenuGraphicsShell\(tail\)')
    $programShellMatch = [regex]::Match($builderProgramText, '(?s)private static int RunBuildMainMenuGraphicsShell\(.*?(?=\r?\n\s*/// <summary>)')
    $patchShellMatch = [regex]::Match($xmlPatcherText, '(?s)public static void PatchShell\(.*?(?=\r?\n\s*/// <summary>)')
    $shellSpriteMatch = [regex]::Match($xmlPatcherText, '(?s)private static void AppendGraphicsShellSpriteAndExport\(.*?(?=\r?\n\s*/// <summary>)')
    foreach ($match in @($shellBuilderShellMatch, $buildShellMatch, $programShellMatch, $patchShellMatch, $shellSpriteMatch)) {
        if (-not $match.Success) { throw 'Current graphics shell call-graph method could not be isolated.' }
    }
    if (-not $programDispatchMatch.Success) { throw 'NativeSubtitleExePatcher does not dispatch the production shell command to RunBuildMainMenuGraphicsShell.' }
    $shellBuilderShellText = $shellBuilderShellMatch.Value
    $buildShellText = $buildShellMatch.Value
    $programShellText = $programShellMatch.Value
    $patchShellText = $patchShellMatch.Value
    $shellSpriteText = $shellSpriteMatch.Value
    foreach ($required in @('build-main-menu-graphics-shell', '--output-dir', '--ffdec', '--ini', 'GraphicsOptionsAssetBuilder.BuildShell(paths)')) {
        Assert-ContainsOrdinal -Text ($rebuildText + $programShellText) -Token $required -Context 'production graphics shell call graph'
    }
    foreach ($required in @('GraphicsOptionsShellBuildPaths paths = GraphicsOptionsShellBuildPaths.FromRoot', 'GraphicsOptionsAssetBuilder.BuildShell(paths)')) {
        Assert-ContainsOrdinal -Text $programShellText -Token $required -Context 'NativeSubtitleExePatcher graphics shell route'
    }
    foreach ($required in @('ValidateShellInputs(paths)', 'BatmanGraphicsIniBootstrapLoader.Load(paths.BatmanUserIniPath)', 'PatchFrontendShellScripts(paths.FrontendWorkingScriptsPath, bootstrapSnapshot)', 'ValidateShellPatchedScriptSet(paths.FrontendWorkingScriptsPath)', 'GraphicsOptionsXmlPatcher.PatchShell(paths.FrontendXmlPath, paths.FrontendPatchedXmlPath)', '"-importScript"', 'paths.FrontendOutputGfxPath', 'paths.FrontendWorkingScriptsPath')) {
        Assert-ContainsOrdinal -Text $buildShellText -Token $required -Context 'GraphicsOptionsAssetBuilder BuildShell route'
    }
    Assert-ContainsOrdinal -Text $shellBuilderShellText -Token 'GraphicsOptionsShellScriptTemplates' -Context 'shell script template route'
    Assert-ContainsOrdinal -Text $patchShellText -Token 'AppendGraphicsShellSpriteAndExport(tags, optionsGamePcSprite)' -Context 'selective sprite-600 patch route'
    foreach ($required in @('ScreenOptionsGraphicsSpriteId', 'PatchGraphicsScreenSprite', 'CreateExportAssetsTag', 'CloneDoInitActionTagForSprite')) {
        Assert-ContainsOrdinal -Text $shellSpriteText -Token $required -Context 'selective sprite-600 import route'
    }
    foreach ($legacyToken in @('Helen_', 'GraphicsExitPrompt', 'DefineSprite_601')) {
        Assert-ContainsOrdinal -Text $legacySourceText -Token $legacyToken -Context "legacy full-controller source ($legacyToken)"
    }
    foreach ($legacyToken in @('public static void Patch(string inputXmlPath, string outputXmlPath)', 'AppendGraphicsSpritesAndExports', 'GraphicsExitPromptSpriteId')) {
        Assert-ContainsOrdinal -Text $xmlPatcherText -Token $legacyToken -Context "legacy XML patch route ($legacyToken)"
    }
    foreach ($forbidden in @('F:\helenhook.7z', 'F:/helenhook.7z', 'batma/', 'batma\', 'Program Files', 'GraphicsVsyncController', 'InitialVsync', 'DraftVsync', 'GraphicsExitPrompt', 'DefineSprite_601', 'Helen_', 'prompt export', 'prompt route', 'old package', 'historical', 'PatchFrontendScripts(', 'GraphicsOptionsScriptTemplates', 'AppendGraphicsSpritesAndExports', 'GraphicsExitPromptSpriteId', 'YesNoPrompt', 'Patch(inputXmlPath, outputXmlPath)')) {
        foreach ($source in @(
            [pscustomobject]@{ Name = 'rebuild'; Text = $rebuildText },
            [pscustomobject]@{ Name = 'current shell templates'; Text = $shellTemplateText },
            [pscustomobject]@{ Name = 'shell builder BuildShell'; Text = $buildShellText },
            [pscustomobject]@{ Name = 'shell builder shell method'; Text = $shellBuilderShellText },
            [pscustomobject]@{ Name = 'NativeSubtitleExePatcher shell route'; Text = $programShellText },
            [pscustomobject]@{ Name = 'XmlPatcher PatchShell'; Text = $patchShellText },
            [pscustomobject]@{ Name = 'XmlPatcher selective sprite route'; Text = $shellSpriteText }
        )) {
            Assert-NotContainsOrdinal -Text $source.Text -Token $forbidden -Context "$($source.Name) graphics provenance"
        }
    }
    $allowListMatch = [regex]::Match($shellBuilderText, '(?s)ShellPatchedScriptRelativePaths\s*=\s*\[(?<items>.*?)\];')
    if (-not $allowListMatch.Success -or (([regex]::Matches($allowListMatch.Groups['items'].Value, '"')).Count / 2) -ne 21) {
        throw 'Current shell builder must retain exactly 21 allow-listed patched source files.'
    }
}

if ($MutationTestsOnly) {
    Assert-RetailProtectedXmlMutationTests
    Write-Output 'PASS'
    return
}

if ([string]::IsNullOrWhiteSpace($BatmanRoot)) { $BatmanRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path } else { $BatmanRoot = (Resolve-Path $BatmanRoot).Path }
if ([string]::IsNullOrWhiteSpace($BuilderRoot)) { $BuilderRoot = Join-Path $BatmanRoot 'builder' } elseif ([IO.Path]::IsPathRooted($BuilderRoot)) { $BuilderRoot = [IO.Path]::GetFullPath($BuilderRoot) } elseif (Test-Path -LiteralPath $BuilderRoot) { $BuilderRoot = (Resolve-Path $BuilderRoot).Path } else { $BuilderRoot = [IO.Path]::GetFullPath((Join-Path $BatmanRoot $BuilderRoot)) }
if ([string]::IsNullOrWhiteSpace($RetailFrontendPackagePath)) { $RetailFrontendPackagePath = Join-Path $BuilderRoot 'extracted\frontend-retail\Frontend.umap' } elseif (-not [IO.Path]::IsPathRooted($RetailFrontendPackagePath)) { $RetailFrontendPackagePath = [IO.Path]::GetFullPath((Join-Path (Get-Location) $RetailFrontendPackagePath)) }
$RetailFrontendPackagePath = [IO.Path]::GetFullPath($RetailFrontendPackagePath)
if ([string]::IsNullOrWhiteSpace($BatmanUserIniPath)) {
    $documentsPath = [Environment]::GetFolderPath([Environment+SpecialFolder]::MyDocuments)
    $BatmanUserIniPath = Join-Path $documentsPath 'Square Enix\Batman Arkham Asylum GOTY\BmGame\Config\BmEngine.ini'
} else {
    $BatmanUserIniPath = [IO.Path]::GetFullPath($BatmanUserIniPath)
}
if (-not (Test-Path -LiteralPath $BatmanUserIniPath -PathType Leaf)) { throw "Batman user INI was not found as a file: $BatmanUserIniPath" }

$match = & (Join-Path $PSScriptRoot 'Get-BatmanSteamBuildMatch.ps1')
if ($match.BuildId -cne 'steam-goty-1.0' -or $match.Executable -cne 'ShippingPC-BmGame.exe' -or [int64]$match.FileSize -ne 38758728 -or $match.Sha256 -cne '4DAC1F5E2AC6710B7378FDCE74601F616F4753E3756CB5FDA63C7519CC2EB028') { throw 'Steam build match is not the verified 4DAC retail executable identity.' }
$baseInfo = Get-Item -LiteralPath $RetailFrontendPackagePath
$baseHash = (Get-FileHash -LiteralPath $RetailFrontendPackagePath -Algorithm SHA256).Hash
if ($baseInfo.Length -ne 2988548 -or $baseHash -cne '271916B888F83374122AF0FCCC5C685804F4C8286A92A772CD71E4F48A00F2CC') { throw 'Retail frontend input does not match the verified 2988548-byte base.' }

$builderProject = Join-Path $BuilderRoot 'tools\NativeSubtitleExePatcher\SubtitleSizeModBuilder\SubtitleSizeModBuilder.csproj'
$patcherProject = Join-Path $BuilderRoot 'tools\NativeSubtitleExePatcher\BmGameGfxPatcher\BmGameGfxPatcher.csproj'
$ffdec = Join-Path $BuilderRoot 'extracted\ffdec\ffdec-cli.exe'
foreach ($path in @($builderProject, $patcherProject, $ffdec, $RetailFrontendPackagePath)) { if (-not (Test-Path -LiteralPath $path)) { throw "Required retail patch input not found: $path" } }

foreach ($projectPath in @($builderProject, $patcherProject)) {
    $build = Invoke-ExternalProcess -FilePath 'dotnet' -Arguments @('build', $projectPath, '-c', $Configuration, '--nologo', '--disable-build-servers', '-nr:false', '-p:UseSharedCompilation=false')
    if ($build.ExitCode -ne 0) { throw "Retail patch dependency build failed for '$projectPath': $($build.Output -join [Environment]::NewLine)" }
}

$tempRoot = Join-Path ([IO.Path]::GetTempPath()) ('HelenBatmanGraphicsRetailPatch-' + [Guid]::NewGuid().ToString('N'))
$bootstrapAIniPath = Join-Path $tempRoot 'bootstrap-a.ini'
$bootstrapBIniPath = Join-Path $tempRoot 'bootstrap-b.ini'
$malformedIniPath = Join-Path $tempRoot 'bootstrap-malformed.ini'
$missingIniPath = Join-Path $tempRoot 'bootstrap-missing.ini'
$bootstrapARoot = Join-Path $tempRoot 'prototype-a'
$bootstrapBRoot = Join-Path $tempRoot 'prototype-b'
$normalRoot = Join-Path $tempRoot 'prototype-normal'
$bootstrapAExportRoot = Join-Path $tempRoot 'export-a'
$bootstrapBExportRoot = Join-Path $tempRoot 'export-b'
$invalidMissingOutputRoot = Join-Path $tempRoot 'invalid-missing-output'
$invalidMalformedOutputRoot = Join-Path $tempRoot 'invalid-malformed-output'
$bootstrapATarget = Join-Path $tempRoot 'Frontend-a.umap'
$bootstrapBTarget = Join-Path $tempRoot 'Frontend-b.umap'
$normalTarget = Join-Path $tempRoot 'Frontend-normal.umap'
$reconstructedTarget = Join-Path $tempRoot 'Frontend-reconstructed.umap'
$bootstrapADelta = Join-Path $tempRoot 'Frontend-a.hgdelta'
$bootstrapBDelta = Join-Path $tempRoot 'Frontend-b.hgdelta'
$normalDelta = Join-Path $tempRoot 'Frontend-normal.hgdelta'
$prototypeRoot = $bootstrapBRoot
$prototypeGfx = Join-Path $prototypeRoot 'MainV2-graphics-options.gfx'
$normalGfx = Join-Path $normalRoot 'MainV2-graphics-options.gfx'
$manifestPath = Join-Path $tempRoot 'patch.manifest.json'
$patchedPackage = $bootstrapBTarget
$extractedGfx = Join-Path $tempRoot 'MainV2.gfx'
$retailBaseGfx = Join-Path $tempRoot 'MainV2-retail.gfx'
$reconstructedGfx = Join-Path $tempRoot 'MainV2-reconstructed.gfx'
$xmlPath = Join-Path $tempRoot 'MainV2.xml'
$retailBaseXmlPath = Join-Path $tempRoot 'MainV2-retail.xml'
$reconstructedXmlPath = Join-Path $tempRoot 'MainV2-reconstructed.xml'
$exportRoot = Join-Path $tempRoot 'export'
$retailBaseExportRoot = Join-Path $tempRoot 'export-retail'
$reconstructedExportRoot = Join-Path $tempRoot 'export-reconstructed'
$retailBaseAssetExportRoot = Join-Path $tempRoot 'assets-retail'
$reconstructedAssetExportRoot = Join-Path $tempRoot 'assets-reconstructed'
$normalTargetPath = Join-Path $BuilderRoot 'generated\graphics-options-experiment\Frontend-graphics-options.umap'
$normalDeltaPath = Join-Path $BatmanRoot 'helengamehook\packs\batman-aa-graphics-options\builds\steam-goty-1.0\assets\deltas\Frontend-graphics-options.hgdelta'
$rebuildSourcePath = Join-Path $PSScriptRoot 'Rebuild-BatmanGraphicsOptionsExperiment.ps1'
$buildHgdeltaPath = Join-Path $PSScriptRoot 'Build-Hgdelta.ps1'
New-Item -ItemType Directory -Force -Path $tempRoot | Out-Null
try {
    . $rebuildSourcePath -FunctionsOnly -BatmanRoot $BatmanRoot -BuilderRoot $BuilderRoot -BatmanUserIniPath $BatmanUserIniPath
    Assert-CurrentGraphicsSourceProvenance -RebuildPath $rebuildSourcePath -ShellTemplatePath (Join-Path $BuilderRoot 'tools\NativeSubtitleExePatcher\SubtitleSizeModBuilder\GraphicsOptionsShellScriptTemplates.cs') -ShellBuilderPath (Join-Path $BuilderRoot 'tools\NativeSubtitleExePatcher\SubtitleSizeModBuilder\GraphicsOptionsAssetBuilder.cs') -BuilderProgramPath (Join-Path $BuilderRoot 'tools\NativeSubtitleExePatcher\SubtitleSizeModBuilder\Program.cs') -XmlPatcherPath (Join-Path $BuilderRoot 'tools\NativeSubtitleExePatcher\SubtitleSizeModBuilder\GraphicsOptionsXmlPatcher.cs')
    New-GraphicsOptionsIniFixture -SourcePath $BatmanUserIniPath -DestinationPath $bootstrapAIniPath -Vsync $false -MsaaSamples 1 -PhysxLevel 0 -Stereo $false
    New-GraphicsOptionsIniFixture -SourcePath $BatmanUserIniPath -DestinationPath $bootstrapBIniPath -Vsync $true -MsaaSamples 16 -PhysxLevel 2 -Stereo $true
    New-MalformedIniFixture -SourcePath $BatmanUserIniPath -DestinationPath $malformedIniPath
    Assert-ProductionGraphicsIniSnapshot -BuilderProject $builderProject -Configuration $Configuration -IniPath $bootstrapAIniPath -ExpectedValues @(0, 0, 0, 0) -Context 'Group 1 A production parser'
    Assert-ProductionGraphicsIniSnapshot -BuilderProject $builderProject -Configuration $Configuration -IniPath $bootstrapBIniPath -ExpectedValues @(1, 5, 2, 1) -Context 'Group 1 B production parser'
    Assert-BuilderRejectsIni -BuilderProject $builderProject -Configuration $Configuration -BuilderRoot $BuilderRoot -OutputDirectory $invalidMissingOutputRoot -FfdecPath $ffdec -IniPath $missingIniPath -ExpectedDiagnostic "Required path not found: $missingIniPath" -Context 'Missing INI validation'
    Assert-BuilderRejectsIni -BuilderProject $builderProject -Configuration $Configuration -BuilderRoot $BuilderRoot -OutputDirectory $invalidMalformedOutputRoot -FfdecPath $ffdec -IniPath $malformedIniPath -ExpectedDiagnostic "INI value 'SystemSettings.UseVsync' must be a boolean-like value but was 'Malformed'." -Context 'Malformed INI validation'
    Invoke-ShellBuild -BuilderProject $builderProject -Configuration $Configuration -BuilderRoot $BuilderRoot -OutputDirectory $bootstrapARoot -FfdecPath $ffdec -IniPath $bootstrapAIniPath
    Invoke-ShellBuild -BuilderProject $builderProject -Configuration $Configuration -BuilderRoot $BuilderRoot -OutputDirectory $bootstrapBRoot -FfdecPath $ffdec -IniPath $bootstrapBIniPath
    Invoke-ShellBuild -BuilderProject $builderProject -Configuration $Configuration -BuilderRoot $BuilderRoot -OutputDirectory $normalRoot -FfdecPath $ffdec -IniPath $BatmanUserIniPath
    Assert-ShellLiveHandshakeExport -GfxPath (Join-Path $bootstrapARoot 'MainV2-graphics-options.gfx') -ExportRoot $bootstrapAExportRoot -FfdecPath $ffdec -Context 'Group 1 A (VSync Off, MSAA Off, PhysX Off, Stereo Off) input'
    Assert-ShellLiveHandshakeExport -GfxPath (Join-Path $bootstrapBRoot 'MainV2-graphics-options.gfx') -ExportRoot $bootstrapBExportRoot -FfdecPath $ffdec -Context 'Group 1 B (VSync On, MSAA 16x, PhysX High, Stereo On) input'
    $aShellHash = (Get-FileHash -LiteralPath (Join-Path $bootstrapARoot 'MainV2-graphics-options.gfx') -Algorithm SHA256).Hash
    $bShellHash = (Get-FileHash -LiteralPath (Join-Path $bootstrapBRoot 'MainV2-graphics-options.gfx') -Algorithm SHA256).Hash
    $normalShellHash = (Get-FileHash -LiteralPath $normalGfx -Algorithm SHA256).Hash
    Assert-ExpectedSha256 -Hash $aShellHash -Expected $ExpectedGraphicsShellSha256 -Context 'Group 1 A shell GFX hash'
    Assert-ExpectedSha256 -Hash $bShellHash -Expected $ExpectedGraphicsShellSha256 -Context 'Group 1 B shell GFX hash'
    Assert-ExpectedSha256 -Hash $normalShellHash -Expected $ExpectedGraphicsShellSha256 -Context 'normal shell GFX hash'
    if ($aShellHash -cne $bShellHash -or $aShellHash -cne $normalShellHash) { throw "Graphics shell bytes vary with build-time Group 1 values. A=$aShellHash B=$bShellHash normal=$normalShellHash" }
    if (-not (Test-Path -LiteralPath $prototypeGfx)) { throw "Shell prototype was not generated: $prototypeGfx" }

    $manifest = [ordered]@{ name = 'MainV2 graphics-options shell retail patch'; patches = @([ordered]@{ owner = 'MainMenu'; exportName = 'MainV2'; exportType = 'GFxMovieInfo'; replacementPath = (Join-Path $bootstrapARoot 'MainV2-graphics-options.gfx'); payloadMagic = 'GFX' }) }
    [IO.File]::WriteAllText($manifestPath, ($manifest | ConvertTo-Json -Depth 5), [Text.UTF8Encoding]::new($false))
    Invoke-RetailGraphicsPatch -PatcherProject $patcherProject -Configuration $Configuration -RetailPackagePath $RetailFrontendPackagePath -ManifestPath $manifestPath -OutputPath $bootstrapATarget
    $manifest.patches[0].replacementPath = (Join-Path $bootstrapBRoot 'MainV2-graphics-options.gfx')
    [IO.File]::WriteAllText($manifestPath, ($manifest | ConvertTo-Json -Depth 5), [Text.UTF8Encoding]::new($false))
    Invoke-RetailGraphicsPatch -PatcherProject $patcherProject -Configuration $Configuration -RetailPackagePath $RetailFrontendPackagePath -ManifestPath $manifestPath -OutputPath $bootstrapBTarget
    $manifest.patches[0].replacementPath = $normalGfx
    [IO.File]::WriteAllText($manifestPath, ($manifest | ConvertTo-Json -Depth 5), [Text.UTF8Encoding]::new($false))
    Invoke-RetailGraphicsPatch -PatcherProject $patcherProject -Configuration $Configuration -RetailPackagePath $RetailFrontendPackagePath -ManifestPath $manifestPath -OutputPath $normalTarget
    Invoke-GraphicsHgdeltaBuild -BuildHgdeltaPath $buildHgdeltaPath -RetailPackagePath $RetailFrontendPackagePath -TargetPackagePath $bootstrapATarget -OutputPath $bootstrapADelta
    Invoke-GraphicsHgdeltaBuild -BuildHgdeltaPath $buildHgdeltaPath -RetailPackagePath $RetailFrontendPackagePath -TargetPackagePath $bootstrapBTarget -OutputPath $bootstrapBDelta
    Invoke-GraphicsHgdeltaBuild -BuildHgdeltaPath $buildHgdeltaPath -RetailPackagePath $RetailFrontendPackagePath -TargetPackagePath $normalTarget -OutputPath $normalDelta
    $hashes = @(
        [pscustomobject]@{ Name = 'shell GFX'; A = $aShellHash; B = $bShellHash; Normal = $normalShellHash },
        [pscustomobject]@{ Name = 'Frontend target'; A = (Get-FileHash -LiteralPath $bootstrapATarget -Algorithm SHA256).Hash; B = (Get-FileHash -LiteralPath $bootstrapBTarget -Algorithm SHA256).Hash; Normal = (Get-FileHash -LiteralPath $normalTarget -Algorithm SHA256).Hash },
        [pscustomobject]@{ Name = 'Frontend delta'; A = (Get-FileHash -LiteralPath $bootstrapADelta -Algorithm SHA256).Hash; B = (Get-FileHash -LiteralPath $bootstrapBDelta -Algorithm SHA256).Hash; Normal = (Get-FileHash -LiteralPath $normalDelta -Algorithm SHA256).Hash }
    )
    foreach ($hash in $hashes) {
        Assert-FullSha256 -Hash $hash.A -Context "$($hash.Name) A hash"
        Assert-FullSha256 -Hash $hash.B -Context "$($hash.Name) B hash"
        Assert-FullSha256 -Hash $hash.Normal -Context "$($hash.Name) normal hash"
        if ($hash.A -cne $hash.B -or $hash.A -cne $hash.Normal) { throw "$($hash.Name) provenance mismatch. A=$($hash.A) B=$($hash.B) normal=$($hash.Normal)" }
    }
    if (-not (Test-Path -LiteralPath $normalTargetPath -PathType Leaf) -or -not (Test-Path -LiteralPath $normalDeltaPath -PathType Leaf)) { throw 'Production graphics target/delta was not regenerated before retail provenance validation.' }
    $productionTargetHash = (Get-FileHash -LiteralPath $normalTargetPath -Algorithm SHA256).Hash
    $productionDeltaHash = (Get-FileHash -LiteralPath $normalDeltaPath -Algorithm SHA256).Hash
    Assert-FullSha256 -Hash $productionTargetHash -Context 'production target hash'
    Assert-FullSha256 -Hash $productionDeltaHash -Context 'production delta hash'
    if ($hashes[1].A -cne $productionTargetHash -or $hashes[2].A -cne $productionDeltaHash) { throw "Isolated outputs do not match the regenerated production artifact. isolatedTarget=$($hashes[1].A) productionTarget=$productionTargetHash isolatedDelta=$($hashes[2].A) productionDelta=$productionDeltaHash" }
    [byte[]]$reconstructedBytes = Reconstruct-RetailHgdeltaTarget -BasePath $RetailFrontendPackagePath -DeltaPath $normalDelta
    [IO.File]::WriteAllBytes($reconstructedTarget, $reconstructedBytes)
    [byte[]]$normalTargetBytes = [IO.File]::ReadAllBytes($normalTarget)
    [byte[]]$productionTargetBytes = [IO.File]::ReadAllBytes($normalTargetPath)
    if (-not [Linq.Enumerable]::SequenceEqual($reconstructedBytes, $normalTargetBytes)) { throw 'Retail delta reconstruction does not byte-match the isolated current-run target.' }
    if (-not [Linq.Enumerable]::SequenceEqual($reconstructedBytes, $productionTargetBytes)) { throw 'Retail delta reconstruction does not byte-match the regenerated production target.' }
    $reconstructedTargetHash = (Get-FileHash -LiteralPath $reconstructedTarget -Algorithm SHA256).Hash
    Assert-FullSha256 -Hash $reconstructedTargetHash -Context 'reconstructed target hash'
    Write-Output "Retail round-trip reconstruction: Target=$reconstructedTargetHash (verified retail base + current delta)"
    Write-Output "Reproducibility hashes: GFX=$aShellHash Target=$($hashes[1].A) Delta=$($hashes[2].A)"

    . (Join-Path $PSScriptRoot 'BatmanPackVerificationHelpers.ps1')
    $patchedPackage = $reconstructedTarget
    $patchedStorage = Get-UnrealPackageStorageInfo -Path $patchedPackage
    if ($patchedStorage.CompressionChunkCount -le 0) { throw 'Retail patch output must remain chunk-compressed.' }
    $baseExtract = Invoke-ExternalProcess -FilePath 'dotnet' -Arguments @('run', '--no-build', '--project', $patcherProject, '-c', $Configuration, '--', 'extract-gfx', '--package', $RetailFrontendPackagePath, '--owner', 'MainMenu', '--name', 'MainV2', '--output', $retailBaseGfx)
    if ($baseExtract.ExitCode -ne 0) { throw 'Failed to extract verified retail MainV2 for preservation comparison.' }
    $baseXml = Invoke-ExternalProcess -FilePath $ffdec -Arguments @('-swf2xml', $retailBaseGfx, $retailBaseXmlPath)
    if ($baseXml.ExitCode -ne 0) { throw 'FFDec failed to export verified retail MainV2 XML.' }
    $baseScriptsAssets = Invoke-ExternalProcess -FilePath $ffdec -Arguments @('-export', 'script,image,shape,sprite', $retailBaseAssetExportRoot, $retailBaseGfx)
    if ($baseScriptsAssets.ExitCode -ne 0) { throw 'FFDec failed to export verified retail MainV2 scripts/assets.' }
    $reconstructedExtract = Invoke-ExternalProcess -FilePath 'dotnet' -Arguments @('run', '--no-build', '--project', $patcherProject, '-c', $Configuration, '--', 'extract-gfx', '--package', $patchedPackage, '--owner', 'MainMenu', '--name', 'MainV2', '--output', $reconstructedGfx)
    if ($reconstructedExtract.ExitCode -ne 0) { throw 'Failed to extract independently reconstructed MainV2.' }
    $reconstructedXml = Invoke-ExternalProcess -FilePath $ffdec -Arguments @('-swf2xml', $reconstructedGfx, $reconstructedXmlPath)
    if ($reconstructedXml.ExitCode -ne 0) { throw 'FFDec failed to export independently reconstructed MainV2 XML.' }
    $reconstructedScriptsAssets = Invoke-ExternalProcess -FilePath $ffdec -Arguments @('-export', 'script,image,shape,sprite', $reconstructedAssetExportRoot, $reconstructedGfx)
    if ($reconstructedScriptsAssets.ExitCode -ne 0) { throw 'FFDec failed to export independently reconstructed MainV2 scripts/assets.' }
    Assert-RetailExportPreservation -RetailXmlPath $retailBaseXmlPath -ReconstructedXmlPath $reconstructedXmlPath -RetailExportRoot $retailBaseAssetExportRoot -ReconstructedExportRoot $reconstructedAssetExportRoot
    Write-Output 'Retail preservation: verified retail XML, scripts, images, shapes, and sprites match outside explicit sprite 333/600 and 21-script allowlists.'
    $extract = Invoke-ExternalProcess -FilePath 'dotnet' -Arguments @('run', '--no-build', '--project', $patcherProject, '-c', $Configuration, '--', 'extract-gfx', '--package', $patchedPackage, '--owner', 'MainMenu', '--name', 'MainV2', '--output', $extractedGfx)
    if ($extract.ExitCode -ne 0) { throw 'Failed to extract patched retail MainV2.' }
    $xmlResult = Invoke-ExternalProcess -FilePath $ffdec -Arguments @('-swf2xml', $extractedGfx, $xmlPath)
    if ($xmlResult.ExitCode -ne 0) { throw 'FFDec failed to reopen patched retail MainV2.' }
    $export = Invoke-ExternalProcess -FilePath $ffdec -Arguments @('-export', 'script', $exportRoot, $extractedGfx)
    if ($export.ExitCode -ne 0) { throw 'FFDec failed to export patched retail scripts.' }

    [xml]$document = Get-Content -LiteralPath $xmlPath -Raw
    $sprites = @($document.SelectNodes("/swf/tags/item[@type='DefineSpriteTag' and @spriteId='600']"))
    if ($sprites.Count -ne 1) { throw "Expected exactly one shell sprite 600, found $($sprites.Count)." }
    if (@($document.SelectNodes("/swf/tags/item[@type='DefineSpriteTag' and @spriteId='601']")).Count -ne 0) { throw 'Patched retail shell must not contain sprite 601.' }
    $exports = @(@($document.SelectNodes("/swf/tags/item[@type='ExportAssetsTag']")) | Where-Object { @($_.names.item | Where-Object { $_ -eq 'ScreenOptionsGraphics' }).Count -gt 0 })
    if ($exports.Count -ne 1 -or @($exports[0].tags.item | Where-Object { $_ -eq '600' }).Count -ne 1) { throw 'Patched retail shell must export exactly sprite 600 as ScreenOptionsGraphics.' }

    $scriptsRoot = Join-Path $exportRoot 'scripts'
    $scriptFiles = @(Get-ChildItem -LiteralPath $scriptsRoot -Recurse -Filter *.as -File)
    $screenDirectories = @(Get-ChildItem -LiteralPath $scriptsRoot -Recurse -Directory | Where-Object { $_.Name -eq 'DefineSprite_600_ScreenOptionsGraphics' })
    if ($screenDirectories.Count -ne 1) { throw "Expected exactly one DefineSprite_600_ScreenOptionsGraphics directory, found $($screenDirectories.Count)." }
    $screenDirectory = $screenDirectories[0]
    $screenScriptPath = Join-Path $screenDirectory.FullName 'frame_1\DoAction.as'
    if (-not (Test-Path -LiteralPath $screenScriptPath)) { throw 'Known Options Graphics screen script was not exported.' }
    $screenText = Get-Content -LiteralPath $screenScriptPath -Raw
    $menuScriptPath = Join-Path $scriptsRoot 'DefineSprite_333_ScreenOptionsMenu\frame_1\PlaceObject2_117_GenericButton_37\CLIPACTIONRECORD onClipEvent(load).as'
    if (-not (Test-Path -LiteralPath $menuScriptPath)) { throw 'Known Options menu script was not exported.' }
    $menuText = Get-Content -LiteralPath $menuScriptPath -Raw
    Assert-ContainsOrdinal $menuText 'GotoScreen("OptionsGraphics")' 'Options menu script'
    Assert-ContainsOrdinal $menuText 'Graphics Options' 'Options menu script'
    foreach ($required in @('Graphics Options', 'CancelScreen', 'ReturnFromScreen', 'FE_SetActiveScreenName","Graphics Options')) { Assert-ContainsOrdinal $screenText $required 'Options Graphics screen script' }
    Assert-RowShellContract -ScreenDirectory $screenDirectory.FullName
    foreach ($required in @('rs.ui.BatmanGraphicsOptionsController', 'InitializationComplete', 'InitializationFailed', 'BeginInitialization', 'PollInitialization', 'GetDraftIndex', 'GetInitialIndex', 'ApplyChanges', 'BeginRollback', 'CompleteRollback', 'FailRollback', 'ReadRequest:4200', 'ReadRequest:4300', 'ReadRequest:4400', 'ReadRequest:4500', 'FE_GetControlType')) { Assert-ContainsOrdinal -Text $screenText -Token $required -Context 'Options Graphics screen script' }
    Assert-ContainsOrdinal -Text $screenText -Token 'this.Screen.BlockInput(true);' -Context 'Options Graphics apply input block'
    Assert-ContainsOrdinal -Text $screenText -Token 'this.Screen.BlockInput(false);' -Context 'Options Graphics apply input unblock'
    if ($screenText -notmatch 'this\.CurrentPendingSetting\.WriteRequestBase\s*\+\s*this\.CurrentPendingSetting\.DraftIndex') { throw 'Options Graphics screen script must dispatch the current setting write request through its draft index.' }
    if ($screenText -notmatch 'FE_SetControlType",4990\s*\+\s*this\.ApplySignalToggle\s*,\s*""\s*\)') { throw 'Options Graphics screen script must dispatch FE_SetControlType 4990 plus ApplySignalToggle with an empty second argument.' }
    foreach ($forbidden in @('Helen_GetInt', 'Helen_SetInt', 'Helen_RunCommand', 'Helen_ApplyBatmanGraphicsDraft', 'GraphicsExitPrompt', 'CaptureInitialState', 'GraphicsVsyncController', 'InitialVsync', 'DraftVsync', 'InitialStateResolved', 'InitialStateFailed', 'CompleteApply', 'SetVsync', 'IncrementVsync', 'DecrementVsync')) { Assert-NotContainsOrdinal -Text $screenText -Token $forbidden -Context 'Options Graphics screen script' }
    foreach ($forbidden in @('Helen_', 'GraphicsExitPrompt', 'loadBatmanGraphicsDraftIntoConfig', 'applyBatmanGraphicsDraft', 'Unsaved graphics changes', 'Some changes require a restart', 'ApplyWasDispatched')) { foreach ($file in $scriptFiles) { Assert-NotContainsOrdinal -Text (Get-Content -LiteralPath $file.FullName -Raw) -Token $forbidden -Context $file.Name } }
}
finally {
    if (Test-Path -LiteralPath $tempRoot) {
        if ($null -eq (Get-Command Remove-SafeMutationTarget -ErrorAction SilentlyContinue)) {
            throw "Safe temporary cleanup helper was not loaded for '$tempRoot'."
        }
        Remove-SafeMutationTarget -Path $tempRoot -AllowedDescendantRoots @(([IO.Path]::GetFullPath([IO.Path]::GetTempPath())).TrimEnd('\'))
    }
}

Write-Output 'PASS'
