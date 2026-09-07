#include <filesystem>
#include <limits>
#include <optional>
#include <string>

#include <HelenHook/FileApiHookSet.h>

#include <HelenHook/Log.h>
#include <HelenHook/Memory.h>

namespace
{
    /**
     * @brief Returns true when a CreateFileW request can be satisfied by a read-only replacement payload.
     * @param dwDesiredAccess Requested open access mask.
     * @param dwCreationDisposition Requested creation disposition.
     * @param dwFlagsAndAttributes Requested file flags and attributes.
     * @return True when the open is compatible with a replace-on-read virtual file.
     */
    bool CanVirtualizeCreateFile(DWORD dwDesiredAccess, DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes)
    {
        if (dwCreationDisposition != OPEN_EXISTING)
        {
            return false;
        }

        if ((dwFlagsAndAttributes & FILE_FLAG_OVERLAPPED) != 0)
        {
            return false;
        }

        constexpr DWORD kWritableAccessMask = GENERIC_WRITE
            | GENERIC_ALL
            | FILE_WRITE_DATA
            | FILE_APPEND_DATA
            | FILE_WRITE_EA
            | FILE_WRITE_ATTRIBUTES
            | DELETE;
        if ((dwDesiredAccess & kWritableAccessMask) != 0)
        {
            return false;
        }

        constexpr DWORD kReadableAccessMask = GENERIC_READ | FILE_READ_DATA;
        return (dwDesiredAccess & kReadableAccessMask) != 0;
    }

    /**
     * @brief Resolves one exported kernel32 function at runtime so fallback calls bypass the patched IAT.
     * @tparam T Exact function pointer type that should be resolved.
     * @param export_name ANSI export name to look up in kernel32.dll.
     * @return The resolved function pointer when available; otherwise nullptr.
     */
    template <typename T>
    T ResolveKernel32Export(const char* export_name) noexcept
    {
        const HMODULE kernel32_module = GetModuleHandleW(L"kernel32.dll");
        if (kernel32_module == nullptr)
        {
            return nullptr;
        }

        return reinterpret_cast<T>(GetProcAddress(kernel32_module, export_name));
    }

    /**
     * @brief Calls the real CreateFileW export without using the main executable import table.
     */
    HANDLE WINAPI CallRealCreateFileW(
        LPCWSTR lpFileName,
        DWORD dwDesiredAccess,
        DWORD dwShareMode,
        LPSECURITY_ATTRIBUTES lpSecurityAttributes,
        DWORD dwCreationDisposition,
        DWORD dwFlagsAndAttributes,
        HANDLE hTemplateFile)
    {
        const auto create_file_w = ResolveKernel32Export<decltype(&CreateFileW)>("CreateFileW");
        if (create_file_w == nullptr)
        {
            SetLastError(ERROR_PROC_NOT_FOUND);
            return INVALID_HANDLE_VALUE;
        }

        return create_file_w(
            lpFileName,
            dwDesiredAccess,
            dwShareMode,
            lpSecurityAttributes,
            dwCreationDisposition,
            dwFlagsAndAttributes,
            hTemplateFile);
    }

    /**
     * @brief Calls the real CreateFileA export without using the main executable import table.
     */
    HANDLE WINAPI CallRealCreateFileA(
        LPCSTR lpFileName,
        DWORD dwDesiredAccess,
        DWORD dwShareMode,
        LPSECURITY_ATTRIBUTES lpSecurityAttributes,
        DWORD dwCreationDisposition,
        DWORD dwFlagsAndAttributes,
        HANDLE hTemplateFile)
    {
        const auto create_file_a = ResolveKernel32Export<decltype(&CreateFileA)>("CreateFileA");
        if (create_file_a == nullptr)
        {
            SetLastError(ERROR_PROC_NOT_FOUND);
            return INVALID_HANDLE_VALUE;
        }

        return create_file_a(
            lpFileName,
            dwDesiredAccess,
            dwShareMode,
            lpSecurityAttributes,
            dwCreationDisposition,
            dwFlagsAndAttributes,
            hTemplateFile);
    }

    /**
     * @brief Calls the real ReadFile export without using the main executable import table.
     */
    BOOL WINAPI CallRealReadFile(
        HANDLE hFile,
        LPVOID lpBuffer,
        DWORD nNumberOfBytesToRead,
        LPDWORD lpNumberOfBytesRead,
        LPOVERLAPPED lpOverlapped)
    {
        const auto read_file = ResolveKernel32Export<decltype(&ReadFile)>("ReadFile");
        if (read_file == nullptr)
        {
            SetLastError(ERROR_PROC_NOT_FOUND);
            return FALSE;
        }

        return read_file(hFile, lpBuffer, nNumberOfBytesToRead, lpNumberOfBytesRead, lpOverlapped);
    }

    /** @brief Calls the real WriteFile export without the current executable IAT. */
    BOOL WINAPI CallRealWriteFile(HANDLE hFile, LPCVOID lpBuffer, DWORD bytes_to_write,
        LPDWORD bytes_written, LPOVERLAPPED overlapped)
    {
        const auto write_file = ResolveKernel32Export<decltype(&WriteFile)>("WriteFile");
        if (write_file == nullptr)
        {
            SetLastError(ERROR_PROC_NOT_FOUND);
            return FALSE;
        }
        return write_file(hFile, lpBuffer, bytes_to_write, bytes_written, overlapped);
    }

    /** @brief Calls the real SetEndOfFile export without the current executable IAT. */
    BOOL WINAPI CallRealSetEndOfFile(HANDLE hFile)
    {
        const auto set_end_of_file = ResolveKernel32Export<decltype(&SetEndOfFile)>("SetEndOfFile");
        if (set_end_of_file == nullptr)
        {
            SetLastError(ERROR_PROC_NOT_FOUND);
            return FALSE;
        }
        return set_end_of_file(hFile);
    }

    /**
     * @brief Calls the real SetFilePointerEx export without using the main executable import table.
     */
    BOOL WINAPI CallRealSetFilePointerEx(
        HANDLE hFile,
        LARGE_INTEGER liDistanceToMove,
        PLARGE_INTEGER lpNewFilePointer,
        DWORD dwMoveMethod)
    {
        const auto set_file_pointer_ex = ResolveKernel32Export<decltype(&SetFilePointerEx)>("SetFilePointerEx");
        if (set_file_pointer_ex == nullptr)
        {
            SetLastError(ERROR_PROC_NOT_FOUND);
            return FALSE;
        }

        return set_file_pointer_ex(hFile, liDistanceToMove, lpNewFilePointer, dwMoveMethod);
    }

    /**
     * @brief Calls the real SetFilePointer export without using the main executable import table.
     */
    DWORD WINAPI CallRealSetFilePointer(
        HANDLE hFile,
        LONG lDistanceToMove,
        PLONG lpDistanceToMoveHigh,
        DWORD dwMoveMethod)
    {
        const auto set_file_pointer = ResolveKernel32Export<decltype(&SetFilePointer)>("SetFilePointer");
        if (set_file_pointer == nullptr)
        {
            SetLastError(ERROR_PROC_NOT_FOUND);
            return INVALID_SET_FILE_POINTER;
        }

        return set_file_pointer(hFile, lDistanceToMove, lpDistanceToMoveHigh, dwMoveMethod);
    }

    /**
     * @brief Calls the real GetFileSizeEx export without using the main executable import table.
     */
    BOOL WINAPI CallRealGetFileSizeEx(HANDLE hFile, PLARGE_INTEGER lpFileSize)
    {
        const auto get_file_size_ex = ResolveKernel32Export<decltype(&GetFileSizeEx)>("GetFileSizeEx");
        if (get_file_size_ex == nullptr)
        {
            SetLastError(ERROR_PROC_NOT_FOUND);
            return FALSE;
        }

        return get_file_size_ex(hFile, lpFileSize);
    }

    /**
     * @brief Calls the real GetFileSize export without using the main executable import table.
     */
    DWORD WINAPI CallRealGetFileSize(HANDLE hFile, LPDWORD lpFileSizeHigh)
    {
        const auto get_file_size = ResolveKernel32Export<decltype(&GetFileSize)>("GetFileSize");
        if (get_file_size == nullptr)
        {
            SetLastError(ERROR_PROC_NOT_FOUND);
            return INVALID_FILE_SIZE;
        }

        return get_file_size(hFile, lpFileSizeHigh);
    }

    /**
     * @brief Calls the real GetFileAttributesW export without using the main executable import table.
     */
    DWORD WINAPI CallRealGetFileAttributesW(LPCWSTR lpFileName)
    {
        const auto get_file_attributes_w = ResolveKernel32Export<decltype(&GetFileAttributesW)>("GetFileAttributesW");
        if (get_file_attributes_w == nullptr)
        {
            SetLastError(ERROR_PROC_NOT_FOUND);
            return INVALID_FILE_ATTRIBUTES;
        }

        return get_file_attributes_w(lpFileName);
    }

    /**
     * @brief Calls the real GetFileAttributesA export without using the main executable import table.
     */
    DWORD WINAPI CallRealGetFileAttributesA(LPCSTR lpFileName)
    {
        const auto get_file_attributes_a = ResolveKernel32Export<decltype(&GetFileAttributesA)>("GetFileAttributesA");
        if (get_file_attributes_a == nullptr)
        {
            SetLastError(ERROR_PROC_NOT_FOUND);
            return INVALID_FILE_ATTRIBUTES;
        }

        return get_file_attributes_a(lpFileName);
    }

    /**
     * @brief Calls the real CreateFileMappingA export without using the main executable import table.
     */
    HANDLE WINAPI CallRealCreateFileMappingA(
        HANDLE hFile,
        LPSECURITY_ATTRIBUTES lpFileMappingAttributes,
        DWORD flProtect,
        DWORD dwMaximumSizeHigh,
        DWORD dwMaximumSizeLow,
        LPCSTR lpName)
    {
        const auto create_file_mapping_a = ResolveKernel32Export<decltype(&CreateFileMappingA)>("CreateFileMappingA");
        if (create_file_mapping_a == nullptr)
        {
            SetLastError(ERROR_PROC_NOT_FOUND);
            return nullptr;
        }

        return create_file_mapping_a(
            hFile,
            lpFileMappingAttributes,
            flProtect,
            dwMaximumSizeHigh,
            dwMaximumSizeLow,
            lpName);
    }

    /**
     * @brief Calls the real CloseHandle export without using the main executable import table.
     */
    BOOL WINAPI CallRealCloseHandle(HANDLE hObject)
    {
        const auto close_handle = ResolveKernel32Export<decltype(&CloseHandle)>("CloseHandle");
        if (close_handle == nullptr)
        {
            SetLastError(ERROR_PROC_NOT_FOUND);
            return FALSE;
        }

        return close_handle(hObject);
    }

    /**
     * @brief Resolves a kernel32 export used by mutation detours so native fallback bypasses the patched IAT.
     * @tparam T Exact native function pointer type.
     * @param export_name Export name to resolve.
     * @return Resolved function pointer or nullptr when unavailable.
     */
    template <typename T>
    T ResolveMutationExport(const char* export_name) noexcept
    {
        const HMODULE module = GetModuleHandleW(L"kernel32.dll");
        return module == nullptr ? nullptr : reinterpret_cast<T>(GetProcAddress(module, export_name));
    }

    /** @brief Calls native DeleteFileW without executing the main executable's detoured import. */
    BOOL WINAPI CallRealDeleteFileW(LPCWSTR path)
    {
        const auto function = ResolveMutationExport<decltype(&DeleteFileW)>("DeleteFileW");
        if (function == nullptr) { SetLastError(ERROR_PROC_NOT_FOUND); return FALSE; }
        return function(path);
    }

    /** @brief Calls native DeleteFileA without executing the main executable's detoured import. */
    BOOL WINAPI CallRealDeleteFileA(LPCSTR path)
    {
        const auto function = ResolveMutationExport<decltype(&DeleteFileA)>("DeleteFileA");
        if (function == nullptr) { SetLastError(ERROR_PROC_NOT_FOUND); return FALSE; }
        return function(path);
    }

    /** @brief Calls native MoveFileExA without executing the main executable's detoured import. */
    BOOL WINAPI CallRealMoveFileExA(LPCSTR source, LPCSTR destination, DWORD flags)
    {
        const auto function = ResolveMutationExport<decltype(&MoveFileExA)>("MoveFileExA");
        if (function == nullptr) { SetLastError(ERROR_PROC_NOT_FOUND); return FALSE; }
        return function(source, destination, flags);
    }

    /** @brief Calls native ReplaceFileA without executing the main executable's detoured import. */
    BOOL WINAPI CallRealReplaceFileA(LPCSTR replaced, LPCSTR replacement, LPCSTR backup, DWORD flags, LPVOID exclude, LPVOID reserved)
    {
        const auto function = ResolveMutationExport<decltype(&ReplaceFileA)>("ReplaceFileA");
        if (function == nullptr) { SetLastError(ERROR_PROC_NOT_FOUND); return FALSE; }
        return function(replaced, replacement, backup, flags, exclude, reserved);
    }

    /** @brief Calls native CopyFileA without executing the main executable's detoured import. */
    BOOL WINAPI CallRealCopyFileA(LPCSTR source, LPCSTR destination, BOOL fail_if_exists)
    {
        const auto function = ResolveMutationExport<decltype(&CopyFileA)>("CopyFileA");
        if (function == nullptr) { SetLastError(ERROR_PROC_NOT_FOUND); return FALSE; }
        return function(source, destination, fail_if_exists);
    }

    /** @brief Calls native SetFileAttributesA without executing the main executable's detoured import. */
    BOOL WINAPI CallRealSetFileAttributesA(LPCSTR path, DWORD attributes)
    {
        const auto function = ResolveMutationExport<decltype(&SetFileAttributesA)>("SetFileAttributesA");
        if (function == nullptr) { SetLastError(ERROR_PROC_NOT_FOUND); return FALSE; }
        return function(path, attributes);
    }

    /** @brief Calls native CreateDirectoryA without executing the main executable's detoured import. */
    BOOL WINAPI CallRealCreateDirectoryA(LPCSTR path, LPSECURITY_ATTRIBUTES security_attributes)
    {
        const auto function = ResolveMutationExport<decltype(&CreateDirectoryA)>("CreateDirectoryA");
        if (function == nullptr) { SetLastError(ERROR_PROC_NOT_FOUND); return FALSE; }
        return function(path, security_attributes);
    }

    /** @brief Calls native RemoveDirectoryA without executing the main executable's detoured import. */
    BOOL WINAPI CallRealRemoveDirectoryA(LPCSTR path)
    {
        const auto function = ResolveMutationExport<decltype(&RemoveDirectoryA)>("RemoveDirectoryA");
        if (function == nullptr) { SetLastError(ERROR_PROC_NOT_FOUND); return FALSE; }
        return function(path);
    }

    /** @brief Calls native MoveFileExW without executing the main executable's detoured import. */
    BOOL WINAPI CallRealMoveFileExW(LPCWSTR source, LPCWSTR destination, DWORD flags)
    {
        const auto function = ResolveMutationExport<decltype(&MoveFileExW)>("MoveFileExW");
        if (function == nullptr) { SetLastError(ERROR_PROC_NOT_FOUND); return FALSE; }
        return function(source, destination, flags);
    }

    /** @brief Calls native ReplaceFileW without executing the main executable's detoured import. */
    BOOL WINAPI CallRealReplaceFileW(LPCWSTR replaced, LPCWSTR replacement, LPCWSTR backup, DWORD flags, LPVOID exclude, LPVOID reserved)
    {
        const auto function = ResolveMutationExport<decltype(&ReplaceFileW)>("ReplaceFileW");
        if (function == nullptr) { SetLastError(ERROR_PROC_NOT_FOUND); return FALSE; }
        return function(replaced, replacement, backup, flags, exclude, reserved);
    }

    /** @brief Calls native CopyFileW without executing the main executable's detoured import. */
    BOOL WINAPI CallRealCopyFileW(LPCWSTR source, LPCWSTR destination, BOOL fail_if_exists)
    {
        const auto function = ResolveMutationExport<decltype(&CopyFileW)>("CopyFileW");
        if (function == nullptr) { SetLastError(ERROR_PROC_NOT_FOUND); return FALSE; }
        return function(source, destination, fail_if_exists);
    }

    /** @brief Calls native SetFileAttributesW without executing the main executable's detoured import. */
    BOOL WINAPI CallRealSetFileAttributesW(LPCWSTR path, DWORD attributes)
    {
        const auto function = ResolveMutationExport<decltype(&SetFileAttributesW)>("SetFileAttributesW");
        if (function == nullptr) { SetLastError(ERROR_PROC_NOT_FOUND); return FALSE; }
        return function(path, attributes);
    }

    /** @brief Calls native CreateDirectoryW without executing the main executable's detoured import. */
    BOOL WINAPI CallRealCreateDirectoryW(LPCWSTR path, LPSECURITY_ATTRIBUTES security_attributes)
    {
        const auto function = ResolveMutationExport<decltype(&CreateDirectoryW)>("CreateDirectoryW");
        if (function == nullptr) { SetLastError(ERROR_PROC_NOT_FOUND); return FALSE; }
        return function(path, security_attributes);
    }

    /** @brief Calls native RemoveDirectoryW without executing the main executable's detoured import. */
    BOOL WINAPI CallRealRemoveDirectoryW(LPCWSTR path)
    {
        const auto function = ResolveMutationExport<decltype(&RemoveDirectoryW)>("RemoveDirectoryW");
        if (function == nullptr) { SetLastError(ERROR_PROC_NOT_FOUND); return FALSE; }
        return function(path);
    }

    /** @brief Calls native DuplicateHandle without executing the main executable's detoured import. */
    BOOL WINAPI CallRealDuplicateHandle(HANDLE source_process, HANDLE source_handle, HANDLE target_process,
        LPHANDLE target_handle, DWORD desired_access, BOOL inherit, DWORD options)
    {
        const auto function = ResolveMutationExport<decltype(&DuplicateHandle)>("DuplicateHandle");
        if (function == nullptr) { SetLastError(ERROR_PROC_NOT_FOUND); return FALSE; }
        return function(source_process, source_handle, target_process, target_handle, desired_access, inherit, options);
    }

    /** @brief Calls native SetFileInformationByHandle without executing the main executable's detoured import. */
    BOOL WINAPI CallRealSetFileInformationByHandle(HANDLE file, FILE_INFO_BY_HANDLE_CLASS information_class,
        LPVOID information, DWORD information_size)
    {
        const auto function = ResolveMutationExport<decltype(&SetFileInformationByHandle)>("SetFileInformationByHandle");
        if (function == nullptr) { SetLastError(ERROR_PROC_NOT_FOUND); return FALSE; }
        return function(file, information_class, information, information_size);
    }

    /** @brief Calls native CreateFileMappingW without executing the main executable's detoured import. */
    HANDLE WINAPI CallRealCreateFileMappingW(HANDLE file, LPSECURITY_ATTRIBUTES security_attributes, DWORD protect,
        DWORD size_high, DWORD size_low, LPCWSTR name)
    {
        const auto function = ResolveMutationExport<decltype(&CreateFileMappingW)>("CreateFileMappingW");
        if (function == nullptr) { SetLastError(ERROR_PROC_NOT_FOUND); return nullptr; }
        return function(file, security_attributes, protect, size_high, size_low, name);
    }

    /** @brief Returns true when a mapping protection flag could grant writable access. */
    bool RequestsWritableMapping(DWORD protection) noexcept
    {
        const DWORD base_protection = protection & 0xFFu;
        return base_protection == PAGE_READWRITE || base_protection == PAGE_WRITECOPY ||
            base_protection == PAGE_EXECUTE_READWRITE || base_protection == PAGE_EXECUTE_WRITECOPY;
    }

    /** @brief Returns true when a process handle denotes this process for local IAT safety checks. */
    bool IsCurrentProcessHandle(HANDLE process_handle) noexcept
    {
        if (process_handle == GetCurrentProcess())
        {
            return true;
        }

        return process_handle != nullptr && GetProcessId(process_handle) == GetCurrentProcessId();
    }

    /**
     * @brief Combines the legacy SetFilePointer low/high offset arguments into one signed 64-bit seek distance.
     * @param low_distance Low-order signed distance argument supplied to SetFilePointer.
     * @param high_distance Optional pointer to the high-order signed distance argument supplied to SetFilePointer.
     * @param distance_to_move Receives the combined signed seek distance on success.
     * @return True when the supplied arguments can be represented as one LARGE_INTEGER distance.
     */
    bool TryBuildLegacySeekDistance(LONG low_distance, PLONG high_distance, LARGE_INTEGER& distance_to_move)
    {
        distance_to_move.LowPart = static_cast<DWORD>(low_distance);
        if (high_distance == nullptr)
        {
            distance_to_move.HighPart = low_distance < 0 ? -1 : 0;
            return true;
        }

        distance_to_move.HighPart = *high_distance;
        return true;
    }

    /**
     * @brief Writes one 64-bit seek result back into the legacy SetFilePointer return value and optional high-order output.
     * @param new_file_pointer Combined seek result produced by the virtual file service.
     * @param high_distance Receives the high-order 32 bits when the caller supplied a storage pointer.
     * @return Low-order 32 bits returned through the SetFilePointer API contract.
     */
    DWORD EncodeLegacySeekResult(LARGE_INTEGER new_file_pointer, PLONG high_distance)
    {
        if (high_distance != nullptr)
        {
            *high_distance = new_file_pointer.HighPart;
        }

        return new_file_pointer.LowPart;
    }

    /**
     * @brief Converts one ANSI file path into UTF-16 using the active Windows ANSI code page.
     * @param path Narrow CreateFileA path that should be converted for virtual-file matching.
     * @return Converted UTF-16 filesystem path when the input is valid; otherwise no value.
     */
    std::optional<std::filesystem::path> TryConvertAnsiPath(LPCSTR path)
    {
        if (path == nullptr)
        {
            return std::nullopt;
        }

        const UINT code_page = AreFileApisANSI() != FALSE ? CP_ACP : CP_OEMCP;
        const int required_length = MultiByteToWideChar(code_page, MB_ERR_INVALID_CHARS, path, -1, nullptr, 0);
        if (required_length <= 0)
        {
            return std::nullopt;
        }

        std::wstring wide_path(static_cast<std::size_t>(required_length), L'\0');
        const int actual_length = MultiByteToWideChar(code_page, MB_ERR_INVALID_CHARS, path, -1, wide_path.data(), required_length);
        if (actual_length != required_length)
        {
            return std::nullopt;
        }

        wide_path.resize(static_cast<std::size_t>(actual_length - 1));
        return std::filesystem::path(wide_path);
    }

    /**
     * @brief Converts a normalized UTF-16 path back to the active Win32 file-API code page.
     * @param path Absolute path captured before native dispatch.
     * @return Byte spelling equivalent to the wide path when the active code page can represent it.
     *
     * The conversion rejects default-character substitution. Callers may then retain the original
     * ANSI operand, preserving native CreateFileA behavior for names that cannot be represented.
     */
    std::optional<std::string> TryConvertWidePathToAnsi(const std::filesystem::path& path)
    {
        const std::wstring wide_path = path.wstring();
        if (wide_path.empty())
        {
            return std::nullopt;
        }

        const UINT code_page = AreFileApisANSI() != FALSE ? CP_ACP : CP_OEMCP;
        BOOL used_default_character = FALSE;
        const DWORD flags = code_page == CP_UTF8 ? WC_ERR_INVALID_CHARS : WC_NO_BEST_FIT_CHARS;
        const int required_length = WideCharToMultiByte(code_page, flags, wide_path.c_str(), -1, nullptr, 0, nullptr,
            code_page == CP_UTF8 ? nullptr : &used_default_character);
        if (required_length <= 0)
        {
            return std::nullopt;
        }

        std::string narrow_path(static_cast<std::size_t>(required_length), '\0');
        used_default_character = FALSE;
        const int actual_length = WideCharToMultiByte(code_page, flags, wide_path.c_str(), -1, narrow_path.data(), required_length,
            nullptr, code_page == CP_UTF8 ? nullptr : &used_default_character);
        if (actual_length != required_length || used_default_character != FALSE)
        {
            return std::nullopt;
        }

        narrow_path.resize(static_cast<std::size_t>(actual_length - 1));
        return narrow_path;
    }

    /**
     * @brief Installs one optional import hook only when the target import exists on the main executable.
     * @param hook Mutable IAT hook that should capture the located import slot.
     * @param module Main executable module whose import table should be patched.
     * @param imported_dll Imported DLL that owns the target symbol.
     * @param imported_name Imported function name that should be replaced.
     * @param replacement Replacement function pointer that should be written into the IAT slot.
     * @param import_present Receives true when the import exists on the main executable.
     * @return True when the import was absent or the hook installed successfully; otherwise false.
     */
    bool InstallOptionalHook(
        helen::IatHook& hook,
        const helen::ModuleView& module,
        std::string_view imported_dll,
        std::string_view imported_name,
        void* replacement,
        bool& import_present)
    {
        void** const slot = helen::FindImportAddress(module, imported_dll, imported_name);
        import_present = slot != nullptr;
        if (!import_present)
        {
            return true;
        }

        return hook.Install(module, imported_dll, imported_name, replacement);
    }

}

namespace helen
{
    FileApiHookSet* FileApiHookSet::active_instance_ = nullptr;

    FileApiHookSet::FileApiHookSet(
        VirtualFileService& virtual_files,
        std::filesystem::path game_installation_root,
        std::filesystem::path request_base_directory,
        std::vector<std::string> hidden_paths)
        : virtual_files_(virtual_files)
        , hidden_path_matcher_(
            std::move(game_installation_root),
            std::move(request_base_directory),
            std::move(hidden_paths))
    {
    }

    FileApiHookSet::FileApiHookSet(
        VirtualFileService& virtual_files,
        FileWriteRoutingService& file_write_routing,
        std::filesystem::path game_installation_root,
        std::filesystem::path request_base_directory,
        std::vector<std::string> hidden_paths)
        : FileApiHookSet(
            virtual_files,
            std::move(game_installation_root),
            std::move(request_base_directory),
            std::move(hidden_paths))
    {
        file_write_routing_ = &file_write_routing;
    }

    FileApiHookSet::~FileApiHookSet()
    {
        Remove();
    }

    bool FileApiHookSet::CanVirtualizeOpen(DWORD dwDesiredAccess, DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes) noexcept
    {
        return CanVirtualizeCreateFile(dwDesiredAccess, dwCreationDisposition, dwFlagsAndAttributes);
    }

    FileApiHookSet* FileApiHookSet::Current()
    {
        return active_instance_;
    }

    bool FileApiHookSet::Install()
    {
        if (IsInstalled())
        {
            return false;
        }

        if (active_instance_ != nullptr && active_instance_ != this)
        {
            return false;
        }

        const std::optional<ModuleView> main_module = QueryMainModule();
        if (!main_module.has_value())
        {
            return false;
        }

        active_instance_ = this;

        bool has_create_file_hook = false;
        bool import_present = false;
        if (!InstallOptionalHook(create_file_w_hook_, *main_module, "kernel32.dll", "CreateFileW", reinterpret_cast<void*>(&CreateFileWDetour), import_present))
        {
            Remove();
            return false;
        }
        has_create_file_hook = has_create_file_hook || import_present;

        if (!InstallOptionalHook(create_file_a_hook_, *main_module, "kernel32.dll", "CreateFileA", reinterpret_cast<void*>(&CreateFileADetour), import_present))
        {
            Remove();
            return false;
        }
        has_create_file_hook = has_create_file_hook || import_present;

        if (!InstallOptionalHook(get_file_attributes_w_hook_, *main_module, "kernel32.dll", "GetFileAttributesW", reinterpret_cast<void*>(&GetFileAttributesWDetour), import_present))
        {
            Remove();
            return false;
        }

        if (!InstallOptionalHook(get_file_attributes_a_hook_, *main_module, "kernel32.dll", "GetFileAttributesA", reinterpret_cast<void*>(&GetFileAttributesADetour), import_present))
        {
            Remove();
            return false;
        }

        if (!has_create_file_hook)
        {
            Remove();
            return false;
        }

        if (!read_file_hook_.Install(*main_module, "kernel32.dll", "ReadFile", reinterpret_cast<void*>(&ReadFileDetour)))
        {
            Remove();
            return false;
        }

        if (!set_file_pointer_hook_.Install(*main_module, "kernel32.dll", "SetFilePointerEx", reinterpret_cast<void*>(&SetFilePointerExDetour)) &&
            !set_file_pointer_hook_.Install(*main_module, "kernel32.dll", "SetFilePointer", reinterpret_cast<void*>(&SetFilePointerDetour)))
        {
            Remove();
            return false;
        }

        bool has_get_file_size_hook = false;
        if (!InstallOptionalHook(get_file_size_ex_hook_, *main_module, "kernel32.dll", "GetFileSizeEx", reinterpret_cast<void*>(&GetFileSizeExDetour), import_present))
        {
            Remove();
            return false;
        }
        has_get_file_size_hook = has_get_file_size_hook || import_present;

        if (!InstallOptionalHook(get_file_size_hook_, *main_module, "kernel32.dll", "GetFileSize", reinterpret_cast<void*>(&GetFileSizeDetour), import_present))
        {
            Remove();
            return false;
        }
        has_get_file_size_hook = has_get_file_size_hook || import_present;

        if (!has_get_file_size_hook)
        {
            Remove();
            return false;
        }

        if (!InstallOptionalHook(create_file_mapping_a_hook_, *main_module, "kernel32.dll", "CreateFileMappingA", reinterpret_cast<void*>(&CreateFileMappingADetour), import_present))
        {
            Remove();
            return false;
        }

        if (file_write_routing_ != nullptr &&
            (!InstallOptionalHook(write_file_hook_, *main_module, "kernel32.dll", "WriteFile", reinterpret_cast<void*>(&WriteFileDetour), import_present) ||
                !InstallOptionalHook(set_end_of_file_hook_, *main_module, "kernel32.dll", "SetEndOfFile", reinterpret_cast<void*>(&SetEndOfFileDetour), import_present)))
        {
            Remove();
            return false;
        }

        if (file_write_routing_ != nullptr)
        {
            if (!InstallOptionalHook(create_file_mapping_w_hook_, *main_module, "kernel32.dll", "CreateFileMappingW", reinterpret_cast<void*>(&CreateFileMappingWDetour), import_present) ||
                !InstallOptionalHook(delete_file_a_hook_, *main_module, "kernel32.dll", "DeleteFileA", reinterpret_cast<void*>(&DeleteFileADetour), import_present) ||
                !InstallOptionalHook(delete_file_w_hook_, *main_module, "kernel32.dll", "DeleteFileW", reinterpret_cast<void*>(&DeleteFileWDetour), import_present) ||
                !InstallOptionalHook(move_file_a_hook_, *main_module, "kernel32.dll", "MoveFileA", reinterpret_cast<void*>(&MoveFileADetour), import_present) ||
                !InstallOptionalHook(move_file_w_hook_, *main_module, "kernel32.dll", "MoveFileW", reinterpret_cast<void*>(&MoveFileWDetour), import_present) ||
                !InstallOptionalHook(move_file_ex_a_hook_, *main_module, "kernel32.dll", "MoveFileExA", reinterpret_cast<void*>(&MoveFileExADetour), import_present) ||
                !InstallOptionalHook(move_file_ex_w_hook_, *main_module, "kernel32.dll", "MoveFileExW", reinterpret_cast<void*>(&MoveFileExWDetour), import_present) ||
                !InstallOptionalHook(replace_file_a_hook_, *main_module, "kernel32.dll", "ReplaceFileA", reinterpret_cast<void*>(&ReplaceFileADetour), import_present) ||
                !InstallOptionalHook(replace_file_w_hook_, *main_module, "kernel32.dll", "ReplaceFileW", reinterpret_cast<void*>(&ReplaceFileWDetour), import_present) ||
                !InstallOptionalHook(copy_file_a_hook_, *main_module, "kernel32.dll", "CopyFileA", reinterpret_cast<void*>(&CopyFileADetour), import_present) ||
                !InstallOptionalHook(copy_file_w_hook_, *main_module, "kernel32.dll", "CopyFileW", reinterpret_cast<void*>(&CopyFileWDetour), import_present) ||
                !InstallOptionalHook(set_file_attributes_a_hook_, *main_module, "kernel32.dll", "SetFileAttributesA", reinterpret_cast<void*>(&SetFileAttributesADetour), import_present) ||
                !InstallOptionalHook(set_file_attributes_w_hook_, *main_module, "kernel32.dll", "SetFileAttributesW", reinterpret_cast<void*>(&SetFileAttributesWDetour), import_present) ||
                !InstallOptionalHook(create_directory_a_hook_, *main_module, "kernel32.dll", "CreateDirectoryA", reinterpret_cast<void*>(&CreateDirectoryADetour), import_present) ||
                !InstallOptionalHook(create_directory_w_hook_, *main_module, "kernel32.dll", "CreateDirectoryW", reinterpret_cast<void*>(&CreateDirectoryWDetour), import_present) ||
                !InstallOptionalHook(remove_directory_a_hook_, *main_module, "kernel32.dll", "RemoveDirectoryA", reinterpret_cast<void*>(&RemoveDirectoryADetour), import_present) ||
                !InstallOptionalHook(remove_directory_w_hook_, *main_module, "kernel32.dll", "RemoveDirectoryW", reinterpret_cast<void*>(&RemoveDirectoryWDetour), import_present) ||
                !InstallOptionalHook(duplicate_handle_hook_, *main_module, "kernel32.dll", "DuplicateHandle", reinterpret_cast<void*>(&DuplicateHandleDetour), import_present) ||
                !InstallOptionalHook(set_file_information_by_handle_hook_, *main_module, "kernel32.dll", "SetFileInformationByHandle", reinterpret_cast<void*>(&SetFileInformationByHandleDetour), import_present))
            {
                Remove();
                return false;
            }
        }

        if (!close_handle_hook_.Install(*main_module, "kernel32.dll", "CloseHandle", reinterpret_cast<void*>(&CloseHandleDetour)))
        {
            Remove();
            return false;
        }

        return true;
    }

    void FileApiHookSet::Remove()
    {
        close_handle_hook_.Remove();
        set_file_information_by_handle_hook_.Remove();
        duplicate_handle_hook_.Remove();
        remove_directory_w_hook_.Remove();
        remove_directory_a_hook_.Remove();
        create_directory_w_hook_.Remove();
        create_directory_a_hook_.Remove();
        set_file_attributes_w_hook_.Remove();
        set_file_attributes_a_hook_.Remove();
        copy_file_w_hook_.Remove();
        copy_file_a_hook_.Remove();
        replace_file_w_hook_.Remove();
        replace_file_a_hook_.Remove();
        move_file_ex_w_hook_.Remove();
        move_file_ex_a_hook_.Remove();
        move_file_w_hook_.Remove();
        move_file_a_hook_.Remove();
        delete_file_w_hook_.Remove();
        delete_file_a_hook_.Remove();
        create_file_mapping_w_hook_.Remove();
        create_file_mapping_a_hook_.Remove();
        set_end_of_file_hook_.Remove();
        write_file_hook_.Remove();
        get_file_attributes_a_hook_.Remove();
        get_file_attributes_w_hook_.Remove();
        get_file_size_ex_hook_.Remove();
        get_file_size_hook_.Remove();
        set_file_pointer_hook_.Remove();
        read_file_hook_.Remove();
        create_file_a_hook_.Remove();
        create_file_w_hook_.Remove();

        if (active_instance_ == this)
        {
            active_instance_ = nullptr;
        }
    }

    bool FileApiHookSet::IsInstalled() const noexcept
    {
        return active_instance_ == this
            && (create_file_w_hook_.IsInstalled() || create_file_a_hook_.IsInstalled())
            && read_file_hook_.IsInstalled()
            && set_file_pointer_hook_.IsInstalled()
            && (get_file_size_ex_hook_.IsInstalled() || get_file_size_hook_.IsInstalled())
            && close_handle_hook_.IsInstalled();
    }

    HANDLE WINAPI FileApiHookSet::CreateFileWDetour(
        LPCWSTR lpFileName,
        DWORD dwDesiredAccess,
        DWORD dwShareMode,
        LPSECURITY_ATTRIBUTES lpSecurityAttributes,
        DWORD dwCreationDisposition,
        DWORD dwFlagsAndAttributes,
        HANDLE hTemplateFile)
    {
        FileApiHookSet* const active = Current();
        if (active == nullptr)
        {
            SetLastError(ERROR_INVALID_HANDLE);
            return INVALID_HANDLE_VALUE;
        }

        std::filesystem::path normalized_path;
        bool has_normalized_path = false;
        if (lpFileName != nullptr)
        {
            const std::filesystem::path requested_path(lpFileName);
            normalized_path = requested_path;
            if (active->file_write_routing_ != nullptr)
            {
                has_normalized_path = active->file_write_routing_->TryNormalizeRequestPath(requested_path, normalized_path);
                const FileWriteRoutingService::PathDisposition disposition = active->file_write_routing_->ClassifyPath(normalized_path);
                if (disposition != FileWriteRoutingService::PathDisposition::Unrelated)
                {
                    return active->file_write_routing_->Open(
                        normalized_path, dwDesiredAccess, dwShareMode, lpSecurityAttributes,
                        dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile);
                }
            }

            const bool should_hide = active->hidden_path_matcher_.ShouldHidePath(requested_path);
            if (should_hide)
            {
                helen::Logf(L"[file] suppressed CreateFileW path=%ls", lpFileName);
                SetLastError(ERROR_FILE_NOT_FOUND);
                return INVALID_HANDLE_VALUE;
            }
        }

        if (!CanVirtualizeOpen(dwDesiredAccess, dwCreationDisposition, dwFlagsAndAttributes))
        {
            return CallRealCreateFileW(
                active->file_write_routing_ != nullptr && has_normalized_path ? normalized_path.c_str() : lpFileName,
                dwDesiredAccess,
                dwShareMode,
                lpSecurityAttributes,
                dwCreationDisposition,
                dwFlagsAndAttributes,
                hTemplateFile);
        }

        if (lpFileName != nullptr)
        {
            const std::optional<HANDLE> virtual_handle = active->virtual_files_.OpenVirtualFile(std::filesystem::path(lpFileName));
            if (virtual_handle.has_value())
            {
                return *virtual_handle;
            }
        }

        return active->file_write_routing_ != nullptr && has_normalized_path
            ? CallRealCreateFileW(
                normalized_path.c_str(),
                dwDesiredAccess,
                dwShareMode,
                lpSecurityAttributes,
                dwCreationDisposition,
                dwFlagsAndAttributes,
                hTemplateFile)
            : CallRealCreateFileW(
                lpFileName,
                dwDesiredAccess,
                dwShareMode,
                lpSecurityAttributes,
                dwCreationDisposition,
                dwFlagsAndAttributes,
                hTemplateFile);
    }

    HANDLE WINAPI FileApiHookSet::CreateFileADetour(
        LPCSTR lpFileName,
        DWORD dwDesiredAccess,
        DWORD dwShareMode,
        LPSECURITY_ATTRIBUTES lpSecurityAttributes,
        DWORD dwCreationDisposition,
        DWORD dwFlagsAndAttributes,
        HANDLE hTemplateFile)
    {
        FileApiHookSet* const active = Current();
        if (active == nullptr)
        {
            SetLastError(ERROR_INVALID_HANDLE);
            return INVALID_HANDLE_VALUE;
        }

        std::filesystem::path normalized_path;
        bool has_normalized_path = false;
        std::optional<std::string> normalized_ansi_path;
        if (lpFileName != nullptr)
        {
            const std::optional<std::filesystem::path> hidden_path = TryConvertAnsiPath(lpFileName);
            if (active->file_write_routing_ != nullptr && !hidden_path.has_value())
            {
                SetLastError(ERROR_NO_UNICODE_TRANSLATION);
                return INVALID_HANDLE_VALUE;
            }
            if (active->file_write_routing_ != nullptr && hidden_path.has_value())
            {
                normalized_path = *hidden_path;
                has_normalized_path = active->file_write_routing_->TryNormalizeRequestPath(*hidden_path, normalized_path);
                if (has_normalized_path)
                {
                    normalized_ansi_path = TryConvertWidePathToAnsi(normalized_path);
                }
                const FileWriteRoutingService::PathDisposition disposition = active->file_write_routing_->ClassifyPath(normalized_path);
                if (disposition != FileWriteRoutingService::PathDisposition::Unrelated)
                {
                    return active->file_write_routing_->Open(
                        normalized_path, dwDesiredAccess, dwShareMode, lpSecurityAttributes,
                        dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile);
                }
            }

            if (hidden_path.has_value())
            {
                const bool should_hide = active->hidden_path_matcher_.ShouldHidePath(*hidden_path);
                if (should_hide)
                {
                    helen::Logf(L"[file] suppressed CreateFileA path=%ls", hidden_path->c_str());
                    SetLastError(ERROR_FILE_NOT_FOUND);
                    return INVALID_HANDLE_VALUE;
                }
            }
        }

        if (!CanVirtualizeOpen(dwDesiredAccess, dwCreationDisposition, dwFlagsAndAttributes))
        {
            if (normalized_ansi_path.has_value())
            {
                return CallRealCreateFileA(
                    normalized_ansi_path->c_str(),
                    dwDesiredAccess,
                    dwShareMode,
                    lpSecurityAttributes,
                    dwCreationDisposition,
                    dwFlagsAndAttributes,
                    hTemplateFile);
            }

            return CallRealCreateFileA(
                lpFileName,
                dwDesiredAccess,
                dwShareMode,
                lpSecurityAttributes,
                dwCreationDisposition,
                dwFlagsAndAttributes,
                hTemplateFile);
        }

        if (lpFileName != nullptr)
        {
            const std::optional<std::filesystem::path> virtual_path = TryConvertAnsiPath(lpFileName);
            if (virtual_path.has_value())
            {
                const std::optional<HANDLE> virtual_handle = active->virtual_files_.OpenVirtualFile(*virtual_path);
                if (virtual_handle.has_value())
                {
                    return *virtual_handle;
                }
            }
        }

        if (normalized_ansi_path.has_value())
        {
            return CallRealCreateFileA(
                normalized_ansi_path->c_str(),
                dwDesiredAccess,
                dwShareMode,
                lpSecurityAttributes,
                dwCreationDisposition,
                dwFlagsAndAttributes,
                hTemplateFile);
        }

        return CallRealCreateFileA(
            lpFileName,
            dwDesiredAccess,
            dwShareMode,
            lpSecurityAttributes,
            dwCreationDisposition,
            dwFlagsAndAttributes,
            hTemplateFile);
    }

    DWORD WINAPI FileApiHookSet::GetFileAttributesWDetour(LPCWSTR lpFileName)
    {
        FileApiHookSet* const active = Current();
        if (active == nullptr)
        {
            SetLastError(ERROR_INVALID_HANDLE);
            return INVALID_FILE_ATTRIBUTES;
        }

        if (lpFileName != nullptr && active->hidden_path_matcher_.ShouldHidePath(std::filesystem::path(lpFileName)))
        {
            SetLastError(ERROR_FILE_NOT_FOUND);
            return INVALID_FILE_ATTRIBUTES;
        }

        std::filesystem::path normalized_path;
        bool has_normalized_path = false;
        if (active->file_write_routing_ != nullptr && lpFileName != nullptr)
        {
            const std::filesystem::path requested_path(lpFileName);
            normalized_path = requested_path;
            has_normalized_path = active->file_write_routing_->TryNormalizeRequestPath(requested_path, normalized_path);
            if (active->file_write_routing_->ClassifyPath(normalized_path) != FileWriteRoutingService::PathDisposition::Unrelated)
            {
                return active->file_write_routing_->GetAttributes(normalized_path);
            }
        }

        return CallRealGetFileAttributesW(
            active->file_write_routing_ != nullptr && has_normalized_path ? normalized_path.c_str() : lpFileName);
    }

    DWORD WINAPI FileApiHookSet::GetFileAttributesADetour(LPCSTR lpFileName)
    {
        FileApiHookSet* const active = Current();
        if (active == nullptr)
        {
            SetLastError(ERROR_INVALID_HANDLE);
            return INVALID_FILE_ATTRIBUTES;
        }

        std::filesystem::path normalized_path;
        bool has_normalized_path = false;
        std::optional<std::string> normalized_ansi_path;
        if (lpFileName != nullptr)
        {
            const std::optional<std::filesystem::path> hidden_path = TryConvertAnsiPath(lpFileName);
            if (active->file_write_routing_ != nullptr && !hidden_path.has_value())
            {
                SetLastError(ERROR_NO_UNICODE_TRANSLATION);
                return INVALID_FILE_ATTRIBUTES;
            }
            if (active->file_write_routing_ != nullptr && hidden_path.has_value())
            {
                normalized_path = *hidden_path;
                has_normalized_path = active->file_write_routing_->TryNormalizeRequestPath(*hidden_path, normalized_path);
                if (has_normalized_path)
                {
                    normalized_ansi_path = TryConvertWidePathToAnsi(normalized_path);
                }
                if (active->file_write_routing_->ClassifyPath(normalized_path) != FileWriteRoutingService::PathDisposition::Unrelated)
                {
                    return active->file_write_routing_->GetAttributes(normalized_path);
                }
            }
            if (hidden_path.has_value() && active->hidden_path_matcher_.ShouldHidePath(*hidden_path))
            {
                SetLastError(ERROR_FILE_NOT_FOUND);
                return INVALID_FILE_ATTRIBUTES;
            }
        }

        return CallRealGetFileAttributesA(
            normalized_ansi_path.has_value() ? normalized_ansi_path->c_str() : lpFileName);
    }

    BOOL WINAPI FileApiHookSet::ReadFileDetour(
        HANDLE hFile,
        LPVOID lpBuffer,
        DWORD nNumberOfBytesToRead,
        LPDWORD lpNumberOfBytesRead,
        LPOVERLAPPED lpOverlapped)
    {
        FileApiHookSet* const active = Current();
        if (active == nullptr)
        {
            SetLastError(ERROR_INVALID_HANDLE);
            return FALSE;
        }

        if (active->virtual_files_.IsVirtualHandle(hFile))
        {
            return active->virtual_files_.Read(hFile, lpBuffer, nNumberOfBytesToRead, lpNumberOfBytesRead);
        }

        return CallRealReadFile(
            hFile,
            lpBuffer,
            nNumberOfBytesToRead,
            lpNumberOfBytesRead,
            lpOverlapped);
    }

    BOOL WINAPI FileApiHookSet::WriteFileDetour(
        HANDLE hFile,
        LPCVOID lpBuffer,
        DWORD nNumberOfBytesToWrite,
        LPDWORD lpNumberOfBytesWritten,
        LPOVERLAPPED lpOverlapped)
    {
        FileApiHookSet* const active = Current();
        if (active == nullptr)
        {
            SetLastError(ERROR_INVALID_HANDLE);
            return FALSE;
        }

        if (active->file_write_routing_ != nullptr &&
            !active->file_write_routing_->IsTrackedHandle(hFile) &&
            active->file_write_routing_->ClassifyHandle(hFile) != FileWriteRoutingService::PathDisposition::Unrelated)
        {
            SetLastError(ERROR_ACCESS_DENIED);
            return FALSE;
        }

        return CallRealWriteFile(hFile, lpBuffer, nNumberOfBytesToWrite, lpNumberOfBytesWritten, lpOverlapped);
    }

    BOOL WINAPI FileApiHookSet::SetEndOfFileDetour(HANDLE hFile)
    {
        FileApiHookSet* const active = Current();
        if (active == nullptr)
        {
            SetLastError(ERROR_INVALID_HANDLE);
            return FALSE;
        }

        if (active->file_write_routing_ != nullptr &&
            !active->file_write_routing_->IsTrackedHandle(hFile) &&
            active->file_write_routing_->ClassifyHandle(hFile) != FileWriteRoutingService::PathDisposition::Unrelated)
        {
            SetLastError(ERROR_ACCESS_DENIED);
            return FALSE;
        }

        return CallRealSetEndOfFile(hFile);
    }

    BOOL WINAPI FileApiHookSet::SetFilePointerExDetour(
        HANDLE hFile,
        LARGE_INTEGER liDistanceToMove,
        PLARGE_INTEGER lpNewFilePointer,
        DWORD dwMoveMethod)
    {
        FileApiHookSet* const active = Current();
        if (active == nullptr)
        {
            SetLastError(ERROR_INVALID_HANDLE);
            return FALSE;
        }

        if (active->virtual_files_.IsVirtualHandle(hFile))
        {
            return active->virtual_files_.Seek(hFile, liDistanceToMove, dwMoveMethod, lpNewFilePointer);
        }

        return CallRealSetFilePointerEx(hFile, liDistanceToMove, lpNewFilePointer, dwMoveMethod);
    }

    DWORD WINAPI FileApiHookSet::SetFilePointerDetour(
        HANDLE hFile,
        LONG lDistanceToMove,
        PLONG lpDistanceToMoveHigh,
        DWORD dwMoveMethod)
    {
        FileApiHookSet* const active = Current();
        if (active == nullptr)
        {
            SetLastError(ERROR_INVALID_HANDLE);
            return INVALID_SET_FILE_POINTER;
        }

        if (active->virtual_files_.IsVirtualHandle(hFile))
        {
            LARGE_INTEGER distance_to_move{};
            if (!TryBuildLegacySeekDistance(lDistanceToMove, lpDistanceToMoveHigh, distance_to_move))
            {
                SetLastError(ERROR_INVALID_PARAMETER);
                return INVALID_SET_FILE_POINTER;
            }

            LARGE_INTEGER new_file_pointer{};
            if (!active->virtual_files_.Seek(hFile, distance_to_move, dwMoveMethod, &new_file_pointer))
            {
                return INVALID_SET_FILE_POINTER;
            }

            SetLastError(ERROR_SUCCESS);
            return EncodeLegacySeekResult(new_file_pointer, lpDistanceToMoveHigh);
        }

        return CallRealSetFilePointer(hFile, lDistanceToMove, lpDistanceToMoveHigh, dwMoveMethod);
    }

    BOOL WINAPI FileApiHookSet::GetFileSizeExDetour(HANDLE hFile, PLARGE_INTEGER lpFileSize)
    {
        FileApiHookSet* const active = Current();
        if (active == nullptr)
        {
            SetLastError(ERROR_INVALID_HANDLE);
            return FALSE;
        }

        if (active->virtual_files_.IsVirtualHandle(hFile))
        {
            return active->virtual_files_.GetSize(hFile, lpFileSize);
        }

        return CallRealGetFileSizeEx(hFile, lpFileSize);
    }

    DWORD WINAPI FileApiHookSet::GetFileSizeDetour(HANDLE hFile, LPDWORD lpFileSizeHigh)
    {
        FileApiHookSet* const active = Current();
        if (active == nullptr)
        {
            SetLastError(ERROR_INVALID_HANDLE);
            return INVALID_FILE_SIZE;
        }

        if (active->virtual_files_.IsVirtualHandle(hFile))
        {
            LARGE_INTEGER file_size{};
            if (!active->virtual_files_.GetSize(hFile, &file_size))
            {
                return INVALID_FILE_SIZE;
            }

            if (lpFileSizeHigh != nullptr)
            {
                *lpFileSizeHigh = file_size.HighPart;
            }

            SetLastError(ERROR_SUCCESS);
            return file_size.LowPart;
        }

        return CallRealGetFileSize(hFile, lpFileSizeHigh);
    }

    HANDLE WINAPI FileApiHookSet::CreateFileMappingADetour(
        HANDLE hFile,
        LPSECURITY_ATTRIBUTES lpFileMappingAttributes,
        DWORD flProtect,
        DWORD dwMaximumSizeHigh,
        DWORD dwMaximumSizeLow,
        LPCSTR lpName)
    {
        static_cast<void>(lpFileMappingAttributes);
        static_cast<void>(lpName);

        FileApiHookSet* const active = Current();
        if (active == nullptr)
        {
            SetLastError(ERROR_INVALID_HANDLE);
            return nullptr;
        }

        if (active->virtual_files_.IsVirtualHandle(hFile))
        {
            const std::optional<HANDLE> mapping_handle = active->virtual_files_.CreateFileMapping(
                hFile,
                flProtect,
                dwMaximumSizeHigh,
                dwMaximumSizeLow);
            if (mapping_handle.has_value())
            {
                return *mapping_handle;
            }

            return nullptr;
        }

        if (active->file_write_routing_ != nullptr &&
            (active->file_write_routing_->IsTrackedHandle(hFile) || active->file_write_routing_->ClassifyHandle(hFile) != FileWriteRoutingService::PathDisposition::Unrelated) &&
            (lpName != nullptr || RequestsWritableMapping(flProtect)))
        {
            SetLastError(ERROR_ACCESS_DENIED);
            return nullptr;
        }

        return CallRealCreateFileMappingA(
            hFile,
            lpFileMappingAttributes,
            flProtect,
            dwMaximumSizeHigh,
            dwMaximumSizeLow,
            lpName);
    }

    HANDLE WINAPI FileApiHookSet::CreateFileMappingWDetour(
        HANDLE hFile,
        LPSECURITY_ATTRIBUTES lpFileMappingAttributes,
        DWORD flProtect,
        DWORD dwMaximumSizeHigh,
        DWORD dwMaximumSizeLow,
        LPCWSTR lpName)
    {
        FileApiHookSet* const active = Current();
        if (active == nullptr)
        {
            SetLastError(ERROR_INVALID_HANDLE);
            return nullptr;
        }

        if (active->file_write_routing_ != nullptr &&
            (active->file_write_routing_->IsTrackedHandle(hFile) || active->file_write_routing_->ClassifyHandle(hFile) != FileWriteRoutingService::PathDisposition::Unrelated) &&
            (lpName != nullptr || RequestsWritableMapping(flProtect)))
        {
            SetLastError(ERROR_ACCESS_DENIED);
            return nullptr;
        }

        return CallRealCreateFileMappingW(hFile, lpFileMappingAttributes, flProtect,
            dwMaximumSizeHigh, dwMaximumSizeLow, lpName);
    }

    BOOL WINAPI FileApiHookSet::DeleteFileWDetour(LPCWSTR lpFileName)
    {
        FileApiHookSet* const active = Current();
        if (active == nullptr) { SetLastError(ERROR_INVALID_HANDLE); return FALSE; }
        if (active->file_write_routing_ != nullptr && lpFileName != nullptr)
        {
            return active->file_write_routing_->Delete(std::filesystem::path(lpFileName));
        }
        return CallRealDeleteFileW(lpFileName);
    }

    BOOL WINAPI FileApiHookSet::DeleteFileADetour(LPCSTR lpFileName)
    {
        FileApiHookSet* const active = Current();
        if (active == nullptr) { SetLastError(ERROR_INVALID_HANDLE); return FALSE; }
        const std::optional<std::filesystem::path> path = TryConvertAnsiPath(lpFileName);
        if (active->file_write_routing_ != nullptr && lpFileName != nullptr && !path.has_value())
        {
            SetLastError(ERROR_NO_UNICODE_TRANSLATION);
            return FALSE;
        }
        if (active->file_write_routing_ != nullptr && path.has_value())
        {
            return active->file_write_routing_->Delete(*path);
        }
        return CallRealDeleteFileA(lpFileName);
    }

    BOOL WINAPI FileApiHookSet::MoveFileExWDetour(LPCWSTR lpExistingFileName, LPCWSTR lpNewFileName, DWORD dwFlags)
    {
        FileApiHookSet* const active = Current();
        if (active == nullptr) { SetLastError(ERROR_INVALID_HANDLE); return FALSE; }
        if (active->file_write_routing_ != nullptr && lpExistingFileName != nullptr && lpNewFileName != nullptr)
        {
            return active->file_write_routing_->Move(std::filesystem::path(lpExistingFileName), std::filesystem::path(lpNewFileName), dwFlags);
        }
        return CallRealMoveFileExW(lpExistingFileName, lpNewFileName, dwFlags);
    }

    BOOL WINAPI FileApiHookSet::MoveFileExADetour(LPCSTR lpExistingFileName, LPCSTR lpNewFileName, DWORD dwFlags)
    {
        FileApiHookSet* const active = Current();
        if (active == nullptr) { SetLastError(ERROR_INVALID_HANDLE); return FALSE; }
        const std::optional<std::filesystem::path> existing_path = TryConvertAnsiPath(lpExistingFileName);
        const std::optional<std::filesystem::path> new_path = TryConvertAnsiPath(lpNewFileName);
        if (active->file_write_routing_ != nullptr &&
            ((lpExistingFileName != nullptr && !existing_path.has_value()) || (lpNewFileName != nullptr && !new_path.has_value())))
        {
            SetLastError(ERROR_NO_UNICODE_TRANSLATION);
            return FALSE;
        }
        if (active->file_write_routing_ != nullptr && existing_path.has_value() && new_path.has_value())
        {
            return active->file_write_routing_->Move(*existing_path, *new_path, dwFlags);
        }
        return CallRealMoveFileExA(lpExistingFileName, lpNewFileName, dwFlags);
    }

    BOOL WINAPI FileApiHookSet::MoveFileWDetour(LPCWSTR lpExistingFileName, LPCWSTR lpNewFileName)
    {
        return MoveFileExWDetour(lpExistingFileName, lpNewFileName, 0);
    }

    BOOL WINAPI FileApiHookSet::MoveFileADetour(LPCSTR lpExistingFileName, LPCSTR lpNewFileName)
    {
        return MoveFileExADetour(lpExistingFileName, lpNewFileName, 0);
    }

    BOOL WINAPI FileApiHookSet::ReplaceFileWDetour(LPCWSTR lpReplacedFileName, LPCWSTR lpReplacementFileName,
        LPCWSTR lpBackupFileName, DWORD dwReplaceFlags, LPVOID lpExclude, LPVOID lpReserved)
    {
        FileApiHookSet* const active = Current();
        if (active == nullptr) { SetLastError(ERROR_INVALID_HANDLE); return FALSE; }
        if (active->file_write_routing_ != nullptr && lpReplacedFileName != nullptr && lpReplacementFileName != nullptr)
        {
            return active->file_write_routing_->Replace(std::filesystem::path(lpReplacedFileName),
                std::filesystem::path(lpReplacementFileName), lpBackupFileName, dwReplaceFlags, lpExclude, lpReserved);
        }
        return CallRealReplaceFileW(lpReplacedFileName, lpReplacementFileName, lpBackupFileName,
            dwReplaceFlags, lpExclude, lpReserved);
    }

    BOOL WINAPI FileApiHookSet::ReplaceFileADetour(LPCSTR lpReplacedFileName, LPCSTR lpReplacementFileName,
        LPCSTR lpBackupFileName, DWORD dwReplaceFlags, LPVOID lpExclude, LPVOID lpReserved)
    {
        FileApiHookSet* const active = Current();
        if (active == nullptr) { SetLastError(ERROR_INVALID_HANDLE); return FALSE; }
        const std::optional<std::filesystem::path> replaced_path = TryConvertAnsiPath(lpReplacedFileName);
        const std::optional<std::filesystem::path> replacement_path = TryConvertAnsiPath(lpReplacementFileName);
        const std::optional<std::filesystem::path> backup_path = lpBackupFileName == nullptr
            ? std::optional<std::filesystem::path>() : TryConvertAnsiPath(lpBackupFileName);
        if (active->file_write_routing_ != nullptr &&
            ((lpReplacedFileName != nullptr && !replaced_path.has_value()) ||
                (lpReplacementFileName != nullptr && !replacement_path.has_value()) ||
                (lpBackupFileName != nullptr && !backup_path.has_value())))
        {
            SetLastError(ERROR_NO_UNICODE_TRANSLATION);
            return FALSE;
        }
        if (active->file_write_routing_ != nullptr && replaced_path.has_value() && replacement_path.has_value() &&
            (lpBackupFileName == nullptr || backup_path.has_value()))
        {
            return active->file_write_routing_->Replace(*replaced_path, *replacement_path,
                backup_path.has_value() ? backup_path->wstring().c_str() : nullptr, dwReplaceFlags, lpExclude, lpReserved);
        }
        return CallRealReplaceFileA(lpReplacedFileName, lpReplacementFileName, lpBackupFileName,
            dwReplaceFlags, lpExclude, lpReserved);
    }

    BOOL WINAPI FileApiHookSet::CopyFileWDetour(LPCWSTR lpExistingFileName, LPCWSTR lpNewFileName, BOOL bFailIfExists)
    {
        FileApiHookSet* const active = Current();
        if (active == nullptr) { SetLastError(ERROR_INVALID_HANDLE); return FALSE; }
        if (active->file_write_routing_ != nullptr && lpExistingFileName != nullptr && lpNewFileName != nullptr)
        {
            return active->file_write_routing_->Copy(std::filesystem::path(lpExistingFileName),
                std::filesystem::path(lpNewFileName), bFailIfExists);
        }
        return CallRealCopyFileW(lpExistingFileName, lpNewFileName, bFailIfExists);
    }

    BOOL WINAPI FileApiHookSet::CopyFileADetour(LPCSTR lpExistingFileName, LPCSTR lpNewFileName, BOOL bFailIfExists)
    {
        FileApiHookSet* const active = Current();
        if (active == nullptr) { SetLastError(ERROR_INVALID_HANDLE); return FALSE; }
        const std::optional<std::filesystem::path> existing_path = TryConvertAnsiPath(lpExistingFileName);
        const std::optional<std::filesystem::path> new_path = TryConvertAnsiPath(lpNewFileName);
        if (active->file_write_routing_ != nullptr &&
            ((lpExistingFileName != nullptr && !existing_path.has_value()) || (lpNewFileName != nullptr && !new_path.has_value())))
        {
            SetLastError(ERROR_NO_UNICODE_TRANSLATION);
            return FALSE;
        }
        if (active->file_write_routing_ != nullptr && existing_path.has_value() && new_path.has_value())
        {
            return active->file_write_routing_->Copy(*existing_path, *new_path, bFailIfExists);
        }
        return CallRealCopyFileA(lpExistingFileName, lpNewFileName, bFailIfExists);
    }

    BOOL WINAPI FileApiHookSet::SetFileAttributesWDetour(LPCWSTR lpFileName, DWORD dwFileAttributes)
    {
        FileApiHookSet* const active = Current();
        if (active == nullptr) { SetLastError(ERROR_INVALID_HANDLE); return FALSE; }
        if (active->file_write_routing_ != nullptr && lpFileName != nullptr)
        {
            return active->file_write_routing_->SetAttributes(std::filesystem::path(lpFileName), dwFileAttributes);
        }
        return CallRealSetFileAttributesW(lpFileName, dwFileAttributes);
    }

    BOOL WINAPI FileApiHookSet::SetFileAttributesADetour(LPCSTR lpFileName, DWORD dwFileAttributes)
    {
        FileApiHookSet* const active = Current();
        if (active == nullptr) { SetLastError(ERROR_INVALID_HANDLE); return FALSE; }
        const std::optional<std::filesystem::path> path = TryConvertAnsiPath(lpFileName);
        if (active->file_write_routing_ != nullptr && lpFileName != nullptr && !path.has_value())
        {
            SetLastError(ERROR_NO_UNICODE_TRANSLATION);
            return FALSE;
        }
        if (active->file_write_routing_ != nullptr && path.has_value())
        {
            return active->file_write_routing_->SetAttributes(*path, dwFileAttributes);
        }
        return CallRealSetFileAttributesA(lpFileName, dwFileAttributes);
    }

    BOOL WINAPI FileApiHookSet::CreateDirectoryWDetour(LPCWSTR lpPathName, LPSECURITY_ATTRIBUTES lpSecurityAttributes)
    {
        FileApiHookSet* const active = Current();
        if (active == nullptr) { SetLastError(ERROR_INVALID_HANDLE); return FALSE; }
        if (active->file_write_routing_ != nullptr && lpPathName != nullptr &&
            active->file_write_routing_->IsProtectedParentPath(std::filesystem::path(lpPathName)))
        {
            SetLastError(ERROR_ACCESS_DENIED);
            return FALSE;
        }
        return CallRealCreateDirectoryW(lpPathName, lpSecurityAttributes);
    }

    BOOL WINAPI FileApiHookSet::CreateDirectoryADetour(LPCSTR lpPathName, LPSECURITY_ATTRIBUTES lpSecurityAttributes)
    {
        FileApiHookSet* const active = Current();
        if (active == nullptr) { SetLastError(ERROR_INVALID_HANDLE); return FALSE; }
        const std::optional<std::filesystem::path> path = TryConvertAnsiPath(lpPathName);
        if (active->file_write_routing_ != nullptr && lpPathName != nullptr && !path.has_value())
        {
            SetLastError(ERROR_NO_UNICODE_TRANSLATION);
            return FALSE;
        }
        if (active->file_write_routing_ != nullptr && path.has_value() && active->file_write_routing_->IsProtectedParentPath(*path))
        {
            SetLastError(ERROR_ACCESS_DENIED);
            return FALSE;
        }
        return CallRealCreateDirectoryA(lpPathName, lpSecurityAttributes);
    }

    BOOL WINAPI FileApiHookSet::RemoveDirectoryWDetour(LPCWSTR lpPathName)
    {
        FileApiHookSet* const active = Current();
        if (active == nullptr) { SetLastError(ERROR_INVALID_HANDLE); return FALSE; }
        if (active->file_write_routing_ != nullptr && lpPathName != nullptr && active->file_write_routing_->IsProtectedParentPath(std::filesystem::path(lpPathName)))
        {
            SetLastError(ERROR_ACCESS_DENIED);
            return FALSE;
        }
        return CallRealRemoveDirectoryW(lpPathName);
    }

    BOOL WINAPI FileApiHookSet::RemoveDirectoryADetour(LPCSTR lpPathName)
    {
        FileApiHookSet* const active = Current();
        if (active == nullptr) { SetLastError(ERROR_INVALID_HANDLE); return FALSE; }
        const std::optional<std::filesystem::path> path = TryConvertAnsiPath(lpPathName);
        if (active->file_write_routing_ != nullptr && lpPathName != nullptr && !path.has_value())
        {
            SetLastError(ERROR_NO_UNICODE_TRANSLATION);
            return FALSE;
        }
        if (active->file_write_routing_ != nullptr && path.has_value() && active->file_write_routing_->IsProtectedParentPath(*path))
        {
            SetLastError(ERROR_ACCESS_DENIED);
            return FALSE;
        }
        return CallRealRemoveDirectoryA(lpPathName);
    }

    BOOL WINAPI FileApiHookSet::DuplicateHandleDetour(HANDLE hSourceProcessHandle, HANDLE hSourceHandle,
        HANDLE hTargetProcessHandle, LPHANDLE lpTargetHandle, DWORD dwDesiredAccess, BOOL bInheritHandle, DWORD dwOptions)
    {
        FileApiHookSet* const active = Current();
        if (active == nullptr) { SetLastError(ERROR_INVALID_HANDLE); return FALSE; }
        if (active->file_write_routing_ != nullptr && IsCurrentProcessHandle(hSourceProcessHandle) &&
            (active->file_write_routing_->IsTrackedHandle(hSourceHandle) || active->file_write_routing_->ClassifyHandle(hSourceHandle) != FileWriteRoutingService::PathDisposition::Unrelated))
        {
            SetLastError(ERROR_ACCESS_DENIED);
            return FALSE;
        }
        return CallRealDuplicateHandle(hSourceProcessHandle, hSourceHandle, hTargetProcessHandle,
            lpTargetHandle, dwDesiredAccess, bInheritHandle, dwOptions);
    }

    BOOL WINAPI FileApiHookSet::SetFileInformationByHandleDetour(HANDLE hFile, FILE_INFO_BY_HANDLE_CLASS FileInformationClass,
        LPVOID lpFileInformation, DWORD dwBufferSize)
    {
        FileApiHookSet* const active = Current();
        if (active == nullptr) { SetLastError(ERROR_INVALID_HANDLE); return FALSE; }
        if (active->file_write_routing_ != nullptr &&
            (active->file_write_routing_->IsTrackedHandle(hFile) || active->file_write_routing_->ClassifyHandle(hFile) != FileWriteRoutingService::PathDisposition::Unrelated))
        {
            SetLastError(ERROR_ACCESS_DENIED);
            return FALSE;
        }
        return CallRealSetFileInformationByHandle(hFile, FileInformationClass, lpFileInformation, dwBufferSize);
    }

    BOOL WINAPI FileApiHookSet::CloseHandleDetour(HANDLE hObject)
    {
        FileApiHookSet* const active = Current();
        if (active == nullptr)
        {
            SetLastError(ERROR_INVALID_HANDLE);
            return FALSE;
        }

        if (active->file_write_routing_ != nullptr && active->file_write_routing_->IsTrackedHandle(hObject))
        {
            return active->file_write_routing_->Close(hObject);
        }

        if (active->virtual_files_.IsVirtualHandle(hObject))
        {
            return active->virtual_files_.Close(hObject);
        }

        return CallRealCloseHandle(hObject);
    }
}
