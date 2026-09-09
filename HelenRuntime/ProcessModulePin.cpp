#include <HelenHook/ProcessModulePin.h>

namespace helen {
    bool ProcessModulePin::TryPin(const void* address) noexcept {
        if (address == nullptr) { return false; }
        HMODULE pinned = nullptr;
        return GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN,
            static_cast<LPCWSTR>(address), &pinned) != FALSE;
    }
}
