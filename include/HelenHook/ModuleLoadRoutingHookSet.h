#pragma once

#include <string>
#include <string_view>
#include <vector>

#include <windows.h>

#include <HelenHook/Hook.h>
#include <HelenHook/Memory.h>
#include <HelenHook/ModuleLoadRoutingService.h>

namespace helen
{
    /**
     * @brief Installs process-local IAT hooks that observe Batman loader requests for Bink DLL names.
     *
     * The hook set only owns the importing module's loader import slots. It does not patch the
     * system loader, and it does not replace the Bink DLLs on disk.
     */
    class ModuleLoadRoutingHookSet
    {
    public:
        /**
         * @brief Binds the hook set to one routing service that classifies loader requests.
         * @param routing_service Pure routing service that formats diagnostics for intercepted requests.
         */
        explicit ModuleLoadRoutingHookSet(ModuleLoadRoutingService& routing_service);

        /**
         * @brief Releases any installed IAT hooks and clears the active-instance bridge.
         */
        ~ModuleLoadRoutingHookSet();

        ModuleLoadRoutingHookSet(const ModuleLoadRoutingHookSet&) = delete;
        ModuleLoadRoutingHookSet& operator=(const ModuleLoadRoutingHookSet&) = delete;
        ModuleLoadRoutingHookSet(ModuleLoadRoutingHookSet&&) = delete;
        ModuleLoadRoutingHookSet& operator=(ModuleLoadRoutingHookSet&&) = delete;

        /**
         * @brief Installs loader hooks for the main executable import table.
         * @return True when at least one targeted loader import was present and every present import was patched successfully.
         */
        bool Install();

        /**
         * @brief Removes every installed hook and restores the original import table entries.
         */
        void Remove();

        /**
         * @brief Returns true when the active instance owns at least one installed loader hook.
         * @return True when the hook set is active.
         */
        bool IsInstalled() const noexcept;

    private:
        /**
         * @brief IAT detour for LoadLibraryA that logs the request and forwards the original load.
         * @param lpLibFileName ANSI module text passed to LoadLibraryA.
         * @return The original LoadLibraryA result after logging the routing decision.
         */
        static HMODULE WINAPI LoadLibraryADetour(LPCSTR lpLibFileName);

        /**
         * @brief IAT detour for LoadLibraryW that logs the request and forwards the original load.
         * @param lpLibFileName Wide module text passed to LoadLibraryW.
         * @return The original LoadLibraryW result after logging the routing decision.
         */
        static HMODULE WINAPI LoadLibraryWDetour(LPCWSTR lpLibFileName);

        /**
         * @brief IAT detour for LoadLibraryExA that logs the request and forwards the original load.
         * @param lpLibFileName ANSI module text passed to LoadLibraryExA.
         * @param hFile Optional module file handle passed to LoadLibraryExA.
         * @param dwFlags Loader flags passed to LoadLibraryExA.
         * @return The original LoadLibraryExA result after logging the routing decision.
         */
        static HMODULE WINAPI LoadLibraryExADetour(LPCSTR lpLibFileName, HANDLE hFile, DWORD dwFlags);

        /**
         * @brief IAT detour for LoadLibraryExW that logs the request and forwards the original load.
         * @param lpLibFileName Wide module text passed to LoadLibraryExW.
         * @param hFile Optional module file handle passed to LoadLibraryExW.
         * @param dwFlags Loader flags passed to LoadLibraryExW.
         * @return The original LoadLibraryExW result after logging the routing decision.
         */
        static HMODULE WINAPI LoadLibraryExWDetour(LPCWSTR lpLibFileName, HANDLE hFile, DWORD dwFlags);

        /**
         * @brief Returns the currently active hook-set instance used by the static detours.
         * @return Current active hook set or nullptr when no instance has been installed.
         */
        static ModuleLoadRoutingHookSet* Current();

        /**
         * @brief Installs one optional import hook only when the target import exists on the main executable.
         * @param hook Mutable IAT hook that should capture the located import slot.
         * @param module Main executable module whose import table should be patched.
         * @param imported_dll Imported DLL that owns the target symbol.
         * @param imported_name Imported function name that should be replaced.
         * @param replacement Replacement function pointer that should be written into the IAT slot.
         * @param import_present Receives true when the import exists on the main executable.
         * @return True when the import was absent or the hook installed successfully; otherwise false.
         */
        static bool InstallOptionalHook(
            IatHook& hook,
            const ModuleView& module,
            std::string_view imported_dll,
            std::string_view imported_name,
            void* replacement,
            bool& import_present);

        /**
         * @brief Resolves one exported kernel32 function at runtime so fallback calls bypass the patched IAT.
         * @tparam T Exact function pointer type that should be resolved.
         * @param export_name ANSI export name to look up in kernel32.dll.
         * @return The resolved function pointer when available; otherwise nullptr.
         */
        template <typename T>
        static T ResolveKernel32Export(const char* export_name) noexcept
        {
            const HMODULE kernel32_module = GetModuleHandleW(L"kernel32.dll");
            if (kernel32_module == nullptr)
            {
                return nullptr;
            }

            return reinterpret_cast<T>(GetProcAddress(kernel32_module, export_name));
        }

        /**
         * @brief Returns the real LoadLibraryA export without using the patched import slot.
         * @param lpLibFileName ANSI module text passed to LoadLibraryA.
         * @return The original LoadLibraryA result when the export can be resolved; otherwise null.
         */
        static HMODULE CallRealLoadLibraryA(LPCSTR lpLibFileName);

        /**
         * @brief Returns the real LoadLibraryW export without using the patched import slot.
         * @param lpLibFileName Wide module text passed to LoadLibraryW.
         * @return The original LoadLibraryW result when the export can be resolved; otherwise null.
         */
        static HMODULE CallRealLoadLibraryW(LPCWSTR lpLibFileName);

        /**
         * @brief Returns the real LoadLibraryExA export without using the patched import slot.
         * @param lpLibFileName ANSI module text passed to LoadLibraryExA.
         * @param hFile Optional module file handle passed to LoadLibraryExA.
         * @param dwFlags Loader flags passed to LoadLibraryExA.
         * @return The original LoadLibraryExA result when the export can be resolved; otherwise null.
         */
        static HMODULE CallRealLoadLibraryExA(LPCSTR lpLibFileName, HANDLE hFile, DWORD dwFlags);

        /**
         * @brief Returns the real LoadLibraryExW export without using the patched import slot.
         * @param lpLibFileName Wide module text passed to LoadLibraryExW.
         * @param hFile Optional module file handle passed to LoadLibraryExW.
         * @param dwFlags Loader flags passed to LoadLibraryExW.
         * @return The original LoadLibraryExW result when the export can be resolved; otherwise null.
         */
        static HMODULE CallRealLoadLibraryExW(LPCWSTR lpLibFileName, HANDLE hFile, DWORD dwFlags);

        /**
         * @brief Converts one ANSI loader request into a wide string for routing and logging.
         * @param lpLibFileName ANSI module text passed to a loader import.
         * @return Wide string representation of the request, or a sentinel string when the input is null.
         */
        static std::wstring ConvertAnsiRequest(LPCSTR lpLibFileName);

        /**
         * @brief Logs one routing decision using the active Helen log path.
         * @param api_name Loader API name used by the detour.
         * @param requested_module_name Raw module text passed to the loader API.
         */
        void LogRequest(std::wstring_view api_name, std::wstring_view requested_module_name) const;

        /**
         * @brief Routing service that classifies intercepted loader requests.
         */
        ModuleLoadRoutingService& routing_service_;

        /**
         * @brief IAT hook used to replace LoadLibraryA in the main executable imports when present.
         */
        IatHook load_library_a_hook_;

        /**
         * @brief IAT hook used to replace LoadLibraryW in the main executable imports when present.
         */
        IatHook load_library_w_hook_;

        /**
         * @brief IAT hook used to replace LoadLibraryExA in the main executable imports when present.
         */
        IatHook load_library_ex_a_hook_;

        /**
         * @brief IAT hook used to replace LoadLibraryExW in the main executable imports when present.
         */
        IatHook load_library_ex_w_hook_;

        /**
         * @brief Singleton-style active hook set used by the static detour functions.
         */
        static ModuleLoadRoutingHookSet* active_instance_;
    };
}
