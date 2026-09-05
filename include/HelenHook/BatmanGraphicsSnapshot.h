#pragma once

#include <HelenHook/BatmanGraphicsField.h>
#include <HelenHook/BatmanGraphicsDraftState.h>
#include <array>
#include <optional>

namespace helen {
    /** @brief Owns independently valid launcher settings from one read, with no dispatcher or file side effects. */
    class BatmanGraphicsSnapshot {
    public:
        /** @brief The fourteen INI-derived fields; desktop and transaction projections belong to the session. */
        using Values = std::array<std::optional<int>, 14>;
    private:
        /** @brief Captured normalized values; absence records a genuine missing or invalid field. */
        Values Fields;
    public:
        /** @brief Takes ownership of a complete set of field read outcomes, including explicit absences. */
        explicit BatmanGraphicsSnapshot(Values fields);
        /** @brief Returns the captured value, or no value for an invalid identifier or failed field; performs no I/O. */
        std::optional<int> Get(BatmanGraphicsField field) const noexcept;
        /** @brief Reports whether all required launcher values were valid in this one capture. */
        bool IsComplete() const noexcept;
        /** @brief Creates an independently editable complete draft only when all captured values are valid. */
        std::optional<BatmanGraphicsDraftState> TryCreateDraft() const;
    };
}
