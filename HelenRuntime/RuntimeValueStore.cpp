#include <HelenHook/RuntimeValueStore.h>

#include <cmath>
#include <limits>
#include <stdexcept>
#include <string_view>

namespace
{
    /**
     * @brief Canonical type string for the float32 runtime slot storage supported by Task 2.
     */
    constexpr std::string_view Float32SlotType = "float32";

    /**
     * @brief Returns whether the supplied runtime slot type is supported by the current runtime store implementation.
     * @param type Runtime slot type string from pack metadata.
     * @return True when the type is the supported float32 storage kind; otherwise false.
     */
    bool IsSupportedSlotType(std::string_view type)
    {
        return type == Float32SlotType;
    }

    /**
     * @brief Tries to convert one double-precision value into supported float32 storage without silently accepting invalid narrowing.
     * @param value Double-precision value that should be stored in one runtime slot.
     * @param converted_value Receives the narrowed float32 value when conversion is valid.
     * @return True when the value is finite and within the representable float32 range; otherwise false.
     */
    bool TryConvertToFloat32(double value, float& converted_value)
    {
        if (!std::isfinite(value))
        {
            return false;
        }

        if (value < static_cast<double>(std::numeric_limits<float>::lowest()) ||
            value > static_cast<double>(std::numeric_limits<float>::max()))
        {
            return false;
        }

        converted_value = static_cast<float>(value);
        return true;
    }
}

namespace helen
{
    RuntimeValueStore::RuntimeValueStore() = default;

    bool RuntimeValueStore::RegisterSlot(const RuntimeSlotDefinition& definition)
    {
        if (definition.Id.empty())
        {
            return false;
        }

        if (!IsSupportedSlotType(definition.Type))
        {
            return false;
        }

        if (slot_definitions_.find(definition.Id) != slot_definitions_.end())
        {
            return false;
        }

        float initial_value = 0.0f;
        if (!TryConvertToFloat32(definition.InitialValue, initial_value))
        {
            return false;
        }

        slot_definitions_.emplace(definition.Id, definition);
        float32_values_.emplace(definition.Id, std::make_unique<float>(initial_value));
        return true;
    }

    bool RuntimeValueStore::SetDouble(const std::string& name, double value)
    {
        const auto found = float32_values_.find(name);
        if (found == float32_values_.end())
        {
            return false;
        }

        float converted_value = 0.0f;
        if (!TryConvertToFloat32(value, converted_value))
        {
            return false;
        }

        *found->second = converted_value;
        return true;
    }

    std::optional<double> RuntimeValueStore::TryGetDouble(const std::string& name) const
    {
        const auto found = float32_values_.find(name);
        if (found == float32_values_.end())
        {
            return std::nullopt;
        }

        return static_cast<double>(*found->second);
    }

    std::optional<void*> RuntimeValueStore::TryGetAddress(const std::string& name)
    {
        const auto found = float32_values_.find(name);
        if (found == float32_values_.end())
        {
            return std::nullopt;
        }

        return static_cast<void*>(found->second.get());
    }

    std::optional<const void*> RuntimeValueStore::TryGetAddress(const std::string& name) const
    {
        const auto found = float32_values_.find(name);
        if (found == float32_values_.end())
        {
            return std::nullopt;
        }

        return static_cast<const void*>(found->second.get());
    }

    RuntimeValueSnapshot RuntimeValueStore::TakeSnapshot() const
    {
        RuntimeValueSnapshot snapshot;
        for (const auto& entry : float32_values_)
        {
            snapshot.emplace(entry.first, static_cast<double>(*entry.second));
        }

        return snapshot;
    }

    std::vector<RuntimeValueStore::DebugSlotView> RuntimeValueStore::GetDebugSlots() const
    {
        std::vector<DebugSlotView> debug_slots;
        debug_slots.reserve(float32_values_.size());

        for (const auto& entry : float32_values_)
        {
            DebugSlotView slot;
            slot.Id = entry.first;
            slot.Address = reinterpret_cast<std::uintptr_t>(entry.second.get());
            slot.Value = static_cast<double>(*entry.second);
            debug_slots.push_back(slot);
        }

        return debug_slots;
    }

    void RuntimeValueStore::RestoreSnapshot(const RuntimeValueSnapshot& snapshot)
    {
        if (snapshot.size() != float32_values_.size())
        {
            throw std::invalid_argument("Runtime snapshot does not match the registered runtime slots.");
        }

        for (const auto& entry : float32_values_)
        {
            const auto found = snapshot.find(entry.first);
            if (found == snapshot.end())
            {
                throw std::invalid_argument("Runtime snapshot does not contain every registered runtime slot.");
            }

            float converted_value = 0.0f;
            if (!TryConvertToFloat32(found->second, converted_value))
            {
                throw std::invalid_argument("Runtime snapshot contains a value that cannot be restored into float32 storage.");
            }

            *entry.second = converted_value;
        }
    }
}