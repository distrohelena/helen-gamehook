#include <HelenHook/FileWriteRoutingService.h>

#include <HelenHook/FileWriteRoutingInspectedPath.h>
#include <HelenHook/FileWriteRoutingPendingRoute.h>
#include <HelenHook/FileWriteRoutingTransaction.h>
#include <HelenHook/Log.h>

#include <algorithm>
#include <cwctype>
#include <filesystem>
#include <map>
#include <set>
#include <string>
#include <system_error>
#include <vector>

namespace {
/**
 * @brief Resolves one kernel32 export so routing internals bypass the executable's patched IAT.
 * @tparam T Exact native function pointer type.
 * @param export_name Export name to resolve.
 * @return Resolved function pointer, or nullptr when the export is unavailable.
 */
template <typename T> T ResolveKernel32Export(const char *export_name) noexcept {
    const HMODULE module = GetModuleHandleW(L"kernel32.dll");
    return module == nullptr ? nullptr : reinterpret_cast<T>(GetProcAddress(module, export_name));
}

/** @brief Calls the native CreateFileW export without the current executable IAT. */
HANDLE WINAPI CallNativeCreateFileW(LPCWSTR path, DWORD access, DWORD share, LPSECURITY_ATTRIBUTES security, DWORD disposition, DWORD flags,
                                    HANDLE template_file) {
    const auto function = ResolveKernel32Export<decltype(&CreateFileW)>("CreateFileW");
    if (function == nullptr) {
        SetLastError(ERROR_PROC_NOT_FOUND);
        return INVALID_HANDLE_VALUE;
    }
    return function(path, access, share, security, disposition, flags, template_file);
}

/** @brief Calls the native GetFileAttributesW export without the current executable IAT. */
DWORD WINAPI CallNativeGetFileAttributesW(LPCWSTR path) {
    const auto function = ResolveKernel32Export<decltype(&GetFileAttributesW)>("GetFileAttributesW");
    if (function == nullptr) {
        SetLastError(ERROR_PROC_NOT_FOUND);
        return INVALID_FILE_ATTRIBUTES;
    }
    return function(path);
}

/** @brief Calls the native CloseHandle export without the current executable IAT. */
BOOL WINAPI CallNativeCloseHandle(HANDLE handle) {
    const auto function = ResolveKernel32Export<decltype(&CloseHandle)>("CloseHandle");
    if (function == nullptr) {
        SetLastError(ERROR_PROC_NOT_FOUND);
        return FALSE;
    }
    return function(handle);
}

/** @brief Calls the native DeleteFileW export without the current executable IAT. */
BOOL WINAPI CallNativeDeleteFileW(LPCWSTR path) {
    const auto function = ResolveKernel32Export<decltype(&DeleteFileW)>("DeleteFileW");
    if (function == nullptr) {
        SetLastError(ERROR_PROC_NOT_FOUND);
        return FALSE;
    }
    return function(path);
}

/** @brief Calls the native MoveFileExW export without the current executable IAT. */
BOOL WINAPI CallNativeMoveFileExW(LPCWSTR existing_path, LPCWSTR new_path, DWORD flags) {
    const auto function = ResolveKernel32Export<decltype(&MoveFileExW)>("MoveFileExW");
    if (function == nullptr) {
        SetLastError(ERROR_PROC_NOT_FOUND);
        return FALSE;
    }
    return function(existing_path, new_path, flags);
}

/** @brief Calls the native ReplaceFileW export without the current executable IAT. */
BOOL WINAPI CallNativeReplaceFileW(LPCWSTR replaced_path, LPCWSTR replacement_path, LPCWSTR backup_path, DWORD flags, LPVOID exclude,
                                   LPVOID reserved) {
    const auto function = ResolveKernel32Export<decltype(&ReplaceFileW)>("ReplaceFileW");
    if (function == nullptr) {
        SetLastError(ERROR_PROC_NOT_FOUND);
        return FALSE;
    }
    return function(replaced_path, replacement_path, backup_path, flags, exclude, reserved);
}

/** @brief Calls the native CopyFileW export without the current executable IAT. */
BOOL WINAPI CallNativeCopyFileW(LPCWSTR existing_path, LPCWSTR new_path, BOOL fail_if_exists) {
    const auto function = ResolveKernel32Export<decltype(&CopyFileW)>("CopyFileW");
    if (function == nullptr) {
        SetLastError(ERROR_PROC_NOT_FOUND);
        return FALSE;
    }
    return function(existing_path, new_path, fail_if_exists);
}

/** @brief Calls the native SetFileAttributesW export without the current executable IAT. */
BOOL WINAPI CallNativeSetFileAttributesW(LPCWSTR path, DWORD attributes) {
    const auto function = ResolveKernel32Export<decltype(&SetFileAttributesW)>("SetFileAttributesW");
    if (function == nullptr) {
        SetLastError(ERROR_PROC_NOT_FOUND);
        return FALSE;
    }
    return function(path, attributes);
}

/** @brief Calls the native CreateDirectoryW export without the current executable IAT. */
BOOL WINAPI CallNativeCreateDirectoryW(LPCWSTR path, LPSECURITY_ATTRIBUTES security_attributes) {
    const auto function = ResolveKernel32Export<decltype(&CreateDirectoryW)>("CreateDirectoryW");
    if (function == nullptr) {
        SetLastError(ERROR_PROC_NOT_FOUND);
        return FALSE;
    }
    return function(path, security_attributes);
}

using InspectedPath = helen::routing_detail::InspectedPath;
using PendingRoute = helen::routing_detail::PendingRoute;

/**
 * @brief Converts a Win32 path into one case-insensitive slash-normalized comparison key.
 * @param path Full or final path to normalize.
 * @param key Receives the normalized key.
 * @return True when the path is non-empty and can be normalized.
 */
bool NormalizePathKey(const std::wstring &path, std::wstring &key) {
    std::wstring without_prefix = path;
    if (without_prefix.rfind(L"\\\\?\\", 0) == 0) {
        without_prefix.erase(0, 4);
    }

    if (without_prefix.rfind(L"UNC\\", 0) == 0) {
        without_prefix.insert(0, L"\\\\");
    }

    std::filesystem::path normalized_path(without_prefix);
    normalized_path = normalized_path.lexically_normal();
    key = normalized_path.generic_wstring();
    std::replace(key.begin(), key.end(), L'\\', L'/');
    std::transform(key.begin(), key.end(), key.begin(), towlower);
    return !key.empty() && key != L".";
}

/** @brief Recognizes device namespaces whose unrelated native behavior must remain outside file routing. */
bool IsKnownNativeNonFilesystemPath(const std::filesystem::path &path) {
    const std::wstring text = path.wstring();
    if (text.size() >= 9 && _wcsnicmp(text.c_str(), L"\\\\.\\pipe\\", 9) == 0) {
        return true;
    }

    if (_wcsicmp(text.c_str(), L"\\\\.\\NUL") == 0 || _wcsicmp(text.c_str(), L"\\\\.\\CON") == 0 ||
        _wcsicmp(text.c_str(), L"\\\\.\\AUX") == 0 || _wcsicmp(text.c_str(), L"\\\\.\\PRN") == 0 ||
        _wcsicmp(text.c_str(), L"\\\\.\\CONIN$") == 0 || _wcsicmp(text.c_str(), L"\\\\.\\CONOUT$") == 0) {
        return true;
    }

    return (text.size() >= 7 && (_wcsnicmp(text.c_str(), L"\\\\.\\COM", 7) == 0 || _wcsnicmp(text.c_str(), L"\\\\.\\LPT", 7) == 0) &&
            text.size() == 8 && iswdigit(text[7]) != 0);
}

/** @brief Recognizes device paths that must fail closed unless they are explicitly known named-pipe requests. */
bool IsUnsafeDevicePath(const std::filesystem::path &path) {
    const std::wstring text = path.wstring();
    return (text.size() >= 4 && _wcsnicmp(text.c_str(), L"\\\\.\\", 4) == 0 && !IsKnownNativeNonFilesystemPath(path)) ||
           (text.size() >= 13 && _wcsnicmp(text.c_str(), L"\\\\?\\GLOBALROOT", 13) == 0);
}

/**
 * @brief Resolves a path through Win32 so relative requests use the process current directory.
 * @param path Caller-supplied path.
 * @param request_base Optional declaration base used only when initialization resolves a relative route.
 * @param full_path Receives GetFullPathNameW output.
 * @return True when Win32 accepted the path.
 */
bool ResolveFullPath(const std::filesystem::path &path, const std::filesystem::path &request_base, std::wstring &full_path) {
    const std::filesystem::path input = path.is_relative() ? request_base / path : path;
    const std::wstring input_string = input.wstring();
    if (input_string.empty()) {
        return false;
    }

    std::vector<wchar_t> buffer(512);
    for (;;) {
        const DWORD length = GetFullPathNameW(input_string.c_str(), static_cast<DWORD>(buffer.size()), buffer.data(), nullptr);
        if (length == 0) {
            return false;
        }

        if (length < buffer.size()) {
            full_path.assign(buffer.data(), length);
            return true;
        }

        buffer.resize(static_cast<std::size_t>(length) + 1);
    }
}

/**
 * @brief Reports whether an existing component of a path is a reparse point.
 * @param full_path Absolute path whose components should be inspected.
 * @return True when any component currently has FILE_ATTRIBUTE_REPARSE_POINT.
 */
bool ContainsReparseComponent(const std::wstring &full_path) {
    std::filesystem::path path(full_path);
    std::filesystem::path current = path.root_path();
    for (const std::filesystem::path &component : path) {
        if (component == path.root_name() || component == path.root_directory()) {
            continue;
        }

        current /= component;
        const DWORD attributes = CallNativeGetFileAttributesW(current.wstring().c_str());
        if (attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0) {
            return true;
        }
    }

    return false;
}

/**
 * @brief Builds a stable identity string from one native file information record.
 * @param information File information returned by GetFileInformationByHandle.
 * @return Stable identity key.
 */
std::wstring BuildIdentity(const BY_HANDLE_FILE_INFORMATION &information) {
    return std::to_wstring(information.dwVolumeSerialNumber) + L":" + std::to_wstring(information.nFileIndexHigh) + L":" +
           std::to_wstring(information.nFileIndexLow);
}

/**
 * @brief Obtains a final path from one open file handle.
 * @param handle Open native handle.
 * @param final_path Receives the final path.
 * @return True when GetFinalPathNameByHandleW succeeds.
 */
bool GetFinalPath(HANDLE handle, std::wstring &final_path) {
    std::vector<wchar_t> buffer(512);
    for (;;) {
        const DWORD length = GetFinalPathNameByHandleW(handle, buffer.data(), static_cast<DWORD>(buffer.size()), FILE_NAME_NORMALIZED);
        if (length == 0 || length >= 32768) {
            return false;
        }

        if (length < buffer.size()) {
            final_path.assign(buffer.data(), length);
            return true;
        }

        buffer.resize(static_cast<std::size_t>(length) + 1);
    }
}

/** @brief Opens one directory with backup semantics and resolves its canonical final path. */
bool GetCanonicalDirectoryPath(const std::wstring &full_path, std::wstring &canonical_path) {
    const HANDLE handle =
        CallNativeCreateFileW(full_path.c_str(), FILE_READ_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
                              OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
    if (handle == INVALID_HANDLE_VALUE) {
        return false;
    }

    const bool result = GetFinalPath(handle, canonical_path);
    CallNativeCloseHandle(handle);
    return result;
}

/**
 * @brief Inspects an existing path for identity, regular-file status, and reparse traversal.
 * @param full_path Absolute path to inspect.
 * @param inspection Receives inspection details.
 * @param error Receives the native failure when inspection cannot open the path.
 * @return True when native metadata was obtained.
 */
bool InspectExistingPath(const std::wstring &full_path, InspectedPath &inspection, DWORD &error) {
    inspection = {};
    inspection.FullPath = full_path;
    inspection.HasReparseComponent = ContainsReparseComponent(full_path);
    const HANDLE handle =
        CallNativeCreateFileW(full_path.c_str(), FILE_READ_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
                              OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (handle == INVALID_HANDLE_VALUE) {
        error = GetLastError();
        return false;
    }

    BY_HANDLE_FILE_INFORMATION information{};
    const BOOL info_result = GetFileInformationByHandle(handle, &information);
    const DWORD info_error = info_result ? ERROR_SUCCESS : GetLastError();
    std::wstring final_path;
    const bool final_result = info_result != FALSE && GetFinalPath(handle, final_path);
    const DWORD final_error = final_result ? ERROR_SUCCESS : GetLastError();
    CallNativeCloseHandle(handle);
    if (!info_result) {
        error = info_error;
        return false;
    }

    inspection.Identity = BuildIdentity(information);
    inspection.NumberOfLinks = information.nNumberOfLinks;
    inspection.IsRegularFile = (information.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)) == 0;
    if (!inspection.IsRegularFile) {
        error = ERROR_INVALID_DATA;
        return false;
    }

    inspection.CanonicalPath = final_result ? final_path : full_path;
    error = final_result ? ERROR_SUCCESS : final_error;
    return final_result;
}

/**
 * @brief Removes one known session directory without changing the caller's meaningful Win32 error.
 * @param path Session directory owned by the routing service.
 */
void RemoveOwnedDirectory(const std::filesystem::path &path) {
    if (path.empty()) {
        return;
    }

    std::error_code error;
    std::filesystem::remove_all(path, error);
    if (error) {
        helen::Logf(L"[runtime] file-write routing cleanup failed path=%ls error=%lu", path.wstring().c_str(),
                    static_cast<unsigned long>(error.value()));
    }
}

/**
 * @brief Returns true when an access/disposition request can mutate a file.
 * @param access Desired access mask.
 * @param disposition Creation disposition.
 * @param flags File flags whose delete-on-close semantics can mutate a file.
 * @return True for write, delete, truncate, or creation semantics.
 */
bool IsWriteRequest(DWORD access, DWORD disposition, DWORD flags) {
    constexpr DWORD mutation_access = GENERIC_WRITE | GENERIC_ALL | FILE_WRITE_DATA | FILE_APPEND_DATA | FILE_WRITE_EA |
                                      FILE_WRITE_ATTRIBUTES | DELETE | WRITE_DAC | WRITE_OWNER | ACCESS_SYSTEM_SECURITY | MAXIMUM_ALLOWED;
    const bool mutation_disposition =
        disposition == CREATE_NEW || disposition == CREATE_ALWAYS || disposition == OPEN_ALWAYS || disposition == TRUNCATE_EXISTING;
    return (access & mutation_access) != 0 || mutation_disposition || (flags & FILE_FLAG_DELETE_ON_CLOSE) != 0;
}

/**
 * @brief Returns true when a route enum pair is one of the supported combinations.
 * @param write_policy Declared write policy.
 * @param read_policy Declared read policy.
 * @return True when the pair is semantically valid.
 */
bool IsValidPolicy(helen::FileWritePolicy write_policy, helen::FileReadPolicy read_policy) {
    if (write_policy == helen::FileWritePolicy::Deny) {
        return read_policy == helen::FileReadPolicy::Original;
    }

    if (write_policy == helen::FileWritePolicy::Redirect) {
        return read_policy == helen::FileReadPolicy::Original || read_policy == helen::FileReadPolicy::Redirected;
    }

    return false;
}

/**
 * @brief Rejects route declarations that could address a device namespace or alternate data stream.
 * @param path Declared route path.
 * @return True when the declaration uses a route-unsafe syntax.
 */
bool HasUnsafeRouteSyntax(const std::filesystem::path &path) {
    if (path.empty() || path.filename().empty()) {
        return true;
    }

    const std::wstring text = path.wstring();
    if (text.rfind(L"\\\\.\\", 0) == 0 || text.rfind(L"\\\\?\\GLOBALROOT", 0) == 0) {
        return true;
    }

    if (path.filename().wstring().find(L':') != std::wstring::npos) {
        return true;
    }

    for (const std::filesystem::path &component : path) {
        if (component == std::filesystem::path(L"..")) {
            return true;
        }
    }

    return false;
}

/**
 * @brief Verifies that a protected original can be opened for trusted validation with native sharing semantics.
 * @param full_path Absolute path of the protected original.
 * @param error Receives the native sharing or access failure.
 * @return True when the validation open succeeded and was closed.
 */
bool ValidateTrustedOriginalOpen(const std::wstring &full_path, DWORD &error) {
    const HANDLE handle = CallNativeCreateFileW(full_path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                                nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (handle == INVALID_HANDLE_VALUE) {
        error = GetLastError();
        return false;
    }

    const BOOL close_result = CallNativeCloseHandle(handle);
    error = close_result != FALSE ? ERROR_SUCCESS : GetLastError();
    return close_result != FALSE;
}
} // namespace

namespace helen {
FileWriteRoutingService::FileWriteRoutingService(std::filesystem::path cache_directory, std::filesystem::path request_base_directory)
    : cache_directory_(std::move(cache_directory)), request_base_directory_(std::move(request_base_directory)) {
}

FileWriteRoutingService::~FileWriteRoutingService() {
    CleanupSession();
}

bool FileWriteRoutingService::Initialize(const std::vector<FileWriteRoute> &routes, DWORD &error) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (initialized_) {
        error = ERROR_ALREADY_INITIALIZED;
        return false;
    }

    std::vector<PendingRoute> pending_routes;
    std::set<std::string> route_ids;
    std::map<std::wstring, std::string> declared_keys;
    for (const FileWriteRoute &declared_route : routes) {
        if (declared_route.Id.empty() || HasUnsafeRouteSyntax(declared_route.OriginalPath) ||
            !IsValidPolicy(declared_route.WritePolicy, declared_route.ReadPolicy) || route_ids.find(declared_route.Id) != route_ids.end()) {
            error = ERROR_INVALID_PARAMETER;
            return false;
        }

        std::wstring full_path;
        if (!ResolveFullPath(declared_route.OriginalPath, request_base_directory_, full_path)) {
            error = ERROR_INVALID_NAME;
            return false;
        }

        InspectedPath inspection;
        DWORD inspection_error = ERROR_SUCCESS;
        if (!InspectExistingPath(full_path, inspection, inspection_error) || inspection.HasReparseComponent || !inspection.IsRegularFile ||
            inspection.NumberOfLinks != 1) {
            error = inspection_error == ERROR_SUCCESS ? ERROR_INVALID_DATA : inspection_error;
            return false;
        }

        std::wstring lexical_key;
        std::wstring canonical_key;
        if (!NormalizePathKey(full_path, lexical_key) || !NormalizePathKey(inspection.CanonicalPath, canonical_key)) {
            error = ERROR_INVALID_NAME;
            return false;
        }

        const auto canonical_collision = declared_keys.find(canonical_key);
        const auto lexical_collision = declared_keys.find(lexical_key);
        if ((canonical_collision != declared_keys.end() && canonical_collision->second != declared_route.Id) ||
            (lexical_collision != declared_keys.end() && lexical_collision->second != declared_route.Id)) {
            error = ERROR_ALREADY_EXISTS;
            return false;
        }

        PendingRoute pending;
        pending.Route = declared_route;
        pending.Route.OriginalPath = std::filesystem::path(inspection.CanonicalPath);
        pending.Key = canonical_key;
        pending.LexicalKey = lexical_key;
        pending.Identity = inspection.Identity;
        pending_routes.push_back(std::move(pending));
        route_ids.insert(declared_route.Id);
        declared_keys[canonical_key] = declared_route.Id;
        declared_keys[lexical_key] = declared_route.Id;
    }

    std::error_code cache_error;
    std::filesystem::create_directories(cache_directory_, cache_error);
    if (cache_error) {
        error = ERROR_PATH_NOT_FOUND;
        return false;
    }

    std::filesystem::path session_directory;
    std::vector<wchar_t> temporary_name(MAX_PATH, L'\0');
    const UINT temporary_result = GetTempFileNameW(cache_directory_.wstring().c_str(), L"fwr", 0, temporary_name.data());
    if (temporary_result == 0) {
        error = GetLastError();
        return false;
    }

    session_directory = std::filesystem::path(temporary_name.data());
    if (!CallNativeDeleteFileW(session_directory.wstring().c_str()) ||
        !CallNativeCreateDirectoryW(session_directory.wstring().c_str(), nullptr)) {
        error = GetLastError();
        CallNativeDeleteFileW(session_directory.wstring().c_str());
        return false;
    }

    for (std::size_t index = 0; index < pending_routes.size(); ++index) {
        pending_routes[index].OverlayPath = session_directory / (L"route_" + std::to_wstring(index) + L".dat");
        if (!CallNativeCopyFileW(pending_routes[index].Route.OriginalPath.wstring().c_str(),
                                 pending_routes[index].OverlayPath.wstring().c_str(), TRUE)) {
            error = GetLastError();
            RemoveOwnedDirectory(session_directory);
            return false;
        }
    }

    session_directory_ = std::move(session_directory);
    for (PendingRoute &pending : pending_routes) {
        routes_.emplace(pending.Key, pending.Route);
        path_aliases_.emplace(pending.Key, pending.Key);
        path_aliases_.emplace(pending.LexicalKey, pending.Key);
        overlay_paths_.emplace(pending.Key, pending.OverlayPath);
        identity_routes_.emplace(pending.Identity, pending.Key);
        route_identities_.emplace(pending.Key, pending.Identity);
    }

    initialized_ = true;
    error = ERROR_SUCCESS;
    return true;
}

std::vector<FileWriteRoutingService::RouteDiagnostics> FileWriteRoutingService::GetRouteDiagnostics() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<RouteDiagnostics> diagnostics;
    diagnostics.reserve(routes_.size());
    for (const auto &[route_key, route] : routes_) {
        RouteDiagnostics detail;
        detail.Id = route.Id;
        detail.OriginalPath = route.OriginalPath;
        const auto overlay = overlay_paths_.find(route_key);
        if (overlay != overlay_paths_.end()) {
            detail.OverlayPath = overlay->second;
        }
        detail.WritePolicy = route.WritePolicy;
        detail.ReadPolicy = route.ReadPolicy;
        diagnostics.push_back(std::move(detail));
    }

    return diagnostics;
}

bool FileWriteRoutingService::TryNormalizeRequestPath(const std::filesystem::path &path, std::filesystem::path &normalized_path) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return TryNormalizeRequestPathUnlocked(path, normalized_path);
}

bool FileWriteRoutingService::TryNormalizeRequestPathUnlocked(const std::filesystem::path &path,
                                                              std::filesystem::path &normalized_path) const {
    if (IsKnownNativeNonFilesystemPath(path)) {
        normalized_path = path;
        return true;
    }

    std::wstring full_path;
    if (!ResolveFullPath(path, {}, full_path)) {
        return false;
    }

    normalized_path = std::filesystem::path(full_path);
    return !normalized_path.empty();
}

bool FileWriteRoutingService::IsProtectedPath(const std::filesystem::path &path) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::wstring route_key;
    DWORD error = ERROR_SUCCESS;
    return ClassifyPathUnlocked(path, route_key, error) != PathDisposition::Unrelated;
}

FileWriteRoutingService::PathDisposition FileWriteRoutingService::ClassifyPath(const std::filesystem::path &path) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::wstring route_key;
    DWORD error = ERROR_SUCCESS;
    return ClassifyPathUnlocked(path, route_key, error);
}

FileWriteRoutingService::PathDisposition FileWriteRoutingService::ClassifyPathUnlocked(const std::filesystem::path &path,
                                                                                       std::wstring &route_key, DWORD &error) const {
    route_key.clear();
    error = ERROR_SUCCESS;
    if (IsUnsafeDevicePath(path)) {
        error = ERROR_ACCESS_DENIED;
        return PathDisposition::Rejected;
    }
    std::wstring full_path;
    if (!ResolveFullPath(path, {}, full_path)) {
        error = ERROR_INVALID_NAME;
        return IsKnownNativeNonFilesystemPath(path) ? PathDisposition::Unrelated : PathDisposition::Rejected;
    }

    std::wstring lexical_key;
    if (!NormalizePathKey(full_path, lexical_key)) {
        error = ERROR_INVALID_NAME;
        return IsKnownNativeNonFilesystemPath(path) ? PathDisposition::Unrelated : PathDisposition::Rejected;
    }

    const auto alias = path_aliases_.find(lexical_key);
    if (alias != path_aliases_.end()) {
        route_key = alias->second;
        InspectedPath inspection;
        DWORD inspection_error = ERROR_SUCCESS;
        if (!InspectExistingPath(full_path, inspection, inspection_error)) {
            error = inspection_error == ERROR_SUCCESS ? ERROR_ACCESS_DENIED : inspection_error;
            return PathDisposition::Rejected;
        }

        if (!ValidateTrustedOriginalOpen(full_path, inspection_error)) {
            error = inspection_error == ERROR_SUCCESS ? ERROR_ACCESS_DENIED : inspection_error;
            return PathDisposition::Rejected;
        }

        const auto identity_route = identity_routes_.find(inspection.Identity);
        if (inspection.HasReparseComponent || identity_route == identity_routes_.end() || identity_route->second != route_key ||
            inspection.NumberOfLinks != 1) {
            error = ERROR_ACCESS_DENIED;
            return PathDisposition::Rejected;
        }

        return PathDisposition::Protected;
    }

    InspectedPath inspection;
    DWORD inspection_error = ERROR_SUCCESS;
    if (!InspectExistingPath(full_path, inspection, inspection_error)) {
        return PathDisposition::Unrelated;
    }

    const auto identity_route = identity_routes_.find(inspection.Identity);
    if (identity_route == identity_routes_.end()) {
        return PathDisposition::Unrelated;
    }

    route_key = identity_route->second;
    if (inspection.HasReparseComponent || inspection.NumberOfLinks != 1) {
        error = ERROR_ACCESS_DENIED;
        return PathDisposition::Rejected;
    }

    if (!ValidateTrustedOriginalOpen(full_path, inspection_error)) {
        error = inspection_error == ERROR_SUCCESS ? ERROR_ACCESS_DENIED : inspection_error;
        return PathDisposition::Rejected;
    }

    return PathDisposition::Protected;
}

bool FileWriteRoutingService::IsTrackedHandle(HANDLE handle) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return tracked_handles_.find(handle) != tracked_handles_.end();
}

FileWriteRoutingService::PathDisposition FileWriteRoutingService::ClassifyHandle(HANDLE handle) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::wstring final_path;
    if (!GetFinalPath(handle, final_path)) {
        return GetFileType(handle) == FILE_TYPE_DISK ? PathDisposition::Rejected : PathDisposition::Unrelated;
    }

    std::wstring route_key;
    DWORD error = ERROR_SUCCESS;
    return ClassifyPathUnlocked(std::filesystem::path(final_path), route_key, error);
}

HANDLE FileWriteRoutingService::Open(const std::filesystem::path &path, DWORD access, DWORD share,
                                     LPSECURITY_ATTRIBUTES security_attributes, DWORD disposition, DWORD flags, HANDLE template_file) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::filesystem::path effective_path;
    const bool normalized = TryNormalizeRequestPathUnlocked(path, effective_path);
    if (!normalized) {
        effective_path = path;
    }
    std::wstring route_key;
    DWORD classification_error = ERROR_SUCCESS;
    const PathDisposition path_disposition = ClassifyPathUnlocked(effective_path, route_key, classification_error);
    if (path_disposition == PathDisposition::Unrelated) {
        return CallNativeCreateFileW(effective_path.wstring().c_str(), access, share, security_attributes, disposition, flags,
                                     template_file);
    }

    if (path_disposition == PathDisposition::Rejected) {
        SetLastError(classification_error == ERROR_SUCCESS ? ERROR_ACCESS_DENIED : classification_error);
        return INVALID_HANDLE_VALUE;
    }

    const auto route = routes_.find(route_key);
    if (route == routes_.end() || failed_routes_.find(route_key) != failed_routes_.end()) {
        SetLastError(ERROR_WRITE_FAULT);
        return INVALID_HANDLE_VALUE;
    }

    if ((security_attributes != nullptr && security_attributes->bInheritHandle != FALSE) || (flags & FILE_FLAG_OVERLAPPED) != 0) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return INVALID_HANDLE_VALUE;
    }

    const bool write_request = IsWriteRequest(access, disposition, flags);
    if (route->second.WritePolicy == FileWritePolicy::Deny && write_request) {
        SetLastError(ERROR_ACCESS_DENIED);
        return INVALID_HANDLE_VALUE;
    }

    std::filesystem::path target_path = route->second.OriginalPath;
    if (route->second.WritePolicy == FileWritePolicy::Redirect &&
        (write_request || route->second.ReadPolicy == FileReadPolicy::Redirected)) {
        target_path = overlay_paths_.at(route_key);
    }

    const HANDLE handle =
        CallNativeCreateFileW(target_path.wstring().c_str(), access, share, security_attributes, disposition, flags, template_file);
    if (handle != INVALID_HANDLE_VALUE) {
        tracked_handles_.emplace(handle, route_key);
    }

    return handle;
}

BOOL FileWriteRoutingService::Close(HANDLE handle) {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto tracked = tracked_handles_.find(handle);
    if (tracked == tracked_handles_.end()) {
        SetLastError(ERROR_INVALID_HANDLE);
        return FALSE;
    }

    const BOOL result = CallNativeCloseHandle(handle);
    if (result != FALSE) {
        tracked_handles_.erase(tracked);
    }

    return result;
}

DWORD FileWriteRoutingService::GetAttributes(const std::filesystem::path &path) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::filesystem::path effective_path;
    const bool normalized = TryNormalizeRequestPathUnlocked(path, effective_path);
    if (!normalized) {
        effective_path = path;
    }
    std::wstring route_key;
    DWORD classification_error = ERROR_SUCCESS;
    const PathDisposition path_disposition = ClassifyPathUnlocked(effective_path, route_key, classification_error);
    if (path_disposition == PathDisposition::Unrelated) {
        return CallNativeGetFileAttributesW(effective_path.wstring().c_str());
    }

    if (path_disposition == PathDisposition::Rejected) {
        SetLastError(classification_error == ERROR_SUCCESS ? ERROR_ACCESS_DENIED : classification_error);
        return INVALID_FILE_ATTRIBUTES;
    }

    if (failed_routes_.find(route_key) != failed_routes_.end()) {
        SetLastError(ERROR_WRITE_FAULT);
        return INVALID_FILE_ATTRIBUTES;
    }

    const FileWriteRoute &route = routes_.at(route_key);
    const std::filesystem::path &source_path =
        route.WritePolicy == FileWritePolicy::Redirect && route.ReadPolicy == FileReadPolicy::Redirected ? overlay_paths_.at(route_key)
                                                                                                         : route.OriginalPath;
    return CallNativeGetFileAttributesW(source_path.wstring().c_str());
}

BOOL FileWriteRoutingService::Delete(const std::filesystem::path &path) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::filesystem::path effective_path;
    const bool normalized = TryNormalizeRequestPathUnlocked(path, effective_path);
    if (!normalized) {
        effective_path = path;
    }
    std::wstring route_key;
    DWORD classification_error = ERROR_SUCCESS;
    const PathDisposition disposition = ClassifyPathUnlocked(effective_path, route_key, classification_error);
    if (disposition == PathDisposition::Unrelated) {
        return CallNativeDeleteFileW(effective_path.wstring().c_str());
    }

    if (disposition == PathDisposition::Rejected) {
        SetLastError(classification_error == ERROR_SUCCESS ? ERROR_ACCESS_DENIED : classification_error);
        return FALSE;
    }

    const auto route = routes_.find(route_key);
    if (route == routes_.end() || failed_routes_.find(route_key) != failed_routes_.end() ||
        route->second.WritePolicy == FileWritePolicy::Deny) {
        SetLastError(route == routes_.end() || failed_routes_.find(route_key) != failed_routes_.end() ? ERROR_WRITE_FAULT
                                                                                                      : ERROR_ACCESS_DENIED);
        return FALSE;
    }

    for (const auto &tracked : tracked_handles_) {
        if (tracked.second == route_key) {
            SetLastError(ERROR_SHARING_VIOLATION);
            return FALSE;
        }
    }

    return CallNativeDeleteFileW(overlay_paths_.at(route_key).wstring().c_str());
}

BOOL FileWriteRoutingService::Move(const std::filesystem::path &existing_path, const std::filesystem::path &new_path, DWORD flags) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::filesystem::path effective_existing_path;
    std::filesystem::path effective_new_path;
    const bool normalized_existing = TryNormalizeRequestPathUnlocked(existing_path, effective_existing_path);
    const bool normalized_new = TryNormalizeRequestPathUnlocked(new_path, effective_new_path);
    if (!normalized_existing) {
        effective_existing_path = existing_path;
    }
    if (!normalized_new) {
        effective_new_path = new_path;
    }
    std::wstring source_route;
    std::wstring destination_route;
    DWORD source_error = ERROR_SUCCESS;
    DWORD destination_error = ERROR_SUCCESS;
    const PathDisposition source_disposition = ClassifyPathUnlocked(effective_existing_path, source_route, source_error);
    const PathDisposition destination_disposition = ClassifyPathUnlocked(effective_new_path, destination_route, destination_error);
    if (source_disposition == PathDisposition::Rejected || destination_disposition == PathDisposition::Rejected) {
        SetLastError(source_disposition == PathDisposition::Rejected
                         ? (source_error == ERROR_SUCCESS ? ERROR_ACCESS_DENIED : source_error)
                         : (destination_error == ERROR_SUCCESS ? ERROR_ACCESS_DENIED : destination_error));
        return FALSE;
    }

    if ((flags & (MOVEFILE_DELAY_UNTIL_REBOOT | MOVEFILE_COPY_ALLOWED)) != 0 &&
        (source_disposition == PathDisposition::Protected || destination_disposition == PathDisposition::Protected)) {
        SetLastError(ERROR_NOT_SUPPORTED);
        return FALSE;
    }

    if (IsProtectedParentPathUnlocked(effective_existing_path) || IsProtectedParentPathUnlocked(effective_new_path)) {
        SetLastError(ERROR_ACCESS_DENIED);
        return FALSE;
    }

    if (source_disposition == PathDisposition::Protected &&
        (destination_disposition != PathDisposition::Protected || source_route != destination_route)) {
        SetLastError(ERROR_ACCESS_DENIED);
        return FALSE;
    }

    if (source_disposition == PathDisposition::Protected && destination_disposition == PathDisposition::Protected) {
        SetLastError(ERROR_NOT_SUPPORTED);
        return FALSE;
    }

    if (destination_disposition == PathDisposition::Unrelated) {
        return CallNativeMoveFileExW(effective_existing_path.wstring().c_str(), effective_new_path.wstring().c_str(), flags);
    }

    const auto route = routes_.find(destination_route);
    if (route == routes_.end() || failed_routes_.find(destination_route) != failed_routes_.end() ||
        route->second.WritePolicy == FileWritePolicy::Deny) {
        SetLastError(route == routes_.end() || failed_routes_.find(destination_route) != failed_routes_.end() ? ERROR_WRITE_FAULT
                                                                                                              : ERROR_ACCESS_DENIED);
        return FALSE;
    }

    for (const auto &tracked : tracked_handles_) {
        if (tracked.second == destination_route) {
            SetLastError(ERROR_SHARING_VIOLATION);
            return FALSE;
        }
    }

    return CallNativeMoveFileExW(effective_existing_path.wstring().c_str(), overlay_paths_.at(destination_route).wstring().c_str(), flags);
}

BOOL FileWriteRoutingService::Replace(const std::filesystem::path &replaced_path, const std::filesystem::path &replacement_path,
                                      LPCWSTR backup_file_name, DWORD replace_flags, LPVOID exclude, LPVOID reserved) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (exclude != nullptr || reserved != nullptr) {
        SetLastError(ERROR_NOT_SUPPORTED);
        return FALSE;
    }

    std::filesystem::path effective_replaced_path;
    std::filesystem::path effective_replacement_path;
    const bool normalized_replaced = TryNormalizeRequestPathUnlocked(replaced_path, effective_replaced_path);
    const bool normalized_replacement = TryNormalizeRequestPathUnlocked(replacement_path, effective_replacement_path);
    if (!normalized_replaced) {
        effective_replaced_path = replaced_path;
    }
    if (!normalized_replacement) {
        effective_replacement_path = replacement_path;
    }
    std::filesystem::path effective_backup_path;
    if (backup_file_name != nullptr) {
        if (!TryNormalizeRequestPathUnlocked(std::filesystem::path(backup_file_name), effective_backup_path)) {
            effective_backup_path = std::filesystem::path(backup_file_name);
        }
    }

    std::wstring replaced_route;
    std::wstring replacement_route;
    DWORD replaced_error = ERROR_SUCCESS;
    DWORD replacement_error = ERROR_SUCCESS;
    const PathDisposition replaced_disposition = ClassifyPathUnlocked(effective_replaced_path, replaced_route, replaced_error);
    const PathDisposition replacement_disposition = ClassifyPathUnlocked(effective_replacement_path, replacement_route, replacement_error);
    if (replaced_disposition == PathDisposition::Rejected || replacement_disposition == PathDisposition::Rejected) {
        SetLastError(replaced_disposition == PathDisposition::Rejected
                         ? (replaced_error == ERROR_SUCCESS ? ERROR_ACCESS_DENIED : replaced_error)
                         : (replacement_error == ERROR_SUCCESS ? ERROR_ACCESS_DENIED : replacement_error));
        return FALSE;
    }

    if (replaced_disposition == PathDisposition::Unrelated && replacement_disposition == PathDisposition::Unrelated) {
        if (backup_file_name != nullptr) {
            std::wstring backup_route;
            DWORD backup_error = ERROR_SUCCESS;
            const PathDisposition backup_disposition = ClassifyPathUnlocked(effective_backup_path, backup_route, backup_error);
            if (backup_disposition != PathDisposition::Unrelated) {
                SetLastError(backup_disposition == PathDisposition::Rejected
                                 ? (backup_error == ERROR_SUCCESS ? ERROR_ACCESS_DENIED : backup_error)
                                 : ERROR_NOT_SUPPORTED);
                return FALSE;
            }
        }
        return CallNativeReplaceFileW(effective_replaced_path.wstring().c_str(), effective_replacement_path.wstring().c_str(),
                                      backup_file_name == nullptr ? nullptr : effective_backup_path.wstring().c_str(), replace_flags,
                                      nullptr, nullptr);
    }

    if (backup_file_name != nullptr) {
        SetLastError(ERROR_NOT_SUPPORTED);
        return FALSE;
    }

    if (replaced_disposition != PathDisposition::Protected || replacement_disposition != PathDisposition::Unrelated) {
        SetLastError(ERROR_NOT_SUPPORTED);
        return FALSE;
    }

    const auto route = routes_.find(replaced_route);
    if (route == routes_.end() || failed_routes_.find(replaced_route) != failed_routes_.end() ||
        route->second.WritePolicy == FileWritePolicy::Deny) {
        SetLastError(route == routes_.end() || failed_routes_.find(replaced_route) != failed_routes_.end() ? ERROR_WRITE_FAULT
                                                                                                           : ERROR_ACCESS_DENIED);
        return FALSE;
    }

    for (const auto &tracked : tracked_handles_) {
        if (tracked.second == replaced_route) {
            SetLastError(ERROR_SHARING_VIOLATION);
            return FALSE;
        }
    }

    return CallNativeReplaceFileW(overlay_paths_.at(replaced_route).wstring().c_str(), effective_replacement_path.wstring().c_str(),
                                  nullptr, replace_flags, nullptr, nullptr);
}

BOOL FileWriteRoutingService::Copy(const std::filesystem::path &existing_path, const std::filesystem::path &new_path, BOOL fail_if_exists) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::filesystem::path effective_existing_path;
    std::filesystem::path effective_new_path;
    const bool normalized_existing = TryNormalizeRequestPathUnlocked(existing_path, effective_existing_path);
    const bool normalized_new = TryNormalizeRequestPathUnlocked(new_path, effective_new_path);
    if (!normalized_existing) {
        effective_existing_path = existing_path;
    }
    if (!normalized_new) {
        effective_new_path = new_path;
    }
    std::wstring source_route;
    std::wstring destination_route;
    DWORD source_error = ERROR_SUCCESS;
    DWORD destination_error = ERROR_SUCCESS;
    const PathDisposition source_disposition = ClassifyPathUnlocked(effective_existing_path, source_route, source_error);
    const PathDisposition destination_disposition = ClassifyPathUnlocked(effective_new_path, destination_route, destination_error);
    if (source_disposition == PathDisposition::Rejected || destination_disposition == PathDisposition::Rejected) {
        SetLastError(source_disposition == PathDisposition::Rejected
                         ? (source_error == ERROR_SUCCESS ? ERROR_ACCESS_DENIED : source_error)
                         : (destination_error == ERROR_SUCCESS ? ERROR_ACCESS_DENIED : destination_error));
        return FALSE;
    }

    if (source_disposition == PathDisposition::Protected && destination_disposition == PathDisposition::Unrelated) {
        SetLastError(ERROR_ACCESS_DENIED);
        return FALSE;
    }

    if (source_disposition == PathDisposition::Protected && destination_disposition == PathDisposition::Protected) {
        SetLastError(ERROR_NOT_SUPPORTED);
        return FALSE;
    }

    if (destination_disposition == PathDisposition::Unrelated) {
        return CallNativeCopyFileW(effective_existing_path.wstring().c_str(), effective_new_path.wstring().c_str(), fail_if_exists);
    }

    const auto route = routes_.find(destination_route);
    if (route == routes_.end() || failed_routes_.find(destination_route) != failed_routes_.end() ||
        route->second.WritePolicy == FileWritePolicy::Deny) {
        SetLastError(route == routes_.end() || failed_routes_.find(destination_route) != failed_routes_.end() ? ERROR_WRITE_FAULT
                                                                                                              : ERROR_ACCESS_DENIED);
        return FALSE;
    }

    for (const auto &tracked : tracked_handles_) {
        if (tracked.second == destination_route) {
            SetLastError(ERROR_SHARING_VIOLATION);
            return FALSE;
        }
    }

    return CallNativeCopyFileW(effective_existing_path.wstring().c_str(), overlay_paths_.at(destination_route).wstring().c_str(),
                               fail_if_exists);
}

BOOL FileWriteRoutingService::SetAttributes(const std::filesystem::path &path, DWORD attributes) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::filesystem::path effective_path;
    const bool normalized = TryNormalizeRequestPathUnlocked(path, effective_path);
    if (!normalized) {
        effective_path = path;
    }
    std::wstring route_key;
    DWORD classification_error = ERROR_SUCCESS;
    const PathDisposition disposition = ClassifyPathUnlocked(effective_path, route_key, classification_error);
    if (disposition == PathDisposition::Unrelated) {
        return CallNativeSetFileAttributesW(effective_path.wstring().c_str(), attributes);
    }

    if (disposition == PathDisposition::Rejected) {
        SetLastError(classification_error == ERROR_SUCCESS ? ERROR_ACCESS_DENIED : classification_error);
        return FALSE;
    }

    const auto route = routes_.find(route_key);
    if (route == routes_.end() || failed_routes_.find(route_key) != failed_routes_.end() ||
        route->second.WritePolicy == FileWritePolicy::Deny) {
        SetLastError(route == routes_.end() || failed_routes_.find(route_key) != failed_routes_.end() ? ERROR_WRITE_FAULT
                                                                                                      : ERROR_ACCESS_DENIED);
        return FALSE;
    }

    return CallNativeSetFileAttributesW(overlay_paths_.at(route_key).wstring().c_str(), attributes);
}

bool FileWriteRoutingService::IsProtectedParentPath(const std::filesystem::path &path) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return IsProtectedParentPathUnlocked(path);
}

bool FileWriteRoutingService::IsProtectedParentPathUnlocked(const std::filesystem::path &path) const {
    std::wstring full_path;
    if (!ResolveFullPath(path, {}, full_path)) {
        return false;
    }

    std::wstring key;
    if (!NormalizePathKey(full_path, key)) {
        return !IsKnownNativeNonFilesystemPath(path);
    }

    std::wstring canonical_path;
    std::wstring canonical_key;
    if (GetCanonicalDirectoryPath(full_path, canonical_path) && NormalizePathKey(canonical_path, canonical_key)) {
        key = canonical_key;
    } else if (CallNativeGetFileAttributesW(full_path.c_str()) != INVALID_FILE_ATTRIBUTES) {
        return true;
    }

    if (!key.empty() && key.back() != L'/') {
        key.push_back(L'/');
    }

    for (const auto &route : routes_) {
        if (route.first.rfind(key, 0) == 0) {
            return true;
        }
    }

    return false;
}

std::unique_ptr<FileWriteRoutingTransaction> FileWriteRoutingService::BeginTrustedWrite(
    const std::vector<std::filesystem::path> &original_paths, DWORD &error) {
    std::unique_lock<std::mutex> lock(mutex_);
    std::vector<std::wstring> route_keys;
    for (const std::filesystem::path &path : original_paths) {
        std::filesystem::path effective_path;
        const bool normalized = TryNormalizeRequestPathUnlocked(path, effective_path);
        if (!normalized) {
            effective_path = path;
        }
        std::wstring route_key;
        DWORD classification_error = ERROR_SUCCESS;
        const PathDisposition path_disposition = ClassifyPathUnlocked(effective_path, route_key, classification_error);
        if (path_disposition == PathDisposition::Rejected) {
            error = classification_error == ERROR_SUCCESS ? ERROR_ACCESS_DENIED : classification_error;
            return nullptr;
        }

        if (path_disposition == PathDisposition::Protected &&
            std::find(route_keys.begin(), route_keys.end(), route_key) == route_keys.end()) {
            route_keys.push_back(route_key);
        }
    }

    for (const auto &tracked : tracked_handles_) {
        if (std::find(route_keys.begin(), route_keys.end(), tracked.second) != route_keys.end()) {
            error = ERROR_SHARING_VIOLATION;
            return nullptr;
        }
    }

    for (const std::wstring &route_key : route_keys) {
        if (failed_routes_.find(route_key) != failed_routes_.end()) {
            error = ERROR_WRITE_FAULT;
            return nullptr;
        }
    }

    error = ERROR_SUCCESS;
    return std::unique_ptr<FileWriteRoutingTransaction>(new FileWriteRoutingTransaction(*this, std::move(route_keys), std::move(lock)));
}

bool FileWriteRoutingService::SynchronizeRoutes(const std::vector<std::wstring> &route_keys, DWORD &error) {
    for (const std::wstring &route_key : route_keys) {
        const auto route = routes_.find(route_key);
        const auto overlay = overlay_paths_.find(route_key);
        const auto identity = route_identities_.find(route_key);
        if (route == routes_.end() || overlay == overlay_paths_.end()) {
            error = ERROR_INVALID_DATA;
            LatchRoutesFailed(route_keys);
            return false;
        }

        InspectedPath inspection;
        DWORD inspection_error = ERROR_SUCCESS;
        std::wstring full_path;
        if (!ResolveFullPath(route->second.OriginalPath, request_base_directory_, full_path) ||
            !InspectExistingPath(full_path, inspection, inspection_error) || inspection.HasReparseComponent || !inspection.IsRegularFile ||
            inspection.NumberOfLinks != 1) {
            error = inspection_error == ERROR_SUCCESS ? ERROR_INVALID_DATA : inspection_error;
            LatchRoutesFailed(route_keys);
            return false;
        }

        const auto current_owner = identity_routes_.find(inspection.Identity);
        if (current_owner != identity_routes_.end() && current_owner->second != route_key) {
            error = ERROR_INVALID_DATA;
            LatchRoutesFailed(route_keys);
            return false;
        }

        if (!CallNativeCopyFileW(route->second.OriginalPath.wstring().c_str(), overlay->second.wstring().c_str(), FALSE)) {
            error = GetLastError();
            LatchRoutesFailed(route_keys);
            return false;
        }

        if (identity != route_identities_.end()) {
            identity_routes_.erase(identity->second);
        }

        route_identities_[route_key] = inspection.Identity;
        identity_routes_[inspection.Identity] = route_key;
        std::wstring canonical_key;
        if (NormalizePathKey(inspection.CanonicalPath, canonical_key)) {
            path_aliases_[canonical_key] = route_key;
        }
    }

    error = ERROR_SUCCESS;
    return true;
}

void FileWriteRoutingService::LatchRoutesFailed(const std::vector<std::wstring> &route_keys) {
    for (const std::wstring &route_key : route_keys) {
        failed_routes_.insert(route_key);
    }
}

void FileWriteRoutingService::CleanupSession() noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    RemoveOwnedDirectory(session_directory_);
    session_directory_.clear();
}
} // namespace helen
