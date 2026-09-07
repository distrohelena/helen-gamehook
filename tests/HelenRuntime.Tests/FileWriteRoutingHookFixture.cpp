#include <HelenHook/FileApiHookSet.h>
#include <HelenHook/FileWritePolicy.h>
#include <HelenHook/FileWriteRoute.h>
#include <HelenHook/FileWriteRoutingService.h>
#include <HelenHook/Memory.h>
#include <HelenHook/VirtualFileService.h>

#include "CapturedPathSwitchGuard.h"
#include "FileApiCodePageGuard.h"

#include <array>
#include <cstddef>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace
{
    /** @brief Keeps SetFilePointerEx materialized so the fixture exercises routed-handle cursor behavior through its patched IAT. */
    volatile decltype(&SetFilePointerEx) SetFilePointerExImportAnchor = &SetFilePointerEx;

    /** @brief Keeps DuplicateHandle materialized so local and foreign-process duplication tests resolve through the patched IAT. */
    volatile decltype(&DuplicateHandle) DuplicateHandleImportAnchor = &DuplicateHandle;

    /** @brief Keeps SetFileInformationByHandle materialized so protected-handle mutation tests resolve through the patched IAT. */
    volatile decltype(&SetFileInformationByHandle) SetFileInformationByHandleImportAnchor = &SetFileInformationByHandle;

    /** @brief Keeps CreateFileW materialized for the Unicode routed-open fixture calls. */
    volatile decltype(&CreateFileW) CreateFileWImportAnchor = &CreateFileW;
    /** @brief Keeps CreateFileA materialized for the ANSI deny-route fixture call. */
    volatile decltype(&CreateFileA) CreateFileAImportAnchor = &CreateFileA;
    /** @brief Keeps GetFileAttributesW materialized for captured-path attribute routing coverage. */
    volatile decltype(&GetFileAttributesW) GetFileAttributesWImportAnchor = &GetFileAttributesW;
    /** @brief Keeps GetFileAttributesA materialized for active-code-page attribute routing coverage. */
    volatile decltype(&GetFileAttributesA) GetFileAttributesAImportAnchor = &GetFileAttributesA;
    /** @brief Keeps CopyFileW materialized for protected-destination alias mutation coverage. */
    volatile decltype(&CopyFileW) CopyFileWImportAnchor = &CopyFileW;
    /** @brief Keeps MoveFileExA materialized for ANSI delayed and routed publication coverage. */
    volatile decltype(&MoveFileExA) MoveFileExAImportAnchor = &MoveFileExA;
    /** @brief Keeps MoveFileExW materialized for Unicode source-escape and native coexistence coverage. */
    volatile decltype(&MoveFileExW) MoveFileExWImportAnchor = &MoveFileExW;
    /** @brief Keeps ReplaceFileW materialized for protected overlay replacement and backup rejection coverage. */
    volatile decltype(&ReplaceFileW) ReplaceFileWImportAnchor = &ReplaceFileW;
    /** @brief Keeps SetFileAttributesA materialized for ANSI protected-overlay attribute coverage. */
    volatile decltype(&SetFileAttributesA) SetFileAttributesAImportAnchor = &SetFileAttributesA;
    /** @brief Keeps CreateFileMappingW materialized for writable and named mapping guards. */
    volatile decltype(&CreateFileMappingW) CreateFileMappingWImportAnchor = &CreateFileMappingW;
    /** @brief Keeps RemoveDirectoryW materialized for protected-parent mutation coverage. */
    volatile decltype(&RemoveDirectoryW) RemoveDirectoryWImportAnchor = &RemoveDirectoryW;
    /** @brief Keeps DeleteFileW materialized for overlay-only deletion and alias mutation coverage. */
    volatile decltype(&DeleteFileW) DeleteFileWImportAnchor = &DeleteFileW;
    /** @brief Keeps CloseHandle materialized for tracked, virtual, and native handle ownership coverage. */
    volatile decltype(&CloseHandle) CloseHandleImportAnchor = &CloseHandle;
    /** @brief Keeps WriteFile materialized for tracked and preactivation protected-handle write coverage. */
    volatile decltype(&WriteFile) WriteFileImportAnchor = &WriteFile;
    /** @brief Keeps SetEndOfFile materialized for preactivation protected-handle truncation coverage. */
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

    /**
     * @brief Encodes one bounded FILE_RENAME_INFO payload for imported handle-rename coverage.
     * @param destination Destination name copied into the payload.
     * @param root_directory Optional native directory handle used for relative names.
     * @param flags Legacy replace byte or FileRenameInfoEx flags.
     * @return Exact bytes and length passed to SetFileInformationByHandle.
     */
    std::vector<BYTE> BuildRenameInformation(const std::filesystem::path& destination, HANDLE root_directory, DWORD flags)
    {
        const std::wstring name = destination.wstring();
        const std::size_t header_size = offsetof(FILE_RENAME_INFO, FileName);
        std::vector<BYTE> payload(header_size + name.size() * sizeof(wchar_t));
        std::memcpy(payload.data(), &flags, sizeof(flags));
        std::memcpy(payload.data() + offsetof(FILE_RENAME_INFO, RootDirectory), &root_directory, sizeof(root_directory));
        const DWORD name_length = static_cast<DWORD>(name.size() * sizeof(wchar_t));
        std::memcpy(payload.data() + offsetof(FILE_RENAME_INFO, FileNameLength), &name_length, sizeof(name_length));
        std::memcpy(payload.data() + header_size, name.data(), name_length);
        return payload;
    }

    /**
     * @brief Restores the process working directory when a relative-path regression changes it.
     *
     * The guard captures the native process CWD before the fixture changes it and restores that
     * exact path even when an assertion throws, preventing one child scenario from contaminating
     * later native tests.
     */
    class CurrentDirectoryGuard
    {
    public:
        /** @brief Captures the current process working directory. */
        CurrentDirectoryGuard()
        {
            std::array<wchar_t, MAX_PATH> buffer{};
            const DWORD length = GetCurrentDirectoryW(static_cast<DWORD>(buffer.size()), buffer.data());
            if (length == 0 || length >= buffer.size())
            {
                throw std::runtime_error("Failed to capture fixture current directory.");
            }

            original_directory_.assign(buffer.data(), length);
        }

        /** @brief Restores the captured process working directory. */
        ~CurrentDirectoryGuard()
        {
            SetCurrentDirectoryW(original_directory_.c_str());
        }

        CurrentDirectoryGuard(const CurrentDirectoryGuard&) = delete;
        CurrentDirectoryGuard& operator=(const CurrentDirectoryGuard&) = delete;

        /**
         * @brief Changes the process working directory for a relative-path fixture operation.
         * @param directory Directory that should become the process CWD.
         */
        void Set(const std::filesystem::path& directory)
        {
            ExpectHookFixture(SetCurrentDirectoryW(directory.c_str()) != FALSE,
                "Failed to change fixture current directory.");
        }

    private:
        /** @brief Native directory spelling captured before the guarded CWD change. */
        std::wstring original_directory_;
    };

    /**
     * @brief Encodes one Unicode filename with the file API's currently selected narrow code page.
     * @param path Unicode path whose narrow request spelling should be produced.
     * @return Exact active-code-page byte spelling accepted by CreateFileA.
     */
    std::string EncodeActiveFileApiPath(const std::filesystem::path& path)
    {
        const std::wstring wide_path = path.wstring();
        const UINT code_page = AreFileApisANSI() != FALSE ? CP_ACP : CP_OEMCP;
        const int required_length = WideCharToMultiByte(code_page, 0, wide_path.c_str(), -1, nullptr, 0, nullptr, nullptr);
        if (required_length <= 0)
        {
            throw std::runtime_error("Failed to encode active file API fixture path.");
        }

        std::string encoded(static_cast<std::size_t>(required_length), '\0');
        const int actual_length = WideCharToMultiByte(code_page, 0, wide_path.c_str(), -1, encoded.data(), required_length, nullptr, nullptr);
        if (actual_length != required_length)
        {
            throw std::runtime_error("Failed to materialize active file API fixture path.");
        }
        encoded.resize(static_cast<std::size_t>(actual_length - 1));
        return encoded;
    }

    /**
     * @brief Attempts an active-code-page conversion without allowing default-character substitution.
     * @param path Unicode path whose exact narrow representability should be checked.
     * @return Exact active-code-page spelling, or no value when any character is unrepresentable.
     */
    std::optional<std::string> TryEncodeActiveFileApiPath(const std::filesystem::path& path)
    {
        const std::wstring wide_path = path.wstring();
        const UINT code_page = AreFileApisANSI() != FALSE ? CP_ACP : CP_OEMCP;
        BOOL used_default_character = FALSE;
        const int required_length = WideCharToMultiByte(code_page, WC_NO_BEST_FIT_CHARS, wide_path.c_str(), -1,
            nullptr, 0, nullptr, code_page == CP_UTF8 ? nullptr : &used_default_character);
        if (required_length <= 0 || used_default_character != FALSE)
        {
            return std::nullopt;
        }

        std::string encoded(static_cast<std::size_t>(required_length), '\0');
        used_default_character = FALSE;
        const int actual_length = WideCharToMultiByte(code_page, WC_NO_BEST_FIT_CHARS, wide_path.c_str(), -1,
            encoded.data(), required_length, nullptr, code_page == CP_UTF8 ? nullptr : &used_default_character);
        if (actual_length != required_length || used_default_character != FALSE)
        {
            return std::nullopt;
        }

        encoded.resize(static_cast<std::size_t>(actual_length - 1));
        return encoded;
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
    const std::filesystem::path original_read_path = root / "OriginalRead.ini";
    const std::filesystem::path deny_path = root / "Deny.ini";
    const std::filesystem::path source_path = root / "BmEngine.tmp";
    const std::filesystem::path unrelated_path = root / "Unrelated.ini";
    const std::filesystem::path alias_source_path = root / "AliasSource.tmp";
    const std::filesystem::path alias_backup_path = root / "AliasBackup.bak";
    const std::filesystem::path alias_escape_path = root / "AliasEscape.ini";
    const std::filesystem::path captured_path = root / "Captured.ini";
    const std::filesystem::path captured_attributes_path = root / "CapturedAttributes.ini";
    const std::filesystem::path ansi_path = root / std::filesystem::path(L"Ansi-\u00E9.ini");
    const std::filesystem::path switch_directory = root / "cwd-switch";
    const std::filesystem::path ansi_unicode_switch_directory = root / std::filesystem::path(L"cwd-\u0416");
    const std::filesystem::path ansi_native_captured_path = ansi_unicode_switch_directory / "NativeCapturedA.ini";
    const std::filesystem::path switched_captured_path = switch_directory / "Captured.ini";
    const std::filesystem::path switched_attributes_path = switch_directory / "CapturedAttributes.ini";
    const std::filesystem::path ansi_native_switched_path = switch_directory / "NativeCapturedA.ini";
    WriteHookFixtureFile(original_path, "ORIGINAL");
    WriteHookFixtureFile(original_read_path, "ORIGINAL-READ");
    WriteHookFixtureFile(deny_path, "DENY");
    WriteHookFixtureFile(source_path, "COPY");
    WriteHookFixtureFile(unrelated_path, "NATIVE");
    WriteHookFixtureFile(alias_source_path, "ALIAS-SOURCE");
    WriteHookFixtureFile(alias_backup_path, "ALIAS-BACKUP");
    ExpectHookFixture(CreateDirectoryW(switch_directory.c_str(), nullptr) != FALSE,
        "Failed to create captured-path CWD switch directory.");
    ExpectHookFixture(CreateDirectoryW(ansi_unicode_switch_directory.c_str(), nullptr) != FALSE,
        "Failed to create ANSI Unicode-CWD switch directory.");
    WriteHookFixtureFile(captured_path, "CAPTURED-ORIGINAL");
    WriteHookFixtureFile(captured_attributes_path, "ATTRIBUTES-ORIGINAL");
    WriteHookFixtureFile(ansi_path, "ANSI-ORIGINAL");
    WriteHookFixtureFile(ansi_native_captured_path, "NATIVE-CAPTURED");
    WriteHookFixtureFile(switched_captured_path, "CAPTURED-SWITCH");
    WriteHookFixtureFile(switched_attributes_path, "ATTRIBUTES-SWITCH");
    WriteHookFixtureFile(ansi_native_switched_path, "NATIVE-SWITCH");
    ExpectHookFixture(SetFileAttributesW(captured_attributes_path.c_str(), FILE_ATTRIBUTE_HIDDEN) != FALSE,
        "Failed to set distinct captured-path original attributes.");
    ExpectHookFixture(SetFileAttributesW(switched_attributes_path.c_str(), FILE_ATTRIBUTE_NORMAL) != FALSE,
        "Failed to set distinct captured-path switched attributes.");
    ExpectHookFixture(SetFileAttributesW(ansi_native_captured_path.c_str(), FILE_ATTRIBUTE_HIDDEN) != FALSE,
        "Failed to set distinct ANSI captured-file attributes.");
    ExpectHookFixture(SetFileAttributesW(ansi_native_switched_path.c_str(), FILE_ATTRIBUTE_NORMAL) != FALSE,
        "Failed to set distinct ANSI switched-file attributes.");

    try
    {
        helen::FileWriteRoutingService routing(root / "cache", root / "stale-startup-base");
        DWORD error = ERROR_SUCCESS;
        ExpectHookFixture(routing.Initialize({
            { "engine", original_path, helen::FileWritePolicy::Redirect, helen::FileReadPolicy::Redirected },
            { "original-read", original_read_path, helen::FileWritePolicy::Redirect, helen::FileReadPolicy::Original },
            { "deny", deny_path, helen::FileWritePolicy::Deny, helen::FileReadPolicy::Original },
            { "captured", captured_path, helen::FileWritePolicy::Redirect, helen::FileReadPolicy::Redirected },
            { "capturedAttributes", captured_attributes_path, helen::FileWritePolicy::Redirect, helen::FileReadPolicy::Original },
            { "ansi", ansi_path, helen::FileWritePolicy::Redirect, helen::FileReadPolicy::Redirected },
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
        {
            CurrentDirectoryGuard current_directory_guard;
            current_directory_guard.Set(root);
            CapturedPathSwitchGuard captured_path_switch(*main_module, L"Captured.ini", switch_directory);
            const auto imported_create_file_w = CallHookedImport<decltype(&CreateFileW)>("CreateFileW");
            HANDLE captured_handle = imported_create_file_w(L"Captured.ini", GENERIC_WRITE,
                FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
            ExpectHookFixture(captured_handle != INVALID_HANDLE_VALUE && captured_path_switch.Triggered(),
                "Captured-path CreateFileW did not cross the one-shot CWD boundary.");
            WriteHookedByte(captured_handle, 'Z');
            ExpectHookFixture(CallHookedImport<decltype(&CloseHandle)>("CloseHandle")(captured_handle) != FALSE,
                "Captured-path routed handle close failed.");
            ExpectHookFixture(ReadHookFixtureFile(captured_path) == "CAPTURED-ORIGINAL",
                "Captured-path CreateFileW mutated the original after the CWD changed.");
            ExpectHookFixture(ReadHookFixtureFile(switched_captured_path) == "CAPTURED-SWITCH",
                "Captured-path CreateFileW touched the post-normalization CWD target.");
            bool captured_overlay_verified = false;
            for (const helen::FileWriteRoutingService::RouteDiagnostics& route : routing.GetRouteDiagnostics())
            {
                if (route.Id == "captured")
                {
                    ExpectHookFixture(!route.OverlayPath.empty() && ReadHookFixtureFile(route.OverlayPath).find('Z') != std::string::npos,
                        "Captured-path CreateFileW did not update the captured overlay.");
                    captured_overlay_verified = true;
                }
            }
            ExpectHookFixture(captured_overlay_verified, "Captured-path route diagnostics omitted the active overlay.");
        }
        {
            CurrentDirectoryGuard current_directory_guard;
            current_directory_guard.Set(root);
            CapturedPathSwitchGuard captured_path_switch(*main_module, L"CapturedAttributes.ini", switch_directory);
            const auto imported_get_file_attributes_w = CallHookedImport<decltype(&GetFileAttributesW)>("GetFileAttributesW");
            const DWORD attributes = imported_get_file_attributes_w(L"CapturedAttributes.ini");
            ExpectHookFixture(attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_HIDDEN) != 0 && captured_path_switch.Triggered(),
                "Captured-path GetFileAttributesW did not retain the normalized original after the CWD changed.");
            ExpectHookFixture((GetFileAttributesW(switched_attributes_path.c_str()) & FILE_ATTRIBUTE_HIDDEN) == 0,
                "Captured-path attribute regression changed the switched target attributes.");
        }
        {
            CurrentDirectoryGuard current_directory_guard;
            current_directory_guard.Set(root);
            FileApiCodePageGuard file_api_code_page_guard;
            const std::string ansi_relative_name = EncodeActiveFileApiPath(std::filesystem::path(L"Ansi-\u00E9.ini"));
            const auto imported_create_file_a = CallHookedImport<decltype(&CreateFileA)>("CreateFileA");
            HANDLE ansi_handle = imported_create_file_a(ansi_relative_name.c_str(), GENERIC_WRITE,
                FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
            ExpectHookFixture(ansi_handle != INVALID_HANDLE_VALUE,
                "CreateFileA did not preserve the active OEM code-page request for a protected Unicode path.");
            WriteHookedByte(ansi_handle, 'Q');
            ExpectHookFixture(CallHookedImport<decltype(&CloseHandle)>("CloseHandle")(ansi_handle) != FALSE,
                "Active-code-page CreateFileA routed handle close failed.");
            ExpectHookFixture(ReadHookFixtureFile(ansi_path) == "ANSI-ORIGINAL",
                "Active-code-page CreateFileA mutated the protected original.");
            bool ansi_overlay_verified = false;
            for (const helen::FileWriteRoutingService::RouteDiagnostics& route : routing.GetRouteDiagnostics())
            {
                if (route.Id == "ansi")
                {
                    ExpectHookFixture(!route.OverlayPath.empty() && ReadHookFixtureFile(route.OverlayPath).find('Q') != std::string::npos,
                        "Active-code-page CreateFileA did not update the ANSI route overlay.");
                    ansi_overlay_verified = true;
                }
            }
            ExpectHookFixture(ansi_overlay_verified, "Active-code-page route diagnostics omitted the ANSI overlay.");
        }
        {
            FileApiCodePageGuard file_api_code_page_guard;
            const std::optional<std::string> unicode_directory_narrow = TryEncodeActiveFileApiPath(
                ansi_unicode_switch_directory.filename());
            if (!unicode_directory_narrow.has_value())
            {
                {
                    CurrentDirectoryGuard current_directory_guard;
                    current_directory_guard.Set(ansi_unicode_switch_directory);
                    ExpectHookFixture(SetFileAttributesW(ansi_native_captured_path.c_str(), FILE_ATTRIBUTE_NORMAL) != FALSE,
                        "Failed to clear captured-file attributes before native CreateFileA dispatch.");
                    CapturedPathSwitchGuard captured_path_switch(*main_module, L"NativeCapturedA.ini", switch_directory);
                    const auto imported_create_file_a = CallHookedImport<decltype(&CreateFileA)>("CreateFileA");
                    HANDLE captured_handle = imported_create_file_a("NativeCapturedA.ini", GENERIC_WRITE,
                        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
                    ExpectHookFixture(captured_handle != INVALID_HANDLE_VALUE && captured_path_switch.Triggered(),
                        "Captured-path CreateFileA did not cross the Unicode-CWD boundary.");
                    WriteHookedByte(captured_handle, 'A');
                    ExpectHookFixture(CallHookedImport<decltype(&CloseHandle)>("CloseHandle")(captured_handle) != FALSE,
                        "Captured-path CreateFileA handle close failed.");
                    ExpectHookFixture(ReadHookFixtureFile(ansi_native_captured_path) == "A",
                        "Captured-path CreateFileA did not update the captured native file.");
                    ExpectHookFixture(ReadHookFixtureFile(ansi_native_switched_path) == "NATIVE-SWITCH",
                        "Captured-path CreateFileA mutated the switched native target.");
                }
                {
                    CurrentDirectoryGuard current_directory_guard;
                    current_directory_guard.Set(ansi_unicode_switch_directory);
                    ExpectHookFixture(SetFileAttributesW(ansi_native_captured_path.c_str(), FILE_ATTRIBUTE_HIDDEN) != FALSE,
                        "Failed to set captured-file attributes before native GetFileAttributesA dispatch.");
                    CapturedPathSwitchGuard captured_path_switch(*main_module, L"NativeCapturedA.ini", switch_directory);
                    const auto imported_get_file_attributes_a = CallHookedImport<decltype(&GetFileAttributesA)>("GetFileAttributesA");
                    const DWORD attributes = imported_get_file_attributes_a("NativeCapturedA.ini");
                    ExpectHookFixture(attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_HIDDEN) != 0 &&
                        captured_path_switch.Triggered(),
                        "Captured-path GetFileAttributesA did not retain the Unicode-CWD file after dispatch.");
                    ExpectHookFixture((GetFileAttributesW(ansi_native_switched_path.c_str()) & FILE_ATTRIBUTE_HIDDEN) == 0,
                        "Captured-path GetFileAttributesA changed the switched target attributes.");
                }
            }
            else
            {
                std::cout << "FILE_ROUTING_A_UNICODE_CWD_BRANCH_SKIPPED codepage="
                    << (AreFileApisANSI() != FALSE ? CP_ACP : CP_OEMCP) << "\n";
            }
        }
        {
            CurrentDirectoryGuard current_directory_guard;
            current_directory_guard.Set(root);
            const auto imported_create_file_w = CallHookedImport<decltype(&CreateFileW)>("CreateFileW");
            HANDLE relative_routed = imported_create_file_w(L"BmEngine.ini", GENERIC_WRITE,
                FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
            ExpectHookFixture(relative_routed != INVALID_HANDLE_VALUE,
                "Relative protected CreateFileW was not routed after the process CWD changed.");
            WriteHookedByte(relative_routed, 'C');
            ExpectHookFixture(CallHookedImport<decltype(&CloseHandle)>("CloseHandle")(relative_routed) != FALSE,
                "Relative routed handle close failed after the process CWD changed.");
            HANDLE relative_read = imported_create_file_w(L"BmEngine.ini", GENERIC_READ,
                FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
            ExpectHookFixture(relative_read != INVALID_HANDLE_VALUE,
                "Relative protected read could not open the redirected session copy.");
            char relative_byte = '\0';
            DWORD relative_bytes_read = 0;
            ExpectHookFixture(CallHookedImport<decltype(&ReadFile)>("ReadFile")(relative_read, &relative_byte, 1, &relative_bytes_read, nullptr) != FALSE &&
                relative_bytes_read == 1 && relative_byte == 'C',
                "Relative protected read did not select the redirected session copy.");
            ExpectHookFixture(CallHookedImport<decltype(&CloseHandle)>("CloseHandle")(relative_read) != FALSE,
                "Relative routed read close failed after the process CWD changed.");
        }
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

        std::filesystem::path original_read_overlay;
        for (const helen::FileWriteRoutingService::RouteDiagnostics& route : routing.GetRouteDiagnostics())
        {
            if (route.Id == "original-read")
            {
                original_read_overlay = route.OverlayPath;
            }
        }
        ExpectHookFixture(!original_read_overlay.empty(), "Imported original-read overlay diagnostics were missing.");
        HANDLE original_read_full_access = imported_create_file_w(original_read_path.c_str(), GENERIC_ALL,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        ExpectHookFixture(original_read_full_access != INVALID_HANDLE_VALUE, "Imported original-read full-access open failed.");
        WriteHookedByte(original_read_full_access, 'Y');
        ExpectHookFixture(CallHookedImport<decltype(&CloseHandle)>("CloseHandle")(original_read_full_access) != FALSE &&
            ReadHookFixtureFile(original_read_path) == "ORIGINAL-READ" && ReadHookFixtureFile(original_read_overlay) == "Y",
            "Imported original-read full-access write changed the original.");
        HANDLE original_read_delete_on_close = imported_create_file_w(original_read_path.c_str(), GENERIC_READ,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL | FILE_FLAG_DELETE_ON_CLOSE, nullptr);
        ExpectHookFixture(original_read_delete_on_close != INVALID_HANDLE_VALUE,
            "Imported original-read delete-on-close open was rejected unexpectedly.");
        ExpectHookFixture(CallHookedImport<decltype(&CloseHandle)>("CloseHandle")(original_read_delete_on_close) != FALSE &&
            std::filesystem::exists(original_read_path) && ReadHookFixtureFile(original_read_path) == "ORIGINAL-READ",
            "Imported original-read delete-on-close removed or changed the original.");

        const std::filesystem::path extended_original = std::filesystem::path(L"\\\\?\\" + original_path.wstring());
        ExpectHookFixture(CallHookedImport<decltype(&CopyFileW)>("CopyFileW")(alias_source_path.c_str(), extended_original.c_str(), FALSE) != FALSE,
            "Extended protected destination did not redirect its overlay.");
        ExpectHookFixture(CallHookedImport<decltype(&DeleteFileW)>("DeleteFileW")(extended_original.c_str()) != FALSE,
            "Extended protected alias did not delete only its overlay.");
        ExpectHookFixture(std::filesystem::exists(original_path) && std::filesystem::exists(alias_source_path) && std::filesystem::exists(alias_backup_path),
            "Extended alias overlay deletion removed a literal operand.");
        ExpectHookFixture(CallHookedImport<decltype(&MoveFileExW)>("MoveFileExW")(extended_original.c_str(), alias_escape_path.c_str(), MOVEFILE_REPLACE_EXISTING) == FALSE &&
            GetLastError() == ERROR_ACCESS_DENIED, "Extended protected source escaped to an unrelated destination.");
        ExpectHookFixture(!std::filesystem::exists(alias_escape_path), "Rejected extended alias move created its destination.");
        ExpectHookFixture(CallHookedImport<decltype(&ReplaceFileW)>("ReplaceFileW")(extended_original.c_str(), alias_source_path.c_str(), alias_backup_path.c_str(),
            REPLACEFILE_IGNORE_MERGE_ERRORS, nullptr, nullptr) == FALSE && GetLastError() == ERROR_NOT_SUPPORTED,
            "Extended protected replacement accepted an unsafe backup operand.");

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

        const std::filesystem::path rename_source_path = root / "handle-rename-source.tmp";
        WriteHookFixtureFile(rename_source_path, "RENAME-SOURCE");
        HANDLE rename_source = imported_create_file_w(rename_source_path.c_str(), DELETE | GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        ExpectHookFixture(rename_source != INVALID_HANDLE_VALUE, "Unprotected handle-rename source could not be opened.");
        const std::vector<BYTE> protected_destination_rename = BuildRenameInformation(original_path, nullptr, 1);
        ExpectHookFixture(CallHookedImport<decltype(&SetFileInformationByHandle)>("SetFileInformationByHandle")(
            rename_source, FileRenameInfo, const_cast<BYTE*>(protected_destination_rename.data()),
            static_cast<DWORD>(protected_destination_rename.size())) == FALSE && GetLastError() == ERROR_ACCESS_DENIED,
            "Unprotected source handle renamed over a protected destination.");
        ExpectHookFixture(ReadHookFixtureFile(rename_source_path) == "RENAME-SOURCE" && ReadHookFixtureFile(original_path) == "ORIGINAL",
            "Rejected protected-destination rename mutated an operand.");
        ExpectHookFixture(CallHookedImport<decltype(&CloseHandle)>("CloseHandle")(rename_source) != FALSE,
            "Protected-destination rename source close failed.");

        HANDLE protected_parent = imported_create_file_w(root.c_str(), DELETE | FILE_LIST_DIRECTORY,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
            FILE_FLAG_BACKUP_SEMANTICS, nullptr);
        ExpectHookFixture(protected_parent != INVALID_HANDLE_VALUE, "Protected parent directory handle could not be opened.");
        const std::vector<BYTE> parent_rename = BuildRenameInformation(root.parent_path() / "renamed-routing-parent", nullptr, 0);
        ExpectHookFixture(CallHookedImport<decltype(&SetFileInformationByHandle)>("SetFileInformationByHandle")(
            protected_parent, FileRenameInfo, const_cast<BYTE*>(parent_rename.data()), static_cast<DWORD>(parent_rename.size())) == FALSE &&
            GetLastError() == ERROR_INVALID_PARAMETER,
            "Protected parent directory handle rename was not rejected before mutation.");
        ExpectHookFixture(std::filesystem::exists(root) && std::filesystem::exists(original_path) && ReadHookFixtureFile(original_path) == "ORIGINAL",
            "Rejected protected-parent rename changed protected state.");
        ExpectHookFixture(CallHookedImport<decltype(&CloseHandle)>("CloseHandle")(protected_parent) != FALSE,
            "Protected parent directory handle close failed.");

        const std::filesystem::path unrelated_rename_directory = root / "unrelated-rename";
        ExpectHookFixture(CreateDirectoryW(unrelated_rename_directory.c_str(), nullptr) != FALSE,
            "Unrelated handle-rename directory could not be created.");
        const std::filesystem::path unrelated_rename_source_path = unrelated_rename_directory / "unrelated-handle-source.tmp";
        const std::filesystem::path unrelated_rename_destination_path = unrelated_rename_directory / "unrelated-handle-destination.tmp";
        WriteHookFixtureFile(unrelated_rename_source_path, "NATIVE-RENAME");
        HANDLE unrelated_rename_source = imported_create_file_w(unrelated_rename_source_path.c_str(), DELETE | GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        ExpectHookFixture(unrelated_rename_source != INVALID_HANDLE_VALUE, "Unrelated handle-rename source could not be opened.");
        const std::vector<BYTE> unrelated_rename = BuildRenameInformation(unrelated_rename_destination_path, nullptr, 0);
        ExpectHookFixture(CallHookedImport<decltype(&SetFileInformationByHandle)>("SetFileInformationByHandle")(
            unrelated_rename_source, FileRenameInfo, const_cast<BYTE*>(unrelated_rename.data()), static_cast<DWORD>(unrelated_rename.size())) != FALSE,
            "Unrelated native handle rename no longer retained native behavior.");
        ExpectHookFixture(CallHookedImport<decltype(&CloseHandle)>("CloseHandle")(unrelated_rename_source) != FALSE &&
            !std::filesystem::exists(unrelated_rename_source_path) && ReadHookFixtureFile(unrelated_rename_destination_path) == "NATIVE-RENAME",
            "Unrelated native handle rename did not publish its destination.");

        WriteHookFixtureFile(rename_source_path, "MALFORMED");
        HANDLE malformed_source = imported_create_file_w(rename_source_path.c_str(), DELETE | GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        ExpectHookFixture(malformed_source != INVALID_HANDLE_VALUE, "Malformed-rename source could not be opened.");
        ExpectHookFixture(CallHookedImport<decltype(&SetFileInformationByHandle)>("SetFileInformationByHandle")(
            malformed_source, FileRenameInfo, nullptr, sizeof(FILE_RENAME_INFO) - 1) == FALSE && GetLastError() == ERROR_INVALID_PARAMETER,
            "Malformed rename buffer was not rejected safely.");
        ExpectHookFixture(CallHookedImport<decltype(&CloseHandle)>("CloseHandle")(malformed_source) != FALSE &&
            ReadHookFixtureFile(rename_source_path) == "MALFORMED",
            "Malformed rename buffer changed its source.");

        const std::filesystem::path rename_cwd_original = root / "rename-cwd-original";
        const std::filesystem::path rename_cwd_switched = root / "rename-cwd-switched";
        ExpectHookFixture(CreateDirectoryW(rename_cwd_original.c_str(), nullptr) != FALSE &&
            CreateDirectoryW(rename_cwd_switched.c_str(), nullptr) != FALSE,
            "CWD-switch rename directories could not be created.");
        const std::filesystem::path cwd_rename_source_path = rename_cwd_original / "cwd-rename-source.tmp";
        const std::filesystem::path cwd_rename_destination_path = rename_cwd_original / "cwd-rename-target.tmp";
        const std::filesystem::path cwd_switched_destination_path = rename_cwd_switched / "cwd-rename-target.tmp";
        WriteHookFixtureFile(cwd_rename_source_path, "CWD-RENAME");
        HANDLE cwd_rename_source = imported_create_file_w(cwd_rename_source_path.c_str(), DELETE | GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        ExpectHookFixture(cwd_rename_source != INVALID_HANDLE_VALUE, "CWD-switch rename source could not be opened.");
        {
            CurrentDirectoryGuard current_directory_guard;
            current_directory_guard.Set(rename_cwd_original);
            CapturedPathSwitchGuard captured_path_switch(*main_module, L"cwd-rename-target.tmp", rename_cwd_switched);
            const std::vector<BYTE> relative_cwd_rename = BuildRenameInformation(L"cwd-rename-target.tmp", nullptr, 0);
            ExpectHookFixture(CallHookedImport<decltype(&SetFileInformationByHandle)>("SetFileInformationByHandle")(
                cwd_rename_source, FileRenameInfo, const_cast<BYTE*>(relative_cwd_rename.data()),
                static_cast<DWORD>(relative_cwd_rename.size())) != FALSE && captured_path_switch.Triggered(),
                "CWD-switch handle rename did not capture its relative destination before dispatch.");
        }
        ExpectHookFixture(CallHookedImport<decltype(&CloseHandle)>("CloseHandle")(cwd_rename_source) != FALSE &&
            ReadHookFixtureFile(cwd_rename_destination_path) == "CWD-RENAME" &&
            !std::filesystem::exists(cwd_switched_destination_path) && !std::filesystem::exists(cwd_rename_source_path),
            "CWD-switch handle rename dispatched its uncaptured relative destination.");

        const std::filesystem::path native_root = root / "native-root";
        CreateDirectoryW(native_root.c_str(), nullptr);
        const std::filesystem::path rooted_source_path = native_root / "rooted-source.tmp";
        const std::filesystem::path rooted_destination_path = native_root / "rooted-destination.tmp";
        WriteHookFixtureFile(rooted_source_path, "ROOTED-RENAME");
        HANDLE native_root_handle = imported_create_file_w(native_root.c_str(), FILE_LIST_DIRECTORY | DELETE,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
        HANDLE rooted_source = imported_create_file_w(rooted_source_path.c_str(), DELETE | GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        ExpectHookFixture(native_root_handle != INVALID_HANDLE_VALUE && rooted_source != INVALID_HANDLE_VALUE,
            "Valid RootDirectory rename handles could not be opened.");
        const std::vector<BYTE> rooted_rename = BuildRenameInformation(L"rooted-destination.tmp", native_root_handle, 0);
        ExpectHookFixture(CallHookedImport<decltype(&SetFileInformationByHandle)>("SetFileInformationByHandle")(
            rooted_source, FileRenameInfo, const_cast<BYTE*>(rooted_rename.data()), static_cast<DWORD>(rooted_rename.size())) != FALSE,
            "Valid unrelated RootDirectory rename was rejected.");
        ExpectHookFixture(CallHookedImport<decltype(&CloseHandle)>("CloseHandle")(rooted_source) != FALSE &&
            CallHookedImport<decltype(&CloseHandle)>("CloseHandle")(native_root_handle) != FALSE &&
            ReadHookFixtureFile(rooted_destination_path) == "ROOTED-RENAME" && !std::filesystem::exists(rooted_source_path),
            "Valid RootDirectory rename did not publish its destination.");

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
        ExpectHookFixture(ReadHookFixtureFile(alias_source_path) == "ALIAS-SOURCE", "Rejected extended alias mutation changed its source operand.");
        ExpectHookFixture(ReadHookFixtureFile(alias_backup_path) == "ALIAS-BACKUP", "Rejected extended alias mutation changed its backup operand.");
        ExpectHookFixture(!std::filesystem::exists(alias_escape_path), "Rejected extended alias mutation left an escape destination.");
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
