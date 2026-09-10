#pragma once
#include "SessionGraphicsDelta.h"
#include <cstdint>
#include <optional>

namespace helen {
    /** @brief Describes the pinned settings payload without owning or directly patching engine memory. */
    class SessionSettingsPayload {
    public:
        /** @brief Complete stock Apply input: exactly the 0xAB dwords copied by C3FDE0. */
        using Values = std::array<std::uint32_t,0xAB>;
        /** @brief Finds a verified field byte offset; PhysX has no member in this settings owner. */
        static std::size_t Offset(BatmanGraphicsField field);
        /** @brief Finds an independent renderer summary value where one actually exists. */
        static std::optional<std::uintptr_t> Mirror(BatmanGraphicsField field);
        /** @brief Copies all unrelated payload bytes and updates only validated supported changed fields. */
        static Values Build(const Values& before, const BatmanGraphicsDraftState& draft, const SessionGraphicsDelta& delta);
        /** @brief Decodes live engine integers back to normalized protocol values, rejecting invalid states. */
        static int Decode(BatmanGraphicsField field, std::uint32_t value);
    };
}
