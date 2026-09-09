#pragma once
#include <windows.h>

namespace helen {
    /** @brief Provides irreversible process-lifetime ownership of callback code in a loaded module. */
    class ProcessModulePin {
    public:
        /** @brief Pins the module containing address; never loads a DLL or releases the resulting pin. */
        static bool TryPin(const void* address) noexcept;
    };
}
