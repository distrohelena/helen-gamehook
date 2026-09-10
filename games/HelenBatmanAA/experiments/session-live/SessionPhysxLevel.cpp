#include "SessionPhysxLevel.h"
#include <stdexcept>
#include <atomic>

namespace helen {
    void SessionPhysxLevel::Apply(int& current, int expected, int selected) {
        if (expected < 0 || expected > 2 || selected < 0 || selected > 2) {
            throw std::invalid_argument("PhysX levels must be Off, Normal or High");
        }
        std::atomic_ref<int> field(current);
        if (!field.compare_exchange_strong(expected,selected)) {
            throw std::runtime_error("Live PhysX level changed before update");
        }
    }
}
