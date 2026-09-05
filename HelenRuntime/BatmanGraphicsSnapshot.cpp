#include <HelenHook/BatmanGraphicsSnapshot.h>

#include <utility>

namespace helen {
    BatmanGraphicsSnapshot::BatmanGraphicsSnapshot(Values fields) : Fields(std::move(fields)) {
    }

    std::optional<int> BatmanGraphicsSnapshot::Get(BatmanGraphicsField field) const noexcept {
        const std::size_t index = static_cast<std::size_t>(field);
        return index < Fields.size() ? Fields[index] : std::nullopt;
    }

    bool BatmanGraphicsSnapshot::IsComplete() const noexcept {
        for (const std::optional<int>& field : Fields) {
            if (!field.has_value()) {
                return false;
            }
        }
        return true;
    }

    std::optional<BatmanGraphicsDraftState> BatmanGraphicsSnapshot::TryCreateDraft() const {
        if (!IsComplete()) {
            return std::nullopt;
        }
        BatmanGraphicsDraftState::Values values;
        for (std::size_t index = 0; index < Fields.size(); ++index) {
            values[index] = *Fields[index];
        }
        return BatmanGraphicsDraftState::TryCreate(values);
    }
}
