#include <HelenHook/BatmanGraphicsDraftState.h>

#include <utility>

namespace helen {
    BatmanGraphicsDraftState::BatmanGraphicsDraftState(Values fields) : Fields(std::move(fields)) {
    }

    std::optional<BatmanGraphicsDraftState> BatmanGraphicsDraftState::TryCreate(Values fields) {
        for (std::size_t index = 0; index < fields.size(); ++index) {
            if (!IsValidValue(static_cast<BatmanGraphicsField>(index), fields[index])) {
                return std::nullopt;
            }
        }
        return BatmanGraphicsDraftState(std::move(fields));
    }

    int BatmanGraphicsDraftState::Get(BatmanGraphicsField field) const {
        return Fields.at(static_cast<std::size_t>(field));
    }

    bool BatmanGraphicsDraftState::IsValidValue(BatmanGraphicsField field, int value) noexcept {
        if (field == BatmanGraphicsField::Msaa) {
            return value == 0 || value == 1 || value == 2 || value == 3 || value == 5;
        } else if (field == BatmanGraphicsField::Physx) {
            return value >= 0 && value <= 2;
        } else if (field == BatmanGraphicsField::PersistedWidth || field == BatmanGraphicsField::PersistedHeight) {
            return value > 0;
        } else if (static_cast<std::size_t>(field) < 12) {
            return value == 0 || value == 1;
        }
        return false;
    }

    bool BatmanGraphicsDraftState::TrySet(BatmanGraphicsField field, int value) noexcept {
        const std::size_t index = static_cast<std::size_t>(field);
        if (index >= 12 || !IsValidValue(field, value)) {
            return false;
        }
        Fields[index] = value;
        return true;
    }

    bool BatmanGraphicsDraftState::TrySetResolution(int width, int height) noexcept {
        if (width <= 0 || height <= 0) {
            return false;
        }
        Fields[static_cast<std::size_t>(BatmanGraphicsField::PersistedWidth)] = width;
        Fields[static_cast<std::size_t>(BatmanGraphicsField::PersistedHeight)] = height;
        return true;
    }
}
