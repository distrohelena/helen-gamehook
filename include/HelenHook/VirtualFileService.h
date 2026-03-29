#pragma once

#include <cstdint>
#include <filesystem>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include <windows.h>

#include <HelenHook/PackAssetResolver.h>
#include <HelenHook/RegisteredVirtualFile.h>
#include <HelenHook/VirtualFileDefinition.h>
#include <HelenHook/VirtualFileHandle.h>

namespace helen
{
    class VirtualFileSource;

    /**
     * @brief Owns virtual-file sources for exact game-relative file paths.
     *
     * Each registered virtual file resolves its declared source through the active pack asset resolver and exposes a
     * synthetic handle whose read cursor is tracked independently from all other opens.
     */
    class VirtualFileService
    {
    public:
        /**
         * @brief Creates a virtual file service bound to the active pack asset resolver.
         * @param resolver Resolver that translates declared asset paths into normalized filesystem locations.
         * @param cache_directory Writable helengamehook cache directory used by source implementations that materialize files.
         */
        VirtualFileService(const PackAssetResolver& resolver, const std::filesystem::path& cache_directory);

        /**
         * @brief Registers one declared virtual file and creates its backing source object.
         * @param definition Virtual file declaration loaded from the active build.
         * @return True when the file path is valid and the backing source could be created.
         */
        bool RegisterVirtualFile(const VirtualFileDefinition& definition);

        /**
         * @brief Opens a synthetic handle for one matching game-relative path.
         * @param game_relative_path Game-relative path requested by the runtime.
         * @return A synthetic handle when the normalized path matches a registered replacement; otherwise no value.
         */
        std::optional<HANDLE> OpenVirtualFile(const std::filesystem::path& game_relative_path);

        /**
         * @brief Creates one real file-mapping handle backed by the source registered for a virtual file handle.
         * @param handle Virtual handle returned by OpenVirtualFile.
         * @param protection Requested page protection flags from CreateFileMapping.
         * @param maximum_size_high High-order requested mapping size.
         * @param maximum_size_low Low-order requested mapping size.
         * @return A real file-mapping handle when the virtual handle exists and the mapping can be created; otherwise no value.
         */
        std::optional<HANDLE> CreateFileMapping(
            HANDLE handle,
            DWORD protection,
            DWORD maximum_size_high,
            DWORD maximum_size_low) const;

        /**
         * @brief Returns true when a handle belongs to this virtual file service.
         * @param handle Handle that should be classified as virtual or non-virtual.
         * @return True when the handle is currently tracked by the service.
         */
        bool IsVirtualHandle(HANDLE handle) const noexcept;

        /**
         * @brief Reads bytes from one synthetic handle at its current read position.
         * @param handle Virtual handle returned by OpenVirtualFile.
         * @param buffer Destination buffer that receives source bytes.
         * @param bytes_to_read Maximum number of bytes the caller requested.
         * @param bytes_read Receives the actual byte count copied into buffer.
         * @return True when the handle is valid and the read completed.
         */
        bool Read(HANDLE handle, void* buffer, DWORD bytes_to_read, DWORD* bytes_read);

        /**
         * @brief Moves the current read position for one synthetic handle.
         * @param handle Virtual handle returned by OpenVirtualFile.
         * @param distance_to_move Signed file offset used with the selected move method.
         * @param move_method FILE_BEGIN, FILE_CURRENT, or FILE_END.
         * @param new_file_pointer Receives the updated absolute read position on success when provided.
         * @return True when the handle is valid and the resulting position is non-negative.
         */
        bool Seek(HANDLE handle, LARGE_INTEGER distance_to_move, DWORD move_method, LARGE_INTEGER* new_file_pointer);

        /**
         * @brief Reports the source size for one synthetic handle.
         * @param handle Virtual handle returned by OpenVirtualFile.
         * @param file_size Receives the source size in bytes.
         * @return True when the handle is valid and the size could be reported.
         */
        bool GetSize(HANDLE handle, LARGE_INTEGER* file_size) const;

        /**
         * @brief Closes one live synthetic handle and removes its tracked read cursor from the open-handle table.
         * @param handle Virtual handle returned by OpenVirtualFile.
         * @return True when the handle existed in the live table and was removed from the service.
         */
        bool Close(HANDLE handle);

    private:
        /**
         * @brief Normalizes one declared virtual-file path for exact comparison against future lookups.
         * @param path Build-declared game-relative path that should be normalized for registration.
         * @param normalized_path Receives the normalized wide string when the path is valid.
         * @return True when the input is a non-absolute relative path that does not escape its root.
         */
        static bool NormalizeDeclaredPath(const std::filesystem::path& path, std::wstring& normalized_path);

        /**
         * @brief Normalizes one incoming game file request path for suffix matching against registered virtual files.
         * @param path Runtime file request path that may be relative, contain parent traversal, or be absolute.
         * @param normalized_path Receives the normalized wide string when the path can participate in lookup.
         * @return True when the path is non-empty and can be normalized into comparable generic form.
         */
        static bool NormalizeLookupPath(const std::filesystem::path& path, std::wstring& normalized_path);

        /**
         * @brief Returns true when one normalized incoming file request matches a registered virtual-file suffix.
         * @param normalized_lookup_path Normalized incoming file request path.
         * @param normalized_declared_path Normalized build-declared game-relative path.
         * @return True when the lookup path is exactly the declared path or ends with it on a path-component boundary.
         */
        static bool MatchesDeclaredPath(
            const std::wstring& normalized_lookup_path,
            const std::wstring& normalized_declared_path);

        /**
         * @brief Loads one full replacement payload through the active pack asset resolver.
         * @param asset_path Resolver-relative source path declared by the virtual file definition.
         * @param bytes Receives the replacement bytes when the file can be read.
         * @return True when the asset exists and the bytes were loaded completely.
         */
        bool LoadReplacementBytes(const std::filesystem::path& asset_path, std::vector<std::uint8_t>& bytes) const;

        /**
         * @brief Creates one backing source object for the supplied virtual file definition and opened game path.
         * @param definition Virtual file definition that should be converted into a source object.
         * @param opened_game_path Actual game file path being opened when a source needs per-open context.
         * @param source Receives the created source object on success.
         * @return True when the definition is supported and the source could be created.
         */
        bool CreateSource(
            const VirtualFileDefinition& definition,
            const std::filesystem::path& opened_game_path,
            std::shared_ptr<VirtualFileSource>& source) const;

        /**
         * @brief Returns one registered virtual-file record for one normalized incoming lookup path when any suffix matches.
         * @param normalized_lookup_path Normalized incoming file request path.
         * @return Registered virtual-file record when the lookup path matches a registered virtual file; otherwise no value.
         */
        std::optional<RegisteredVirtualFile> FindRegisteredVirtualFile(
            const std::wstring& normalized_lookup_path) const;

        /** @brief Resolver used to map declared replacement asset paths into validated filesystem paths. */
        PackAssetResolver resolver_;

        /** @brief Writable helengamehook cache directory used by source implementations that materialize files. */
        std::filesystem::path cache_directory_;

        /** @brief Registered virtual-file records keyed by normalized wide-string game-relative path. */
        std::map<std::wstring, RegisteredVirtualFile> virtual_files_;

        /** @brief Live synthetic handles keyed by their opaque kernel handle value. */
        std::map<HANDLE, VirtualFileHandle> open_handles_;

        /** @brief Synchronizes all virtual file registration, handle allocation, and handle state access. */
        mutable std::mutex mutex_;
    };
}
