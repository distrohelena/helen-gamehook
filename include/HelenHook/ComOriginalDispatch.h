#pragma once
#include <cstddef>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace helen {
    /** Preserves original shared COM dispatch independently of optional object tracking.
     * No method invokes COM. Returned addresses/copies do not borrow registry storage.
     */
    class ComOriginalDispatch {
        /** Serializes snapshot publication and lookup; never held by a caller's COM invocation. */
        std::mutex Mutex;
        /** Original table images retained even after the last tracked object is released. */
        std::unordered_map<void**, std::vector<void*>> Originals;
    public:
        /** Captures an unpatched table once and returns an owned copy; repeated captures preserve originals. */
        std::vector<void*> Capture(void** table, std::size_t count);
        /** Resolves an original slot, or a live slot of an uncaptured table; invalid captured slots throw. */
        void* Resolve(void* instance, std::size_t slot);
        /** Publishes refreshed driver entries for a captured table with the same interface size. */
        void Replace(void** table, const std::vector<void*>& originals);
    };
}
