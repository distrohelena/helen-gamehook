#include <HelenHook/FileApiHookSet.h>
#include <HelenHook/FileWritePolicy.h>
#include <HelenHook/FileWriteRoute.h>
#include <HelenHook/FileWriteRoutingService.h>
#include <HelenHook/Memory.h>
#include <HelenHook/Hook.h>
#include <HelenHook/VirtualFileService.h>

#include <array>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace
{
    /** @brief Keeps required and safety-sensitive kernel32 imports materialized in the fixture PE import table. */
    volatile decltype(&SetFilePointerEx) SetFilePointerExImportAnchor = &SetFilePointerEx;

    /** @brief Keeps duplicate-handle coverage present even when the call is resolved through the inspected IAT. */
    volatile decltype(&DuplicateHandle) DuplicateHandleImportAnchor = &DuplicateHandle;

    /** @brief Keeps handle-information coverage present even when the call is resolved through the inspected IAT. */
    volatile decltype(&SetFileInformationByHandle) SetFileInformationByHandleImportAnchor = &SetFileInformationByHandle;

    /** @brief Keeps every A/W mutation import used by the fixture in the child PE. */
    volatile decltype(&CreateFileW) CreateFileWImportAnchor = &CreateFileW;
    volatile decltype(&CreateFileA) CreateFileAImportAnchor = &CreateFileA;
    volatile decltype(&CopyFileW) CopyFileWImportAnchor = &CopyFileW;
    volatile decltype(&MoveFileExA) MoveFileExAImportAnchor = &MoveFileExA;
    volatile decltype(&MoveFileExW) MoveFileExWImportAnchor = &MoveFileExW;
    volatile decltype(&ReplaceFileW) ReplaceFileWImportAnchor = &ReplaceFileW;
    volatile decltype(&SetFileAttributesA) SetFileAttributesAImportAnchor = &SetFileAttributesA;
    volatile decltype(&CreateFileMappingW) CreateFileMappingWImportAnchor = &CreateFileMappingW;
    volatile decltype(&RemoveDirectoryW) RemoveDirectoryWImportAnchor = &RemoveDirectoryW;
    volatile decltype(&DeleteFileW) DeleteFileWImportAnchor = &DeleteFileW;
    volatile decltype(&CloseHandle) CloseHandleImportAnchor = &CloseHandle;

    /** @brief Throws a descriptive failure when one real hooked-fixture assertion is false. */
    void ExpectHookFixture(bool condition, const char* message)
    {
        if (!condition)
        {
            throw std::runtime_error(message);
        }
    }

    /** @brief Creates one fixture file with exact bytes before the IAT is patched. */
    void WriteHookFixtureFile(const std::filesystem::path& path, std::string_view bytes)
    {
        std::ofstream stream(path, std::ios::binary | std::ios::trunc);
        if (!stream)
        {
            throw std::runtime_error("Failed to create hooked routing fixture file.");
        }
        stream.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
        ExpectHookFixture(static_cast<bool>(stream), "Failed to write hooked routing fixture file.");
    }

    /** @brief Reads one fixture file through the CRT after hooks are removed. */
    std::string ReadHookFixtureFile(const std::filesystem::path& path)
    {
        std::ifstream stream(path, std::ios::binary);
        ExpectHookFixture(static_cast<bool>(stream), "Failed to read hooked routing fixture file.");
        return std::string(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
    }

    /** @brief Writes one byte through a real imported WriteFile call on a routed handle. */
    void WriteHookedByte(HANDLE handle, char value)
    {
        DWORD bytes_written = 0;
        ExpectHookFixture(WriteFile(handle, &value, 1, &bytes_written, nullptr) != FALSE && bytes_written == 1,
            "Imported WriteFile failed for routed fixture handle.");
    }

    /**
     * @brief Resolves one named kernel32 import slot in the current executable after the adapter is installed.
     * @tparam T Exact imported function pointer type.
     * @param import_name PE import name whose patched slot should be called.
     * @return Current function pointer stored in the executable IAT.
     */
    template <typename T>
    T CallHookedImport(const char* import_name)
    {
        const std::optional<helen::ModuleView> module = helen::QueryMainModule();
        ExpectHookFixture(module.has_value(), "Hook fixture could not query its import module.");
        void** const slot = helen::FindImportAddress(*module, "kernel32.dll", import_name);
        if (slot == nullptr)
        {
            throw std::runtime_error(std::string("Hook fixture could not find import slot: ") + import_name);
        }
        return reinterpret_cast<T>(*slot);
    }
}

/**
 * @brief Installs the production IAT adapter and exercises real imported A/W file mutations.
 *
 * The routing service's native calls resolve kernel32 exports directly, so helper operations cannot recurse through
 * the patched test executable IAT. This fixture deliberately keeps API calls in this executable to prove the adapter
 * boundary and removes the hook before the next native test group.
 */
void RunFileWriteRoutingHookFixtureTests()
{
    const std::filesystem::path root = std::filesystem::temp_directory_path() / "HelenRuntimeTests" / "FileWriteRoutingHook";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
    const std::filesystem::path original_path = root / "BmEngine.ini";
    const std::filesystem::path deny_path = root / "Deny.ini";
    const std::filesystem::path source_path = root / "BmEngine.tmp";
    const std::filesystem::path unrelated_path = root / "Unrelated.ini";
    WriteHookFixtureFile(original_path, "ORIGINAL");
    WriteHookFixtureFile(deny_path, "DENY");
    WriteHookFixtureFile(source_path, "COPY");
    WriteHookFixtureFile(unrelated_path, "NATIVE");

    try
    {
        helen::FileWriteRoutingService routing(root / "cache", root);
        DWORD error = ERROR_SUCCESS;
        ExpectHookFixture(routing.Initialize({
            { "engine", original_path, helen::FileWritePolicy::Redirect, helen::FileReadPolicy::Redirected },
            { "deny", deny_path, helen::FileWritePolicy::Deny, helen::FileReadPolicy::Original },
        }, error), "Hook fixture routing initialization failed.");
        helen::VirtualFileService virtual_files(root / "virtual-cache");
        ExpectHookFixture(routing.ClassifyPath(original_path) == helen::FileWriteRoutingService::PathDisposition::Protected,
            "Hook fixture route was not classified as protected before installation.");
        HANDLE preexisting_read = CreateFileW(original_path.c_str(), GENERIC_READ,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        ExpectHookFixture(preexisting_read != INVALID_HANDLE_VALUE, "Pre-existing original-read fixture handle could not be opened.");
        helen::FileApiHookSet hooks(virtual_files, routing, root, root, {});
        const std::optional<helen::ModuleView> main_module = helen::QueryMainModule();
        ExpectHookFixture(main_module.has_value(), "Hook fixture could not query its main module.");
        void** const create_file_slot = helen::FindImportAddress(*main_module, "kernel32.dll", "CreateFileW");
        ExpectHookFixture(create_file_slot != nullptr, "Hook fixture could not find CreateFileW import.");
        void* const original_create_file_target = *create_file_slot;
        ExpectHookFixture(hooks.Install(), "Hook fixture IAT installation failed.");
        ExpectHookFixture(hooks.IsInstalled(), "Hook fixture reported inactive immediately after installation.");
        ExpectHookFixture(*create_file_slot != original_create_file_target, "Hook fixture CreateFileW IAT slot was not changed.");

        const auto imported_create_file_w = CallHookedImport<decltype(&CreateFileW)>("CreateFileW");
        HANDLE routed = imported_create_file_w(original_path.c_str(), GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        ExpectHookFixture(routed != INVALID_HANDLE_VALUE, "Imported routed CreateFileW failed.");
        ExpectHookFixture(routing.IsTrackedHandle(routed), "Imported routed CreateFileW was not tracked.");
        LARGE_INTEGER fixture_offset{};
        ExpectHookFixture(CallHookedImport<decltype(&SetFilePointerEx)>("SetFilePointerEx")(routed, fixture_offset, nullptr, FILE_BEGIN) != FALSE,
            "Imported SetFilePointerEx failed for routed fixture handle.");
        WriteHookedByte(routed, 'W');
        ExpectHookFixture(CallHookedImport<decltype(&CloseHandle)>("CloseHandle")(routed) != FALSE, "Imported tracked CloseHandle failed.");

        HANDLE denied = CallHookedImport<decltype(&CreateFileA)>("CreateFileA")(deny_path.string().c_str(), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        ExpectHookFixture(denied == INVALID_HANDLE_VALUE && GetLastError() == ERROR_ACCESS_DENIED, "Imported denied CreateFileA was not rejected.");

        WriteHookFixtureFile(source_path, "C");
        ExpectHookFixture(CallHookedImport<decltype(&CopyFileW)>("CopyFileW")(source_path.c_str(), original_path.c_str(), FALSE) != FALSE, "Imported CopyFileW did not route destination.");
        WriteHookFixtureFile(source_path, "M");
        ExpectHookFixture(CallHookedImport<decltype(&MoveFileExA)>("MoveFileExA")(source_path.string().c_str(), original_path.string().c_str(), MOVEFILE_DELAY_UNTIL_REBOOT) == FALSE &&
            GetLastError() == ERROR_NOT_SUPPORTED, "Imported delayed move was not rejected before mutation.");
        ExpectHookFixture(std::filesystem::exists(source_path), "Rejected delayed move mutated its source.");
        ExpectHookFixture(CallHookedImport<decltype(&MoveFileExW)>("MoveFileExW")(original_path.c_str(), unrelated_path.c_str(), MOVEFILE_REPLACE_EXISTING) == FALSE &&
            GetLastError() == ERROR_ACCESS_DENIED, "Imported protected source escape was not rejected.");
        ExpectHookFixture(CallHookedImport<decltype(&MoveFileExA)>("MoveFileExA")(source_path.string().c_str(), original_path.string().c_str(), MOVEFILE_REPLACE_EXISTING) != FALSE,
            "Imported MoveFileExA did not route destination.");
        WriteHookFixtureFile(source_path, "R");
        ExpectHookFixture(CallHookedImport<decltype(&ReplaceFileW)>("ReplaceFileW")(original_path.c_str(), source_path.c_str(), nullptr, REPLACEFILE_IGNORE_MERGE_ERRORS, nullptr, nullptr) != FALSE,
            "Imported ReplaceFileW did not route destination.");
        ExpectHookFixture(CallHookedImport<decltype(&SetFileAttributesA)>("SetFileAttributesA")(original_path.string().c_str(), FILE_ATTRIBUTE_HIDDEN) != FALSE,
            "Imported SetFileAttributesA did not route destination.");
        HANDLE routed_read = CreateFileW(original_path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        ExpectHookFixture(routed_read != INVALID_HANDLE_VALUE, "Imported routed read handle could not be opened.");
        HANDLE duplicate = nullptr;
        ExpectHookFixture(CallHookedImport<decltype(&DuplicateHandle)>("DuplicateHandle")(GetCurrentProcess(), routed_read, GetCurrentProcess(), &duplicate, 0, FALSE, 0) == FALSE &&
            GetLastError() == ERROR_ACCESS_DENIED, "Imported DuplicateHandle escaped a routed handle.");
        HANDLE writable_mapping = CallHookedImport<decltype(&CreateFileMappingW)>("CreateFileMappingW")(routed_read, nullptr, PAGE_READWRITE, 0, 0, nullptr);
        ExpectHookFixture(writable_mapping == nullptr && GetLastError() == ERROR_ACCESS_DENIED,
            "Imported writable mapping escaped a routed handle.");
        HANDLE named_mapping = CallHookedImport<decltype(&CreateFileMappingW)>("CreateFileMappingW")(routed_read, nullptr, PAGE_READONLY, 0, 0, L"HelenRoutingNamed");
        ExpectHookFixture(named_mapping == nullptr && GetLastError() == ERROR_ACCESS_DENIED,
            "Imported named mapping escaped a routed handle.");
        HANDLE read_mapping = CallHookedImport<decltype(&CreateFileMappingW)>("CreateFileMappingW")(routed_read, nullptr, PAGE_READONLY, 0, 0, nullptr);
        ExpectHookFixture(read_mapping != nullptr, "Imported read-only mapping was incorrectly rejected.");
        ExpectHookFixture(CallHookedImport<decltype(&CloseHandle)>("CloseHandle")(read_mapping) != FALSE, "Imported mapping close failed.");
        FILE_DISPOSITION_INFO disposition_info{};
        disposition_info.DeleteFile = TRUE;
        ExpectHookFixture(CallHookedImport<decltype(&SetFileInformationByHandle)>("SetFileInformationByHandle")(routed_read, FileDispositionInfo, &disposition_info, sizeof(disposition_info)) == FALSE &&
            GetLastError() == ERROR_ACCESS_DENIED, "Imported handle disposition escaped a routed handle.");
        ExpectHookFixture(CallHookedImport<decltype(&CloseHandle)>("CloseHandle")(routed_read) != FALSE, "Imported routed read close failed.");
        ExpectHookFixture(CallHookedImport<decltype(&RemoveDirectoryW)>("RemoveDirectoryW")(root.c_str()) == FALSE && GetLastError() == ERROR_ACCESS_DENIED,
            "Imported parent-directory mutation escaped a protected route.");
        HANDLE preexisting_duplicate = nullptr;
        ExpectHookFixture(CallHookedImport<decltype(&DuplicateHandle)>("DuplicateHandle")(GetCurrentProcess(), preexisting_read, GetCurrentProcess(), &preexisting_duplicate, 0, FALSE, 0) == FALSE &&
            GetLastError() == ERROR_ACCESS_DENIED, "Imported DuplicateHandle escaped a pre-existing protected handle.");
        HANDLE preexisting_mapping = CallHookedImport<decltype(&CreateFileMappingW)>("CreateFileMappingW")(preexisting_read, nullptr, PAGE_READWRITE, 0, 0, nullptr);
        ExpectHookFixture(preexisting_mapping == nullptr && GetLastError() == ERROR_ACCESS_DENIED,
            "Imported writable mapping escaped a pre-existing protected handle.");
        ExpectHookFixture(CallHookedImport<decltype(&CloseHandle)>("CloseHandle")(preexisting_read) != FALSE, "Pre-existing original-read fixture handle close failed.");
        ExpectHookFixture(CallHookedImport<decltype(&DeleteFileW)>("DeleteFileW")(original_path.c_str()) != FALSE, "Imported DeleteFileW did not delete only the overlay.");
        ExpectHookFixture(CallHookedImport<decltype(&CreateFileW)>("CreateFileW")(original_path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr) == INVALID_HANDLE_VALUE,
            "Deleted overlay fell back to the original through imported CreateFileW.");

        HANDLE unrelated = CallHookedImport<decltype(&CreateFileW)>("CreateFileW")(unrelated_path.c_str(), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        ExpectHookFixture(unrelated != INVALID_HANDLE_VALUE && !routing.IsTrackedHandle(unrelated), "Unrelated imported CreateFileW was routed.");
        WriteHookedByte(unrelated, 'U');
        ExpectHookFixture(CallHookedImport<decltype(&CloseHandle)>("CloseHandle")(unrelated) != FALSE, "Unrelated imported CloseHandle failed.");
        hooks.Remove();
        ExpectHookFixture(ReadHookFixtureFile(original_path) == "ORIGINAL", "Redirected imported mutations changed the original file.");
        ExpectHookFixture(ReadHookFixtureFile(unrelated_path) == "UATIVE", "Unrelated imported write did not retain native behavior.");
    }
    catch (...)
    {
        std::filesystem::remove_all(root);
        throw;
    }

    std::filesystem::remove_all(root);
}
