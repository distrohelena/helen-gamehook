#pragma once
#include <filesystem>
#include <HelenHook/BatmanGraphicsField.h>

namespace helen {
    /** @brief Edits only the selected Bloom or DynamicShadows key in an explicitly resolved session file, never the original. */
    class BloomOverlayEdit {
    public:
        /** @brief Requires distinct existing files and a present supported Boolean key; rejects malformed requests before writing. */
        static void Stage(const std::filesystem::path& original, const std::filesystem::path& overlay, int selected,
            BatmanGraphicsField field = BatmanGraphicsField::Bloom);
    };
}
