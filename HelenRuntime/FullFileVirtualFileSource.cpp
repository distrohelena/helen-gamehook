#include <HelenHook/FullFileVirtualFileSource.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>

namespace helen
{
    FullFileVirtualFileSource::FullFileVirtualFileSource(std::vector<std::uint8_t> bytes)
        : Bytes(std::make_shared<const std::vector<std::uint8_t>>(std::move(bytes)))
    {
    }

    std::uint64_t FullFileVirtualFileSource::GetSize() const
    {
        return static_cast<std::uint64_t>(Bytes->size());
    }

    bool FullFileVirtualFileSource::Read(
        std::uint64_t offset,
        void* buffer,
        std::size_t bytes_to_read,
        std::size_t& bytes_read)
    {
        bytes_read = 0;
        if (bytes_to_read == 0)
        {
            return true;
        }

        if (buffer == nullptr)
        {
            return false;
        }

        const std::uint64_t size = GetSize();
        const std::uint64_t start = std::min(offset, size);
        const std::uint64_t available = size - start;
        const std::uint64_t requested = static_cast<std::uint64_t>(bytes_to_read);
        const std::uint64_t copied = std::min(available, requested);
        if (copied == 0)
        {
            return true;
        }

        if (copied > static_cast<std::uint64_t>((std::numeric_limits<std::size_t>::max)()))
        {
            return false;
        }

        std::memcpy(
            buffer,
            Bytes->data() + static_cast<std::size_t>(start),
            static_cast<std::size_t>(copied));

        bytes_read = static_cast<std::size_t>(copied);
        return true;
    }

    std::optional<HANDLE> FullFileVirtualFileSource::CreateFileMapping(
        DWORD protection,
        DWORD maximum_size_high,
        DWORD maximum_size_low)
    {
        if (Bytes == nullptr || Bytes->empty())
        {
            SetLastError(ERROR_FILE_INVALID);
            return std::nullopt;
        }

        const std::uint64_t requested_size = (static_cast<std::uint64_t>(maximum_size_high) << 32) | maximum_size_low;
        const std::uint64_t payload_size = GetSize();
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

        std::memcpy(view, Bytes->data(), Bytes->size());
        UnmapViewOfFile(view);

        static_cast<void>(protection);
        return mapping_handle;
    }
}
