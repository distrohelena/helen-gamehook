#pragma once

#include <filesystem>
#include <vector>

#include <windows.h>

#include <HelenHook/FileWriteRoutingService.h>
#include <HelenHook/HiddenPathMatcher.h>
#include <HelenHook/Hook.h>
#include <HelenHook/VirtualFileService.h>

namespace helen {
/**
 * @brief Installs process-local IAT hooks that redirect file opens into the virtual file service.
 *
 * The hook set only owns the main executable import table entries. Non-virtual paths and handles
 * still flow through the original Win32 APIs unchanged.
 */
class FileApiHookSet {
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
     * @brief Binds the hook set to the virtual file service and the active hidden-path declarations.
     * @param virtual_files Service that owns the RAM-backed replacement payloads.
     * @param game_installation_root Absolute game installation root used to relativize hidden paths.
     * @param request_base_directory Absolute directory used to resolve relative file requests.
     * @param hidden_paths Canonical relative paths that should be reported as missing.
     */
    FileApiHookSet(VirtualFileService &virtual_files, std::filesystem::path game_installation_root,
                   std::filesystem::path request_base_directory, std::vector<std::string> hidden_paths);

    /**
     * @brief Binds the hook set to virtual files and one initialized session write-routing owner.
     * @param virtual_files Service that supplies matching replacement payloads.
     * @param file_write_routing Service that owns protected paths, handles, and mutation serialization.
     * @param game_installation_root Absolute game installation root used to relativize hidden paths.
     * @param request_base_directory Absolute directory used to resolve relative file requests.
     * @param hidden_paths Canonical relative paths that should be reported as missing.
     */
    FileApiHookSet(VirtualFileService &virtual_files, FileWriteRoutingService &file_write_routing,
                   std::filesystem::path game_installation_root, std::filesystem::path request_base_directory,
                   std::vector<std::string> hidden_paths);

    /**
     * @brief Releases any installed IAT hooks.
     */
    ~FileApiHookSet();

    FileApiHookSet(const FileApiHookSet &) = delete;
    FileApiHookSet &operator=(const FileApiHookSet &) = delete;
    FileApiHookSet(FileApiHookSet &&) = delete;
    FileApiHookSet &operator=(FileApiHookSet &&) = delete;

    /**
     * @brief Installs CreateFileW, ReadFile, one compatible seek import, GetFileSizeEx, and CloseHandle hooks on the main executable.
     * @return True when every required import hook was installed successfully, using SetFilePointerEx when it is present or falling back to
     * SetFilePointer on older import tables.
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
    /** @brief Virtual file service that supplies matching replacement payloads. */
    VirtualFileService &virtual_files_;

    /** @brief Hidden-path matcher that decides which file requests should be reported as missing. */
    HiddenPathMatcher hidden_path_matcher_;

    /** @brief Optional service that owns protected write routing; null retains legacy behavior. */
    FileWriteRoutingService *file_write_routing_ = nullptr;

    /** @brief IAT hook used to replace CreateFileW in the main executable imports when that import is present. */
    IatHook create_file_w_hook_;
    /** @brief IAT hook used to replace CreateFileA in the main executable imports when that import is present. */
    IatHook create_file_a_hook_;
    /** @brief IAT hook used to replace GetFileAttributesW in the main executable imports when that import is present. */
    IatHook get_file_attributes_w_hook_;
    /** @brief IAT hook used to replace GetFileAttributesA in the main executable imports when that import is present. */
    IatHook get_file_attributes_a_hook_;
    /** @brief IAT hook used to replace ReadFile in the main executable imports. */
    IatHook read_file_hook_;
    /** @brief IAT hook used to guard WriteFile on pre-existing protected originals. */
    IatHook write_file_hook_;
    /** @brief IAT hook used to guard SetEndOfFile on pre-existing protected originals. */
    IatHook set_end_of_file_hook_;
    /** @brief IAT hook used to replace either SetFilePointerEx or SetFilePointer in the main executable imports. */
    IatHook set_file_pointer_hook_;
    /** @brief IAT hook used to replace GetFileSizeEx in the main executable imports when that import is present. */
    IatHook get_file_size_ex_hook_;
    /** @brief IAT hook used to replace GetFileSize in the main executable imports when that import is present. */
    IatHook get_file_size_hook_;
    /** @brief IAT hook used to replace CreateFileMappingA in the main executable imports when that import is present. */
    IatHook create_file_mapping_a_hook_;
    /** @brief IAT hook used to replace CreateFileMappingW in the main executable imports when that import is present. */
    IatHook create_file_mapping_w_hook_;
    /** @brief IAT hook for ANSI exact-file deletion; absent imports retain native behavior. */
    IatHook delete_file_a_hook_;
    /** @brief IAT hook for Unicode exact-file deletion; absent imports retain native behavior. */
    IatHook delete_file_w_hook_;
    /** @brief IAT hook for ANSI basic moves; absent imports retain native behavior. */
    IatHook move_file_a_hook_;
    /** @brief IAT hook for Unicode basic moves; absent imports retain native behavior. */
    IatHook move_file_w_hook_;
    /** @brief IAT hook for ANSI flag-bearing moves; absent imports retain native behavior. */
    IatHook move_file_ex_a_hook_;
    /** @brief IAT hook for Unicode flag-bearing moves; absent imports retain native behavior. */
    IatHook move_file_ex_w_hook_;
    /** @brief IAT hook for ANSI replacement publication; absent imports retain native behavior. */
    IatHook replace_file_a_hook_;
    /** @brief IAT hook for Unicode replacement publication; absent imports retain native behavior. */
    IatHook replace_file_w_hook_;
    /** @brief IAT hook for ANSI source-to-destination copies; absent imports retain native behavior. */
    IatHook copy_file_a_hook_;
    /** @brief IAT hook for Unicode source-to-destination copies; absent imports retain native behavior. */
    IatHook copy_file_w_hook_;
    /** @brief IAT hook for ANSI attribute mutation; absent imports retain native behavior. */
    IatHook set_file_attributes_a_hook_;
    /** @brief IAT hook for Unicode attribute mutation; absent imports retain native behavior. */
    IatHook set_file_attributes_w_hook_;
    /** @brief IAT hook for ANSI parent-directory creation; absent imports retain native behavior. */
    IatHook create_directory_a_hook_;
    /** @brief IAT hook for Unicode parent-directory creation; absent imports retain native behavior. */
    IatHook create_directory_w_hook_;
    /** @brief IAT hook for ANSI parent-directory removal; absent imports retain native behavior. */
    IatHook remove_directory_a_hook_;
    /** @brief IAT hook for Unicode parent-directory removal; absent imports retain native behavior. */
    IatHook remove_directory_w_hook_;
    /** @brief IAT hook that rejects duplication of protected handles. */
    IatHook duplicate_handle_hook_;
    /** @brief IAT hook that rejects handle-based protected mutations. */
    IatHook set_file_information_by_handle_hook_;
    /** @brief IAT hook used to replace CloseHandle in the main executable imports. */
    IatHook close_handle_hook_;
    /** @brief Singleton-style active hook set used by the static detour functions. */
    static FileApiHookSet *active_instance_;

    /**
     * @brief IAT detour for CreateFileW that opens matching paths through the virtual file service.
     */
    static HANDLE WINAPI CreateFileWDetour(LPCWSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode,
                                           LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition,
                                           DWORD dwFlagsAndAttributes, HANDLE hTemplateFile);

    /**
     * @brief IAT detour for CreateFileA that opens matching ANSI paths through the virtual file service.
     */
    static HANDLE WINAPI CreateFileADetour(LPCSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode,
                                           LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition,
                                           DWORD dwFlagsAndAttributes, HANDLE hTemplateFile);

    /**
     * @brief IAT detour for GetFileAttributesW that hides declared missing paths.
     */
    static DWORD WINAPI GetFileAttributesWDetour(LPCWSTR lpFileName);

    /**
     * @brief IAT detour for GetFileAttributesA that hides declared missing paths.
     */
    static DWORD WINAPI GetFileAttributesADetour(LPCSTR lpFileName);

    /**
     * @brief IAT detour for ReadFile that serves bytes from synthetic virtual handles.
     */
    static BOOL WINAPI ReadFileDetour(HANDLE hFile, LPVOID lpBuffer, DWORD nNumberOfBytesToRead, LPDWORD lpNumberOfBytesRead,
                                      LPOVERLAPPED lpOverlapped);

    /** @brief IAT detour that permits native writes only for tracked redirected handles or unrelated handles. */
    static BOOL WINAPI WriteFileDetour(HANDLE hFile, LPCVOID lpBuffer, DWORD nNumberOfBytesToWrite, LPDWORD lpNumberOfBytesWritten,
                                       LPOVERLAPPED lpOverlapped);

    /** @brief IAT detour that permits end-of-file changes only for tracked redirected handles or unrelated handles. */
    static BOOL WINAPI SetEndOfFileDetour(HANDLE hFile);

    /**
     * @brief IAT detour for SetFilePointerEx that moves the read cursor for synthetic virtual handles.
     */
    static BOOL WINAPI SetFilePointerExDetour(HANDLE hFile, LARGE_INTEGER liDistanceToMove, PLARGE_INTEGER lpNewFilePointer,
                                              DWORD dwMoveMethod);

    /**
     * @brief IAT detour for SetFilePointer that moves the read cursor for synthetic virtual handles on builds that import the legacy seek
     * API.
     */
    static DWORD WINAPI SetFilePointerDetour(HANDLE hFile, LONG lDistanceToMove, PLONG lpDistanceToMoveHigh, DWORD dwMoveMethod);

    /**
     * @brief IAT detour for GetFileSizeEx that reports the in-memory payload size for synthetic virtual handles.
     */
    static BOOL WINAPI GetFileSizeExDetour(HANDLE hFile, PLARGE_INTEGER lpFileSize);

    /**
     * @brief IAT detour for GetFileSize that reports the in-memory payload size for synthetic virtual handles on builds that import the
     * legacy size API.
     */
    static DWORD WINAPI GetFileSizeDetour(HANDLE hFile, LPDWORD lpFileSizeHigh);

    /**
     * @brief IAT detour for CreateFileMappingA that creates synthetic read-only mappings for virtual files.
     */
    static HANDLE WINAPI CreateFileMappingADetour(HANDLE hFile, LPSECURITY_ATTRIBUTES lpFileMappingAttributes, DWORD flProtect,
                                                  DWORD dwMaximumSizeHigh, DWORD dwMaximumSizeLow, LPCSTR lpName);

    /**
     * @brief IAT detour that rejects writable or named mappings for routed handles.
     * @param hFile File handle used as the mapping source.
     * @param lpFileMappingAttributes Optional native mapping security attributes.
     * @param flProtect Requested mapping protection.
     * @param dwMaximumSizeHigh High-order mapping size.
     * @param dwMaximumSizeLow Low-order mapping size.
     * @param lpName Optional mapping name, which is unsafe for routed handles.
     * @return Native read-only mapping handle or a fail-closed null result.
     */
    static HANDLE WINAPI CreateFileMappingWDetour(HANDLE hFile, LPSECURITY_ATTRIBUTES lpFileMappingAttributes, DWORD flProtect,
                                                  DWORD dwMaximumSizeHigh, DWORD dwMaximumSizeLow, LPCWSTR lpName);

    /** @brief IAT detour that classifies an ANSI path before deleting it. */
    static BOOL WINAPI DeleteFileADetour(LPCSTR lpFileName);
    /** @brief IAT detour that classifies a Unicode path before deleting it. */
    static BOOL WINAPI DeleteFileWDetour(LPCWSTR lpFileName);

    /** @brief IAT detour that classifies ANSI source and destination before moving them. */
    static BOOL WINAPI MoveFileADetour(LPCSTR lpExistingFileName, LPCSTR lpNewFileName);
    /** @brief IAT detour that classifies Unicode source and destination before moving them. */
    static BOOL WINAPI MoveFileWDetour(LPCWSTR lpExistingFileName, LPCWSTR lpNewFileName);

    /** @brief IAT detour that classifies ANSI source and destination with move flags. */
    static BOOL WINAPI MoveFileExADetour(LPCSTR lpExistingFileName, LPCSTR lpNewFileName, DWORD dwFlags);
    /** @brief IAT detour that classifies Unicode source and destination with move flags. */
    static BOOL WINAPI MoveFileExWDetour(LPCWSTR lpExistingFileName, LPCWSTR lpNewFileName, DWORD dwFlags);

    /** @brief IAT detour that classifies ANSI replacement operands before publication. */
    static BOOL WINAPI ReplaceFileADetour(LPCSTR lpReplacedFileName, LPCSTR lpReplacementFileName, LPCSTR lpBackupFileName,
                                          DWORD dwReplaceFlags, LPVOID lpExclude, LPVOID lpReserved);
    /** @brief IAT detour that classifies Unicode replacement operands before publication. */
    static BOOL WINAPI ReplaceFileWDetour(LPCWSTR lpReplacedFileName, LPCWSTR lpReplacementFileName, LPCWSTR lpBackupFileName,
                                          DWORD dwReplaceFlags, LPVOID lpExclude, LPVOID lpReserved);

    /** @brief IAT detour that classifies ANSI source and destination before copying. */
    static BOOL WINAPI CopyFileADetour(LPCSTR lpExistingFileName, LPCSTR lpNewFileName, BOOL bFailIfExists);
    /** @brief IAT detour that classifies Unicode source and destination before copying. */
    static BOOL WINAPI CopyFileWDetour(LPCWSTR lpExistingFileName, LPCWSTR lpNewFileName, BOOL bFailIfExists);

    /** @brief IAT detour that classifies an ANSI path before changing attributes. */
    static BOOL WINAPI SetFileAttributesADetour(LPCSTR lpFileName, DWORD dwFileAttributes);
    /** @brief IAT detour that classifies a Unicode path before changing attributes. */
    static BOOL WINAPI SetFileAttributesWDetour(LPCWSTR lpFileName, DWORD dwFileAttributes);

    /** @brief IAT detour that rejects ANSI parent-directory mutation affecting a protected file. */
    static BOOL WINAPI CreateDirectoryADetour(LPCSTR lpPathName, LPSECURITY_ATTRIBUTES lpSecurityAttributes);
    /** @brief IAT detour that rejects Unicode parent-directory mutation affecting a protected file. */
    static BOOL WINAPI CreateDirectoryWDetour(LPCWSTR lpPathName, LPSECURITY_ATTRIBUTES lpSecurityAttributes);
    /** @brief IAT detour that rejects ANSI removal of a directory containing a protected file. */
    static BOOL WINAPI RemoveDirectoryADetour(LPCSTR lpPathName);
    /** @brief IAT detour that rejects Unicode removal of a directory containing a protected file. */
    static BOOL WINAPI RemoveDirectoryWDetour(LPCWSTR lpPathName);

    /** @brief IAT detour that rejects duplication of routed handles. */
    static BOOL WINAPI DuplicateHandleDetour(HANDLE hSourceProcessHandle, HANDLE hSourceHandle, HANDLE hTargetProcessHandle,
                                             LPHANDLE lpTargetHandle, DWORD dwDesiredAccess, BOOL bInheritHandle, DWORD dwOptions);

    /** @brief IAT detour that rejects handle-based rename and disposition operations for routed handles. */
    static BOOL WINAPI SetFileInformationByHandleDetour(HANDLE hFile, FILE_INFO_BY_HANDLE_CLASS FileInformationClass,
                                                        LPVOID lpFileInformation, DWORD dwBufferSize);

    /**
     * @brief IAT detour for CloseHandle that releases synthetic virtual handles only.
     */
    static BOOL WINAPI CloseHandleDetour(HANDLE hObject);

    /**
     * @brief Returns the active hook set instance used by the static detours.
     * @return Current active hook set or nullptr when no instance has been installed.
     */
    static FileApiHookSet *Current();
};
} // namespace helen
