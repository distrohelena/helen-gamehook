#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>

#include <windows.h>

namespace helen
{
    /**
     * @brief Describes one opened virtual-file data source that can serve reads and mappings.
     *
     * Implementations own the concrete backing data for one virtualized file. The service keeps separate read cursors
     * per opened handle, while the source itself answers size queries, offset-based reads, and optional file mappings.
     */
    class VirtualFileSource
    {
    public:
        /**
         * @brief Destroys the virtual-file source through the base interface.
         */
        virtual ~VirtualFileSource() = default;

        /**
         * @brief Returns the exact byte size exposed by this virtual-file source.
         * @return Virtual-file payload size in bytes.
         */
        virtual std::uint64_t GetSize() const = 0;

        /**
         * @brief Reads bytes starting at one absolute source offset.
         * @param offset Zero-based source offset where the read should begin.
         * @param buffer Destination buffer that receives copied bytes.
         * @param bytes_to_read Maximum byte count requested by the caller.
         * @param bytes_read Receives the actual number of bytes copied into buffer.
         * @return True when the read completed successfully; otherwise false.
         */
        virtual bool Read(
            std::uint64_t offset,
            void* buffer,
            std::size_t bytes_to_read,
            std::size_t& bytes_read) = 0;

        /**
         * @brief Creates a file-mapping handle that exposes this source through normal Win32 mapping APIs.
         * @param protection Requested mapping protection flags from CreateFileMapping.
         * @param maximum_size_high High-order requested mapping size.
         * @param maximum_size_low Low-order requested mapping size.
         * @return Real file-mapping handle when the source can back one mapping; otherwise no value.
         */
        virtual std::optional<HANDLE> CreateFileMapping(
            DWORD protection,
            DWORD maximum_size_high,
            DWORD maximum_size_low) = 0;
    };
}
