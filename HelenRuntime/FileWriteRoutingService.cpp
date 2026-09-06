#include <HelenHook/FileWriteRoutingService.h>

#include <HelenHook/FileWriteRoutingTransaction.h>

#include <algorithm>
#include <cwctype>
#include <filesystem>
#include <map>
#include <set>
#include <string>
#include <system_error>
#include <vector>

namespace
{
    /**
     * @brief Captures the stable identity and canonical name of one inspected native file.
     */
    struct InspectedPath
    {
        /** @brief Full lexical path produced by GetFullPathNameW. */
        std::wstring FullPath;

        /** @brief Canonical final path produced by GetFinalPathNameByHandleW. */
        std::wstring CanonicalPath;

        /** @brief Volume serial and file-index key for hard-link and rename detection. */
        std::wstring Identity;

        /** @brief True when any existing path component is a reparse point. */
        bool HasReparseComponent = false;

        /** @brief True when the final object is a regular file rather than a directory or reparse object. */
        bool IsRegularFile = false;

        /** @brief Number of native hard links reported for the final object. */
        DWORD NumberOfLinks = 0;
    };

    /**
     * @brief Captures one validated route before service state is committed atomically.
     */
    struct PendingRoute
    {
        /** @brief Route copy with its original path resolved to the canonical native path. */
        helen::FileWriteRoute Route;

        /** @brief Canonical lower-case path key. */
        std::wstring Key;

        /** @brief Lexical lower-case path key accepted as an alias. */
        std::wstring LexicalKey;

        /** @brief Stable native file identity. */
        std::wstring Identity;

        /** @brief Session overlay path. */
        std::filesystem::path OverlayPath;
    };

    /**
     * @brief Converts a Win32 path into one case-insensitive slash-normalized comparison key.
     * @param path Full or final path to normalize.
     * @param key Receives the normalized key.
     * @return True when the path is non-empty and can be normalized.
     */
    bool NormalizePathKey(const std::wstring& path, std::wstring& key)
    {
        std::wstring without_prefix = path;
        if (without_prefix.rfind(L"\\\\?\\", 0) == 0)
        {
            without_prefix.erase(0, 4);
        }

        if (without_prefix.rfind(L"UNC\\", 0) == 0)
        {
            without_prefix.insert(0, L"\\\\");
        }

        std::filesystem::path normalized_path(without_prefix);
        normalized_path = normalized_path.lexically_normal();
        key = normalized_path.generic_wstring();
        std::replace(key.begin(), key.end(), L'\\', L'/');
        std::transform(key.begin(), key.end(), key.begin(), towlower);
        return !key.empty() && key != L".";
    }

    /**
     * @brief Resolves a possibly relative request against the service request base.
     * @param path Caller-supplied path.
     * @param request_base Base directory used for relative requests.
     * @param full_path Receives GetFullPathNameW output.
     * @return True when Win32 accepted the path.
     */
    bool ResolveFullPath(const std::filesystem::path& path, const std::filesystem::path& request_base, std::wstring& full_path)
    {
        const std::filesystem::path input = path.is_relative() ? request_base / path : path;
        const std::wstring input_string = input.wstring();
        if (input_string.empty())
        {
            return false;
        }

        std::vector<wchar_t> buffer(512);
        for (;;)
        {
            const DWORD length = GetFullPathNameW(input_string.c_str(), static_cast<DWORD>(buffer.size()), buffer.data(), nullptr);
            if (length == 0)
            {
                return false;
            }

            if (length < buffer.size())
            {
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
    bool ContainsReparseComponent(const std::wstring& full_path)
    {
        std::filesystem::path path(full_path);
        std::filesystem::path current = path.root_path();
        for (const std::filesystem::path& component : path)
        {
            if (component == path.root_name() || component == path.root_directory())
            {
                continue;
            }

            current /= component;
            const DWORD attributes = GetFileAttributesW(current.wstring().c_str());
            if (attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0)
            {
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
    std::wstring BuildIdentity(const BY_HANDLE_FILE_INFORMATION& information)
    {
        return std::to_wstring(information.dwVolumeSerialNumber) + L":" +
            std::to_wstring(information.nFileIndexHigh) + L":" +
            std::to_wstring(information.nFileIndexLow);
    }

    /**
     * @brief Obtains a final path from one open file handle.
     * @param handle Open native handle.
     * @param final_path Receives the final path.
     * @return True when GetFinalPathNameByHandleW succeeds.
     */
    bool GetFinalPath(HANDLE handle, std::wstring& final_path)
    {
        std::vector<wchar_t> buffer(512);
        for (;;)
        {
            const DWORD length = GetFinalPathNameByHandleW(handle, buffer.data(), static_cast<DWORD>(buffer.size()), FILE_NAME_NORMALIZED);
            if (length == 0 || length >= 32768)
            {
                return false;
            }

            if (length < buffer.size())
            {
                final_path.assign(buffer.data(), length);
                return true;
            }

            buffer.resize(static_cast<std::size_t>(length) + 1);
        }
    }

    /**
     * @brief Inspects an existing path for identity, regular-file status, and reparse traversal.
     * @param full_path Absolute path to inspect.
     * @param inspection Receives inspection details.
     * @param error Receives the native failure when inspection cannot open the path.
     * @return True when native metadata was obtained.
     */
    bool InspectExistingPath(const std::wstring& full_path, InspectedPath& inspection, DWORD& error)
    {
        inspection = {};
        inspection.FullPath = full_path;
        inspection.HasReparseComponent = ContainsReparseComponent(full_path);
        const HANDLE handle = CreateFileW(
            full_path.c_str(),
            FILE_READ_ATTRIBUTES,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            nullptr,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            nullptr);
        if (handle == INVALID_HANDLE_VALUE)
        {
            error = GetLastError();
            return false;
        }

        BY_HANDLE_FILE_INFORMATION information{};
        const BOOL info_result = GetFileInformationByHandle(handle, &information);
        const DWORD info_error = info_result ? ERROR_SUCCESS : GetLastError();
        std::wstring final_path;
        const bool final_result = info_result != FALSE && GetFinalPath(handle, final_path);
        const DWORD final_error = final_result ? ERROR_SUCCESS : GetLastError();
        CloseHandle(handle);
        if (!info_result)
        {
            error = info_error;
            return false;
        }

        inspection.Identity = BuildIdentity(information);
        inspection.NumberOfLinks = information.nNumberOfLinks;
        inspection.IsRegularFile = (information.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)) == 0;
        if (!inspection.IsRegularFile)
        {
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
    void RemoveOwnedDirectory(const std::filesystem::path& path)
    {
        if (path.empty())
        {
            return;
        }

        std::error_code error;
        std::filesystem::remove_all(path, error);
    }

    /**
     * @brief Returns true when an access/disposition request can mutate a file.
     * @param access Desired access mask.
     * @param disposition Creation disposition.
     * @return True for write, delete, truncate, or creation semantics.
     */
    bool IsWriteRequest(DWORD access, DWORD disposition)
    {
        constexpr DWORD write_access = GENERIC_WRITE | FILE_WRITE_DATA | FILE_APPEND_DATA | FILE_WRITE_EA | FILE_WRITE_ATTRIBUTES | DELETE;
        return (access & write_access) != 0 || disposition == CREATE_NEW || disposition == CREATE_ALWAYS || disposition == OPEN_ALWAYS || disposition == TRUNCATE_EXISTING;
    }

    /**
     * @brief Returns true when a route enum pair is one of the supported combinations.
     * @param write_policy Declared write policy.
     * @param read_policy Declared read policy.
     * @return True when the pair is semantically valid.
     */
    bool IsValidPolicy(helen::FileWritePolicy write_policy, helen::FileReadPolicy read_policy)
    {
        if (write_policy == helen::FileWritePolicy::Deny)
        {
            return read_policy == helen::FileReadPolicy::Original;
        }

        if (write_policy == helen::FileWritePolicy::Redirect)
        {
            return read_policy == helen::FileReadPolicy::Original || read_policy == helen::FileReadPolicy::Redirected;
        }

        return false;
    }

    /**
     * @brief Rejects route declarations that could address a device namespace or alternate data stream.
     * @param path Declared route path.
     * @return True when the declaration uses a route-unsafe syntax.
     */
    bool HasUnsafeRouteSyntax(const std::filesystem::path& path)
    {
        if (path.empty() || path.filename().empty())
        {
            return true;
        }

        const std::wstring text = path.wstring();
        if (text.rfind(L"\\\\.\\", 0) == 0 || text.rfind(L"\\\\?\\GLOBALROOT", 0) == 0)
        {
            return true;
        }

        if (path.filename().wstring().find(L':') != std::wstring::npos)
        {
            return true;
        }

        for (const std::filesystem::path& component : path)
        {
            if (component == std::filesystem::path(L".."))
            {
                return true;
            }
        }

        return false;
    }
}

namespace helen
{
    FileWriteRoutingService::FileWriteRoutingService(std::filesystem::path cache_directory, std::filesystem::path request_base_directory)
        : cache_directory_(std::move(cache_directory)),
          request_base_directory_(std::move(request_base_directory))
    {
    }

    FileWriteRoutingService::~FileWriteRoutingService()
    {
        CleanupSession();
    }

    bool FileWriteRoutingService::Initialize(const std::vector<FileWriteRoute>& routes, DWORD& error)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (initialized_)
        {
            error = ERROR_ALREADY_INITIALIZED;
            return false;
        }

        std::vector<PendingRoute> pending_routes;
        std::set<std::string> route_ids;
        std::map<std::wstring, std::string> declared_keys;
        for (const FileWriteRoute& declared_route : routes)
        {
            if (declared_route.Id.empty() || HasUnsafeRouteSyntax(declared_route.OriginalPath) || !IsValidPolicy(declared_route.WritePolicy, declared_route.ReadPolicy) || route_ids.find(declared_route.Id) != route_ids.end())
            {
                error = ERROR_INVALID_PARAMETER;
                return false;
            }

            std::wstring full_path;
            if (!ResolveFullPath(declared_route.OriginalPath, request_base_directory_, full_path))
            {
                error = ERROR_INVALID_NAME;
                return false;
            }

            InspectedPath inspection;
            DWORD inspection_error = ERROR_SUCCESS;
            if (!InspectExistingPath(full_path, inspection, inspection_error) || inspection.HasReparseComponent || !inspection.IsRegularFile || inspection.NumberOfLinks != 1)
            {
                error = inspection_error == ERROR_SUCCESS ? ERROR_INVALID_DATA : inspection_error;
                return false;
            }

            std::wstring lexical_key;
            std::wstring canonical_key;
            if (!NormalizePathKey(full_path, lexical_key) || !NormalizePathKey(inspection.CanonicalPath, canonical_key))
            {
                error = ERROR_INVALID_NAME;
                return false;
            }

            const auto canonical_collision = declared_keys.find(canonical_key);
            const auto lexical_collision = declared_keys.find(lexical_key);
            if ((canonical_collision != declared_keys.end() && canonical_collision->second != declared_route.Id) ||
                (lexical_collision != declared_keys.end() && lexical_collision->second != declared_route.Id))
            {
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
        if (cache_error)
        {
            error = ERROR_PATH_NOT_FOUND;
            return false;
        }

        std::filesystem::path session_directory;
        std::vector<wchar_t> temporary_name(MAX_PATH, L'\0');
        const UINT temporary_result = GetTempFileNameW(cache_directory_.wstring().c_str(), L"fwr", 0, temporary_name.data());
        if (temporary_result == 0)
        {
            error = GetLastError();
            return false;
        }

        session_directory = std::filesystem::path(temporary_name.data());
        if (!DeleteFileW(session_directory.wstring().c_str()) || !CreateDirectoryW(session_directory.wstring().c_str(), nullptr))
        {
            error = GetLastError();
            DeleteFileW(session_directory.wstring().c_str());
            return false;
        }

        for (std::size_t index = 0; index < pending_routes.size(); ++index)
        {
            pending_routes[index].OverlayPath = session_directory / (L"route_" + std::to_wstring(index) + L".dat");
            if (!CopyFileW(pending_routes[index].Route.OriginalPath.wstring().c_str(), pending_routes[index].OverlayPath.wstring().c_str(), TRUE))
            {
                error = GetLastError();
                RemoveOwnedDirectory(session_directory);
                return false;
            }
        }

        session_directory_ = std::move(session_directory);
        for (PendingRoute& pending : pending_routes)
        {
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

    bool FileWriteRoutingService::IsProtectedPath(const std::filesystem::path& path) const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        std::wstring full_path;
        if (!ResolveFullPath(path, request_base_directory_, full_path))
        {
            return false;
        }

        std::wstring lexical_key;
        if (!NormalizePathKey(full_path, lexical_key))
        {
            return false;
        }

        if (path_aliases_.find(lexical_key) != path_aliases_.end())
        {
            return true;
        }

        InspectedPath inspection;
        DWORD inspection_error = ERROR_SUCCESS;
        if (InspectExistingPath(full_path, inspection, inspection_error))
        {
            return identity_routes_.find(inspection.Identity) != identity_routes_.end();
        }

        return false;
    }

    bool FileWriteRoutingService::IsTrackedHandle(HANDLE handle) const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return tracked_handles_.find(handle) != tracked_handles_.end();
    }

    HANDLE FileWriteRoutingService::Open(
        const std::filesystem::path& path,
        DWORD access,
        DWORD share,
        LPSECURITY_ATTRIBUTES security_attributes,
        DWORD disposition,
        DWORD flags,
        HANDLE template_file)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        std::wstring full_path;
        const bool has_full_path = ResolveFullPath(path, request_base_directory_, full_path);
        std::wstring lexical_key;
        const bool has_lexical_key = has_full_path && NormalizePathKey(full_path, lexical_key);
        std::wstring route_key;
        bool unsafe_alias = false;
        if (has_lexical_key)
        {
            const auto alias = path_aliases_.find(lexical_key);
            if (alias != path_aliases_.end())
            {
                route_key = alias->second;
                InspectedPath inspection;
                DWORD inspection_error = ERROR_SUCCESS;
                if (InspectExistingPath(full_path, inspection, inspection_error))
                {
                    const auto identity_route = identity_routes_.find(inspection.Identity);
                    unsafe_alias = inspection.HasReparseComponent ||
                        identity_route == identity_routes_.end() ||
                        identity_route->second != route_key ||
                        inspection.NumberOfLinks != 1;
                }
            }
            else
            {
                InspectedPath inspection;
                DWORD inspection_error = ERROR_SUCCESS;
                if (InspectExistingPath(full_path, inspection, inspection_error))
                {
                    const auto identity = identity_routes_.find(inspection.Identity);
                    if (identity != identity_routes_.end())
                    {
                        route_key = identity->second;
                        unsafe_alias = inspection.HasReparseComponent || inspection.NumberOfLinks != 1;
                    }
                }
            }
        }

        if (route_key.empty())
        {
            return CreateFileW(path.wstring().c_str(), access, share, security_attributes, disposition, flags, template_file);
        }

        const auto route = routes_.find(route_key);
        if (route == routes_.end() || unsafe_alias || failed_routes_.find(route_key) != failed_routes_.end())
        {
            SetLastError(unsafe_alias ? ERROR_ACCESS_DENIED : ERROR_WRITE_FAULT);
            return INVALID_HANDLE_VALUE;
        }

        if ((security_attributes != nullptr && security_attributes->bInheritHandle != FALSE) || (flags & FILE_FLAG_OVERLAPPED) != 0)
        {
            SetLastError(ERROR_INVALID_PARAMETER);
            return INVALID_HANDLE_VALUE;
        }

        const bool write_request = IsWriteRequest(access, disposition);
        if (route->second.WritePolicy == FileWritePolicy::Deny && write_request)
        {
            SetLastError(ERROR_ACCESS_DENIED);
            return INVALID_HANDLE_VALUE;
        }

        std::filesystem::path target_path = route->second.OriginalPath;
        if (route->second.WritePolicy == FileWritePolicy::Redirect && (write_request || route->second.ReadPolicy == FileReadPolicy::Redirected))
        {
            target_path = overlay_paths_.at(route_key);
        }

        if (route->second.WritePolicy == FileWritePolicy::Deny && (flags & FILE_FLAG_DELETE_ON_CLOSE) != 0)
        {
            SetLastError(ERROR_ACCESS_DENIED);
            return INVALID_HANDLE_VALUE;
        }

        const HANDLE handle = CreateFileW(target_path.wstring().c_str(), access, share, security_attributes, disposition, flags, template_file);
        if (handle != INVALID_HANDLE_VALUE)
        {
            tracked_handles_.emplace(handle, route_key);
        }

        return handle;
    }

    BOOL FileWriteRoutingService::Close(HANDLE handle)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        const auto tracked = tracked_handles_.find(handle);
        if (tracked == tracked_handles_.end())
        {
            SetLastError(ERROR_INVALID_HANDLE);
            return FALSE;
        }

        const BOOL result = CloseHandle(handle);
        if (result != FALSE)
        {
            tracked_handles_.erase(tracked);
        }

        return result;
    }

    DWORD FileWriteRoutingService::GetAttributes(const std::filesystem::path& path)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        std::wstring full_path;
        std::wstring lexical_key;
        if (!ResolveFullPath(path, request_base_directory_, full_path) || !NormalizePathKey(full_path, lexical_key))
        {
            return GetFileAttributesW(path.wstring().c_str());
        }

        std::wstring route_key;
        const auto alias = path_aliases_.find(lexical_key);
        if (alias != path_aliases_.end())
        {
            route_key = alias->second;
            InspectedPath inspection;
            DWORD inspection_error = ERROR_SUCCESS;
            if (InspectExistingPath(full_path, inspection, inspection_error))
            {
                const auto identity_route = identity_routes_.find(inspection.Identity);
                if (inspection.HasReparseComponent || identity_route == identity_routes_.end() || identity_route->second != route_key || inspection.NumberOfLinks != 1)
                {
                    SetLastError(ERROR_ACCESS_DENIED);
                    return INVALID_FILE_ATTRIBUTES;
                }
            }
        }
        else
        {
            InspectedPath inspection;
            DWORD inspection_error = ERROR_SUCCESS;
            if (InspectExistingPath(full_path, inspection, inspection_error))
            {
                const auto identity = identity_routes_.find(inspection.Identity);
                if (identity != identity_routes_.end())
                {
                    route_key = identity->second;
                }
            }
        }

        if (!route_key.empty())
        {
            InspectedPath inspection;
            DWORD inspection_error = ERROR_SUCCESS;
            if (InspectExistingPath(full_path, inspection, inspection_error) && inspection.HasReparseComponent)
            {
                SetLastError(ERROR_ACCESS_DENIED);
                return INVALID_FILE_ATTRIBUTES;
            }
        }

        if (route_key.empty())
        {
            return GetFileAttributesW(path.wstring().c_str());
        }

        if (failed_routes_.find(route_key) != failed_routes_.end())
        {
            SetLastError(ERROR_WRITE_FAULT);
            return INVALID_FILE_ATTRIBUTES;
        }

        const FileWriteRoute& route = routes_.at(route_key);
        const std::filesystem::path& source_path = route.WritePolicy == FileWritePolicy::Redirect && route.ReadPolicy == FileReadPolicy::Redirected ? overlay_paths_.at(route_key) : route.OriginalPath;
        return GetFileAttributesW(source_path.wstring().c_str());
    }

    std::unique_ptr<FileWriteRoutingTransaction> FileWriteRoutingService::BeginTrustedWrite(
        const std::vector<std::filesystem::path>& original_paths,
        DWORD& error)
    {
        std::unique_lock<std::mutex> lock(mutex_);
        std::vector<std::wstring> route_keys;
        for (const std::filesystem::path& path : original_paths)
        {
            std::wstring full_path;
            std::wstring lexical_key;
            if (!ResolveFullPath(path, request_base_directory_, full_path) || !NormalizePathKey(full_path, lexical_key))
            {
                error = ERROR_INVALID_NAME;
                return nullptr;
            }

            std::wstring route_key;
            const auto alias = path_aliases_.find(lexical_key);
            if (alias != path_aliases_.end())
            {
                route_key = alias->second;
                InspectedPath inspection;
                DWORD inspection_error = ERROR_SUCCESS;
                if (InspectExistingPath(full_path, inspection, inspection_error) &&
                    (inspection.HasReparseComponent || identity_routes_.find(inspection.Identity) == identity_routes_.end() || identity_routes_.at(inspection.Identity) != route_key || inspection.NumberOfLinks != 1))
                {
                    error = ERROR_ACCESS_DENIED;
                    return nullptr;
                }
            }
            else
            {
                InspectedPath inspection;
                DWORD inspection_error = ERROR_SUCCESS;
                if (InspectExistingPath(full_path, inspection, inspection_error))
                {
                    const auto identity = identity_routes_.find(inspection.Identity);
                    if (identity != identity_routes_.end())
                    {
                        route_key = identity->second;
                    }
                }
            }

            if (!route_key.empty() && std::find(route_keys.begin(), route_keys.end(), route_key) == route_keys.end())
            {
                route_keys.push_back(route_key);
            }
        }

        for (const auto& tracked : tracked_handles_)
        {
            if (std::find(route_keys.begin(), route_keys.end(), tracked.second) != route_keys.end())
            {
                error = ERROR_SHARING_VIOLATION;
                return nullptr;
            }
        }

        for (const std::wstring& route_key : route_keys)
        {
            if (failed_routes_.find(route_key) != failed_routes_.end())
            {
                error = ERROR_WRITE_FAULT;
                return nullptr;
            }
        }

        error = ERROR_SUCCESS;
        return std::unique_ptr<FileWriteRoutingTransaction>(new FileWriteRoutingTransaction(*this, std::move(route_keys), std::move(lock)));
    }

    bool FileWriteRoutingService::SynchronizeRoutes(const std::vector<std::wstring>& route_keys, DWORD& error)
    {
        for (const std::wstring& route_key : route_keys)
        {
            const auto route = routes_.find(route_key);
            const auto overlay = overlay_paths_.find(route_key);
            const auto identity = route_identities_.find(route_key);
            if (route == routes_.end() || overlay == overlay_paths_.end())
            {
                error = ERROR_INVALID_DATA;
                LatchRoutesFailed(route_keys);
                return false;
            }

            InspectedPath inspection;
            DWORD inspection_error = ERROR_SUCCESS;
            std::wstring full_path;
            if (!ResolveFullPath(route->second.OriginalPath, request_base_directory_, full_path) ||
                !InspectExistingPath(full_path, inspection, inspection_error) ||
                inspection.HasReparseComponent ||
                (identity != route_identities_.end() && inspection.Identity != identity->second))
            {
                error = inspection_error == ERROR_SUCCESS ? ERROR_INVALID_DATA : inspection_error;
                LatchRoutesFailed(route_keys);
                return false;
            }

            if (!CopyFileW(route->second.OriginalPath.wstring().c_str(), overlay->second.wstring().c_str(), FALSE))
            {
                error = GetLastError();
                LatchRoutesFailed(route_keys);
                return false;
            }
        }

        error = ERROR_SUCCESS;
        return true;
    }

    void FileWriteRoutingService::LatchRoutesFailed(const std::vector<std::wstring>& route_keys)
    {
        for (const std::wstring& route_key : route_keys)
        {
            failed_routes_.insert(route_key);
        }
    }

    void FileWriteRoutingService::CleanupSession() noexcept
    {
        std::lock_guard<std::mutex> lock(mutex_);
        RemoveOwnedDirectory(session_directory_);
        session_directory_.clear();
    }
}
