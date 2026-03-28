#include <filesystem>
#include <limits>
#include <optional>
#include <string>

#include <HelenHook/FileApiHookSet.h>

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

        const int required_length = MultiByteToWideChar(CP_ACP, MB_ERR_INVALID_CHARS, path, -1, nullptr, 0);
        if (required_length <= 0)
        {
            return std::nullopt;
        }

        std::wstring wide_path(static_cast<std::size_t>(required_length - 1), L'\0');
        const int actual_length = MultiByteToWideChar(CP_ACP, MB_ERR_INVALID_CHARS, path, -1, wide_path.data(), required_length);
        if (actual_length != required_length)
        {
            return std::nullopt;
        }

        return std::filesystem::path(wide_path);
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

    FileApiHookSet::FileApiHookSet(VirtualFileService& virtual_files)
        : virtual_files_(virtual_files)
    {
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
        create_file_mapping_a_hook_.Remove();
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

        if (!CanVirtualizeOpen(dwDesiredAccess, dwCreationDisposition, dwFlagsAndAttributes))
        {
            return CallRealCreateFileW(
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
            const std::optional<HANDLE> virtual_handle = active->virtual_files_.OpenVirtualFile(std::filesystem::path(lpFileName));
            if (virtual_handle.has_value())
            {
                return *virtual_handle;
            }
        }

        return CallRealCreateFileW(
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

        if (!CanVirtualizeOpen(dwDesiredAccess, dwCreationDisposition, dwFlagsAndAttributes))
        {
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

        return CallRealCreateFileA(
            lpFileName,
            dwDesiredAccess,
            dwShareMode,
            lpSecurityAttributes,
            dwCreationDisposition,
            dwFlagsAndAttributes,
            hTemplateFile);
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

        return CallRealCreateFileMappingA(
            hFile,
            lpFileMappingAttributes,
            flProtect,
            dwMaximumSizeHigh,
            dwMaximumSizeLow,
            lpName);
    }

    BOOL WINAPI FileApiHookSet::CloseHandleDetour(HANDLE hObject)
    {
        FileApiHookSet* const active = Current();
        if (active == nullptr)
        {
            SetLastError(ERROR_INVALID_HANDLE);
            return FALSE;
        }

        if (active->virtual_files_.IsVirtualHandle(hObject))
        {
            return active->virtual_files_.Close(hObject);
        }

        return CallRealCloseHandle(hObject);
    }
}