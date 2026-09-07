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
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

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
    volatile decltype(&WriteFile) WriteFileImportAnchor = &WriteFile;
    volatile decltype(&SetEndOfFile) SetEndOfFileImportAnchor = &SetEndOfFile;

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

    /** @brief Creates one unique directory owned by this fixture without deleting any fixed shared temp path. */
    std::filesystem::path CreateUniqueHookFixtureRoot()
    {
        std::array<wchar_t, MAX_PATH> temp_path{};
        const DWORD temp_length = GetTempPathW(static_cast<DWORD>(temp_path.size()), temp_path.data());
        if (temp_length == 0 || temp_length >= temp_path.size())
        {
            throw std::runtime_error("Failed to resolve temporary directory for hooked routing fixture.");
        }

        std::array<wchar_t, MAX_PATH> file_name{};
        if (GetTempFileNameW(temp_path.data(), L"hrf", 0, file_name.data()) == 0)
        {
            throw std::runtime_error("Failed to allocate unique hooked routing fixture path.");
        }

        const std::filesystem::path root(file_name.data());
        if (!DeleteFileW(root.c_str()) || !CreateDirectoryW(root.c_str(), nullptr))
        {
            throw std::runtime_error("Failed to create unique hooked routing fixture directory.");
        }
        return root;
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

    /** @brief Returns a short-name spelling when Windows can materialize one for the fixture directory. */
    std::filesystem::path GetShortFixturePath(const std::filesystem::path& candidate)
    {
        std::array<wchar_t, MAX_PATH> buffer{};
        const DWORD length = GetShortPathNameW(candidate.c_str(), buffer.data(), static_cast<DWORD>(buffer.size()));
        return length > 0 && length < buffer.size() ? std::filesystem::path(buffer.data()) : candidate;
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
    const std::filesystem::path root = CreateUniqueHookFixtureRoot();
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
        ExpectHookFixture(routing.ClassifyPath(std::filesystem::path(L"\\\\?\\GLOBALROOT\\Device\\HelenUnknown")) ==
            helen::FileWriteRoutingService::PathDisposition::Rejected, "Unsafe disk path was treated as unrelated.");
        ExpectHookFixture(routing.ClassifyPath(std::filesystem::path(L"\\\\.\\pipe\\HelenUnrelated")) ==
            helen::FileWriteRoutingService::PathDisposition::Unrelated, "Unrelated named pipe was not preserved as native.");
        ExpectHookFixture(routing.ClassifyPath(std::filesystem::path(L"\\\\.\\NUL")) ==
            helen::FileWriteRoutingService::PathDisposition::Unrelated, "Unrelated NUL device was not preserved as native.");
        HANDLE preexisting_read = CreateFileW(original_path.c_str(), GENERIC_READ,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        ExpectHookFixture(preexisting_read != INVALID_HANDLE_VALUE, "Pre-existing original-read fixture handle could not be opened.");
        HANDLE preexisting_write = CreateFileW(original_path.c_str(), GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        ExpectHookFixture(preexisting_write != INVALID_HANDLE_VALUE, "Pre-existing writable fixture handle could not be opened.");
        const std::filesystem::path short_parent = GetShortFixturePath(root);
        ExpectHookFixture(routing.IsProtectedParentPath(short_parent), "Short protected parent alias was not recognized.");
        helen::FileApiHookSet hooks(virtual_files, routing, root, root, {});
        const std::optional<helen::ModuleView> main_module = helen::QueryMainModule();
        ExpectHookFixture(main_module.has_value(), "Hook fixture could not query its main module.");
        void** const create_file_slot = helen::FindImportAddress(*main_module, "kernel32.dll", "CreateFileW");
        ExpectHookFixture(create_file_slot != nullptr, "Hook fixture could not find CreateFileW import.");
        void* const original_create_file_target = *create_file_slot;
        ExpectHookFixture(hooks.Install(), "Hook fixture IAT installation failed.");
        ExpectHookFixture(hooks.IsInstalled(), "Hook fixture reported inactive immediately after installation.");
        ExpectHookFixture(*create_file_slot != original_create_file_target, "Hook fixture CreateFileW IAT slot was not changed.");
        HANDLE unsafe_device = CallHookedImport<decltype(&CreateFileW)>("CreateFileW")(L"\\\\?\\GLOBALROOT\\Device\\HelenUnknown", GENERIC_READ,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        ExpectHookFixture(unsafe_device == INVALID_HANDLE_VALUE && GetLastError() == ERROR_ACCESS_DENIED,
            "Imported unsafe disk device path escaped to native CreateFileW.");
        char preexisting_byte = 'X';
        DWORD preexisting_bytes_written = 0;
        ExpectHookFixture(CallHookedImport<decltype(&WriteFile)>("WriteFile")(preexisting_write, &preexisting_byte, 1, &preexisting_bytes_written, nullptr) == FALSE &&
            GetLastError() == ERROR_ACCESS_DENIED, "Imported WriteFile mutated a pre-existing protected writable handle.");
        ExpectHookFixture(CallHookedImport<decltype(&SetEndOfFile)>("SetEndOfFile")(preexisting_write) == FALSE &&
            GetLastError() == ERROR_ACCESS_DENIED, "Imported SetEndOfFile mutated a pre-existing protected writable handle.");
        ExpectHookFixture(CallHookedImport<decltype(&CloseHandle)>("CloseHandle")(preexisting_write) != FALSE, "Pre-existing writable fixture close failed.");

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
        std::vector<wchar_t> foreign_command{ L'c', L'm', L'd', L'.', L'e', L'x', L'e', L' ', L'/', L'c', L' ', L'e', L'x', L'i', L't', L' ', L'0', L'\0' };
        STARTUPINFOW foreign_startup{};
        foreign_startup.cb = sizeof(foreign_startup);
        PROCESS_INFORMATION foreign_process{};
        ExpectHookFixture(CreateProcessW(nullptr, foreign_command.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr,
            &foreign_startup, &foreign_process) != FALSE, "Foreign duplicate source process could not be created.");
        HANDLE foreign_duplicate = nullptr;
        const BOOL foreign_duplicate_result = CallHookedImport<decltype(&DuplicateHandle)>("DuplicateHandle")(
            foreign_process.hProcess, routed_read, GetCurrentProcess(), &foreign_duplicate, 0, FALSE, 0);
        ExpectHookFixture(foreign_duplicate_result != FALSE || GetLastError() != ERROR_ACCESS_DENIED,
            "Foreign-process handle was rejected by a local numeric collision.");
        if (foreign_duplicate_result != FALSE)
        {
            ExpectHookFixture(CallHookedImport<decltype(&CloseHandle)>("CloseHandle")(foreign_duplicate) != FALSE,
                "Foreign duplicate result could not be closed.");
        }
        WaitForSingleObject(foreign_process.hProcess, INFINITE);
        CloseHandle(foreign_process.hThread);
        CloseHandle(foreign_process.hProcess);
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
        const std::filesystem::path native_move_source = root / "native-move.tmp";
        const std::filesystem::path native_move_destination = root / "native-move.ini";
        WriteHookFixtureFile(native_move_source, "MOVE");
        ExpectHookFixture(CallHookedImport<decltype(&MoveFileExW)>("MoveFileExW")(native_move_source.c_str(), native_move_destination.c_str(), MOVEFILE_COPY_ALLOWED) != FALSE,
            "Unrelated MoveFileExW lost native COPY_ALLOWED behavior.");
        hooks.Remove();
        ExpectHookFixture(ReadHookFixtureFile(original_path) == "ORIGINAL", "Redirected imported mutations changed the original file.");
        ExpectHookFixture(ReadHookFixtureFile(unrelated_path) == "UATIVE", "Unrelated imported write did not retain native behavior.");
        ExpectHookFixture(ReadHookFixtureFile(native_move_destination) == "MOVE", "Unrelated native MoveFileExW did not publish its destination.");
    }
    catch (...)
    {
        std::filesystem::remove_all(root);
        throw;
    }

    std::filesystem::remove_all(root);
}

/**
 * @brief Launches one isolated child copy of the test executable for real IAT fixture execution.
 * @return Only when the child exits successfully; otherwise throws with the child exit code.
 */
void RunFileWriteRoutingHookFixtureChildProcess()
{
    std::array<wchar_t, MAX_PATH> executable_buffer{};
    const DWORD executable_length = GetModuleFileNameW(nullptr, executable_buffer.data(), static_cast<DWORD>(executable_buffer.size()));
    if (executable_length == 0 || executable_length >= executable_buffer.size())
    {
        throw std::runtime_error("Failed to resolve routing test executable for child fixture.");
    }

    std::wstring command_line = L"\"" + std::wstring(executable_buffer.data(), executable_length) + L"\" --file-routing-hook-child";
    std::vector<wchar_t> mutable_command(command_line.begin(), command_line.end());
    mutable_command.push_back(L'\0');
    SECURITY_ATTRIBUTES pipe_attributes{};
    pipe_attributes.nLength = sizeof(pipe_attributes);
    pipe_attributes.bInheritHandle = TRUE;
    HANDLE child_output_read = nullptr;
    HANDLE child_output_write = nullptr;
    if (!CreatePipe(&child_output_read, &child_output_write, &pipe_attributes, 0))
    {
        throw std::runtime_error("Failed to create isolated routing fixture output pipe.");
    }
    if (!SetHandleInformation(child_output_read, HANDLE_FLAG_INHERIT, 0))
    {
        CloseHandle(child_output_read);
        CloseHandle(child_output_write);
        throw std::runtime_error("Failed to configure isolated routing fixture output pipe.");
    }
    STARTUPINFOW startup_info{};
    startup_info.cb = sizeof(startup_info);
    startup_info.dwFlags = STARTF_USESTDHANDLES;
    startup_info.hStdOutput = child_output_write;
    startup_info.hStdError = child_output_write;
    PROCESS_INFORMATION process_info{};
    if (!CreateProcessW(nullptr, mutable_command.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, nullptr,
        &startup_info, &process_info))
    {
        CloseHandle(child_output_read);
        CloseHandle(child_output_write);
        throw std::runtime_error("Failed to launch isolated routing hook fixture child.");
    }

    CloseHandle(child_output_write);
    WaitForSingleObject(process_info.hProcess, INFINITE);
    DWORD exit_code = 1;
    GetExitCodeProcess(process_info.hProcess, &exit_code);
    CloseHandle(process_info.hThread);
    CloseHandle(process_info.hProcess);
    std::string child_output;
    std::array<char, 256> output_buffer{};
    for (;;)
    {
        DWORD bytes_read = 0;
        if (!ReadFile(child_output_read, output_buffer.data(), static_cast<DWORD>(output_buffer.size()), &bytes_read, nullptr) || bytes_read == 0)
        {
            break;
        }
        child_output.append(output_buffer.data(), bytes_read);
    }
    CloseHandle(child_output_read);
    if (exit_code != 0 || child_output.find("FILE_ROUTING_HOOK_CHILD_PASS") == std::string::npos)
    {
        throw std::runtime_error("Isolated routing hook fixture child failed or omitted its pass marker.");
    }
    std::cout << child_output << "FILE_ROUTING_HOOK_CHILD_EXIT=" << exit_code << "\n";
}
