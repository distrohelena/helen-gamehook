#pragma once

#include <HelenHook/BatmanGraphicsField.h>
#include <array>
#include <optional>

namespace helen {
    /** @brief Owns one complete validated graphics draft, independent of dispatcher state and persistence. */
    class BatmanGraphicsDraftState {
    public:
        /** @brief Complete normalized launcher fields in stable graphics-field order. */
        using Values = std::array<int, 14>;
    private:
        /** @brief Required normalized values; constructor is reachable only after validation. */
        Values Fields;
        /** @brief Takes already validated values without applying settings or performing I/O. */
        explicit BatmanGraphicsDraftState(Values fields);
        /** @brief Checks the domain of a persisted scalar; session projections are never valid draft fields. */
        static bool IsValidValue(BatmanGraphicsField field, int value) noexcept;
    public:
        /** @brief Constructs a draft only when every supplied field is valid; never substitutes defaults. */
        static std::optional<BatmanGraphicsDraftState> TryCreate(Values fields);
        /** @brief Reads a required persisted field; throws out_of_range for session projections or invalid identifiers. */
        int Get(BatmanGraphicsField field) const;
        /** @brief Sets one writable scalar after validation; rejects dimensions, which require an atomic pair update. */
        bool TrySet(BatmanGraphicsField field, int value) noexcept;
        /** @brief Updates both positive dimensions together, retaining both prior values on failure. */
        bool TrySetResolution(int width, int height) noexcept;
    };
}
