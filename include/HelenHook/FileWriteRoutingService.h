#pragma once

#include <filesystem>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <vector>

#include <windows.h>

#include <HelenHook/FileWriteRoute.h>

namespace helen
{
    class FileWriteRoutingTransaction;

    /**
     * @brief Owns one fresh session of exact-file write routes and mediates native Win32 opens and attributes.
     *
     * Redirected files are real files below one unique cache child. The service never reuses a prior session directory,
     * never treats an unavailable protected path as unrelated, and serializes opens, handle tracking, and trusted saves.
     */
    class FileWriteRoutingService
    {
    public:
        /**
         * @brief Describes whether a path is unrelated, protected, or must be rejected before native fallback.
         */
        enum class PathDisposition
        {
            /** @brief Path has no relationship to a declared route and may use native behavior. */
            Unrelated,

            /** @brief Path resolves to a validated protected route. */
            Protected,

            /** @brief Path is a known or unsafe route spelling that cannot be verified safely. */
            Rejected,
        };

        /**
         * @brief Binds routing to a cache root and the base used to resolve relative request paths.
         * @param cache_directory Writable runtime cache root; one unique child is created at initialization.
         * @param request_base_directory Directory used to resolve relative route and request paths.
         */
        FileWriteRoutingService(std::filesystem::path cache_directory, std::filesystem::path request_base_directory);

        /**
         * @brief Releases this service's session-owned directory without touching any other cache content.
         */
        ~FileWriteRoutingService();

        FileWriteRoutingService(const FileWriteRoutingService&) = delete;
        FileWriteRoutingService& operator=(const FileWriteRoutingService&) = delete;
        FileWriteRoutingService(FileWriteRoutingService&&) = delete;
        FileWriteRoutingService& operator=(FileWriteRoutingService&&) = delete;

        /**
         * @brief Validates all declarations and materializes one fresh session atomically.
         * @param routes Exact file declarations to install.
         * @param error Receives ERROR_SUCCESS or the Win32 reason initialization failed.
         * @return True only when every route is validated and every redirected copy is initialized.
         */
        bool Initialize(const std::vector<FileWriteRoute>& routes, DWORD& error);

        /**
         * @brief Reports whether a path is a protected route or an unsafe spelling that must fail closed.
         * @param path Native path supplied by an API caller.
         * @return True for a declared route and for an alias/reparse form that resolves to a declared route.
         */
        bool IsProtectedPath(const std::filesystem::path& path) const;

        /**
         * @brief Classifies a path so hook adapters can distinguish native pass-through from fail-closed routing errors.
         * @param path Native path supplied by an API caller.
         * @return Unrelated for native pass-through, Protected for routing, or Rejected for unsafe/indeterminate route forms.
         */
        PathDisposition ClassifyPath(const std::filesystem::path& path) const;

        /**
         * @brief Reports whether a handle was returned by a protected routed open and remains live.
         * @param handle Native handle to classify.
         * @return True while the service still owns tracking for the handle.
         */
        bool IsTrackedHandle(HANDLE handle) const;

        /**
         * @brief Opens an exact path through routing or forwards an unrelated path to native CreateFileW.
         * @param path Path requested by the caller.
         * @param access Desired access mask.
         * @param share Share mode passed to native CreateFileW.
         * @param security_attributes Native security attributes; inheritable handles are rejected for routes.
         * @param disposition Native creation disposition.
         * @param flags Native flags and attributes; overlapped routed handles are rejected.
         * @param template_file Native template handle.
         * @return Native file handle, or INVALID_HANDLE_VALUE with a preserved Win32 error.
         */
        HANDLE Open(
            const std::filesystem::path& path,
            DWORD access,
            DWORD share,
            LPSECURITY_ATTRIBUTES security_attributes,
            DWORD disposition,
            DWORD flags,
            HANDLE template_file);

        /**
         * @brief Closes one tracked routed handle after native CloseHandle succeeds.
         * @param handle Routed handle returned by Open.
         * @return Native CloseHandle result; untracked handles fail with ERROR_INVALID_HANDLE.
         */
        BOOL Close(HANDLE handle);

        /**
         * @brief Returns attributes for the selected route source or forwards unrelated paths to native GetFileAttributesW.
         * @param path Path whose attributes should be read.
         * @return Native attributes, or INVALID_FILE_ATTRIBUTES on failure.
         */
        DWORD GetAttributes(const std::filesystem::path& path);

        /**
         * @brief Acquires a no-bypass trusted save scope for the named exact routes.
         * @param original_paths Original paths being written by HelenHook.
         * @param error Receives ERROR_SUCCESS or the acquisition failure.
         * @return Transaction retaining routing serialization, or null when validation or open-handle checks fail.
         */
        std::unique_ptr<FileWriteRoutingTransaction> BeginTrustedWrite(
            const std::vector<std::filesystem::path>& original_paths,
            DWORD& error);

    private:
        friend class FileWriteRoutingTransaction;

        /** @brief Copies verified current originals to their session overlays while the transaction lock is held. */
        bool SynchronizeRoutes(const std::vector<std::wstring>& route_keys, DWORD& error);

        /** @brief Latches routes failed so future protected operations fail closed. */
        void LatchRoutesFailed(const std::vector<std::wstring>& route_keys);

        /** @brief Removes this service's unique session directory and ignores cleanup failure. */
        void CleanupSession() noexcept;

        /** @brief Classifies one path while the service mutex is already held and returns the route key when protected. */
        PathDisposition ClassifyPathUnlocked(const std::filesystem::path& path, std::wstring& route_key, DWORD& error) const;

        /** @brief Runtime cache root from which this service creates exactly one unique child. */
        std::filesystem::path cache_directory_;

        /** @brief Base directory used for relative route and request resolution. */
        std::filesystem::path request_base_directory_;

        /** @brief Unique session directory owned exclusively by this service. */
        std::filesystem::path session_directory_;

        /** @brief Routes keyed by canonical final path. */
        std::map<std::wstring, FileWriteRoute> routes_;

        /** @brief Lexical and canonical aliases mapped to canonical route keys. */
        std::map<std::wstring, std::wstring> path_aliases_;

        /** @brief Session overlay path keyed by canonical route key. */
        std::map<std::wstring, std::filesystem::path> overlay_paths_;

        /** @brief Stable native file identities mapped to canonical route keys. */
        std::map<std::wstring, std::wstring> identity_routes_;

        /** @brief Stable native identity keyed by canonical route key for transaction verification. */
        std::map<std::wstring, std::wstring> route_identities_;

        /** @brief Routes latched after abandoned or failed trusted synchronization. */
        std::set<std::wstring> failed_routes_;

        /** @brief Live routed native handles mapped to canonical route keys. */
        std::map<HANDLE, std::wstring> tracked_handles_;

        /** @brief Serializes route mutation, tracking, initialization, and retained trusted transactions. */
        mutable std::mutex mutex_;

        /** @brief True once a session has been successfully initialized. */
        bool initialized_ = false;
    };
}
