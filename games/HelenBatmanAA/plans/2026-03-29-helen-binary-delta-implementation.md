# Helen Binary Delta Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a generic `.hgdelta` binary delta format and runtime support so Helen packs can ship either full replacement files or exact-hash delta-backed files without storing giant rebuilt binaries in git.

**Architecture:** Keep the runtime generic by introducing a chunked delta reader, a richer virtual-file source model, and a virtual-file service that dispatches to either in-memory full replacements or on-demand delta-backed readers. For mapped files, materialize one verified resolved file into `helengamehook\cache\resolved` and reuse it until the base file or delta identity changes.

**Tech Stack:** C++20, Win32 file APIs, current Helen runtime/test harness, PowerShell Batman scripts, git on `master`

---

## File Map

### New runtime files

- `include/HelenHook/FileFingerprint.h`
  - Generic exact-file identity for non-executable files.
- `HelenRuntime/FileFingerprint.cpp`
  - SHA-256 and size hashing for arbitrary files.
- `include/HelenHook/HgdeltaChunkKind.h`
  - Enum for `base-copy` and `delta-bytes`.
- `include/HelenHook/HgdeltaChunkDefinition.h`
  - Parsed chunk-table entry model.
- `include/HelenHook/HgdeltaFile.h`
  - Parsed `.hgdelta` container model and loader.
- `HelenRuntime/HgdeltaFile.cpp`
  - Binary parser and validation for `.hgdelta`.
- `include/HelenHook/VirtualFileHashDefinition.h`
  - Exact size and SHA-256 metadata for base/target files.
- `include/HelenHook/VirtualFileSourceKind.h`
  - Explicit source kind enum for `full-file` and `delta-file`.
- `include/HelenHook/VirtualFileSourceDefinition.h`
  - Source metadata owned by one virtual file definition.
- `include/HelenHook/VirtualFileSource.h`
  - Common interface for full-file and delta-file sources.
- `include/HelenHook/FullFileVirtualFileSource.h`
  - Existing full replacement behavior behind the new source abstraction.
- `HelenRuntime/FullFileVirtualFileSource.cpp`
  - Loads full replacement bytes and exposes read/mapping operations.
- `include/HelenHook/DeltaVirtualFileSource.h`
  - Delta-backed read and mapping source.
- `HelenRuntime/DeltaVirtualFileSource.cpp`
  - Base file validation, chunked reads, and resolved-file creation.
- `include/HelenHook/ResolvedFileCache.h`
  - Cache-keyed resolved target file manager.
- `HelenRuntime/ResolvedFileCache.cpp`
  - Materializes and reuses reconstructed target files.

### Modified runtime files

- `include/HelenHook/VirtualFileDefinition.h`
  - Replace plain `Source` path with structured source metadata.
- `include/HelenHook/VirtualFileHandle.h`
  - Track an opened `VirtualFileSource` instance instead of raw replacement bytes.
- `include/HelenHook/VirtualFileService.h`
  - Register and serve polymorphic file sources.
- `HelenRuntime/VirtualFileService.cpp`
  - Dispatch reads and mappings through the source abstraction.
- `include/HelenHook/PackAssetResolver.h`
  - Add exact game-file resolver support for base files.
- `HelenRuntime/PackAssetResolver.cpp`
  - Resolve installed game file paths safely relative to the host executable.
- `include/HelenHook/ExecutableFingerprint.h`
  - Reuse or delegate hashing logic through `FileFingerprint`.
- `HelenRuntime/ExecutableFingerprint.cpp`
  - Remove duplicated hashing code by delegating to `FileFingerprint`.
- `HelenRuntime/PackRepository.cpp`
  - Parse `source.kind`, `base`, `target`, and `chunkSize`.
- `HelenRuntime/HelenRuntime.vcxproj`
  - Compile the new runtime sources.

### New tests

- `tests/HelenRuntime.Tests/FileFingerprintTests.cpp`
- `tests/HelenRuntime.Tests/HgdeltaFileTests.cpp`
- `tests/HelenRuntime.Tests/DeltaVirtualFileSourceTests.cpp`
- `tests/HelenRuntime.Tests/ResolvedFileCacheTests.cpp`

### Modified tests

- `tests/HelenRuntime.Tests/PackRepositoryTests.cpp`
- `tests/HelenRuntime.Tests/VirtualFileServiceTests.cpp`
- `tests/HelenRuntime.Tests/TestMain.cpp`
- `tests/HelenRuntime.Tests/HelenRuntime.Tests.vcxproj`

### Batman-specific files

- `games/HelenBatmanAA/helengamehook/packs/batman-aa-subtitles/builds/steam-goty-1.0/files.json`
  - Switch `BmGame.u` from full file to delta declaration.
- `games/HelenBatmanAA/scripts/Rebuild-BatmanPack.ps1`
  - Generate `.hgdelta` after building the patched target file.
- `games/HelenBatmanAA/scripts/Build-Hgdelta.ps1`
  - PowerShell entry point that creates a `.hgdelta` from one base file and one target file.

---

### Task 1: Add generic file fingerprinting and `.hgdelta` parsing

**Files:**
- Create: `include/HelenHook/FileFingerprint.h`
- Create: `HelenRuntime/FileFingerprint.cpp`
- Create: `include/HelenHook/HgdeltaChunkKind.h`
- Create: `include/HelenHook/HgdeltaChunkDefinition.h`
- Create: `include/HelenHook/HgdeltaFile.h`
- Create: `HelenRuntime/HgdeltaFile.cpp`
- Create: `tests/HelenRuntime.Tests/FileFingerprintTests.cpp`
- Create: `tests/HelenRuntime.Tests/HgdeltaFileTests.cpp`
- Modify: `include/HelenHook/ExecutableFingerprint.h`
- Modify: `HelenRuntime/ExecutableFingerprint.cpp`
- Modify: `tests/HelenRuntime.Tests/TestMain.cpp`
- Modify: `tests/HelenRuntime.Tests/HelenRuntime.Tests.vcxproj`
- Modify: `HelenRuntime/HelenRuntime.vcxproj`

- [ ] **Step 1: Write the failing tests**

```cpp
void RunFileFingerprintTests()
{
    const std::filesystem::path root = std::filesystem::temp_directory_path() / "HelenRuntimeTests" / "FileFingerprint";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);

    const std::filesystem::path file_path = root / "sample.bin";
    WriteAllBytes(file_path, "ABCDE");

    const helen::FileFingerprint fingerprint = helen::FileFingerprint::FromPath(file_path);
    Expect(fingerprint.FileSize == 5, "File fingerprint size mismatch.");
    Expect(fingerprint.Sha256 == "f0393febe8baaa55e32f7be2a7cc180bf34e52137d99e056c817a9c07b8f239a", "File fingerprint SHA-256 mismatch.");

    std::filesystem::remove_all(root);
}

void RunHgdeltaFileTests()
{
    const std::filesystem::path root = std::filesystem::temp_directory_path() / "HelenRuntimeTests" / "HgdeltaFile";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);

    const std::filesystem::path delta_path = root / "sample.hgdelta";
    WriteAllBytes(delta_path, BuildSampleHgdeltaBytes());

    const helen::HgdeltaFile delta = helen::HgdeltaFile::Load(delta_path);
    Expect(delta.ChunkSize == 4, "Chunk size mismatch.");
    Expect(delta.TargetFileSize == 8, "Target file size mismatch.");
    Expect(delta.Chunks.size() == 2, "Chunk count mismatch.");
    Expect(delta.Chunks[0].Kind == helen::HgdeltaChunkKind::BaseCopy, "First chunk kind mismatch.");
    Expect(delta.Chunks[1].Kind == helen::HgdeltaChunkKind::DeltaBytes, "Second chunk kind mismatch.");

    WriteAllBytes(delta_path, "BAD!");
    bool threw = false;
    try
    {
        static_cast<void>(helen::HgdeltaFile::Load(delta_path));
    }
    catch (const std::runtime_error&)
    {
        threw = true;
    }

    Expect(threw, "Malformed hgdelta header unexpectedly loaded.");
    std::filesystem::remove_all(root);
}
```

- [ ] **Step 2: Run the tests to verify they fail**

Run:

```powershell
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' tests\HelenRuntime.Tests\HelenRuntime.Tests.vcxproj /p:Configuration=Debug /p:Platform=Win32 /m
C:\dev\helenhook\bin\Win32\Debug\tests\HelenRuntimeTests.exe
```

Expected:

- build fails because the new test files and runtime types do not exist yet, or
- test binary runs and reports missing symbols for `FileFingerprint` / `HgdeltaFile`

- [ ] **Step 3: Implement the minimal runtime types**

```cpp
class FileFingerprint
{
public:
    std::uintmax_t FileSize = 0;
    std::string Sha256;

    static FileFingerprint FromPath(const std::filesystem::path& file_path);
};

enum class HgdeltaChunkKind
{
    BaseCopy = 0,
    DeltaBytes = 1
};

class HgdeltaChunkDefinition
{
public:
    HgdeltaChunkKind Kind = HgdeltaChunkKind::BaseCopy;
    std::uint64_t TargetOffset = 0;
    std::uint32_t TargetSize = 0;
    std::uint64_t PayloadOffset = 0;
    std::uint32_t PayloadSize = 0;
};

class HgdeltaFile
{
public:
    std::uint32_t ChunkSize = 0;
    std::uint64_t BaseFileSize = 0;
    std::uint64_t TargetFileSize = 0;
    std::string BaseSha256;
    std::string TargetSha256;
    std::vector<HgdeltaChunkDefinition> Chunks;
    std::vector<std::uint8_t> PayloadBytes;

    static HgdeltaFile Load(const std::filesystem::path& file_path);
};
```

- [ ] **Step 4: Run the focused tests and then the full suite**

Run:

```powershell
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' tests\HelenRuntime.Tests\HelenRuntime.Tests.vcxproj /p:Configuration=Debug /p:Platform=Win32 /m
C:\dev\helenhook\bin\Win32\Debug\tests\HelenRuntimeTests.exe
```

Expected:

- `PASS`

- [ ] **Step 5: Commit**

```powershell
git add include/HelenHook/FileFingerprint.h HelenRuntime/FileFingerprint.cpp include/HelenHook/HgdeltaChunkKind.h include/HelenHook/HgdeltaChunkDefinition.h include/HelenHook/HgdeltaFile.h HelenRuntime/HgdeltaFile.cpp include/HelenHook/ExecutableFingerprint.h HelenRuntime/ExecutableFingerprint.cpp tests/HelenRuntime.Tests/FileFingerprintTests.cpp tests/HelenRuntime.Tests/HgdeltaFileTests.cpp tests/HelenRuntime.Tests/TestMain.cpp tests/HelenRuntime.Tests/HelenRuntime.Tests.vcxproj HelenRuntime/HelenRuntime.vcxproj
git commit -m "Add generic file fingerprinting and hgdelta parsing"
```

### Task 2: Extend virtual-file metadata and pack parsing for delta-backed sources

**Files:**
- Create: `include/HelenHook/VirtualFileHashDefinition.h`
- Create: `include/HelenHook/VirtualFileSourceKind.h`
- Create: `include/HelenHook/VirtualFileSourceDefinition.h`
- Modify: `include/HelenHook/VirtualFileDefinition.h`
- Modify: `HelenRuntime/PackRepository.cpp`
- Modify: `tests/HelenRuntime.Tests/PackRepositoryTests.cpp`

- [ ] **Step 1: Write the failing parser test**

```cpp
WriteAllText(
    valid_build_root / "files.json",
    R"({
  "virtualFiles": [
    {
      "id": "bmgameGameplayPackage",
      "path": "BmGame/CookedPC/BmGame.u",
      "mode": "delta-on-read",
      "source": {
        "kind": "delta-file",
        "path": "assets/deltas/BmGame-subtitle-signal.hgdelta",
        "base": {
          "size": 101403981,
          "sha256": "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
        },
        "target": {
          "size": 101405329,
          "sha256": "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
        },
        "chunkSize": 65536
      }
    }
  ]
})");

Expect(loaded_valid_pack->Build.VirtualFiles[0].Mode == "delta-on-read", "Virtual file mode mismatch.");
Expect(loaded_valid_pack->Build.VirtualFiles[0].Source.Kind == helen::VirtualFileSourceKind::DeltaFile, "Virtual file source kind mismatch.");
Expect(loaded_valid_pack->Build.VirtualFiles[0].Source.Base.FileSize == 101403981, "Virtual file base size mismatch.");
Expect(loaded_valid_pack->Build.VirtualFiles[0].Source.Target.FileSize == 101405329, "Virtual file target size mismatch.");
Expect(loaded_valid_pack->Build.VirtualFiles[0].Source.ChunkSize == 65536, "Virtual file chunk size mismatch.");
```

- [ ] **Step 2: Run the tests to verify they fail**

Run:

```powershell
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' tests\HelenRuntime.Tests\HelenRuntime.Tests.vcxproj /p:Configuration=Debug /p:Platform=Win32 /m
C:\dev\helenhook\bin\Win32\Debug\tests\HelenRuntimeTests.exe
```

Expected:

- `PackRepositoryTests` fails because `ParseVirtualFile` only understands a plain source path

- [ ] **Step 3: Implement explicit source metadata**

```cpp
enum class VirtualFileSourceKind
{
    FullFile,
    DeltaFile
};

class VirtualFileHashDefinition
{
public:
    std::uintmax_t FileSize = 0;
    std::string Sha256;
};

class VirtualFileSourceDefinition
{
public:
    VirtualFileSourceKind Kind = VirtualFileSourceKind::FullFile;
    std::filesystem::path Path;
    VirtualFileHashDefinition Base;
    VirtualFileHashDefinition Target;
    std::uint32_t ChunkSize = 0;
};

class VirtualFileDefinition
{
public:
    std::string Id;
    std::filesystem::path GamePath;
    std::string Mode;
    VirtualFileSourceDefinition Source;
};
```

- [ ] **Step 4: Re-run the tests**

Run:

```powershell
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' tests\HelenRuntime.Tests\HelenRuntime.Tests.vcxproj /p:Configuration=Debug /p:Platform=Win32 /m
C:\dev\helenhook\bin\Win32\Debug\tests\HelenRuntimeTests.exe
```

Expected:

- `PASS`

- [ ] **Step 5: Commit**

```powershell
git add include/HelenHook/VirtualFileHashDefinition.h include/HelenHook/VirtualFileSourceKind.h include/HelenHook/VirtualFileSourceDefinition.h include/HelenHook/VirtualFileDefinition.h HelenRuntime/PackRepository.cpp tests/HelenRuntime.Tests/PackRepositoryTests.cpp
git commit -m "Add delta-backed virtual file metadata parsing"
```

### Task 3: Refactor virtual-file serving behind a source abstraction and preserve full-file behavior

**Files:**
- Create: `include/HelenHook/VirtualFileSource.h`
- Create: `include/HelenHook/FullFileVirtualFileSource.h`
- Create: `HelenRuntime/FullFileVirtualFileSource.cpp`
- Modify: `include/HelenHook/VirtualFileHandle.h`
- Modify: `include/HelenHook/VirtualFileService.h`
- Modify: `HelenRuntime/VirtualFileService.cpp`
- Modify: `tests/HelenRuntime.Tests/VirtualFileServiceTests.cpp`
- Modify: `tests/HelenRuntime.Tests/TestMain.cpp`
- Modify: `tests/HelenRuntime.Tests/HelenRuntime.Tests.vcxproj`
- Modify: `HelenRuntime/HelenRuntime.vcxproj`

- [ ] **Step 1: Write the failing full-file behavior regression test**

```cpp
const helen::VirtualFileDefinition definition = CreateFullFileVirtualFileDefinition(
    "BmGame/CookedPC/BmGame.u",
    "assets/packages/BmGame-subtitle-signal.u");

Expect(service.RegisterVirtualFile(definition), "Expected the full-file virtual package to register.");
const std::optional<HANDLE> handle = service.OpenVirtualFile(R"(C:\Games\Batman Arkham Asylum GOTY\BmGame\CookedPC\BmGame.u)");
Expect(handle.has_value(), "Expected the full-file virtual package to open.");

std::array<char, 5> bytes{};
DWORD bytes_read = 0;
Expect(service.Read(*handle, bytes.data(), 5, &bytes_read), "Expected the full-file read to succeed.");
Expect(std::string_view(bytes.data(), 5) == "ABCDE", "Full-file virtual payload mismatch.");
```

- [ ] **Step 2: Run the tests to verify they fail after the handle/source refactor stubs are introduced**

Run:

```powershell
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' tests\HelenRuntime.Tests\HelenRuntime.Tests.vcxproj /p:Configuration=Debug /p:Platform=Win32 /m
C:\dev\helenhook\bin\Win32\Debug\tests\HelenRuntimeTests.exe
```

Expected:

- build or test failure because `VirtualFileHandle` and `VirtualFileService` still assume raw replacement bytes

- [ ] **Step 3: Implement the source abstraction with the existing full-file behavior**

```cpp
class VirtualFileSource
{
public:
    virtual ~VirtualFileSource() = default;
    virtual std::uint64_t GetSize() const = 0;
    virtual bool Read(std::uint64_t offset, void* buffer, std::size_t bytes_to_read, std::size_t& bytes_read) = 0;
    virtual std::optional<HANDLE> CreateFileMapping(DWORD protection, DWORD maximum_size_high, DWORD maximum_size_low) = 0;
};

class FullFileVirtualFileSource final : public VirtualFileSource
{
public:
    explicit FullFileVirtualFileSource(std::vector<std::uint8_t> bytes);

    std::uint64_t GetSize() const override;
    bool Read(std::uint64_t offset, void* buffer, std::size_t bytes_to_read, std::size_t& bytes_read) override;
    std::optional<HANDLE> CreateFileMapping(DWORD protection, DWORD maximum_size_high, DWORD maximum_size_low) override;

private:
    std::shared_ptr<const std::vector<std::uint8_t>> bytes_;
};
```

- [ ] **Step 4: Re-run the tests**

Run:

```powershell
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' tests\HelenRuntime.Tests\HelenRuntime.Tests.vcxproj /p:Configuration=Debug /p:Platform=Win32 /m
C:\dev\helenhook\bin\Win32\Debug\tests\HelenRuntimeTests.exe
```

Expected:

- `PASS`

- [ ] **Step 5: Commit**

```powershell
git add include/HelenHook/VirtualFileSource.h include/HelenHook/FullFileVirtualFileSource.h HelenRuntime/FullFileVirtualFileSource.cpp include/HelenHook/VirtualFileHandle.h include/HelenHook/VirtualFileService.h HelenRuntime/VirtualFileService.cpp tests/HelenRuntime.Tests/VirtualFileServiceTests.cpp tests/HelenRuntime.Tests/TestMain.cpp tests/HelenRuntime.Tests/HelenRuntime.Tests.vcxproj HelenRuntime/HelenRuntime.vcxproj
git commit -m "Refactor virtual file service to use file sources"
```

### Task 4: Add delta-backed streamed reads and exact base-file validation

**Files:**
- Create: `include/HelenHook/DeltaVirtualFileSource.h`
- Create: `HelenRuntime/DeltaVirtualFileSource.cpp`
- Create: `tests/HelenRuntime.Tests/DeltaVirtualFileSourceTests.cpp`
- Modify: `include/HelenHook/PackAssetResolver.h`
- Modify: `HelenRuntime/PackAssetResolver.cpp`
- Modify: `HelenRuntime/VirtualFileService.cpp`
- Modify: `tests/HelenRuntime.Tests/TestMain.cpp`
- Modify: `tests/HelenRuntime.Tests/HelenRuntime.Tests.vcxproj`
- Modify: `HelenRuntime/HelenRuntime.vcxproj`

- [ ] **Step 1: Write the failing streamed-read test**

```cpp
void RunDeltaVirtualFileSourceTests()
{
    const std::filesystem::path root = std::filesystem::temp_directory_path() / "HelenRuntimeTests" / "DeltaVirtualFileSource";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root / "game" / "BmGame" / "CookedPC");
    std::filesystem::create_directories(root / "pack" / "builds" / "steam-goty-1.0" / "assets" / "deltas");

    WriteAllBytes(root / "game" / "BmGame" / "CookedPC" / "BmGame.u", "ABCDEFGH");
    WriteAllBytes(root / "pack" / "builds" / "steam-goty-1.0" / "assets" / "deltas" / "BmGame.hgdelta", BuildSampleHgdeltaBytes());

    const helen::VirtualFileDefinition definition = CreateDeltaVirtualFileDefinition(
        "BmGame/CookedPC/BmGame.u",
        "assets/deltas/BmGame.hgdelta",
        8,
        "9ac2197d9258257b1ae8463e4214e4cd0a578bc1517f2415928b91be4283fc48",
        8,
        "7bdee4c4987c1b91a0c9d619e16441d2914f2f5582b012e219903f5c84a8e18b",
        4);

    const helen::PackAssetResolver resolver(root / "pack", root / "pack" / "builds" / "steam-goty-1.0");
    helen::DeltaVirtualFileSource source(resolver, root / "game", definition);

    std::array<char, 8> bytes{};
    std::size_t bytes_read = 0;
    Expect(source.Read(0, bytes.data(), bytes.size(), bytes_read), "Expected delta source read to succeed.");
    Expect(std::string_view(bytes.data(), 8) == "ABCDWXYZ", "Delta source payload mismatch.");

    std::filesystem::remove_all(root);
}
```

- [ ] **Step 2: Run the tests to verify they fail**

Run:

```powershell
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' tests\HelenRuntime.Tests\HelenRuntime.Tests.vcxproj /p:Configuration=Debug /p:Platform=Win32 /m
C:\dev\helenhook\bin\Win32\Debug\tests\HelenRuntimeTests.exe
```

Expected:

- build fails because `DeltaVirtualFileSource` does not exist yet

- [ ] **Step 3: Implement exact-hash delta reads**

```cpp
class DeltaVirtualFileSource final : public VirtualFileSource
{
public:
    DeltaVirtualFileSource(
        const PackAssetResolver& asset_resolver,
        const std::filesystem::path& game_root,
        const VirtualFileDefinition& definition);

    std::uint64_t GetSize() const override;
    bool Read(std::uint64_t offset, void* buffer, std::size_t bytes_to_read, std::size_t& bytes_read) override;
    std::optional<HANDLE> CreateFileMapping(DWORD protection, DWORD maximum_size_high, DWORD maximum_size_low) override;

private:
    FileFingerprint base_fingerprint_;
    HgdeltaFile delta_;
    std::filesystem::path base_file_path_;
};
```

- [ ] **Step 4: Re-run the tests**

Run:

```powershell
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' tests\HelenRuntime.Tests\HelenRuntime.Tests.vcxproj /p:Configuration=Debug /p:Platform=Win32 /m
C:\dev\helenhook\bin\Win32\Debug\tests\HelenRuntimeTests.exe
```

Expected:

- `PASS`

- [ ] **Step 5: Commit**

```powershell
git add include/HelenHook/DeltaVirtualFileSource.h HelenRuntime/DeltaVirtualFileSource.cpp tests/HelenRuntime.Tests/DeltaVirtualFileSourceTests.cpp include/HelenHook/PackAssetResolver.h HelenRuntime/PackAssetResolver.cpp HelenRuntime/VirtualFileService.cpp tests/HelenRuntime.Tests/TestMain.cpp tests/HelenRuntime.Tests/HelenRuntime.Tests.vcxproj HelenRuntime/HelenRuntime.vcxproj
git commit -m "Add streamed delta-backed virtual file reads"
```

### Task 5: Materialize mapped delta-backed files into a resolved cache

**Files:**
- Create: `include/HelenHook/ResolvedFileCache.h`
- Create: `HelenRuntime/ResolvedFileCache.cpp`
- Create: `tests/HelenRuntime.Tests/ResolvedFileCacheTests.cpp`
- Modify: `include/HelenHook/DeltaVirtualFileSource.h`
- Modify: `HelenRuntime/DeltaVirtualFileSource.cpp`
- Modify: `tests/HelenRuntime.Tests/TestMain.cpp`
- Modify: `tests/HelenRuntime.Tests/HelenRuntime.Tests.vcxproj`
- Modify: `HelenRuntime/HelenRuntime.vcxproj`

- [ ] **Step 1: Write the failing resolved-cache test**

```cpp
void RunResolvedFileCacheTests()
{
    const std::filesystem::path root = std::filesystem::temp_directory_path() / "HelenRuntimeTests" / "ResolvedFileCache";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root / "cache");

    helen::ResolvedFileCache cache(root / "cache");
    const std::filesystem::path resolved_path = cache.GetResolvedPath(
        "batman-aa-subtitles",
        "steam-goty-1.0",
        "bmgameGameplayPackage",
        "basehash",
        "deltahash");

    Expect(resolved_path.parent_path().filename() == "bmgameGameplayPackage", "Resolved cache file-id directory mismatch.");

    std::filesystem::remove_all(root);
}
```

- [ ] **Step 2: Run the tests to verify they fail**

Run:

```powershell
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' tests\HelenRuntime.Tests\HelenRuntime.Tests.vcxproj /p:Configuration=Debug /p:Platform=Win32 /m
C:\dev\helenhook\bin\Win32\Debug\tests\HelenRuntimeTests.exe
```

Expected:

- build fails because `ResolvedFileCache` does not exist yet

- [ ] **Step 3: Implement resolved-file materialization**

```cpp
class ResolvedFileCache
{
public:
    explicit ResolvedFileCache(const std::filesystem::path& cache_root);

    std::filesystem::path GetResolvedPath(
        const std::string& pack_id,
        const std::string& build_id,
        const std::string& file_id,
        const std::string& base_sha256,
        const std::string& delta_sha256) const;
};
```

```cpp
std::optional<HANDLE> DeltaVirtualFileSource::CreateFileMapping(DWORD protection, DWORD maximum_size_high, DWORD maximum_size_low)
{
    const std::filesystem::path resolved_path = EnsureResolvedFile();
    const HANDLE file_handle = ::CreateFileW(resolved_path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file_handle == INVALID_HANDLE_VALUE)
    {
        return std::nullopt;
    }

    const HANDLE mapping_handle = ::CreateFileMappingW(file_handle, nullptr, protection, maximum_size_high, maximum_size_low, nullptr);
    ::CloseHandle(file_handle);
    if (mapping_handle == nullptr)
    {
        return std::nullopt;
    }

    return mapping_handle;
}
```

- [ ] **Step 4: Re-run the tests**

Run:

```powershell
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' tests\HelenRuntime.Tests\HelenRuntime.Tests.vcxproj /p:Configuration=Debug /p:Platform=Win32 /m
C:\dev\helenhook\bin\Win32\Debug\tests\HelenRuntimeTests.exe
```

Expected:

- `PASS`

- [ ] **Step 5: Commit**

```powershell
git add include/HelenHook/ResolvedFileCache.h HelenRuntime/ResolvedFileCache.cpp tests/HelenRuntime.Tests/ResolvedFileCacheTests.cpp include/HelenHook/DeltaVirtualFileSource.h HelenRuntime/DeltaVirtualFileSource.cpp tests/HelenRuntime.Tests/TestMain.cpp tests/HelenRuntime.Tests/HelenRuntime.Tests.vcxproj HelenRuntime/HelenRuntime.vcxproj
git commit -m "Add resolved cache for mapped delta-backed files"
```

### Task 6: Generate Batman `.hgdelta` assets and switch the pack from full file to delta

**Files:**
- Create: `games/HelenBatmanAA/scripts/Build-Hgdelta.ps1`
- Modify: `games/HelenBatmanAA/scripts/Rebuild-BatmanPack.ps1`
- Modify: `games/HelenBatmanAA/helengamehook/packs/batman-aa-subtitles/builds/steam-goty-1.0/files.json`
- Modify: `tests/HelenRuntime.Tests/PackRepositoryTests.cpp`

- [ ] **Step 1: Write the failing Batman-pack parser assertion**

```cpp
Expect(loaded_batman_pack->Build.VirtualFiles[0].Mode == "delta-on-read", "Checked-in Batman gameplay package is not delta-backed.");
Expect(loaded_batman_pack->Build.VirtualFiles[0].Source.Kind == helen::VirtualFileSourceKind::DeltaFile, "Checked-in Batman gameplay package source kind mismatch.");
Expect(loaded_batman_pack->Build.VirtualFiles[0].Source.Path == std::filesystem::path("assets/deltas/BmGame-subtitle-signal.hgdelta"), "Checked-in Batman gameplay package delta path mismatch.");
```

- [ ] **Step 2: Run the tests to verify they fail**

Run:

```powershell
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' tests\HelenRuntime.Tests\HelenRuntime.Tests.vcxproj /p:Configuration=Debug /p:Platform=Win32 /m
C:\dev\helenhook\bin\Win32\Debug\tests\HelenRuntimeTests.exe
```

Expected:

- `PackRepositoryTests` fails because the Batman pack still points at the full `.u`

- [ ] **Step 3: Add the Batman-local delta build step and switch the pack**

```powershell
param(
    [Parameter(Mandatory = $true)][string]$BaseFile,
    [Parameter(Mandatory = $true)][string]$TargetFile,
    [Parameter(Mandatory = $true)][string]$OutputFile,
    [int]$ChunkSize = 65536
)

$baseBytes = [System.IO.File]::ReadAllBytes($BaseFile)
$targetBytes = [System.IO.File]::ReadAllBytes($TargetFile)
$baseHash = (Get-FileHash -LiteralPath $BaseFile -Algorithm SHA256).Hash.ToLowerInvariant()
$targetHash = (Get-FileHash -LiteralPath $TargetFile -Algorithm SHA256).Hash.ToLowerInvariant()

$chunkCount = [int][Math]::Ceiling($targetBytes.Length / [double]$ChunkSize)
$chunkEntries = New-Object System.Collections.Generic.List[byte[]]
$payloadStream = New-Object System.IO.MemoryStream

for ($chunkIndex = 0; $chunkIndex -lt $chunkCount; $chunkIndex++) {
    $offset = $chunkIndex * $ChunkSize
    $targetLength = [Math]::Min($ChunkSize, $targetBytes.Length - $offset)
    $baseLength = [Math]::Min($ChunkSize, $baseBytes.Length - $offset)
    $targetChunk = $targetBytes[$offset..($offset + $targetLength - 1)]
    $sameAsBase = ($targetLength -eq $baseLength)
    if ($sameAsBase) {
        for ($i = 0; $i -lt $targetLength; $i++) {
            if ($baseBytes[$offset + $i] -ne $targetChunk[$i]) { $sameAsBase = $false; break }
        }
    }

    if ($sameAsBase) {
        $chunkEntries.Add((New-HgdeltaChunkEntry -Kind 0 -TargetSize $targetLength -PayloadOffset 0 -PayloadSize 0))
    }
    else {
        $payloadOffset = [uint64]$payloadStream.Position
        $payloadStream.Write($targetChunk, 0, $targetChunk.Length)
        $chunkEntries.Add((New-HgdeltaChunkEntry -Kind 1 -TargetSize $targetLength -PayloadOffset $payloadOffset -PayloadSize $targetChunk.Length))
    }
}

Write-HgdeltaFile -OutputFile $OutputFile -ChunkSize $ChunkSize -BaseSize $baseBytes.Length -TargetSize $targetBytes.Length -BaseSha256 $baseHash -TargetSha256 $targetHash -ChunkEntries $chunkEntries -PayloadBytes $payloadStream.ToArray()
```

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
                    size = (Get-Item -LiteralPath $BaseFile).Length
                    sha256 = $baseHash
                }
                target = @{
                    size = (Get-Item -LiteralPath $TargetFile).Length
                    sha256 = $targetHash
                }
                chunkSize = $ChunkSize
            }
        }
    )
}

$filesManifest | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $FilesJsonPath
```

- [ ] **Step 4: Rebuild the Batman pack and run the full tests**

Run:

```powershell
powershell -ExecutionPolicy Bypass -File .\games\HelenBatmanAA\scripts\Rebuild-BatmanPack.ps1
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' tests\HelenRuntime.Tests\HelenRuntime.Tests.vcxproj /p:Configuration=Debug /p:Platform=Win32 /m
C:\dev\helenhook\bin\Win32\Debug\tests\HelenRuntimeTests.exe
```

Expected:

- Batman pack rebuild succeeds
- `PASS`

- [ ] **Step 5: Commit**

```powershell
git add games/HelenBatmanAA/scripts/Build-Hgdelta.ps1 games/HelenBatmanAA/scripts/Rebuild-BatmanPack.ps1 games/HelenBatmanAA/helengamehook/packs/batman-aa-subtitles/builds/steam-goty-1.0/files.json tests/HelenRuntime.Tests/PackRepositoryTests.cpp
git commit -m "Switch Batman gameplay package to hgdelta delivery"
```

### Task 7: Deploy and verify Batman end to end on the delta-backed package

**Files:**
- Modify: `games/HelenBatmanAA/scripts/Deploy-Batman.ps1`
- Modify: `games/HelenBatmanAA/README.md`

- [ ] **Step 1: Update deployment to include the delta asset**

```powershell
$packFiles = @(
    'pack.json',
    'builds\steam-goty-1.0\build.json',
    'builds\steam-goty-1.0\commands.json',
    'builds\steam-goty-1.0\hooks.json',
    'builds\steam-goty-1.0\files.json',
    'builds\steam-goty-1.0\assets\deltas\BmGame-subtitle-signal.hgdelta',
    'builds\steam-goty-1.0\assets\native\batman-global-text-scale.bin'
)
```

- [ ] **Step 2: Deploy and verify the runtime**

Run:

```powershell
powershell -ExecutionPolicy Bypass -File .\games\HelenBatmanAA\scripts\Deploy-Batman.ps1 -Configuration Debug
Get-Content 'D:\steam\steamapps\common\Batman Arkham Asylum GOTY\Binaries\helengamehook\packs\batman-aa-subtitles\builds\steam-goty-1.0\files.json'
```

Expected:

- deploy script prints `DEPLOYED`
- deployed `files.json` shows `mode = "delta-on-read"`

- [ ] **Step 3: Launch Batman and verify one full end-to-end path**

Run:

```powershell
powershell -ExecutionPolicy Bypass -File .\games\HelenBatmanAA\scripts\Launch-Check-Batman.ps1
```

Expected:

- `PROCESS_STARTED`

Manual verification:

- pause menu still shows `Subtitle Size`
- changing the value updates the subtitle size live
- `D:\steam\steamapps\common\Batman Arkham Asylum GOTY\Binaries\helengamehook\config\runtime.json` updates
- on-disk `D:\steam\steamapps\common\Batman Arkham Asylum GOTY\BmGame\CookedPC\BmGame.u` remains unchanged

- [ ] **Step 4: Commit**

```powershell
git add games/HelenBatmanAA/scripts/Deploy-Batman.ps1 games/HelenBatmanAA/README.md
git commit -m "Document and deploy hgdelta-backed Batman package"
```

---

## Self-Review

### Spec coverage

- Full-file and delta-file support: Tasks 2 and 3
- Exact base validation: Task 4
- Avoid full RAM reconstruction: Task 4
- Materialize for mappings: Task 5
- Batman builder integration: Task 6
- Batman pack switch and deployment: Task 7

### Placeholder scan

- No `TODO`, `TBD`, or “similar to above” steps remain.
- Every task names exact files, exact commands, and concrete test snippets.

### Type consistency

- `VirtualFileSourceKind`, `VirtualFileHashDefinition`, and `VirtualFileSourceDefinition` are introduced in Task 2 and then reused consistently in Tasks 3 through 7.
- `HgdeltaFile` is introduced in Task 1 and then consumed by `DeltaVirtualFileSource` in Tasks 4 and 5.
