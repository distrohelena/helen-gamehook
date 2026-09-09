#pragma once
#include <atomic>
#include <cstdint>

namespace helen {
    /** @brief Experimental Batman binding that requests one stock rebuild without changing display dimensions. */
    class SameSizeRefresh {
    private:
        /** @brief Published only after the permanent startup splice and its immutable destinations are ready. */
        static std::atomic<bool> Installed;
    public:
        /** @brief Installs only inside the pinned proxy startup window; a foreign executable remains untouched. */
        static void InstallAtStartup();
        /** @brief Refuses before presentation-policy publication if startup authorization or installation failed. */
        static void RequireInstalled();
        /** @brief Runs the verified resize with live dimensions/mode; requires synchronous decision consumption. */
        static void Invoke(std::uintptr_t owner, std::uintptr_t renderer, unsigned width, unsigned height, int fullscreen);
    };
}
