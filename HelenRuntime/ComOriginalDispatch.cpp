#include <HelenHook/ComOriginalDispatch.h>
#include <stdexcept>

namespace helen {
    std::vector<void*> ComOriginalDispatch::Capture(void** table, std::size_t count) {
        if (table == nullptr || count == 0) { throw std::invalid_argument("Cannot capture an empty COM table."); }
        std::lock_guard<std::mutex> lock(Mutex);
        const auto found = Originals.find(table);
        if (found != Originals.end()) {
            if (found->second.size() != count) { throw std::logic_error("COM interface size changed."); }
            return found->second;
        }
        std::vector<void*> copy(table, table + count);
        Originals.emplace(table, copy);
        return copy;
    }

    void* ComOriginalDispatch::Resolve(void* instance, std::size_t slot) {
        if (instance == nullptr) { throw std::invalid_argument("Cannot resolve a null COM object."); }
        void** const table = *static_cast<void***>(instance);
        if (table == nullptr) { throw std::invalid_argument("Cannot resolve a null COM table."); }
        std::lock_guard<std::mutex> lock(Mutex);
        const auto found = Originals.find(table);
        void* const original = found == Originals.end() ? table[slot] : found->second.at(slot);
        if (original == nullptr) { throw std::logic_error("COM dispatch has no original method."); }
        return original;
    }

    void ComOriginalDispatch::Replace(void** table, const std::vector<void*>& originals) {
        std::lock_guard<std::mutex> lock(Mutex);
        const auto found = Originals.find(table);
        if (found == Originals.end() || originals.size() != found->second.size()) {
            throw std::logic_error("Cannot refresh an uncaptured or differently sized COM table.");
        }
        found->second = originals;
    }
}
