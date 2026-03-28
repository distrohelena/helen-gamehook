#pragma once

#include <windows.h>

#include <HelenHook/Hook.h>
#include <HelenHook/VirtualFileService.h>

namespace helen
{
    /**
     * @brief Installs process-local IAT hooks that redirect file opens into the virtual file service.
     *
     * The hook set only owns the main executable import table entries. Non-virtual paths and handles
     * still flow through the original Win32 APIs unchanged.
     */
    class FileApiHookSet
    {
    public:
        /**
         * @brief Returns true when the supplied CreateFileW request is compatible with replace-on-read virtualization.
         * @param dwDesiredAccess Requested open access mask.
         * @param dwCreationDisposition Requested creation disposition.
         * @param dwFlagsAndAttributes Requested file flags and attributes.
         * @return True when the request is read-only and uses OPEN_EXISTING.
         */
        static bool CanVirtualizeOpen(DWORD dwDesiredAccess, DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes) noexcept;

        /**
         * @brief Binds the hook set to the virtual file service that should answer matching file requests.
         * @param virtual_files Service that owns the RAM-backed replacement payloads.
         */
        explicit FileApiHookSet(VirtualFileService& virtual_files);

        /**
         * @brief Releases any installed IAT hooks.
         */
        ~FileApiHookSet();

        FileApiHookSet(const FileApiHookSet&) = delete;
        FileApiHookSet& operator=(const FileApiHookSet&) = delete;
        FileApiHookSet(FileApiHookSet&&) = delete;
        FileApiHookSet& operator=(FileApiHookSet&&) = delete;

        /**
         * @brief Installs CreateFileW, ReadFile, one compatible seek import, GetFileSizeEx, and CloseHandle hooks on the main executable.
         * @return True when every required import hook was installed successfully, using SetFilePointerEx when it is present or falling back to SetFilePointer on older import tables.
         */
        bool Install();

        /**
         * @brief Removes every installed hook and restores the original import table entries.
         */
        void Remove();

        /**
         * @brief Returns true when every required import hook is currently installed.
         * @return True when the hook set is active.
         */
        bool IsInstalled() const noexcept;

    private:
        /**
         * @brief IAT detour for CreateFileW that opens matching paths through the virtual file service.
         */
        static HANDLE WINAPI CreateFileWDetour(
            LPCWSTR lpFileName,
            DWORD dwDesiredAccess,
            DWORD dwShareMode,
            LPSECURITY_ATTRIBUTES lpSecurityAttributes,
            DWORD dwCreationDisposition,
            DWORD dwFlagsAndAttributes,
            HANDLE hTemplateFile);

        /**
         * @brief IAT detour for CreateFileA that opens matching ANSI paths through the virtual file service.
         */
        static HANDLE WINAPI CreateFileADetour(
            LPCSTR lpFileName,
            DWORD dwDesiredAccess,
            DWORD dwShareMode,
            LPSECURITY_ATTRIBUTES lpSecurityAttributes,
            DWORD dwCreationDisposition,
            DWORD dwFlagsAndAttributes,
            HANDLE hTemplateFile);

        /**
         * @brief IAT detour for ReadFile that serves bytes from synthetic virtual handles.
         */
        static BOOL WINAPI ReadFileDetour(
            HANDLE hFile,
            LPVOID lpBuffer,
            DWORD nNumberOfBytesToRead,
            LPDWORD lpNumberOfBytesRead,
            LPOVERLAPPED lpOverlapped);

        /**
         * @brief IAT detour for SetFilePointerEx that moves the read cursor for synthetic virtual handles.
         */
        static BOOL WINAPI SetFilePointerExDetour(
            HANDLE hFile,
            LARGE_INTEGER liDistanceToMove,
            PLARGE_INTEGER lpNewFilePointer,
            DWORD dwMoveMethod);

        /**
         * @brief IAT detour for SetFilePointer that moves the read cursor for synthetic virtual handles on builds that import the legacy seek API.
         */
        static DWORD WINAPI SetFilePointerDetour(
            HANDLE hFile,
            LONG lDistanceToMove,
            PLONG lpDistanceToMoveHigh,
            DWORD dwMoveMethod);

        /**
         * @brief IAT detour for GetFileSizeEx that reports the in-memory payload size for synthetic virtual handles.
         */
        static BOOL WINAPI GetFileSizeExDetour(HANDLE hFile, PLARGE_INTEGER lpFileSize);

        /**
         * @brief IAT detour for GetFileSize that reports the in-memory payload size for synthetic virtual handles on builds that import the legacy size API.
         */
        static DWORD WINAPI GetFileSizeDetour(HANDLE hFile, LPDWORD lpFileSizeHigh);

        /**
         * @brief IAT detour for CreateFileMappingA that creates synthetic read-only mappings for virtual files.
         */
        static HANDLE WINAPI CreateFileMappingADetour(
            HANDLE hFile,
            LPSECURITY_ATTRIBUTES lpFileMappingAttributes,
            DWORD flProtect,
            DWORD dwMaximumSizeHigh,
            DWORD dwMaximumSizeLow,
            LPCSTR lpName);

        /**
         * @brief IAT detour for CloseHandle that releases synthetic virtual handles only.
         */
        static BOOL WINAPI CloseHandleDetour(HANDLE hObject);

        /**
         * @brief Returns the active hook set instance used by the static detours.
         * @return Current active hook set or nullptr when no instance has been installed.
         */
        static FileApiHookSet* Current();

        /** @brief Virtual file service that supplies matching replacement payloads. */
        VirtualFileService& virtual_files_;

        /** @brief IAT hook used to replace CreateFileW in the main executable imports when that import is present. */
        IatHook create_file_w_hook_;

        /** @brief IAT hook used to replace CreateFileA in the main executable imports when that import is present. */
        IatHook create_file_a_hook_;

        /** @brief IAT hook used to replace ReadFile in the main executable imports. */
        IatHook read_file_hook_;

        /** @brief IAT hook used to replace either SetFilePointerEx or SetFilePointer in the main executable imports. */
        IatHook set_file_pointer_hook_;

        /** @brief IAT hook used to replace GetFileSizeEx in the main executable imports when that import is present. */
        IatHook get_file_size_ex_hook_;

        /** @brief IAT hook used to replace GetFileSize in the main executable imports when that import is present. */
        IatHook get_file_size_hook_;

        /** @brief IAT hook used to replace CreateFileMappingA in the main executable imports when that import is present. */
        IatHook create_file_mapping_a_hook_;

        /** @brief IAT hook used to replace CloseHandle in the main executable imports. */
        IatHook close_handle_hook_;

        /** @brief Singleton-style active hook set used by the static detour functions. */
        static FileApiHookSet* active_instance_;
    };
}
