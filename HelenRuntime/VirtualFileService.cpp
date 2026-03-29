#include <HelenHook/DeltaVirtualFileSource.h>
#include <HelenHook/FullFileVirtualFileSource.h>
#include <HelenHook/HgdeltaFile.h>
#include <HelenHook/VirtualFileService.h>

#include <HelenHook/VirtualFileSource.h>
#include <HelenHook/Log.h>

#include <algorithm>
#include <cctype>
#include <cwctype>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <memory>
#include <mutex>
#include <system_error>

namespace
{
    /**
     * @brief Converts ASCII text into lowercase so manifest digests compare consistently.
     * @param text ASCII text that should be normalized.
     * @return Lowercase copy of the supplied text.
     */
    std::string ToLowerAscii(std::string text)
    {
        for (char& character : text)
        {
            character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
        }

        return text;
    }

    /**
     * @brief Returns whether one parsed hgdelta container matches the exact manifest metadata declaration.
     * @param delta Parsed hgdelta container loaded from the declared asset path.
     * @param definition Virtual file definition whose source metadata should match the container.
     * @return True when chunk size and exact base/target fingerprints match the manifest; otherwise false.
     */
    bool MatchesDeclaredDeltaMetadata(const helen::HgdeltaFile& delta, const helen::VirtualFileDefinition& definition)
    {
        return delta.ChunkSize == definition.Source.ChunkSize &&
            delta.BaseFileSize == definition.Source.Base.FileSize &&
            delta.TargetFileSize == definition.Source.Target.FileSize &&
            delta.BaseSha256 == ToLowerAscii(definition.Source.Base.Sha256) &&
            delta.TargetSha256 == ToLowerAscii(definition.Source.Target.Sha256);
    }

    /**
     * @brief Validates that one delta-backed source asset exists and contains a parseable hgdelta container.
     * @param resolver Active pack asset resolver that locates the declared hgdelta asset.
     * @param definition Delta-backed virtual file definition whose declared asset should be validated.
     * @return True when the asset exists, parses successfully, and matches the declared metadata; otherwise false.
     */
    bool ValidateDeltaSourceAsset(const helen::PackAssetResolver& resolver, const helen::VirtualFileDefinition& definition)
    {
        const std::optional<std::filesystem::path> delta_file_path = resolver.Resolve(definition.Source.Path);
        if (!delta_file_path.has_value())
        {
            return false;
        }

        try
        {
            const helen::HgdeltaFile delta = helen::HgdeltaFile::Load(*delta_file_path);
            return MatchesDeclaredDeltaMetadata(delta, definition);
        }
        catch (...)
        {
            return false;
        }
    }
}

namespace helen
{
    VirtualFileService::VirtualFileService(const PackAssetResolver& resolver, const std::filesystem::path& cache_directory)
        : resolver_(resolver),
          cache_directory_(cache_directory)
    {
    }

    bool VirtualFileService::NormalizeDeclaredPath(const std::filesystem::path& path, std::wstring& normalized_path)
    {
        if (path.empty() || path.has_root_name() || path.has_root_directory())
        {
            return false;
        }

        const std::filesystem::path normalized = path.lexically_normal();
        if (normalized.empty() || normalized == std::filesystem::path("."))
        {
            return false;
        }

        for (const std::filesystem::path& component : normalized)
        {
            if (component == std::filesystem::path(".."))
            {
                return false;
            }
        }

        normalized_path = normalized.generic_wstring();
        std::transform(normalized_path.begin(), normalized_path.end(), normalized_path.begin(), towlower);
        return !normalized_path.empty();
    }

    bool VirtualFileService::NormalizeLookupPath(const std::filesystem::path& path, std::wstring& normalized_path)
    {
        if (path.empty())
        {
            return false;
        }

        const std::filesystem::path normalized = path.lexically_normal();
        if (normalized.empty() || normalized == std::filesystem::path("."))
        {
            return false;
        }

        normalized_path = normalized.generic_wstring();
        std::transform(normalized_path.begin(), normalized_path.end(), normalized_path.begin(), towlower);
        return !normalized_path.empty();
    }

    bool VirtualFileService::MatchesDeclaredPath(
        const std::wstring& normalized_lookup_path,
        const std::wstring& normalized_declared_path)
    {
        if (normalized_lookup_path == normalized_declared_path)
        {
            return true;
        }

        if (normalized_lookup_path.size() <= normalized_declared_path.size())
        {
            return false;
        }

        const std::size_t suffix_offset = normalized_lookup_path.size() - normalized_declared_path.size();
        if (normalized_lookup_path.compare(suffix_offset, normalized_declared_path.size(), normalized_declared_path) != 0)
        {
            return false;
        }

        return normalized_lookup_path[suffix_offset - 1] == L'/';
    }

    bool VirtualFileService::LoadReplacementBytes(const std::filesystem::path& asset_path, std::vector<std::uint8_t>& bytes) const
    {
        const std::optional<std::filesystem::path> resolved_path = resolver_.Resolve(asset_path);
        if (!resolved_path.has_value())
        {
            return false;
        }

        const std::filesystem::path full_path = *resolved_path;
        std::error_code error;
        const std::uintmax_t file_size = std::filesystem::file_size(full_path, error);
        if (error || file_size > static_cast<std::uintmax_t>((std::numeric_limits<std::size_t>::max)()) || file_size > static_cast<std::uintmax_t>((std::numeric_limits<std::streamsize>::max)()))
        {
            return false;
        }

        std::ifstream stream(full_path, std::ios::binary);
        if (!stream)
        {
            return false;
        }

        bytes.clear();
        bytes.resize(static_cast<std::size_t>(file_size));
        if (file_size == 0)
        {
            return true;
        }

        stream.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        if (!stream.good() && !stream.eof())
        {
            bytes.clear();
            return false;
        }

        if (static_cast<std::size_t>(stream.gcount()) != bytes.size())
        {
            bytes.clear();
            return false;
        }

        return true;
    }

    bool VirtualFileService::RegisterVirtualFile(const VirtualFileDefinition& definition)
    {
        std::wstring normalized_game_path;
        if (!NormalizeDeclaredPath(definition.GamePath, normalized_game_path))
        {
            return false;
        }

        RegisteredVirtualFile registered_virtual_file;
        registered_virtual_file.Definition = definition;
        if (definition.Mode == "replace-on-read")
        {
            if (!CreateSource(definition, std::filesystem::path(), registered_virtual_file.SharedSource))
            {
                return false;
            }
        }
        else if (definition.Mode != "delta-on-read" || definition.Source.Kind != VirtualFileSourceKind::DeltaFile)
        {
            return false;
        }
        else if (!ValidateDeltaSourceAsset(resolver_, definition))
        {
            return false;
        }

        std::lock_guard<std::mutex> lock(mutex_);
        const auto inserted = virtual_files_.emplace(normalized_game_path, std::move(registered_virtual_file));
        if (inserted.second)
        {
            const RegisteredVirtualFile& inserted_virtual_file = inserted.first->second;
            const std::uint64_t registered_size = inserted_virtual_file.SharedSource != nullptr ?
                inserted_virtual_file.SharedSource->GetSize() :
                inserted_virtual_file.Definition.Source.Target.FileSize;
            Logf(
                L"[runtime] registered virtual file path=%ls bytes=%llu",
                normalized_game_path.c_str(),
                static_cast<unsigned long long>(registered_size));
        }

        return inserted.second;
    }

    bool VirtualFileService::CreateSource(
        const VirtualFileDefinition& definition,
        const std::filesystem::path& opened_game_path,
        std::shared_ptr<VirtualFileSource>& source) const
    {
        source.reset();
        if (definition.Mode == "replace-on-read")
        {
            if (definition.Source.Kind != VirtualFileSourceKind::FullFile)
            {
                return false;
            }

            std::vector<std::uint8_t> replacement_bytes;
            if (!LoadReplacementBytes(definition.Source.Path, replacement_bytes))
            {
                return false;
            }

            source = std::make_shared<FullFileVirtualFileSource>(std::move(replacement_bytes));
            return true;
        }

        if (definition.Mode != "delta-on-read" || definition.Source.Kind != VirtualFileSourceKind::DeltaFile)
        {
            return false;
        }

        try
        {
            source = std::make_shared<DeltaVirtualFileSource>(resolver_, cache_directory_, opened_game_path, definition);
            return true;
        }
        catch (...)
        {
            source.reset();
            return false;
        }
    }

    std::optional<RegisteredVirtualFile> VirtualFileService::FindRegisteredVirtualFile(
        const std::wstring& normalized_lookup_path) const
    {
        const auto exact_match = virtual_files_.find(normalized_lookup_path);
        if (exact_match != virtual_files_.end())
        {
            return exact_match->second;
        }

        for (const auto& entry : virtual_files_)
        {
            if (MatchesDeclaredPath(normalized_lookup_path, entry.first))
            {
                return entry.second;
            }
        }

        return std::nullopt;
    }

    std::optional<HANDLE> VirtualFileService::OpenVirtualFile(const std::filesystem::path& game_relative_path)
    {
        std::wstring normalized_game_path;
        if (!NormalizeLookupPath(game_relative_path, normalized_game_path))
        {
            return std::nullopt;
        }

        const HANDLE handle = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        if (handle == nullptr || handle == INVALID_HANDLE_VALUE)
        {
            return std::nullopt;
        }

        std::optional<RegisteredVirtualFile> registered_virtual_file;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            registered_virtual_file = FindRegisteredVirtualFile(normalized_game_path);
        }

        if (!registered_virtual_file.has_value())
        {
            CloseHandle(handle);
            return std::nullopt;
        }

        std::shared_ptr<VirtualFileSource> source = registered_virtual_file->SharedSource;
        if (source == nullptr &&
            !CreateSource(registered_virtual_file->Definition, game_relative_path, source))
        {
            CloseHandle(handle);
            return std::nullopt;
        }

        VirtualFileHandle virtual_handle;
        virtual_handle.Source = std::move(source);
        virtual_handle.ReadPosition = 0;

        bool inserted = false;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            inserted = open_handles_.emplace(handle, std::move(virtual_handle)).second;
        }

        if (!inserted)
        {
            CloseHandle(handle);
            return std::nullopt;
        }

        Logf(L"[runtime] serving virtual file path=%ls", normalized_game_path.c_str());
        return handle;
    }

    std::optional<HANDLE> VirtualFileService::CreateFileMapping(
        HANDLE handle,
        DWORD protection,
        DWORD maximum_size_high,
        DWORD maximum_size_low) const
    {
        std::shared_ptr<VirtualFileSource> source;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            const auto found = open_handles_.find(handle);
            if (found == open_handles_.end())
            {
                return std::nullopt;
            }

            source = found->second.Source;
        }

        if (source == nullptr)
        {
            return std::nullopt;
        }

        return source->CreateFileMapping(protection, maximum_size_high, maximum_size_low);
    }

    bool VirtualFileService::IsVirtualHandle(HANDLE handle) const noexcept
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (handle == nullptr || handle == INVALID_HANDLE_VALUE)
        {
            return false;
        }

        return open_handles_.contains(handle);
    }

    bool VirtualFileService::Read(HANDLE handle, void* buffer, DWORD bytes_to_read, DWORD* bytes_read)
    {
        if (bytes_read == nullptr)
        {
            SetLastError(ERROR_INVALID_PARAMETER);
            return false;
        }

        *bytes_read = 0;
        std::lock_guard<std::mutex> lock(mutex_);
        if (handle == nullptr || handle == INVALID_HANDLE_VALUE)
        {
            SetLastError(ERROR_INVALID_HANDLE);
            return false;
        }

        auto found = open_handles_.find(handle);
        if (found == open_handles_.end())
        {
            SetLastError(ERROR_INVALID_HANDLE);
            return false;
        }

        VirtualFileHandle& virtual_handle = found->second;
        if (bytes_to_read == 0)
        {
            return true;
        }

        if (buffer == nullptr)
        {
            SetLastError(ERROR_INVALID_PARAMETER);
            return false;
        }

        if (virtual_handle.Source == nullptr)
        {
            SetLastError(ERROR_INVALID_HANDLE);
            return false;
        }

        const std::uint64_t original_position = virtual_handle.ReadPosition;
        const std::uint64_t size = virtual_handle.Source->GetSize();
        const std::uint64_t start = std::min(original_position, size);
        std::size_t copied = 0;
        if (!virtual_handle.Source->Read(start, buffer, static_cast<std::size_t>(bytes_to_read), copied))
        {
            return false;
        }

        if (copied > static_cast<std::size_t>((std::numeric_limits<DWORD>::max)()))
        {
            SetLastError(ERROR_INVALID_PARAMETER);
            return false;
        }

        virtual_handle.ReadPosition = start + static_cast<std::uint64_t>(copied);
        *bytes_read = static_cast<DWORD>(copied);
        return true;
    }

    bool VirtualFileService::Seek(HANDLE handle, LARGE_INTEGER distance_to_move, DWORD move_method, LARGE_INTEGER* new_file_pointer)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (handle == nullptr || handle == INVALID_HANDLE_VALUE)
        {
            SetLastError(ERROR_INVALID_HANDLE);
            return false;
        }

        auto found = open_handles_.find(handle);
        if (found == open_handles_.end())
        {
            SetLastError(ERROR_INVALID_HANDLE);
            return false;
        }

        VirtualFileHandle& virtual_handle = found->second;
        std::int64_t base_position = 0;
        if (move_method == FILE_BEGIN)
        {
            base_position = 0;
        }
        else if (move_method == FILE_CURRENT)
        {
            if (virtual_handle.ReadPosition > static_cast<std::uint64_t>((std::numeric_limits<std::int64_t>::max)()))
            {
                SetLastError(ERROR_INVALID_PARAMETER);
                return false;
            }

            base_position = static_cast<std::int64_t>(virtual_handle.ReadPosition);
        }
        else if (move_method == FILE_END)
        {
            if (virtual_handle.Source == nullptr)
            {
                SetLastError(ERROR_INVALID_HANDLE);
                return false;
            }

            const std::uint64_t size = virtual_handle.Source->GetSize();
            if (size > static_cast<std::uint64_t>((std::numeric_limits<std::int64_t>::max)()))
            {
                SetLastError(ERROR_INVALID_PARAMETER);
                return false;
            }

            base_position = static_cast<std::int64_t>(size);
        }
        else
        {
            SetLastError(ERROR_INVALID_PARAMETER);
            return false;
        }

        const std::int64_t offset = distance_to_move.QuadPart;
        if ((offset > 0 && base_position > (std::numeric_limits<std::int64_t>::max)() - offset) ||
            (offset < 0 && base_position < (std::numeric_limits<std::int64_t>::min)() - offset))
        {
            SetLastError(ERROR_INVALID_PARAMETER);
            return false;
        }

        const std::int64_t target_position = base_position + offset;
        if (target_position < 0)
        {
            SetLastError(ERROR_NEGATIVE_SEEK);
            return false;
        }

        virtual_handle.ReadPosition = static_cast<std::uint64_t>(target_position);
        if (new_file_pointer != nullptr)
        {
            new_file_pointer->QuadPart = target_position;
        }

        return true;
    }

    bool VirtualFileService::GetSize(HANDLE handle, LARGE_INTEGER* file_size) const
    {
        if (file_size == nullptr)
        {
            SetLastError(ERROR_INVALID_PARAMETER);
            return false;
        }

        file_size->QuadPart = 0;
        std::lock_guard<std::mutex> lock(mutex_);
        if (handle == nullptr || handle == INVALID_HANDLE_VALUE)
        {
            SetLastError(ERROR_INVALID_HANDLE);
            return false;
        }

        const auto found = open_handles_.find(handle);
        if (found == open_handles_.end())
        {
            SetLastError(ERROR_INVALID_HANDLE);
            return false;
        }

        if (found->second.Source == nullptr)
        {
            SetLastError(ERROR_INVALID_HANDLE);
            return false;
        }

        const std::uint64_t size = found->second.Source->GetSize();
        if (size > static_cast<std::uint64_t>((std::numeric_limits<LONGLONG>::max)()))
        {
            SetLastError(ERROR_INVALID_PARAMETER);
            return false;
        }

        file_size->QuadPart = static_cast<LONGLONG>(size);
        return true;
    }

    bool VirtualFileService::Close(HANDLE handle)
    {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (handle == nullptr || handle == INVALID_HANDLE_VALUE)
            {
                SetLastError(ERROR_INVALID_HANDLE);
                return false;
            }

            const auto erased = open_handles_.erase(handle);
            if (erased == 0)
            {
                SetLastError(ERROR_INVALID_HANDLE);
                return false;
            }
        }

        if (!CloseHandle(handle))
        {
            return false;
        }

        return true;
    }
}
