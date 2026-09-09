#include "RefreshRequestScope.h"

namespace helen {
    RefreshRequestScope::RefreshRequestScope(RefreshRequest& request, std::uint64_t operation) noexcept
        : Request(request), Operation(operation) {}
    RefreshRequestScope::~RefreshRequestScope() noexcept {
        if (Armed) { Request.Disarm(Operation); }
    }
    bool RefreshRequestScope::Arm(std::uintptr_t renderer, std::uintptr_t device, DWORD thread, std::uintptr_t anchor) noexcept {
        if (Armed) { return false; }
        Armed = Request.Arm(Operation, renderer, device, thread, anchor);
        return Armed;
    }
}
