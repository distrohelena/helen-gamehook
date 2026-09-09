#include "RefreshRequest.h"

namespace helen {
    bool RefreshRequest::Arm(std::uint64_t operation, std::uintptr_t renderer, std::uintptr_t device,
        DWORD thread, std::uintptr_t anchor) noexcept {
        AcquireSRWLockExclusive(&Lock);
        const bool accepted = Renderer == 0 && operation > LastOperation && renderer != 0 && device != 0 && thread != 0 && anchor != 0;
        if (accepted) {
            LastOperation = operation;
            Renderer = renderer;
            Device = device;
            Thread = thread;
            Anchor = anchor;
            Consumed = false;
        }
        ReleaseSRWLockExclusive(&Lock);
        return accepted;
    }
    bool RefreshRequest::Consume(std::uintptr_t renderer, std::uintptr_t device, DWORD thread, std::uintptr_t anchor) noexcept {
        AcquireSRWLockExclusive(&Lock);
        const bool accepted = Renderer != 0 && !Consumed && Renderer == renderer && Device == device && Thread == thread && Anchor == anchor;
        if (accepted) { Consumed = true; }
        ReleaseSRWLockExclusive(&Lock);
        return accepted;
    }
    void RefreshRequest::Disarm(std::uint64_t operation) noexcept {
        AcquireSRWLockExclusive(&Lock);
        if (operation == LastOperation) { Renderer = 0; Device = 0; Thread = 0; Anchor = 0; }
        ReleaseSRWLockExclusive(&Lock);
    }
    bool RefreshRequest::WasConsumed(std::uint64_t operation) noexcept {
        AcquireSRWLockShared(&Lock);
        const bool consumed = operation != 0 && operation == LastOperation && Consumed;
        ReleaseSRWLockShared(&Lock);
        return consumed;
    }
}
