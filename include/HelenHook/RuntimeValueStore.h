#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <HelenHook/RuntimeSlotDefinition.h>

namespace helen
{
    /**
     * @brief Captures the current numeric contents of every declared runtime slot for transactional rollback.
     */
    using RuntimeValueSnapshot = std::map<std::string, double>;

    /**
     * @brief Hosts declared runtime slots, exposes native-address accessors, and preserves rollback snapshots.
     *
     * Task 2 supports only declared float32 slots. Slots must be registered before they can be written, which keeps
     * declarative command execution aligned with pack validation and prevents accidental runtime state creation.
     */
    class RuntimeValueStore
    {
    public:
        /**
         * @brief Captures one declared runtime slot for structured debug snapshots.
         */
        class DebugSlotView
        {
        public:
            /** @brief Stable runtime slot identifier. */
            std::string Id;
            /** @brief Absolute writable address of the backing storage. */
            std::uintptr_t Address = 0;
            /** @brief Current numeric slot value converted to double precision. */
            double Value = 0.0;
        };

        /**
         * @brief Creates an empty runtime value store with no declared slots.
         */
        RuntimeValueStore();

        /**
         * @brief Registers one declared runtime slot and seeds it with the definition's initial value.
         * @param definition Slot metadata that describes the runtime slot identifier, storage type, and initial value.
         * @return True when the slot type is supported, the initial value fits the slot storage, and the identifier was not already registered; otherwise false.
         */
        bool RegisterSlot(const RuntimeSlotDefinition& definition);

        /**
         * @brief Updates one declared runtime slot from a double-precision command value.
         * @param name Stable runtime slot identifier that should receive the supplied value.
         * @param value Double value that should be converted into the slot's supported storage type.
         * @return True when the slot exists and the supplied value can be represented by that storage type; otherwise false.
         */
        bool SetDouble(const std::string& name, double value);

        /**
         * @brief Returns the current numeric value for one declared runtime slot.
         * @param name Runtime slot identifier to query.
         * @return Stored numeric value converted to double when the slot exists; otherwise no value.
         */
        std::optional<double> TryGetDouble(const std::string& name) const;

        /**
         * @brief Returns a writable address for one declared runtime slot so mutating native consumers can bind directly to storage.
         * @param name Runtime slot identifier whose backing storage address should be returned.
         * @return Writable address of the slot storage when the slot exists; otherwise no value.
         */
        std::optional<void*> TryGetAddress(const std::string& name);

        /**
         * @brief Returns a read-only address for one declared runtime slot so const observers can inspect storage safely.
         * @param name Runtime slot identifier whose backing storage address should be returned.
         * @return Read-only address of the slot storage when the slot exists; otherwise no value.
         */
        std::optional<const void*> TryGetAddress(const std::string& name) const;

        /**
         * @brief Captures the current contents of every declared runtime slot for transactional rollback.
         * @return Snapshot of the numeric value stored in each registered slot.
         */
        RuntimeValueSnapshot TakeSnapshot() const;

        /**
         * @brief Returns the current declared runtime slots as structured debug views.
         * @return Debug views ordered by stable slot identifier.
         */
        std::vector<DebugSlotView> GetDebugSlots() const;

        /**
         * @brief Restores every declared runtime slot from a previously captured snapshot.
         * @param snapshot Snapshot whose contents should replace the current values for all registered runtime slots.
         */
        void RestoreSnapshot(const RuntimeValueSnapshot& snapshot);

    private:
        /** @brief Registered runtime slot metadata keyed by stable slot identifier. */
        std::map<std::string, RuntimeSlotDefinition> slot_definitions_;
        /** @brief Owned float32 storage blocks keyed by stable slot identifier. */
        std::map<std::string, std::unique_ptr<float>> float32_values_;
    };
}