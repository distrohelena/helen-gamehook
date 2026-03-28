#include <HelenHook/RuntimeSlotDefinition.h>
#include <HelenHook/RuntimeValueStore.h>

#include <cmath>
#include <cstdint>
#include <optional>
#include <stdexcept>

namespace
{
    /**
     * @brief Throws when one required boolean condition is false so the shared test harness stops at the first failure.
     * @param condition Boolean condition that must evaluate to true.
     * @param message Failure message reported by the shared test runner.
     */
    void Expect(bool condition, const char* message)
    {
        if (!condition)
        {
            throw std::runtime_error(message);
        }
    }

    /**
     * @brief Throws when two floating-point values differ by more than the supplied tolerance.
     * @param actual Value produced by the runtime value store.
     * @param expected Value required by the current test scenario.
     * @param tolerance Maximum permitted absolute difference.
     * @param message Failure message reported by the shared test runner.
     */
    void ExpectNear(double actual, double expected, double tolerance, const char* message)
    {
        if (std::fabs(actual - expected) > tolerance)
        {
            throw std::runtime_error(message);
        }
    }

    /**
     * @brief Builds one float32 runtime slot definition used by the runtime value store coverage.
     * @param id Stable slot identifier that should be registered.
     * @param initial_value Initial numeric value stored in the slot.
     * @return Fully populated float32 runtime slot definition.
     */
    helen::RuntimeSlotDefinition CreateFloat32Slot(const char* id, double initial_value)
    {
        helen::RuntimeSlotDefinition definition;
        definition.Id = id;
        definition.Type = "float32";
        definition.InitialValue = initial_value;
        return definition;
    }
}

/**
 * @brief Verifies that declared runtime slots register predictably, expose stable addresses, and round-trip through snapshots.
 */
void RunRuntimeValueStoreTests()
{
    helen::RuntimeValueStore store;

    Expect(store.RegisterSlot(CreateFloat32Slot("subtitle.scale", 1.5)), "Expected the first float32 runtime slot to register.");
    Expect(!store.RegisterSlot(CreateFloat32Slot("subtitle.scale", 2.0)), "Duplicate runtime slot registration unexpectedly succeeded.");

    helen::RuntimeSlotDefinition unsupported_slot;
    unsupported_slot.Id = "subtitle.unsupported";
    unsupported_slot.Type = "int32";
    unsupported_slot.InitialValue = 5.0;
    Expect(!store.RegisterSlot(unsupported_slot), "Unsupported runtime slot type unexpectedly registered.");

    const std::optional<double> initial_value = store.TryGetDouble("subtitle.scale");
    Expect(initial_value.has_value(), "Registered runtime slot did not return an initial value.");
    ExpectNear(*initial_value, 1.5, 0.0001, "Runtime slot initial value mismatch.");

    Expect(store.SetDouble("subtitle.scale", 2.25), "Expected SetDouble to update a registered slot.");
    Expect(!store.SetDouble("missing.slot", 2.25), "Missing runtime slot unexpectedly accepted SetDouble.");

    const std::optional<double> updated_value = store.TryGetDouble("subtitle.scale");
    Expect(updated_value.has_value(), "Updated runtime slot value became unavailable.");
    ExpectNear(*updated_value, 2.25, 0.0001, "Updated runtime slot value mismatch.");

    const std::optional<void*> writable_address = store.TryGetAddress("subtitle.scale");
    const std::optional<const void*> readonly_address = static_cast<const helen::RuntimeValueStore&>(store).TryGetAddress("subtitle.scale");
    Expect(writable_address.has_value(), "Runtime slot did not expose a writable address.");
    Expect(readonly_address.has_value(), "Runtime slot did not expose a read-only address.");
    Expect(reinterpret_cast<std::uintptr_t>(*writable_address) == reinterpret_cast<std::uintptr_t>(*readonly_address), "Writable and read-only slot addresses diverged.");

    const std::vector<helen::RuntimeValueStore::DebugSlotView> debug_slots = store.GetDebugSlots();
    Expect(debug_slots.size() == 1, "Runtime value store debug view count mismatch.");
    Expect(debug_slots[0].Id == "subtitle.scale", "Runtime value store debug slot identifier mismatch.");
    Expect(debug_slots[0].Address == reinterpret_cast<std::uintptr_t>(*writable_address), "Runtime value store debug slot address mismatch.");
    ExpectNear(debug_slots[0].Value, 2.25, 0.0001, "Runtime value store debug slot value mismatch.");

    const helen::RuntimeValueSnapshot snapshot = store.TakeSnapshot();
    Expect(store.SetDouble("subtitle.scale", 4.5), "Expected SetDouble to update the runtime slot before snapshot restore.");
    store.RestoreSnapshot(snapshot);

    const std::optional<double> restored_value = store.TryGetDouble("subtitle.scale");
    Expect(restored_value.has_value(), "Restored runtime slot value became unavailable.");
    ExpectNear(*restored_value, 2.25, 0.0001, "Runtime slot snapshot restore mismatch.");

    helen::RuntimeValueSnapshot malformed_snapshot;
    malformed_snapshot.emplace("missing.slot", 1.0);

    bool restore_threw = false;
    try
    {
        store.RestoreSnapshot(malformed_snapshot);
    }
    catch (const std::invalid_argument&)
    {
        restore_threw = true;
    }

    Expect(restore_threw, "Malformed runtime snapshot unexpectedly restored.");
}
