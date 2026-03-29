#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

#include <windows.h>

#include <HelenHook/VirtualFileSource.h>

namespace helen
{
    /**
     * @brief Exposes one complete in-memory replacement payload through the generic virtual-file source interface.
     *
     * This source preserves the existing replace-on-read behavior by keeping the full payload in memory and answering
     * reads directly from that immutable byte buffer.
     */
    class FullFileVirtualFileSource final : public VirtualFileSource
    {
    public:
        /** @brief Shared immutable payload bytes backing this source. */
        std::shared_ptr<const std::vector<std::uint8_t>> Bytes;

        /**
         * @brief Creates one full-file source that owns the supplied replacement bytes.
         * @param bytes Complete replacement payload moved into shared immutable storage.
         */
        explicit FullFileVirtualFileSource(std::vector<std::uint8_t> bytes);

        /**
         * @brief Returns the exact payload size exposed by this full-file source.
         * @return Replacement payload size in bytes.
         */
        std::uint64_t GetSize() const override;

        /**
         * @brief Reads bytes from the in-memory payload starting at one absolute offset.
         * @param offset Zero-based source offset where the read should begin.
         * @param buffer Destination buffer that receives copied bytes.
         * @param bytes_to_read Maximum byte count requested by the caller.
         * @param bytes_read Receives the actual number of bytes copied into buffer.
         * @return True when the read completed successfully; otherwise false.
         */
        bool Read(
            std::uint64_t offset,
            void* buffer,
            std::size_t bytes_to_read,
            std::size_t& bytes_read) override;

        /**
         * @brief Creates one Win32 file mapping backed by the in-memory replacement payload.
         * @param protection Requested mapping protection flags from CreateFileMapping.
         * @param maximum_size_high High-order requested mapping size.
         * @param maximum_size_low Low-order requested mapping size.
         * @return Real file-mapping handle when the payload can back one mapping; otherwise no value.
         */
        std::optional<HANDLE> CreateFileMapping(
            DWORD protection,
            DWORD maximum_size_high,
            DWORD maximum_size_low) override;
    };
}
