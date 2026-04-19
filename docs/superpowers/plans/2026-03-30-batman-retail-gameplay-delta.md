# Batman Retail Gameplay Delta Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make the shipped Batman subtitle-size gameplay pack work against a true retail vanilla compressed `BmGame.u`, while keeping the frontend completely vanilla and out of the pack.

**Architecture:** Extend `BmGameGfxPatcher` with a logical package view that can read compressed retail `BmGame.u`, then add a compressed-package write path that reapplies patched export objects and rebuilds a valid compressed package file. After the patcher can read and rewrite retail gameplay packages directly, retarget the Batman builder and shipped pack workflow to a gameplay-only manifest whose base fingerprint is the real retail `BmGame.u`.

**Tech Stack:** C#/.NET 8 `BmGameGfxPatcher`, C#/.NET 8 `SubtitleSizeModBuilder`, PowerShell Batman build and deployment scripts, FFDec CLI, Helen runtime pack manifest/tests, Win32 MSBuild test binary

---

## File Map

### New files

- `docs/superpowers/plans/2026-03-30-batman-retail-gameplay-delta.md`
  - This implementation plan.
- `games/HelenBatmanAA/scripts/Test-BatmanRetailGameplayPackageReader.ps1`
  - Red/green regression for reading retail compressed `BmGame.u`, locating `PauseMenu.Pause` and `GameHUD.HUD`, and extracting the same GFX payload bytes as the trusted builder assets.
- `games/HelenBatmanAA/scripts/Test-BatmanRetailGameplayPatchBuild.ps1`
  - Red/green regression for patching the retail compressed `BmGame.u`, reopening the rebuilt output, and proving the patched Pause/HUD GFX payloads match the generated runtime-scale assets.
- `games/HelenBatmanAA/builder/tools/NativeSubtitleExePatcher/BmGameGfxPatcher/CompressionChunkRecord.cs`
  - One parsed retail compression chunk mapping between physical compressed bytes and logical package bytes.
- `games/HelenBatmanAA/builder/tools/NativeSubtitleExePatcher/BmGameGfxPatcher/LogicalPackageImage.cs`
  - Holds the logical package bytes plus the original compression metadata needed for rebuild decisions.
- `games/HelenBatmanAA/builder/tools/NativeSubtitleExePatcher/BmGameGfxPatcher/CompressedPackageReader.cs`
  - Reads the retail chunk table, inflates compressed chunks, and returns the logical package image used by export inspection.
- `games/HelenBatmanAA/builder/tools/NativeSubtitleExePatcher/BmGameGfxPatcher/CompressedPackageWriter.cs`
  - Recompresses patched logical bytes back into a retail-style chunk-compressed package file.

### Modified patcher source

- `games/HelenBatmanAA/builder/tools/NativeSubtitleExePatcher/BmGameGfxPatcher/UnrealPackage.cs`
  - Loads physical bytes, projects a logical package image, and reads names/imports/exports from logical offsets instead of blindly slicing physical bytes.
- `games/HelenBatmanAA/builder/tools/NativeSubtitleExePatcher/BmGameGfxPatcher/GfxPatchApplier.cs`
  - Switches from physical append-and-repoint behavior to a logical package patch path for compressed packages, then verifies the rebuilt output through the same parser.
- `games/HelenBatmanAA/builder/tools/NativeSubtitleExePatcher/BmGameGfxPatcher/EmbeddedPayloadExtractor.cs`
  - Uses the new logical package reader without claiming the input package must be unpacked.
- `games/HelenBatmanAA/builder/tools/NativeSubtitleExePatcher/BmGameGfxPatcher/README.md`
  - Documents the new retail compressed-package support and the gameplay-only Batman build flow.

### Modified Batman build and verification scripts

- `games/HelenBatmanAA/scripts/Prepare-BatmanBuilderWorkspace.ps1`
  - Prepares a gameplay-only builder workspace from a trusted retail `BmGame.u` plus FFDec and extracts Pause/HUD assets directly from the retail package.
- `games/HelenBatmanAA/scripts/Test-BatmanBuilderWorkspacePreparation.ps1`
  - Verifies the prepared gameplay-only workspace uses the retail `BmGame.u` base and still extracts the expected Pause/HUD assets.
- `games/HelenBatmanAA/scripts/Rebuild-BatmanPack.ps1`
  - Rebuilds only the gameplay package and writes a gameplay-only shipped manifest against the retail `BmGame.u` base.
- `games/HelenBatmanAA/scripts/Test-BatmanKnownGoodGameplayPackage.ps1`
  - Verifies the shipped Batman pack contains exactly one gameplay virtual file whose base fingerprint is the retail `BmGame.u`.
- `games/HelenBatmanAA/scripts/Test-BatmanInstalledBaseCompatibility.ps1`
  - Remains the mandatory deploy preflight and becomes stricter about the gameplay-only manifest shape.
- `games/HelenBatmanAA/scripts/Deploy-Batman.ps1`
  - Stages and deploys only the gameplay delta and rejects any leftover frontend virtual file entry.

### Modified runtime tests and shipped outputs

- `tests/HelenRuntime.Tests/PackRepositoryTests.cpp`
  - Keeps the generic multi-file synthetic coverage, but reverts the checked-in Batman pack expectations to the new single-file gameplay-only manifest and retail base fingerprint.
- `games/HelenBatmanAA/helengamehook/packs/batman-aa-subtitles/builds/steam-goty-1.0/files.json`
  - Becomes a gameplay-only `delta-on-read` manifest whose base fingerprint is the retail `BmGame.u`.
- `games/HelenBatmanAA/helengamehook/packs/batman-aa-subtitles/builds/steam-goty-1.0/assets/deltas/BmGame-subtitle-signal.hgdelta`
  - Rebuilt gameplay delta against the retail compressed base.

### Removed shipped frontend artifact

- `games/HelenBatmanAA/helengamehook/packs/batman-aa-subtitles/builds/steam-goty-1.0/assets/deltas/Frontend-main-menu-subtitle-size.hgdelta`
  - Delete this stale frontend experiment so the shipped Batman pack is unambiguously gameplay-only.

---

## Task 1: Add Retail Gameplay Reader Coverage

**Files:**
- Create: `games/HelenBatmanAA/scripts/Test-BatmanRetailGameplayPackageReader.ps1`
- Create: `games/HelenBatmanAA/builder/tools/NativeSubtitleExePatcher/BmGameGfxPatcher/CompressionChunkRecord.cs`
- Create: `games/HelenBatmanAA/builder/tools/NativeSubtitleExePatcher/BmGameGfxPatcher/LogicalPackageImage.cs`
- Create: `games/HelenBatmanAA/builder/tools/NativeSubtitleExePatcher/BmGameGfxPatcher/CompressedPackageReader.cs`
- Modify: `games/HelenBatmanAA/builder/tools/NativeSubtitleExePatcher/BmGameGfxPatcher/UnrealPackage.cs`
- Modify: `games/HelenBatmanAA/builder/tools/NativeSubtitleExePatcher/BmGameGfxPatcher/EmbeddedPayloadExtractor.cs`

- [ ] **Step 1: Write the failing retail-reader regression**

Create `games/HelenBatmanAA/scripts/Test-BatmanRetailGameplayPackageReader.ps1`:

```powershell
param(
    [Parameter(Mandatory = $true)]
    [string]$RetailBasePackagePath,
    [string]$BatmanRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path,
    [string]$Configuration = 'Debug'
)

$ErrorActionPreference = 'Stop'

$RetailBasePackagePath = (Resolve-Path $RetailBasePackagePath).Path
$BatmanRoot = (Resolve-Path $BatmanRoot).Path
$ToolProjectPath = Join-Path $BatmanRoot 'builder\tools\NativeSubtitleExePatcher\BmGameGfxPatcher\BmGameGfxPatcher.csproj'
$TemporaryRoot = Join-Path ([System.IO.Path]::GetTempPath()) ('BatmanRetailGameplayReader-' + [System.Guid]::NewGuid().ToString('N'))
$PauseOutputPath = Join-Path $TemporaryRoot 'Pause-extracted.gfx'
$HudOutputPath = Join-Path $TemporaryRoot 'HUD-extracted.gfx'
$ExpectedPauseSha256 = '0426443F03642194D888199D7BB190DE48E3F7C7EB589FB9E8D732728330A630'
$ExpectedHudSha256 = 'EF62EB89EB090E607B45AAF4AE46922CB2A78B678452F652F042480F4670D770'

try {
    New-Item -ItemType Directory -Force -Path $TemporaryRoot | Out-Null

    & dotnet build $ToolProjectPath -c $Configuration
    if ($LASTEXITCODE -ne 0) {
        throw 'dotnet build failed for BmGameGfxPatcher.csproj'
    }

    $PauseDescribeOutput = & dotnet run --project $ToolProjectPath -c $Configuration -- `
        describe-export `
        --package $RetailBasePackagePath `
        --owner PauseMenu `
        --name Pause `
        --type GFxMovieInfo 2>&1
    if ($LASTEXITCODE -ne 0) {
        throw "describe-export failed for PauseMenu.Pause: $PauseDescribeOutput"
    }

    $HudDescribeOutput = & dotnet run --project $ToolProjectPath -c $Configuration -- `
        describe-export `
        --package $RetailBasePackagePath `
        --owner GameHUD `
        --name HUD `
        --type GFxMovieInfo 2>&1
    if ($LASTEXITCODE -ne 0) {
        throw "describe-export failed for GameHUD.HUD: $HudDescribeOutput"
    }

    & dotnet run --project $ToolProjectPath -c $Configuration -- `
        extract-gfx `
        --package $RetailBasePackagePath `
        --owner PauseMenu `
        --name Pause `
        --output $PauseOutputPath
    if ($LASTEXITCODE -ne 0) {
        throw 'extract-gfx failed for PauseMenu.Pause'
    }

    & dotnet run --project $ToolProjectPath -c $Configuration -- `
        extract-gfx `
        --package $RetailBasePackagePath `
        --owner GameHUD `
        --name HUD `
        --output $HudOutputPath
    if ($LASTEXITCODE -ne 0) {
        throw 'extract-gfx failed for GameHUD.HUD'
    }

    if (-not (Test-Path -LiteralPath $PauseOutputPath)) {
        throw "Pause output was not created: $PauseOutputPath"
    }

    if (-not (Test-Path -LiteralPath $HudOutputPath)) {
        throw "HUD output was not created: $HudOutputPath"
    }

    $PauseSha256 = (Get-FileHash -LiteralPath $PauseOutputPath -Algorithm SHA256).Hash
    if ($PauseSha256 -ne $ExpectedPauseSha256) {
        throw "Retail Pause GFX hash mismatch. Expected $ExpectedPauseSha256 but found $PauseSha256."
    }

    $HudSha256 = (Get-FileHash -LiteralPath $HudOutputPath -Algorithm SHA256).Hash
    if ($HudSha256 -ne $ExpectedHudSha256) {
        throw "Retail HUD GFX hash mismatch. Expected $ExpectedHudSha256 but found $HudSha256."
    }

    Write-Output 'PASS'
}
finally {
    if (Test-Path -LiteralPath $TemporaryRoot) {
        Remove-Item -LiteralPath $TemporaryRoot -Recurse -Force
    }
}
```

- [ ] **Step 2: Run the new regression to prove the current reader fails on the retail package**

Run:

```powershell
powershell -ExecutionPolicy Bypass -File .\games\HelenBatmanAA\scripts\Test-BatmanRetailGameplayPackageReader.ps1 `
  -RetailBasePackagePath 'D:\steam\steamapps\common\Batman Arkham Asylum GOTY\BmGame\CookedPC\BmGame.u.original-backup' `
  -BatmanRoot .\games\HelenBatmanAA `
  -Configuration Debug
```

Expected:

- FAIL with the current `BmGameGfxPatcher` reader, typically from `Arithmetic operation resulted in an overflow.` before `describe-export` or `extract-gfx` can succeed

- [ ] **Step 3: Add logical-image support for retail compressed package reads**

Create `games/HelenBatmanAA/builder/tools/NativeSubtitleExePatcher/BmGameGfxPatcher/CompressionChunkRecord.cs`:

```csharp
namespace BmGameGfxPatcher;

/// <summary>
/// Describes one retail compression chunk mapping between the physical package file and the logical package image.
/// </summary>
internal sealed class CompressionChunkRecord
{
    /// <summary>
    /// Initializes one compression chunk mapping.
    /// </summary>
    /// <param name="compressedOffset">Physical file offset of the compressed chunk payload.</param>
    /// <param name="compressedSize">Physical byte length of the compressed chunk payload.</param>
    /// <param name="uncompressedOffset">Logical package offset where the decompressed chunk bytes begin.</param>
    /// <param name="uncompressedSize">Logical byte length produced after decompression.</param>
    public CompressionChunkRecord(int compressedOffset, int compressedSize, int uncompressedOffset, int uncompressedSize)
    {
        CompressedOffset = compressedOffset;
        CompressedSize = compressedSize;
        UncompressedOffset = uncompressedOffset;
        UncompressedSize = uncompressedSize;
    }

    /// <summary>
    /// Gets the physical file offset of the compressed payload.
    /// </summary>
    public int CompressedOffset { get; }

    /// <summary>
    /// Gets the physical compressed payload length.
    /// </summary>
    public int CompressedSize { get; }

    /// <summary>
    /// Gets the logical package offset of the decompressed chunk.
    /// </summary>
    public int UncompressedOffset { get; }

    /// <summary>
    /// Gets the logical byte length of the decompressed chunk.
    /// </summary>
    public int UncompressedSize { get; }
}
```

Create `games/HelenBatmanAA/builder/tools/NativeSubtitleExePatcher/BmGameGfxPatcher/LogicalPackageImage.cs`:

```csharp
namespace BmGameGfxPatcher;

/// <summary>
/// Holds the logical package bytes used by export parsing plus the original storage metadata needed for rebuild decisions.
/// </summary>
internal sealed class LogicalPackageImage
{
    /// <summary>
    /// Initializes one logical package image.
    /// </summary>
    /// <param name="bytes">Full logical package bytes that export offsets refer to.</param>
    /// <param name="compressionChunks">Retail compression chunks read from the physical package file.</param>
    /// <param name="usesCompressedStorage">True when the original package used chunk-compressed storage.</param>
    public LogicalPackageImage(byte[] bytes, IReadOnlyList<CompressionChunkRecord> compressionChunks, bool usesCompressedStorage)
    {
        Bytes = bytes;
        CompressionChunks = compressionChunks;
        UsesCompressedStorage = usesCompressedStorage;
    }

    /// <summary>
    /// Gets the logical package bytes that names, imports, exports, and object offsets refer to.
    /// </summary>
    public byte[] Bytes { get; }

    /// <summary>
    /// Gets the parsed retail compression chunk table from the source package.
    /// </summary>
    public IReadOnlyList<CompressionChunkRecord> CompressionChunks { get; }

    /// <summary>
    /// Gets a value indicating whether the original package used compressed physical storage.
    /// </summary>
    public bool UsesCompressedStorage { get; }
}
```

Create `games/HelenBatmanAA/builder/tools/NativeSubtitleExePatcher/BmGameGfxPatcher/CompressedPackageReader.cs`:

```csharp
using System.IO.Compression;

namespace BmGameGfxPatcher;

/// <summary>
/// Reads retail chunk-compressed Unreal packages into the logical byte image used by export parsing.
/// </summary>
internal static class CompressedPackageReader
{
    /// <summary>
    /// Builds the logical package image from one physical package file.
    /// </summary>
    /// <param name="physicalBytes">Exact bytes read from the physical package file.</param>
    /// <param name="header">Parsed package header.</param>
    /// <returns>The logical package image used by export parsing.</returns>
    public static LogicalPackageImage Read(byte[] physicalBytes, PackageHeader header)
    {
        IReadOnlyList<CompressionChunkRecord> chunks = ReadCompressionChunks(physicalBytes, header);
        if (chunks.Count == 0)
        {
            return new LogicalPackageImage((byte[])physicalBytes.Clone(), chunks, false);
        }

        byte[] logicalBytes = new byte[header.PackageSize];
        int prefixLength = Math.Min(header.CompressionChunkTableOffset, logicalBytes.Length);
        Array.Copy(physicalBytes, 0, logicalBytes, 0, prefixLength);

        foreach (CompressionChunkRecord chunk in chunks)
        {
            byte[] decompressedBytes = DecompressChunk(physicalBytes, chunk);
            decompressedBytes.CopyTo(logicalBytes, chunk.UncompressedOffset);
        }

        return new LogicalPackageImage(logicalBytes, chunks, true);
    }

    /// <summary>
    /// Reads the retail compression chunk table from the physical package header trailer.
    /// </summary>
    /// <param name="physicalBytes">Exact bytes read from the physical package file.</param>
    /// <param name="header">Parsed package header.</param>
    /// <returns>The ordered chunk table records.</returns>
    public static IReadOnlyList<CompressionChunkRecord> ReadCompressionChunks(byte[] physicalBytes, PackageHeader header)
    {
        if (header.CompressionChunkCount == 0)
        {
            return Array.Empty<CompressionChunkRecord>();
        }

        using var stream = new MemoryStream(physicalBytes, writable: false);
        using var reader = new BinaryReader(stream);
        reader.BaseStream.Seek(header.CompressionChunkTableOffset, SeekOrigin.Begin);

        var chunks = new List<CompressionChunkRecord>(header.CompressionChunkCount);
        for (int index = 0; index < header.CompressionChunkCount; index++)
        {
            chunks.Add(
                new CompressionChunkRecord(
                    reader.ReadInt32(),
                    reader.ReadInt32(),
                    reader.ReadInt32(),
                    reader.ReadInt32()));
        }

        return chunks;
    }

    /// <summary>
    /// Inflates one retail compressed chunk into its logical bytes.
    /// </summary>
    /// <param name="physicalBytes">Exact bytes read from the physical package file.</param>
    /// <param name="chunk">Chunk metadata describing the physical source bytes and logical destination span.</param>
    /// <returns>The decompressed logical chunk bytes.</returns>
    private static byte[] DecompressChunk(byte[] physicalBytes, CompressionChunkRecord chunk)
    {
        ValidatePhysicalSlice(physicalBytes, chunk.CompressedOffset, chunk.CompressedSize);

        using var input = new MemoryStream(physicalBytes, chunk.CompressedOffset, chunk.CompressedSize, writable: false);
        using var zlib = new ZLibStream(input, CompressionMode.Decompress, leaveOpen: false);
        byte[] buffer = new byte[chunk.UncompressedSize];
        int totalRead = 0;

        while (totalRead < buffer.Length)
        {
            int bytesRead = zlib.Read(buffer, totalRead, buffer.Length - totalRead);
            if (bytesRead == 0)
            {
                break;
            }

            totalRead += bytesRead;
        }

        if (totalRead != buffer.Length)
        {
            throw new InvalidOperationException(
                $"Compression chunk decompressed {totalRead} bytes, expected {buffer.Length}.");
        }

        return buffer;
    }

    /// <summary>
    /// Validates one physical package byte range before it is read.
    /// </summary>
    /// <param name="physicalBytes">Exact bytes read from the physical package file.</param>
    /// <param name="offset">Physical start offset.</param>
    /// <param name="length">Physical byte length.</param>
    private static void ValidatePhysicalSlice(byte[] physicalBytes, int offset, int length)
    {
        if (offset < 0 || length < 0 || offset + length > physicalBytes.Length)
        {
            throw new InvalidOperationException(
                $"Compression chunk points outside the physical package. offset={offset} length={length}");
        }
    }
}
```

Modify the relevant parts of `games/HelenBatmanAA/builder/tools/NativeSubtitleExePatcher/BmGameGfxPatcher/UnrealPackage.cs`:

```csharp
private UnrealPackage(
    string fullPath,
    byte[] physicalBytes,
    LogicalPackageImage logicalImage,
    PackageHeader header,
    IReadOnlyList<string> names,
    IReadOnlyList<ImportEntry> imports,
    IReadOnlyList<ExportEntry> exports)
{
    FullPath = fullPath;
    PhysicalBytes = physicalBytes;
    LogicalImage = logicalImage;
    Header = header;
    Names = names;
    Imports = imports;
    Exports = exports;
}

public byte[] PhysicalBytes { get; }

public LogicalPackageImage LogicalImage { get; }

public static UnrealPackage Load(string packagePath)
{
    string fullPath = Path.GetFullPath(packagePath);
    byte[] physicalBytes = File.ReadAllBytes(fullPath);
    using var headerStream = new MemoryStream(physicalBytes, writable: false);
    using var headerReader = new BinaryReader(headerStream, Encoding.ASCII, leaveOpen: false);

    PackageHeader header = ReadHeader(headerReader);
    if (header.Signature == EncryptedSignature)
    {
        throw new InvalidOperationException("Encrypted packages are not supported.");
    }

    if (header.Signature != PackageSignature)
    {
        throw new InvalidOperationException("File is not a valid Unreal package.");
    }

    LogicalPackageImage logicalImage = CompressedPackageReader.Read(physicalBytes, header);
    using var logicalStream = new MemoryStream(logicalImage.Bytes, writable: false);
    using var logicalReader = new BinaryReader(logicalStream, Encoding.ASCII, leaveOpen: false);

    List<string> names = ReadNames(logicalReader, header);
    List<ImportEntry> imports = ReadImports(logicalReader, header, names);
    List<ExportEntry> exports = ReadExports(logicalReader, header, names);

    var package = new UnrealPackage(fullPath, physicalBytes, logicalImage, header, names, imports, exports);
    foreach (ImportEntry importEntry in package.Imports)
    {
        importEntry.ResolveNames(package.ResolveObjectName);
    }

    foreach (ExportEntry exportEntry in package.Exports)
    {
        exportEntry.ResolveNames(package.ResolveObjectName);
    }

    return package;
}

public ReadOnlyMemory<byte> ReadObjectBytes(ExportEntry exportEntry)
{
    ValidateSlice(exportEntry.SerialDataOffset, exportEntry.SerialDataSize);
    return LogicalImage.Bytes.AsMemory(exportEntry.SerialDataOffset, exportEntry.SerialDataSize);
}
```

Replace the `PackageHeader` record in the same file with:

```csharp
internal sealed record PackageHeader(
    uint Signature,
    ushort Version,
    ushort Licensee,
    int PackageSize,
    uint Flags,
    int NameTableCount,
    int NameTableOffset,
    int ExportTableCount,
    int ExportTableOffset,
    int ImportTableCount,
    int ImportTableOffset,
    int DependsTableOffset,
    uint CompressionFlags,
    int CompressionChunkCount,
    int CompressionChunkTableOffset,
    int HeaderSize);
```

Update `ReadHeader` in the same file so it captures the retail compression metadata:

```csharp
uint compressionFlags = reader.ReadUInt32();
int compressionChunkCount = reader.ReadInt32();
int compressionChunkTableOffset = checked((int)reader.BaseStream.Position);

if (compressionChunkCount > 0)
{
    reader.BaseStream.Seek(compressionChunkCount * 16L, SeekOrigin.Current);
}

reader.BaseStream.Seek(8, SeekOrigin.Current);
int headerSize = checked((int)reader.BaseStream.Position);

return new PackageHeader(
    signature,
    version,
    licensee,
    packageSize,
    flags,
    nameCount,
    nameOffset,
    exportCount,
    exportOffset,
    importCount,
    importOffset,
    dependsOffset,
    compressionFlags,
    compressionChunkCount,
    compressionChunkTableOffset,
    headerSize);
```

Update the XML docs in `games/HelenBatmanAA/builder/tools/NativeSubtitleExePatcher/BmGameGfxPatcher/EmbeddedPayloadExtractor.cs`:

```csharp
/// <param name="packagePath">Path to the Unreal package that contains the export object.</param>
```

- [ ] **Step 4: Re-run the retail-reader regression and the existing direct export command**

Run:

```powershell
powershell -ExecutionPolicy Bypass -File .\games\HelenBatmanAA\scripts\Test-BatmanRetailGameplayPackageReader.ps1 `
  -RetailBasePackagePath 'D:\steam\steamapps\common\Batman Arkham Asylum GOTY\BmGame\CookedPC\BmGame.u.original-backup' `
  -BatmanRoot .\games\HelenBatmanAA `
  -Configuration Debug

dotnet run --project .\games\HelenBatmanAA\builder\tools\NativeSubtitleExePatcher\BmGameGfxPatcher\BmGameGfxPatcher.csproj -c Debug -- `
  describe-export `
  --package 'D:\steam\steamapps\common\Batman Arkham Asylum GOTY\BmGame\CookedPC\BmGame.u.original-backup' `
  --owner PauseMenu `
  --name Pause `
  --type GFxMovieInfo
```

Expected:

- `PASS` from `Test-BatmanRetailGameplayPackageReader.ps1`
- `describe-export` prints a normal export description instead of throwing an overflow exception

- [ ] **Step 5: Commit the retail-reader support**

Run:

```powershell
git add games/HelenBatmanAA/scripts/Test-BatmanRetailGameplayPackageReader.ps1 `
  games/HelenBatmanAA/builder/tools/NativeSubtitleExePatcher/BmGameGfxPatcher/CompressionChunkRecord.cs `
  games/HelenBatmanAA/builder/tools/NativeSubtitleExePatcher/BmGameGfxPatcher/LogicalPackageImage.cs `
  games/HelenBatmanAA/builder/tools/NativeSubtitleExePatcher/BmGameGfxPatcher/CompressedPackageReader.cs `
  games/HelenBatmanAA/builder/tools/NativeSubtitleExePatcher/BmGameGfxPatcher/UnrealPackage.cs `
  games/HelenBatmanAA/builder/tools/NativeSubtitleExePatcher/BmGameGfxPatcher/EmbeddedPayloadExtractor.cs
git commit -m "Read retail Batman gameplay packages"
```

---

## Task 2: Retarget Builder Prep To The Retail Gameplay Base

**Files:**
- Modify: `games/HelenBatmanAA/scripts/Prepare-BatmanBuilderWorkspace.ps1`
- Modify: `games/HelenBatmanAA/scripts/Test-BatmanBuilderWorkspacePreparation.ps1`

- [ ] **Step 1: Change the builder-workspace verifier so it expects a gameplay-only retail base**

Replace the setup section in `games/HelenBatmanAA/scripts/Test-BatmanBuilderWorkspacePreparation.ps1` with:

```powershell
$PrepareScriptPath = Join-Path $BatmanRoot 'scripts\Prepare-BatmanBuilderWorkspace.ps1'
$BasePackagePath = 'D:\steam\steamapps\common\Batman Arkham Asylum GOTY\BmGame\CookedPC\BmGame.u.original-backup'
$FfdecCliPath = Join-Path $BatmanRoot 'builder\extracted\ffdec\ffdec-cli.exe'
$ExpectedBaseSha256 = '4306148E7627EC2C0DE4144FD6AB45521B3B7E090D1028A0B685CADAFAFB89E6'
$ExpectedPauseGfxSha256 = '0426443F03642194D888199D7BB190DE48E3F7C7EB589FB9E8D732728330A630'
$ExpectedHudGfxSha256 = 'EF62EB89EB090E607B45AAF4AE46922CB2A78B678452F652F042480F4670D770'
$TemporaryRoot = Join-Path ([System.IO.Path]::GetTempPath()) ('BatmanBuilderPrep-' + [System.Guid]::NewGuid().ToString('N'))
$BuilderRoot = Join-Path $TemporaryRoot 'builder'
$PreparedBasePackagePath = Join-Path $BuilderRoot 'extracted\bmgame-retail\BmGame.u'
$PreparedPauseGfxPath = Join-Path $BuilderRoot 'extracted\pause\Pause-extracted.gfx'
$PreparedPauseXmlPath = Join-Path $BuilderRoot 'extracted\pause\Pause.xml'
$PreparedPauseScriptsRoot = Join-Path $BuilderRoot 'extracted\pause\pause-ffdec-export\scripts'
$PreparedHudGfxPath = Join-Path $BuilderRoot 'extracted\hud\HUD-extracted.gfx'
$PreparedHudXmlPath = Join-Path $BuilderRoot 'extracted\hud\HUD.xml'
$PreparedHudScriptsRoot = Join-Path $BuilderRoot 'extracted\hud\hud-ffdec-scripts\scripts'
$PreparedFfdecCliPath = Join-Path $BuilderRoot 'extracted\ffdec\ffdec-cli.exe'
$PauseScreenActionPath = Join-Path $PreparedPauseScriptsRoot 'DefineSprite_394_ScreenOptionsAudio\frame_1\DoAction.as'
$HudFrameActionPath = Join-Path $PreparedHudScriptsRoot 'DefineSprite_987\frame_1\DoAction.as'
```

Replace the PowerShell prep invocation and `requiredPaths` array with:

```powershell
& powershell -ExecutionPolicy Bypass -File $PrepareScriptPath `
    -BatmanRoot $BatmanRoot `
    -BuilderRoot $BuilderRoot `
    -BasePackagePath $BasePackagePath `
    -FfdecCliPath $FfdecCliPath `
    -Configuration $Configuration

$requiredPaths = @(
    $PreparedBasePackagePath,
    $PreparedPauseGfxPath,
    $PreparedPauseXmlPath,
    $PreparedPauseScriptsRoot,
    $PreparedHudGfxPath,
    $PreparedHudXmlPath,
    $PreparedHudScriptsRoot,
    $PreparedFfdecCliPath,
    $PauseScreenActionPath,
    $HudFrameActionPath
)
```

Delete the old frontend-path checks entirely.

- [ ] **Step 2: Run the updated prep verifier to prove the current prep script still has the wrong contract**

Run:

```powershell
powershell -ExecutionPolicy Bypass -File .\games\HelenBatmanAA\scripts\Test-BatmanBuilderWorkspacePreparation.ps1 `
  -BatmanRoot .\games\HelenBatmanAA `
  -Configuration Debug
```

Expected:

- FAIL because `Prepare-BatmanBuilderWorkspace.ps1` still requires `-FrontendBasePackagePath`
- or FAIL because it still stages `builder\extracted\bmgame-unpacked\BmGame.u` instead of `builder\extracted\bmgame-retail\BmGame.u`

- [ ] **Step 3: Make workspace prep gameplay-only and retail-based**

Update `games/HelenBatmanAA/scripts/Prepare-BatmanBuilderWorkspace.ps1` so its parameter block becomes:

```powershell
param(
    [string]$BatmanRoot,
    [string]$BuilderRoot,
    [Parameter(Mandatory = $true)]
    [string]$BasePackagePath,
    [Parameter(Mandatory = $true)]
    [string]$FfdecCliPath,
    [string]$Configuration = 'Release'
)
```

Replace the gameplay/frontend path setup with:

```powershell
$BasePackagePath = (Resolve-Path $BasePackagePath).Path
$FfdecCliPath = (Resolve-Path $FfdecCliPath).Path
$SourceBuilderRoot = Join-Path $BatmanRoot 'builder'
$ExtractedRoot = Join-Path $BuilderRoot 'extracted'
$ToolRoot = Join-Path $SourceBuilderRoot 'tools\NativeSubtitleExePatcher'
$BmGameGfxPatcherProjectPath = Join-Path $ToolRoot 'BmGameGfxPatcher\BmGameGfxPatcher.csproj'
$PreparedBasePackagePath = Join-Path $ExtractedRoot 'bmgame-retail\BmGame.u'
$PreparedFfdecRoot = Join-Path $ExtractedRoot 'ffdec'
$PreparedFfdecCliPath = Join-Path $PreparedFfdecRoot 'ffdec-cli.exe'
$PreparedPauseGfxPath = Join-Path $ExtractedRoot 'pause\Pause-extracted.gfx'
$PreparedPauseXmlPath = Join-Path $ExtractedRoot 'pause\Pause.xml'
$PreparedPauseExportRoot = Join-Path $ExtractedRoot 'pause\pause-ffdec-export'
$PreparedHudGfxPath = Join-Path $ExtractedRoot 'hud\HUD-extracted.gfx'
$PreparedHudXmlPath = Join-Path $ExtractedRoot 'hud\HUD.xml'
$PreparedHudExportRoot = Join-Path $ExtractedRoot 'hud\hud-ffdec-scripts'
$SourceFfdecRoot = Split-Path -Parent $FfdecCliPath

New-Item -ItemType Directory -Force -Path (Split-Path -Parent $PreparedBasePackagePath) | Out-Null
New-Item -ItemType Directory -Force -Path (Split-Path -Parent $PreparedPauseGfxPath) | Out-Null
New-Item -ItemType Directory -Force -Path (Split-Path -Parent $PreparedHudGfxPath) | Out-Null

if (-not [System.StringComparer]::OrdinalIgnoreCase.Equals($BasePackagePath, $PreparedBasePackagePath)) {
    Copy-Item -LiteralPath $BasePackagePath -Destination $PreparedBasePackagePath -Force
}
```

Keep the `extract-gfx` calls, but point them at `$PreparedBasePackagePath` and remove the frontend extraction block entirely. Replace the final summary with:

```powershell
Write-Output 'Prepared Batman builder workspace:'
Write-Output "  Builder root:   $BuilderRoot"
Write-Output "  Base package:   $PreparedBasePackagePath"
Write-Output "  Pause GFX:      $PreparedPauseGfxPath"
Write-Output "  Pause XML:      $PreparedPauseXmlPath"
Write-Output "  Pause scripts:  $(Join-Path $PreparedPauseExportRoot 'scripts')"
Write-Output "  HUD GFX:        $PreparedHudGfxPath"
Write-Output "  HUD XML:        $PreparedHudXmlPath"
Write-Output "  HUD scripts:    $(Join-Path $PreparedHudExportRoot 'scripts')"
Write-Output "  FFDec root:     $PreparedFfdecRoot"
```

- [ ] **Step 4: Re-run the gameplay-only prep verifier**

Run:

```powershell
powershell -ExecutionPolicy Bypass -File .\games\HelenBatmanAA\scripts\Test-BatmanBuilderWorkspacePreparation.ps1 `
  -BatmanRoot .\games\HelenBatmanAA `
  -Configuration Debug
```

Expected:

- `PASS`

- [ ] **Step 5: Commit the retail builder-prep flow**

Run:

```powershell
git add games/HelenBatmanAA/scripts/Prepare-BatmanBuilderWorkspace.ps1 `
  games/HelenBatmanAA/scripts/Test-BatmanBuilderWorkspacePreparation.ps1
git commit -m "Prepare Batman workspace from retail gameplay package"
```

---

## Task 3: Add Retail Gameplay Patch-Write Coverage

**Files:**
- Create: `games/HelenBatmanAA/scripts/Test-BatmanRetailGameplayPatchBuild.ps1`
- Create: `games/HelenBatmanAA/builder/tools/NativeSubtitleExePatcher/BmGameGfxPatcher/CompressedPackageWriter.cs`
- Modify: `games/HelenBatmanAA/builder/tools/NativeSubtitleExePatcher/BmGameGfxPatcher/GfxPatchApplier.cs`
- Modify: `games/HelenBatmanAA/builder/tools/NativeSubtitleExePatcher/BmGameGfxPatcher/UnrealPackage.cs`

- [ ] **Step 1: Add a red/green regression for patching the retail gameplay package**

Create `games/HelenBatmanAA/scripts/Test-BatmanRetailGameplayPatchBuild.ps1`:

```powershell
param(
    [Parameter(Mandatory = $true)]
    [string]$RetailBasePackagePath,
    [string]$BatmanRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path,
    [string]$Configuration = 'Debug'
)

$ErrorActionPreference = 'Stop'

function Get-CompressionChunkCount {
    param([string]$PackagePath)

    $stream = [System.IO.File]::OpenRead($PackagePath)
    try {
        $reader = New-Object System.IO.BinaryReader($stream)
        try {
            $reader.ReadUInt32() | Out-Null
            $reader.ReadUInt16() | Out-Null
            $reader.ReadUInt16() | Out-Null
            $reader.ReadInt32() | Out-Null
            $nameLength = $reader.ReadInt32()
            if ($nameLength -gt 0) {
                $reader.ReadBytes($nameLength) | Out-Null
            } elseif ($nameLength -lt 0) {
                $reader.ReadBytes((-1 * $nameLength) * 2) | Out-Null
            }

            $reader.ReadUInt32() | Out-Null
            $reader.ReadBytes(28) | Out-Null
            $reader.ReadBytes(16) | Out-Null
            $generationCount = $reader.ReadInt32()
            $reader.ReadBytes($generationCount * 8) | Out-Null
            $reader.ReadBytes(8) | Out-Null
            $reader.ReadUInt32() | Out-Null
            return $reader.ReadInt32()
        }
        finally {
            $reader.Dispose()
        }
    }
    finally {
        $stream.Dispose()
    }
}

$BatmanRoot = (Resolve-Path $BatmanRoot).Path
$RetailBasePackagePath = (Resolve-Path $RetailBasePackagePath).Path
$TemporaryRoot = Join-Path ([System.IO.Path]::GetTempPath()) ('BatmanRetailGameplayPatch-' + [System.Guid]::NewGuid().ToString('N'))
$BuilderRoot = Join-Path $TemporaryRoot 'builder'
$GeneratedRoot = Join-Path $BuilderRoot 'generated\pause-runtime-scale'
$PreparedBasePackagePath = Join-Path $BuilderRoot 'extracted\bmgame-retail\BmGame.u'
$PreparedFfdecCliPath = Join-Path $BuilderRoot 'extracted\ffdec\ffdec-cli.exe'
$ToolRoot = Join-Path $BatmanRoot 'builder\tools\NativeSubtitleExePatcher'
$SubtitleSizeModBuilderProjectPath = Join-Path $ToolRoot 'SubtitleSizeModBuilder\SubtitleSizeModBuilder.csproj'
$BmGameGfxPatcherProjectPath = Join-Path $ToolRoot 'BmGameGfxPatcher\BmGameGfxPatcher.csproj'
$PrepareScriptPath = Join-Path $BatmanRoot 'scripts\Prepare-BatmanBuilderWorkspace.ps1'
$ManifestPath = Join-Path $GeneratedRoot 'pause-runtime-scale.manifest.jsonc'
$PatchedPackagePath = Join-Path $TemporaryRoot 'BmGame-retail-patched.u'
$PauseExtractedPath = Join-Path $TemporaryRoot 'Pause-runtime-scale.gfx'
$HudExtractedPath = Join-Path $TemporaryRoot 'HUD-runtime-scale.gfx'
$ExpectedPausePath = Join-Path $GeneratedRoot 'Pause-runtime-scale.gfx'
$ExpectedHudPath = Join-Path $GeneratedRoot 'HUD-runtime-scale.gfx'
$RepoFfdecCliPath = Join-Path $BatmanRoot 'builder\extracted\ffdec\ffdec-cli.exe'

try {
    New-Item -ItemType Directory -Force -Path $TemporaryRoot | Out-Null

    & powershell -ExecutionPolicy Bypass -File $PrepareScriptPath `
        -BatmanRoot $BatmanRoot `
        -BuilderRoot $BuilderRoot `
        -BasePackagePath $RetailBasePackagePath `
        -FfdecCliPath $RepoFfdecCliPath `
        -Configuration $Configuration
    if ($LASTEXITCODE -ne 0) {
        throw "Prepare-BatmanBuilderWorkspace.ps1 failed with exit code $LASTEXITCODE."
    }

    & dotnet build $SubtitleSizeModBuilderProjectPath -c $Configuration
    if ($LASTEXITCODE -ne 0) {
        throw 'dotnet build failed for SubtitleSizeModBuilder.csproj'
    }

    & dotnet build $BmGameGfxPatcherProjectPath -c $Configuration
    if ($LASTEXITCODE -ne 0) {
        throw 'dotnet build failed for BmGameGfxPatcher.csproj'
    }

    & dotnet run --project $SubtitleSizeModBuilderProjectPath -c $Configuration -- `
        build-pause-runtime-scale `
        --root $BuilderRoot `
        --output-dir $GeneratedRoot `
        --ffdec $PreparedFfdecCliPath
    if ($LASTEXITCODE -ne 0) {
        throw 'build-pause-runtime-scale failed.'
    }

    & dotnet run --project $BmGameGfxPatcherProjectPath -c $Configuration -- `
        patch `
        --package $PreparedBasePackagePath `
        --manifest $ManifestPath `
        --output $PatchedPackagePath
    if ($LASTEXITCODE -ne 0) {
        throw 'Retail gameplay patch build failed.'
    }

    if ((Get-CompressionChunkCount -PackagePath $PatchedPackagePath) -le 0) {
        throw 'Patched retail gameplay package is no longer chunk-compressed.'
    }

    & dotnet run --project $BmGameGfxPatcherProjectPath -c $Configuration -- `
        extract-gfx `
        --package $PatchedPackagePath `
        --owner PauseMenu `
        --name Pause `
        --output $PauseExtractedPath
    if ($LASTEXITCODE -ne 0) {
        throw 'extract-gfx failed for patched PauseMenu.Pause'
    }

    & dotnet run --project $BmGameGfxPatcherProjectPath -c $Configuration -- `
        extract-gfx `
        --package $PatchedPackagePath `
        --owner GameHUD `
        --name HUD `
        --output $HudExtractedPath
    if ($LASTEXITCODE -ne 0) {
        throw 'extract-gfx failed for patched GameHUD.HUD'
    }

    $ExpectedPauseSha256 = (Get-FileHash -LiteralPath $ExpectedPausePath -Algorithm SHA256).Hash
    $ActualPauseSha256 = (Get-FileHash -LiteralPath $PauseExtractedPath -Algorithm SHA256).Hash
    if ($ActualPauseSha256 -ne $ExpectedPauseSha256) {
        throw "Patched Pause GFX hash mismatch. Expected $ExpectedPauseSha256 but found $ActualPauseSha256."
    }

    $ExpectedHudSha256 = (Get-FileHash -LiteralPath $ExpectedHudPath -Algorithm SHA256).Hash
    $ActualHudSha256 = (Get-FileHash -LiteralPath $HudExtractedPath -Algorithm SHA256).Hash
    if ($ActualHudSha256 -ne $ExpectedHudSha256) {
        throw "Patched HUD GFX hash mismatch. Expected $ExpectedHudSha256 but found $ActualHudSha256."
    }

    Write-Output 'PASS'
}
finally {
    if (Test-Path -LiteralPath $TemporaryRoot) {
        Remove-Item -LiteralPath $TemporaryRoot -Recurse -Force
    }
}
```

- [ ] **Step 2: Run the patch-build regression to prove the current patch writer cannot round-trip the retail package**

Run:

```powershell
powershell -ExecutionPolicy Bypass -File .\games\HelenBatmanAA\scripts\Test-BatmanRetailGameplayPatchBuild.ps1 `
  -RetailBasePackagePath 'D:\steam\steamapps\common\Batman Arkham Asylum GOTY\BmGame\CookedPC\BmGame.u.original-backup' `
  -BatmanRoot .\games\HelenBatmanAA `
  -Configuration Debug
```

Expected:

- FAIL because `GfxPatchApplier` still appends raw bytes to the physical file instead of rebuilding a valid retail compressed package

- [ ] **Step 3: Implement the compressed-package write path**

Create `games/HelenBatmanAA/builder/tools/NativeSubtitleExePatcher/BmGameGfxPatcher/CompressedPackageWriter.cs`:

```csharp
using System.IO.Compression;

namespace BmGameGfxPatcher;

/// <summary>
/// Rebuilds a retail chunk-compressed package file from patched logical package bytes.
/// </summary>
internal static class CompressedPackageWriter
{
    /// <summary>
    /// Writes one patched logical package image back into a retail-style physical package file.
    /// </summary>
    /// <param name="package">Original package metadata and physical header bytes.</param>
    /// <param name="logicalBytes">Patched logical package bytes.</param>
    /// <param name="outputPath">Destination physical package path.</param>
    public static void Write(UnrealPackage package, byte[] logicalBytes, string outputPath)
    {
        string fullOutputPath = Path.GetFullPath(outputPath);
        string? outputDirectory = Path.GetDirectoryName(fullOutputPath);
        if (!string.IsNullOrWhiteSpace(outputDirectory))
        {
            Directory.CreateDirectory(outputDirectory);
        }

        using var stream = new FileStream(fullOutputPath, FileMode.Create, FileAccess.ReadWrite, FileShare.None);
        stream.Write(package.PhysicalBytes, 0, package.Header.HeaderSize);

        var rebuiltChunks = new List<CompressionChunkRecord>(package.LogicalImage.CompressionChunks.Count);
        foreach (CompressionChunkRecord chunk in package.LogicalImage.CompressionChunks)
        {
            byte[] logicalChunkBytes = logicalBytes.AsSpan(chunk.UncompressedOffset, chunk.UncompressedSize).ToArray();
            byte[] compressedBytes = CompressChunk(logicalChunkBytes);
            int compressedOffset = checked((int)stream.Position);
            stream.Write(compressedBytes, 0, compressedBytes.Length);

            rebuiltChunks.Add(
                new CompressionChunkRecord(
                    compressedOffset,
                    compressedBytes.Length,
                    chunk.UncompressedOffset,
                    chunk.UncompressedSize));
        }

        WriteInt32(stream, 8, logicalBytes.Length);
        RewriteChunkTable(stream, package.Header.CompressionChunkTableOffset, rebuiltChunks);
    }

    /// <summary>
    /// Compresses one logical chunk using the zlib framing expected by the retail package.
    /// </summary>
    /// <param name="logicalChunkBytes">Exact logical chunk bytes to compress.</param>
    /// <returns>Compressed physical bytes.</returns>
    private static byte[] CompressChunk(byte[] logicalChunkBytes)
    {
        using var output = new MemoryStream();
        using (var zlib = new ZLibStream(output, CompressionLevel.SmallestSize, leaveOpen: true))
        {
            zlib.Write(logicalChunkBytes, 0, logicalChunkBytes.Length);
        }

        return output.ToArray();
    }

    /// <summary>
    /// Rewrites the retail chunk table after the compressed payloads have been rebuilt.
    /// </summary>
    /// <param name="stream">Output package stream.</param>
    /// <param name="chunkTableOffset">Physical file offset of the chunk table.</param>
    /// <param name="chunks">Rebuilt chunk table records.</param>
    private static void RewriteChunkTable(FileStream stream, int chunkTableOffset, IReadOnlyList<CompressionChunkRecord> chunks)
    {
        stream.Seek(chunkTableOffset, SeekOrigin.Begin);
        foreach (CompressionChunkRecord chunk in chunks)
        {
            WriteInt32(stream, chunk.CompressedOffset);
            WriteInt32(stream, chunk.CompressedSize);
            WriteInt32(stream, chunk.UncompressedOffset);
            WriteInt32(stream, chunk.UncompressedSize);
        }
    }

    /// <summary>
    /// Writes one 32-bit integer to the current stream position.
    /// </summary>
    /// <param name="stream">Destination stream.</param>
    /// <param name="value">Value to write.</param>
    private static void WriteInt32(FileStream stream, int value)
    {
        Span<byte> buffer = stackalloc byte[sizeof(int)];
        BitConverter.TryWriteBytes(buffer, value);
        stream.Write(buffer);
    }

    /// <summary>
    /// Writes one 32-bit integer to a specific stream offset.
    /// </summary>
    /// <param name="stream">Destination stream.</param>
    /// <param name="offset">Destination file offset.</param>
    /// <param name="value">Value to write.</param>
    private static void WriteInt32(FileStream stream, int offset, int value)
    {
        stream.Seek(offset, SeekOrigin.Begin);
        WriteInt32(stream, value);
    }
}
```

Update `games/HelenBatmanAA/builder/tools/NativeSubtitleExePatcher/BmGameGfxPatcher/GfxPatchApplier.cs` so `ApplyManifest` branches on storage mode and patches logical bytes for compressed packages:

```csharp
public static PatchApplicationResult ApplyManifest(
    string packagePath,
    string outputPath,
    PatchManifest manifest,
    bool verifyOutput)
{
    UnrealPackage package = UnrealPackage.Load(packagePath);
    return package.LogicalImage.UsesCompressedStorage
        ? ApplyCompressedManifest(package, outputPath, manifest, verifyOutput)
        : ApplyUnpackedManifest(package, outputPath, manifest, verifyOutput);
}

private static PatchApplicationResult ApplyCompressedManifest(
    UnrealPackage package,
    string outputPath,
    PatchManifest manifest,
    bool verifyOutput)
{
    using var logicalStream = new MemoryStream(package.LogicalImage.Bytes.Length + (manifest.Patches.Count * 1024 * 1024));
    logicalStream.Write(package.LogicalImage.Bytes, 0, package.LogicalImage.Bytes.Length);
    var appliedPatches = new List<AppliedPatch>(manifest.Patches.Count);

    foreach (GfxPatchDefinition definition in manifest.Patches)
    {
        ExportEntry exportEntry = package.FindExport(definition.Owner, definition.ExportName, definition.ExportType);
        ReadOnlyMemory<byte> originalObject = package.ReadObjectBytes(exportEntry);
        byte[] replacementBytes = definition.ResolvedReplacementPath is null ? [] : File.ReadAllBytes(definition.ResolvedReplacementPath);
        byte[] patchedObject = BuildPatchedObject(definition, originalObject.Span, replacementBytes);

        int patchedOffset = checked((int)logicalStream.Length);
        logicalStream.Seek(0, SeekOrigin.End);
        logicalStream.Write(patchedObject, 0, patchedObject.Length);

        byte[] logicalBytes = logicalStream.GetBuffer();
        WriteInt32(logicalBytes, exportEntry.SerialDataSizeFieldOffset, patchedObject.Length);
        WriteInt32(logicalBytes, exportEntry.SerialDataOffsetFieldOffset, patchedOffset);

        appliedPatches.Add(
            new AppliedPatch(
                definition,
                exportEntry,
                patchedOffset,
                patchedObject.Length,
                Hashing.Sha256Hex(patchedObject)));
    }

    byte[] patchedLogicalBytes = logicalStream.ToArray();
    WriteInt32(patchedLogicalBytes, 8, patchedLogicalBytes.Length);
    CompressedPackageWriter.Write(package, patchedLogicalBytes, outputPath);

    if (verifyOutput)
    {
        VerifyOutput(outputPath, appliedPatches);
    }

    return new PatchApplicationResult(appliedPatches, verifyOutput);
}
```

Keep the old physical append code in a renamed `ApplyUnpackedManifest` helper for the unpacked fast path.

- [ ] **Step 4: Re-run the retail patch-build regression**

Run:

```powershell
powershell -ExecutionPolicy Bypass -File .\games\HelenBatmanAA\scripts\Test-BatmanRetailGameplayPatchBuild.ps1 `
  -RetailBasePackagePath 'D:\steam\steamapps\common\Batman Arkham Asylum GOTY\BmGame\CookedPC\BmGame.u.original-backup' `
  -BatmanRoot .\games\HelenBatmanAA `
  -Configuration Debug
```

Expected:

- `PASS`

- [ ] **Step 5: Commit the compressed patch writer**

Run:

```powershell
git add games/HelenBatmanAA/scripts/Test-BatmanRetailGameplayPatchBuild.ps1 `
  games/HelenBatmanAA/builder/tools/NativeSubtitleExePatcher/BmGameGfxPatcher/CompressedPackageWriter.cs `
  games/HelenBatmanAA/builder/tools/NativeSubtitleExePatcher/BmGameGfxPatcher/GfxPatchApplier.cs `
  games/HelenBatmanAA/builder/tools/NativeSubtitleExePatcher/BmGameGfxPatcher/UnrealPackage.cs
git commit -m "Patch retail Batman gameplay packages"
```

---

## Task 4: Rebuild And Ship A Gameplay-Only Retail Pack

**Files:**
- Modify: `games/HelenBatmanAA/scripts/Rebuild-BatmanPack.ps1`
- Modify: `games/HelenBatmanAA/scripts/Test-BatmanKnownGoodGameplayPackage.ps1`
- Modify: `games/HelenBatmanAA/helengamehook/packs/batman-aa-subtitles/builds/steam-goty-1.0/files.json`
- Delete: `games/HelenBatmanAA/helengamehook/packs/batman-aa-subtitles/builds/steam-goty-1.0/assets/deltas/Frontend-main-menu-subtitle-size.hgdelta`

- [ ] **Step 1: Tighten the shipped-pack verifier around the new gameplay-only retail contract**

Replace the expected-file block in `games/HelenBatmanAA/scripts/Test-BatmanKnownGoodGameplayPackage.ps1` with:

```powershell
$PackBuildRoot = Join-Path $BatmanRoot 'helengamehook\packs\batman-aa-subtitles\builds\steam-goty-1.0'
$FilesJsonPath = Join-Path $PackBuildRoot 'files.json'
$GameplayDeltaPath = Join-Path $PackBuildRoot 'assets\deltas\BmGame-subtitle-signal.hgdelta'
$UnexpectedFrontendDeltaPath = Join-Path $PackBuildRoot 'assets\deltas\Frontend-main-menu-subtitle-size.hgdelta'
$TrustedGameplayBasePath = Join-Path $BatmanRoot 'builder\extracted\bmgame-retail\BmGame.u'
$GeneratedGameplayPackagePath = Join-Path $BatmanRoot 'builder\generated\pause-runtime-scale\BmGame-subtitle-signal.u'
$ExpectedRetailBaseSize = 59857525
$ExpectedRetailBaseSha256 = '4306148e7627ec2c0de4144fd6ab45521b3b7e090d1028a0b685cadafafb89e6'
$ExpectedVirtualFiles = @(
    @{
        Id = 'bmgameGameplayPackage'
        Path = 'BmGame/CookedPC/BmGame.u'
        Mode = 'delta-on-read'
        Kind = 'delta-file'
        DeltaPath = 'assets/deltas/BmGame-subtitle-signal.hgdelta'
        BasePath = $TrustedGameplayBasePath
        TargetPath = $GeneratedGameplayPackagePath
        DeltaFilePath = $GameplayDeltaPath
        ChunkSize = 65536
        ChunkTableOffset = 116
    }
)
```

Add these gameplay-only assertions before the virtual-file loop:

```powershell
if (Test-Path -LiteralPath $UnexpectedFrontendDeltaPath) {
    throw "Unexpected shipped frontend delta still exists: $UnexpectedFrontendDeltaPath"
}

$ActualRetailBaseSize = (Get-Item -LiteralPath $TrustedGameplayBasePath).Length
if ($ActualRetailBaseSize -ne $ExpectedRetailBaseSize) {
    throw "Trusted retail gameplay base size mismatch. Expected $ExpectedRetailBaseSize but found $ActualRetailBaseSize."
}

$ActualRetailBaseSha256 = (Get-FileHash -LiteralPath $TrustedGameplayBasePath -Algorithm SHA256).Hash.ToLowerInvariant()
if ($ActualRetailBaseSha256 -ne $ExpectedRetailBaseSha256) {
    throw "Trusted retail gameplay base hash mismatch. Expected $ExpectedRetailBaseSha256 but found $ActualRetailBaseSha256."
}
```

- [ ] **Step 2: Run the verifier and prove the current shipped pack still has the wrong shape**

Run:

```powershell
powershell -ExecutionPolicy Bypass -File .\games\HelenBatmanAA\scripts\Test-BatmanKnownGoodGameplayPackage.ps1 -BatmanRoot .\games\HelenBatmanAA
```

Expected:

- FAIL because the current `files.json` still contains a `frontendMapPackage`
- or FAIL because the gameplay base fingerprint is still the unpacked `100365345` byte base instead of the retail `59857525` byte base

- [ ] **Step 3: Rebuild only the gameplay package and write a gameplay-only manifest**

In `games/HelenBatmanAA/scripts/Rebuild-BatmanPack.ps1`, replace the source path block with:

```powershell
$BuildAssetsRoot = Join-Path $GeneratedRoot 'pause-runtime-scale'
$FfdecPath = Join-Path $ExtractedRoot 'ffdec\ffdec-cli.exe'
$BasePackagePath = Join-Path $ExtractedRoot 'bmgame-retail\BmGame.u'
$FilesJsonPath = Join-Path $PackBuildRoot 'files.json'
$ManifestPath = Join-Path $BuildAssetsRoot 'pause-runtime-scale.manifest.jsonc'
$GeneratedGameplayPackagePath = Join-Path $BuildAssetsRoot 'BmGame-subtitle-signal.u'
$GameplayDeltaPath = Join-Path $PackBuildRoot 'assets\deltas\BmGame-subtitle-signal.hgdelta'
$GlobalBlobPath = Join-Path $PackBuildRoot 'assets\native\batman-global-text-scale.bin'
```

Replace the prerequisite list with:

```powershell
$builderWorkspacePrerequisites = @(
    $FfdecPath,
    $BasePackagePath,
    (Join-Path $ExtractedRoot 'pause\Pause-extracted.gfx'),
    (Join-Path $ExtractedRoot 'pause\Pause.xml'),
    (Join-Path $ExtractedRoot 'pause\pause-ffdec-export\scripts'),
    (Join-Path $ExtractedRoot 'hud\HUD-extracted.gfx'),
    (Join-Path $ExtractedRoot 'hud\HUD.xml'),
    (Join-Path $ExtractedRoot 'hud\hud-ffdec-scripts\scripts')
)
```

Delete the entire frontend build block and replace the manifest write with:

```powershell
$filesManifest = @{
    virtualFiles = @(
        @{
            id = 'bmgameGameplayPackage'
            path = 'BmGame/CookedPC/BmGame.u'
            mode = 'delta-on-read'
            source = @{
                kind = 'delta-file'
                path = 'assets/deltas/BmGame-subtitle-signal.hgdelta'
                base = @{
                    size = $deltaInfo.BaseSize
                    sha256 = $deltaInfo.BaseSha256
                }
                target = @{
                    size = $deltaInfo.TargetSize
                    sha256 = $deltaInfo.TargetSha256
                }
                chunkSize = $deltaInfo.ChunkSize
            }
        }
    )
}

$filesManifest | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $FilesJsonPath
```

Replace the final summary with:

```powershell
Write-Output 'Rebuilt Batman pack outputs:'
Write-Output "  Gameplay delta:  $GameplayDeltaPath"
Write-Output "  Gameplay target: $GeneratedGameplayPackagePath"
Write-Output "  Native blob:     $GlobalBlobPath"
```

- [ ] **Step 4: Rebuild the shipped pack and verify the gameplay-only manifest**

Run:

```powershell
powershell -ExecutionPolicy Bypass -File .\games\HelenBatmanAA\scripts\Rebuild-BatmanPack.ps1 -Configuration Debug
powershell -ExecutionPolicy Bypass -File .\games\HelenBatmanAA\scripts\Test-BatmanKnownGoodGameplayPackage.ps1 -BatmanRoot .\games\HelenBatmanAA
```

Expected:

- `Rebuild-BatmanPack.ps1` succeeds and only reports gameplay outputs
- `Test-BatmanKnownGoodGameplayPackage.ps1` prints `PASS`

- [ ] **Step 5: Commit the gameplay-only retail shipped pack**

Run:

```powershell
git add games/HelenBatmanAA/scripts/Rebuild-BatmanPack.ps1 `
  games/HelenBatmanAA/scripts/Test-BatmanKnownGoodGameplayPackage.ps1 `
  games/HelenBatmanAA/helengamehook/packs/batman-aa-subtitles/builds/steam-goty-1.0/files.json `
  games/HelenBatmanAA/helengamehook/packs/batman-aa-subtitles/builds/steam-goty-1.0/assets/deltas/BmGame-subtitle-signal.hgdelta
git rm games/HelenBatmanAA/helengamehook/packs/batman-aa-subtitles/builds/steam-goty-1.0/assets/deltas/Frontend-main-menu-subtitle-size.hgdelta
git commit -m "Ship Batman gameplay delta against retail base"
```

---

## Task 5: Align Deploy, Runtime Tests, And Docs With Gameplay-Only Retail Shipping

**Files:**
- Modify: `games/HelenBatmanAA/scripts/Test-BatmanInstalledBaseCompatibility.ps1`
- Modify: `games/HelenBatmanAA/scripts/Deploy-Batman.ps1`
- Modify: `tests/HelenRuntime.Tests/PackRepositoryTests.cpp`
- Modify: `games/HelenBatmanAA/README.md`
- Modify: `games/HelenBatmanAA/builder/tools/NativeSubtitleExePatcher/BmGameGfxPatcher/README.md`

- [ ] **Step 1: Tighten the installed-base preflight and deployment script around one gameplay file**

In `games/HelenBatmanAA/scripts/Test-BatmanInstalledBaseCompatibility.ps1`, add this manifest-shape check after `$VirtualFiles = @($Manifest.virtualFiles)`:

```powershell
if ($VirtualFiles.Count -ne 1) {
    throw "Batman pack manifest expected exactly one virtual file, found $($VirtualFiles.Count)."
}

if ($VirtualFiles[0].id -ne 'bmgameGameplayPackage') {
    throw "Batman pack manifest expected only bmgameGameplayPackage, found $($VirtualFiles[0].id)."
}

if ($VirtualFiles[0].path -ne 'BmGame/CookedPC/BmGame.u') {
    throw "Batman pack manifest expected BmGame/CookedPC/BmGame.u but found $($VirtualFiles[0].path)."
}
```

In `games/HelenBatmanAA/scripts/Deploy-Batman.ps1`, replace the source-delta and expected-file block with:

```powershell
$SourceGameplayDeltaPath = Join-Path $PackSource 'builds\steam-goty-1.0\assets\deltas\BmGame-subtitle-signal.hgdelta'
$VerifierPath = Join-Path $PSScriptRoot 'Test-BatmanKnownGoodGameplayPackage.ps1'
$InstalledBaseVerifierPath = Join-Path $PSScriptRoot 'Test-BatmanInstalledBaseCompatibility.ps1'
$ExpectedVirtualFiles = @(
    @{
        Id = 'bmgameGameplayPackage'
        Path = 'BmGame/CookedPC/BmGame.u'
        Mode = 'delta-on-read'
        Kind = 'delta-file'
        DeltaPath = 'assets/deltas/BmGame-subtitle-signal.hgdelta'
        SourceDeltaPath = $SourceGameplayDeltaPath
        DeltaHash = (Get-FileHash -LiteralPath $SourceGameplayDeltaPath -Algorithm SHA256).Hash
    }
)
```

Delete every `Frontend` path reference from the rest of the file.

- [ ] **Step 2: Update the checked-in runtime pack expectations to the new gameplay-only manifest**

In `tests/HelenRuntime.Tests/PackRepositoryTests.cpp`, replace only the checked-in Batman pack expectations with:

```cpp
Expect(loaded_batman_pack->Build.VirtualFiles.size() == 1, "Checked-in Batman pack virtual file count mismatch.");

const helen::VirtualFileDefinition& checked_in_gameplay_file = loaded_batman_pack->Build.VirtualFiles[0];
Expect(checked_in_gameplay_file.Id == "bmgameGameplayPackage", "Checked-in Batman gameplay virtual file identifier mismatch.");
Expect(checked_in_gameplay_file.Mode == "delta-on-read", "Checked-in Batman gameplay package is not delta-backed.");
Expect(checked_in_gameplay_file.GamePath == std::filesystem::path("BmGame/CookedPC/BmGame.u"), "Checked-in Batman gameplay package path mismatch.");
Expect(checked_in_gameplay_file.Source.Kind == helen::VirtualFileSourceKind::DeltaFile, "Checked-in Batman gameplay package source kind mismatch.");
Expect(checked_in_gameplay_file.Source.Path == std::filesystem::path("assets/deltas/BmGame-subtitle-signal.hgdelta"), "Checked-in Batman gameplay package delta path mismatch.");
Expect(checked_in_gameplay_file.Source.Base.FileSize == 59857525, "Checked-in Batman gameplay package base size mismatch.");
Expect(checked_in_gameplay_file.Source.Base.Sha256 == "4306148e7627ec2c0de4144fd6ab45521b3b7e090d1028a0b685cadafafb89e6", "Checked-in Batman gameplay package base hash mismatch.");
Expect(checked_in_gameplay_file.Source.ChunkSize == 65536, "Checked-in Batman gameplay package chunk size mismatch.");
```

Do not change the synthetic multi-file repository test coverage at the top of the file.

- [ ] **Step 3: Update the Batman docs and patcher docs to the new truthful workflow**

In `games/HelenBatmanAA/README.md`, replace the warning section with:

```markdown
Current state:

- The shipped Batman pack is gameplay-only.
- The shipped `BmGame.u` delta is built against the retail compressed `BmGame.u` base.
- `Frontend.umap` remains vanilla and is not part of the shipped pack.
```

Replace the prepare command with:

```powershell
powershell -ExecutionPolicy Bypass -File .\games\HelenBatmanAA\scripts\Prepare-BatmanBuilderWorkspace.ps1 `
  -BasePackagePath D:\trusted\BmGame.u `
  -FfdecCliPath C:\tools\ffdec\ffdec-cli.exe
```

Replace the live verification checklist with:

```text
After deployment, verify:
1. Pause `Options -> Audio` still shows `Subtitle Size`.
2. The title screen, press-start flow, and save/profile selection still behave like vanilla.
3. Main-menu `Options -> Audio` remains vanilla and does not show `Subtitle Size`.
4. `HelenGameHook.log` registers and serves only `BmGame.u`.
5. No frontend virtual file is registered or served.
```

In `games/HelenBatmanAA/builder/tools/NativeSubtitleExePatcher/BmGameGfxPatcher/README.md`, replace the scope section with:

```markdown
This tool currently supports:

- Arkham-style `.u` packages whose storage is either unpacked or retail chunk-compressed
- replacing embedded `GFX` payloads inside `GFxMovieInfo` exports
- replacing whole export objects from raw extracted bytes
- patching multiple exports into one output package
- export validation by length and SHA-256
- rebuilding retail chunk-compressed package output

This tool does not currently support:

- encrypted packages
- frontend `Frontend.umap` shipping in the Batman pack
- bytecode editing or native Unreal function patching
```

- [ ] **Step 4: Run the full automated suite and do the live gameplay-only smoke flow**

Run:

```powershell
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' tests\HelenRuntime.Tests\HelenRuntime.Tests.vcxproj /p:Configuration=Debug /p:Platform=Win32 /m
C:\dev\helenhook\bin\Win32\Debug\tests\HelenRuntimeTests.exe
powershell -ExecutionPolicy Bypass -File .\games\HelenBatmanAA\scripts\Test-BatmanBuilderWorkspacePreparation.ps1 -BatmanRoot .\games\HelenBatmanAA -Configuration Debug
powershell -ExecutionPolicy Bypass -File .\games\HelenBatmanAA\scripts\Test-BatmanRetailGameplayPackageReader.ps1 -RetailBasePackagePath 'D:\steam\steamapps\common\Batman Arkham Asylum GOTY\BmGame\CookedPC\BmGame.u.original-backup' -BatmanRoot .\games\HelenBatmanAA -Configuration Debug
powershell -ExecutionPolicy Bypass -File .\games\HelenBatmanAA\scripts\Test-BatmanRetailGameplayPatchBuild.ps1 -RetailBasePackagePath 'D:\steam\steamapps\common\Batman Arkham Asylum GOTY\BmGame\CookedPC\BmGame.u.original-backup' -BatmanRoot .\games\HelenBatmanAA -Configuration Debug
powershell -ExecutionPolicy Bypass -File .\games\HelenBatmanAA\scripts\Test-BatmanKnownGoodGameplayPackage.ps1 -BatmanRoot .\games\HelenBatmanAA
powershell -ExecutionPolicy Bypass -File .\games\HelenBatmanAA\scripts\Deploy-Batman.ps1 -Configuration Debug
powershell -ExecutionPolicy Bypass -File .\games\HelenBatmanAA\scripts\Launch-Check-Batman.ps1
```

Expected:

- MSBuild succeeds
- `HelenRuntimeTests.exe` prints `PASS`
- each Batman PowerShell verifier prints `PASS`
- deploy prints `DEPLOYED`
- launch check prints `PROCESS_STARTED`

Then perform the live smoke flow manually:

1. Press Start from the title screen.
2. Reach the save/profile selection flow without freezes or broken prompts.
3. Open the main-menu `Options -> Audio` screen and confirm it remains vanilla.
4. Start or load gameplay, open pause `Options -> Audio`, and confirm `Subtitle Size` is still present.
5. Open `D:\steam\steamapps\common\Batman Arkham Asylum GOTY\Binaries\helengamehook\logs\HelenGameHook.log` and confirm it serves only `BmGame.u`.

- [ ] **Step 5: Commit the gameplay-only deploy/runtime alignment**

Run:

```powershell
git add games/HelenBatmanAA/scripts/Test-BatmanInstalledBaseCompatibility.ps1 `
  games/HelenBatmanAA/scripts/Deploy-Batman.ps1 `
  tests/HelenRuntime.Tests/PackRepositoryTests.cpp `
  games/HelenBatmanAA/README.md `
  games/HelenBatmanAA/builder/tools/NativeSubtitleExePatcher/BmGameGfxPatcher/README.md
git commit -m "Validate retail-safe Batman gameplay deployment"
```
