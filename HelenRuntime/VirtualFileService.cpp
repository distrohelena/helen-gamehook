#include <HelenHook/VirtualFileService.h>

#include <HelenHook/Log.h>

#include <algorithm>
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

namespace helen
{
    VirtualFileService::VirtualFileService(const PackAssetResolver& resolver)
        : resolver_(resolver)
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
        if (definition.Mode != "replace-on-read")
        {
            return false;
        }

        std::wstring normalized_game_path;
        if (!NormalizeDeclaredPath(definition.GamePath, normalized_game_path))
        {
            return false;
        }

        std::vector<std::uint8_t> replacement_bytes;
        if (!LoadReplacementBytes(definition.Source.Path, replacement_bytes))
        {
            return false;
        }

        const std::shared_ptr<const std::vector<std::uint8_t>> replacement_payload =
            std::make_shared<std::vector<std::uint8_t>>(std::move(replacement_bytes));

        std::lock_guard<std::mutex> lock(mutex_);
        const auto inserted = virtual_files_.emplace(normalized_game_path, replacement_payload);
        if (inserted.second)
        {
            Logf(
                L"[runtime] registered virtual file path=%ls bytes=%llu",
                normalized_game_path.c_str(),
                static_cast<unsigned long long>(replacement_payload->size()));
        }

        return inserted.second;
    }

    std::optional<std::shared_ptr<const std::vector<std::uint8_t>>> VirtualFileService::FindReplacementBytes(
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

        bool inserted = false;
        bool found_matching_file = false;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            const std::optional<std::shared_ptr<const std::vector<std::uint8_t>>> replacement_bytes = FindReplacementBytes(normalized_game_path);
            if (replacement_bytes.has_value())
            {
                found_matching_file = true;

                VirtualFileHandle virtual_handle;
                virtual_handle.ReplacementBytes = *replacement_bytes;
                virtual_handle.ReadPosition = 0;

                inserted = open_handles_.emplace(handle, std::move(virtual_handle)).second;
            }
        }

        if (!found_matching_file)
        {
            CloseHandle(handle);
            return std::nullopt;
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
        std::shared_ptr<const std::vector<std::uint8_t>> replacement_bytes;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            const auto found = open_handles_.find(handle);
            if (found == open_handles_.end())
            {
                return std::nullopt;
            }

            replacement_bytes = found->second.ReplacementBytes;
        }

        if (replacement_bytes == nullptr || replacement_bytes->empty())
        {
            SetLastError(ERROR_FILE_INVALID);
            return std::nullopt;
        }

        const std::uint64_t requested_size = (static_cast<std::uint64_t>(maximum_size_high) << 32) | maximum_size_low;
        const std::uint64_t payload_size = static_cast<std::uint64_t>(replacement_bytes->size());
        const std::uint64_t mapping_size = requested_size == 0 ? payload_size : requested_size;
        if (mapping_size < payload_size || mapping_size > static_cast<std::uint64_t>((std::numeric_limits<DWORD>::max)()))
        {
            SetLastError(ERROR_INVALID_PARAMETER);
            return std::nullopt;
        }

        const HANDLE mapping_handle = ::CreateFileMappingW(
            INVALID_HANDLE_VALUE,
            nullptr,
            PAGE_READWRITE,
            0,
            static_cast<DWORD>(mapping_size),
            nullptr);
        if (mapping_handle == nullptr)
        {
            return std::nullopt;
        }

        void* const view = MapViewOfFile(mapping_handle, FILE_MAP_WRITE, 0, 0, static_cast<SIZE_T>(payload_size));
        if (view == nullptr)
        {
            CloseHandle(mapping_handle);
            return std::nullopt;
        }

        std::memcpy(view, replacement_bytes->data(), replacement_bytes->size());
        UnmapViewOfFile(view);

        static_cast<void>(protection);
        return mapping_handle;
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

        const std::shared_ptr<const std::vector<std::uint8_t>>& replacement_bytes = virtual_handle.ReplacementBytes;
        const std::uint64_t original_position = virtual_handle.ReadPosition;
        const std::uint64_t size = static_cast<std::uint64_t>(replacement_bytes->size());
        const std::uint64_t start = std::min(original_position, size);
        const std::uint64_t available = size - start;
        const std::uint64_t requested = static_cast<std::uint64_t>(bytes_to_read);
        const std::uint64_t copied = std::min(available, requested);

        if (copied > 0)
        {
            std::memcpy(
                buffer,
                replacement_bytes->data() + static_cast<std::size_t>(start),
                static_cast<std::size_t>(copied));
        }

        virtual_handle.ReadPosition = start + copied;
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
            const std::shared_ptr<const std::vector<std::uint8_t>>& replacement_bytes = virtual_handle.ReplacementBytes;
            const std::uint64_t size = static_cast<std::uint64_t>(replacement_bytes->size());
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

        const std::shared_ptr<const std::vector<std::uint8_t>>& replacement_bytes = found->second.ReplacementBytes;
        if (replacement_bytes->size() > static_cast<std::size_t>((std::numeric_limits<LONGLONG>::max)()))
        {
            SetLastError(ERROR_INVALID_PARAMETER);
            return false;
        }

        file_size->QuadPart = static_cast<LONGLONG>(replacement_bytes->size());
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
