#pragma once
#include <HelenHook/BatmanGraphicsDraftState.h>
#include <array>
#include <vector>

namespace helen {
    /** @brief Validates a complete normalized edit before any session file or engine mutation. */
    class SessionGraphicsDelta {
    public:
        /** @brief One explicit native capability decision for each editable protocol field. */
        using Capabilities = std::array<bool, 14>;
    private:
        /** @brief Only changed fields, in stable protocol order; an empty vector means no work. */
        std::vector<BatmanGraphicsField> Changed;
    public:
        /** @brief Rejects the whole edit when any changed field lacks verified live support. */
        SessionGraphicsDelta(const BatmanGraphicsDraftState& baseline, const BatmanGraphicsDraftState& draft,
            const Capabilities& capabilities);
        /** @brief Returns immutable changed fields; unchanged unsupported fields do not block other edits. */
        const std::vector<BatmanGraphicsField>& Fields() const noexcept;
        /** @brief Encodes a normalized field to its engine integer representation, including inverse lighting. */
        static int Encode(BatmanGraphicsField field, int normalized);
        /** @brief Returns the exact INI key; read-only projections are rejected. */
        static const char* Key(BatmanGraphicsField field);
        /** @brief Returns the owning INI section, distinguishing PhysX from renderer settings. */
        static const char* Section(BatmanGraphicsField field);
    };
}
