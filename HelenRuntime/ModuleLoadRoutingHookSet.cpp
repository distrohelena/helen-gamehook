#include <HelenHook/ModuleLoadRoutingHookSet.h>

#include <HelenHook/Log.h>
#include <HelenHook/Memory.h>

#include <cstring>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

/**
 * @brief Minimal UNICODE_STRING layout needed to read LdrLoadDll requests without pulling in the full ntdll header set.
 */
struct _UNICODE_STRING
{
    /**
     * @brief Size of the string in bytes, excluding any trailing null terminator.
     */
    USHORT Length;

    /**
     * @brief Maximum size of the backing buffer in bytes.
     */
    USHORT MaximumLength;

    /**
     * @brief Wide-character buffer that stores the module text.
     */
    PWSTR Buffer;
};

/**
 * @brief Function pointer signature for the ntdll LdrLoadDll export.
 */
using LdrLoadDllFunction = NTSTATUS (NTAPI*)(PWSTR, PULONG, PUNICODE_STRING, PHANDLE);

namespace
{
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
    bool InstallOptionalHook(
        helen::IatHook& hook,
        const helen::ModuleView& module,
        std::string_view imported_dll,
        std::string_view imported_name,
        void* replacement,
        bool& import_present)
    {
        void** const slot = helen::FindImportAddress(module, imported_dll, imported_name);
        import_present = slot != nullptr;
        if (!import_present)
        {
            return true;
        }

        return hook.Install(module, imported_dll, imported_name, replacement);
    }

    /**
     * @brief Appends one module-load routing message using the active Helen logger.
     * @param routing_service Service that formats the log line.
     * @param api_name Loader API that handled the request.
     * @param requested_module_name Raw module text supplied to the loader API.
     */
    void LogRequest(
        const helen::ModuleLoadRoutingService& routing_service,
        std::wstring_view api_name,
        std::wstring_view requested_module_name)
    {
        const helen::ModuleLoadRoutingDecision decision =
            routing_service.DescribeRequest(api_name, requested_module_name);
        helen::Log(routing_service.BuildLogMessage(decision));
    }
}

namespace helen
{
    ModuleLoadRoutingHookSet* ModuleLoadRoutingHookSet::active_instance_ = nullptr;

    /**
     * @brief Binds the hook set to one routing service and stores the runtime loader flags that should be honored.
     * @param routing_service Pure routing service that formats diagnostics for intercepted requests.
     * @param enable_load_library_hooks True when the main-module LoadLibrary* routing hooks should be installed.
     * @param enable_ldr_load_dll_hook True when the deeper ntdll LdrLoadDll detour should be installed.
     */
    ModuleLoadRoutingHookSet::ModuleLoadRoutingHookSet(
        ModuleLoadRoutingService& routing_service,
        bool enable_load_library_hooks,
        bool enable_ldr_load_dll_hook)
        : routing_service_(routing_service)
        , enable_load_library_hooks_(enable_load_library_hooks)
        , enable_ldr_load_dll_hook_(enable_ldr_load_dll_hook)
    {
    }

    /**
     * @brief Releases any installed IAT hooks and clears the active-instance bridge.
     */
    ModuleLoadRoutingHookSet::~ModuleLoadRoutingHookSet()
    {
        Remove();
    }

    /**
     * @brief Returns the currently active hook-set instance used by the static detours.
     * @return Current active hook set or nullptr when no instance has been installed.
     */
    ModuleLoadRoutingHookSet* ModuleLoadRoutingHookSet::Current()
    {
        return active_instance_;
    }

    /**
     * @brief Logs one routing decision using the active Helen log path.
     * @param api_name Loader API name used by the detour.
     * @param requested_module_name Raw module text passed to the loader API.
     */
    void ModuleLoadRoutingHookSet::LogRequest(std::wstring_view api_name, std::wstring_view requested_module_name) const
    {
        const ModuleLoadRoutingDecision decision = routing_service_.DescribeRequest(api_name, requested_module_name);
        Log(routing_service_.BuildLogMessage(decision));
    }

    /**
     * @brief Converts one ANSI loader request into a wide string for routing and logging.
     * @param lpLibFileName ANSI module text passed to a loader import.
     * @return Wide string representation of the request, or a sentinel string when the input is null.
     */
    std::wstring ModuleLoadRoutingHookSet::ConvertAnsiRequest(LPCSTR lpLibFileName)
    {
        if (lpLibFileName == nullptr)
        {
            return L"<null>";
        }

        return std::wstring(lpLibFileName, lpLibFileName + std::strlen(lpLibFileName));
    }

    /**
     * @brief Converts one UNICODE_STRING loader request into a wide string for routing and logging.
     * @param module_file_name Structured module text passed to LdrLoadDll.
     * @return Wide string representation of the request, or a sentinel string when the input is null.
     */
    std::wstring ModuleLoadRoutingHookSet::ConvertUnicodeRequest(PUNICODE_STRING module_file_name)
    {
        if (module_file_name == nullptr || module_file_name->Buffer == nullptr)
        {
            return L"<null>";
        }

        const std::size_t character_count = static_cast<std::size_t>(module_file_name->Length / sizeof(wchar_t));
        return std::wstring(module_file_name->Buffer, module_file_name->Buffer + character_count);
    }

    /**
     * @brief Returns the real LoadLibraryA export without using the patched import slot.
     * @param lpLibFileName ANSI module text passed to LoadLibraryA.
     * @return The original LoadLibraryA result when the export can be resolved; otherwise null.
     */
    HMODULE ModuleLoadRoutingHookSet::CallRealLoadLibraryA(LPCSTR lpLibFileName)
    {
        const auto load_library_a = ResolveKernel32Export<decltype(&LoadLibraryA)>("LoadLibraryA");
        if (load_library_a == nullptr)
        {
            SetLastError(ERROR_PROC_NOT_FOUND);
            return nullptr;
        }

        return load_library_a(lpLibFileName);
    }

    /**
     * @brief Returns the real LoadLibraryW export without using the patched import slot.
     * @param lpLibFileName Wide module text passed to LoadLibraryW.
     * @return The original LoadLibraryW result when the export can be resolved; otherwise null.
     */
    HMODULE ModuleLoadRoutingHookSet::CallRealLoadLibraryW(LPCWSTR lpLibFileName)
    {
        const auto load_library_w = ResolveKernel32Export<decltype(&LoadLibraryW)>("LoadLibraryW");
        if (load_library_w == nullptr)
        {
            SetLastError(ERROR_PROC_NOT_FOUND);
            return nullptr;
        }

        return load_library_w(lpLibFileName);
    }

    /**
     * @brief Returns the real LoadLibraryExA export without using the patched import slot.
     * @param lpLibFileName ANSI module text passed to LoadLibraryExA.
     * @param hFile Optional module file handle passed to LoadLibraryExA.
     * @param dwFlags Loader flags passed to LoadLibraryExA.
     * @return The original LoadLibraryExA result when the export can be resolved; otherwise null.
     */
    HMODULE ModuleLoadRoutingHookSet::CallRealLoadLibraryExA(LPCSTR lpLibFileName, HANDLE hFile, DWORD dwFlags)
    {
        const auto load_library_ex_a = ResolveKernel32Export<decltype(&LoadLibraryExA)>("LoadLibraryExA");
        if (load_library_ex_a == nullptr)
        {
            SetLastError(ERROR_PROC_NOT_FOUND);
            return nullptr;
        }

        return load_library_ex_a(lpLibFileName, hFile, dwFlags);
    }

    /**
     * @brief Returns the real LoadLibraryExW export without using the patched import slot.
     * @param lpLibFileName Wide module text passed to LoadLibraryExW.
     * @param hFile Optional module file handle passed to LoadLibraryExW.
     * @param dwFlags Loader flags passed to LoadLibraryExW.
     * @return The original LoadLibraryExW result when the export can be resolved; otherwise null.
     */
    HMODULE ModuleLoadRoutingHookSet::CallRealLoadLibraryExW(LPCWSTR lpLibFileName, HANDLE hFile, DWORD dwFlags)
    {
        const auto load_library_ex_w = ResolveKernel32Export<decltype(&LoadLibraryExW)>("LoadLibraryExW");
        if (load_library_ex_w == nullptr)
        {
            SetLastError(ERROR_PROC_NOT_FOUND);
            return nullptr;
        }

        return load_library_ex_w(lpLibFileName, hFile, dwFlags);
    }

    /**
     * @brief Returns the real LdrLoadDll export without using the patched entry point.
     * @param search_path Optional search-path override passed to LdrLoadDll.
     * @param load_flags Optional flag word passed to LdrLoadDll.
     * @param module_file_name Module name or path passed to LdrLoadDll.
     * @param module_handle Receives the loaded module handle when the original export succeeds.
     * @return The original LdrLoadDll status when the export can be resolved; otherwise a failure status.
     */
    NTSTATUS ModuleLoadRoutingHookSet::CallRealLdrLoadDll(
        PWSTR search_path,
        PULONG load_flags,
        PUNICODE_STRING module_file_name,
        PHANDLE module_handle)
    {
        const ModuleLoadRoutingHookSet* current = Current();
        if (current == nullptr)
        {
            return static_cast<NTSTATUS>(0xC0000001L);
        }

        const auto ldr_load_dll = current->ldr_load_dll_hook_.Original<LdrLoadDllFunction>();
        if (ldr_load_dll == nullptr)
        {
            return static_cast<NTSTATUS>(0xC0000001L);
        }

        return ldr_load_dll(search_path, load_flags, module_file_name, module_handle);
    }

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
    bool ModuleLoadRoutingHookSet::InstallOptionalHook(
        IatHook& hook,
        const ModuleView& module,
        std::string_view imported_dll,
        std::string_view imported_name,
        void* replacement,
        bool& import_present)
    {
        void** const slot = FindImportAddress(module, imported_dll, imported_name);
        import_present = slot != nullptr;
        if (!import_present)
        {
            return true;
        }

        return hook.Install(module, imported_dll, imported_name, replacement);
    }

    /**
     * @brief Installs the enabled loader hooks for the main executable import table and the optional ntdll LdrLoadDll export.
     * @return True when at least one enabled loader entry point was present and every present hook was patched successfully.
     */
    bool ModuleLoadRoutingHookSet::Install()
    {
        if (IsInstalled())
        {
            return false;
        }

        if (active_instance_ != nullptr && active_instance_ != this)
        {
            return false;
        }

        const std::optional<ModuleView> main_module = QueryMainModule();
        if (!main_module.has_value())
        {
            return false;
        }

        active_instance_ = this;

        bool any_hook_requested = false;
        bool any_hook_installed = false;

        if (enable_load_library_hooks_)
        {
            any_hook_requested = true;

            bool import_present = false;
            if (!InstallOptionalHook(
                    load_library_a_hook_,
                    *main_module,
                    "kernel32.dll",
                    "LoadLibraryA",
                    reinterpret_cast<void*>(&LoadLibraryADetour),
                    import_present))
            {
                Remove();
                return false;
            }

            any_hook_installed = any_hook_installed || import_present;

            if (!InstallOptionalHook(
                    load_library_w_hook_,
                    *main_module,
                    "kernel32.dll",
                    "LoadLibraryW",
                    reinterpret_cast<void*>(&LoadLibraryWDetour),
                    import_present))
            {
                Remove();
                return false;
            }

            any_hook_installed = any_hook_installed || import_present;

            if (!InstallOptionalHook(
                    load_library_ex_a_hook_,
                    *main_module,
                    "kernel32.dll",
                    "LoadLibraryExA",
                    reinterpret_cast<void*>(&LoadLibraryExADetour),
                    import_present))
            {
                Remove();
                return false;
            }

            any_hook_installed = any_hook_installed || import_present;

            if (!InstallOptionalHook(
                    load_library_ex_w_hook_,
                    *main_module,
                    "kernel32.dll",
                    "LoadLibraryExW",
                    reinterpret_cast<void*>(&LoadLibraryExWDetour),
                    import_present))
            {
                Remove();
                return false;
            }

            any_hook_installed = any_hook_installed || import_present;
        }

        if (enable_ldr_load_dll_hook_)
        {
            any_hook_requested = true;

            const HMODULE ntdll_module = GetModuleHandleW(L"ntdll.dll");
            if (ntdll_module == nullptr)
            {
                Remove();
                return false;
            }

            const auto ldr_load_dll = reinterpret_cast<LdrLoadDllFunction>(GetProcAddress(ntdll_module, "LdrLoadDll"));
            if (ldr_load_dll == nullptr)
            {
                Remove();
                return false;
            }

            if (!ldr_load_dll_hook_.Install(
                    reinterpret_cast<void*>(ldr_load_dll),
                    reinterpret_cast<void*>(&LdrLoadDllDetour),
                    5))
            {
                Remove();
                return false;
            }

            any_hook_installed = true;
        }

        if (!any_hook_requested || !any_hook_installed)
        {
            Remove();
            return false;
        }

        return true;
    }

    /**
     * @brief Removes every installed hook and restores the original import table entries.
     */
    void ModuleLoadRoutingHookSet::Remove()
    {
        ldr_load_dll_hook_.Remove();
        load_library_ex_w_hook_.Remove();
        load_library_ex_a_hook_.Remove();
        load_library_w_hook_.Remove();
        load_library_a_hook_.Remove();

        if (active_instance_ == this)
        {
            active_instance_ = nullptr;
        }
    }

    /**
     * @brief Returns true when the active instance owns at least one installed loader hook.
     * @return True when the hook set is active.
     */
    bool ModuleLoadRoutingHookSet::IsInstalled() const noexcept
    {
        return active_instance_ == this
            && (load_library_a_hook_.IsInstalled()
                || load_library_w_hook_.IsInstalled()
                || load_library_ex_a_hook_.IsInstalled()
                || load_library_ex_w_hook_.IsInstalled()
                || ldr_load_dll_hook_.IsInstalled());
    }

    /**
     * @brief IAT detour for LoadLibraryA that logs the request and forwards the original load.
     * @param lpLibFileName ANSI module text passed to LoadLibraryA.
     * @return The original LoadLibraryA result after logging the routing decision.
     */
    HMODULE WINAPI ModuleLoadRoutingHookSet::LoadLibraryADetour(LPCSTR lpLibFileName)
    {
        const ModuleLoadRoutingHookSet* current = Current();
        if (current != nullptr)
        {
            current->LogRequest(L"LoadLibraryA", ConvertAnsiRequest(lpLibFileName));
        }

        return CallRealLoadLibraryA(lpLibFileName);
    }

    /**
     * @brief IAT detour for LoadLibraryW that logs the request and forwards the original load.
     * @param lpLibFileName Wide module text passed to LoadLibraryW.
     * @return The original LoadLibraryW result after logging the routing decision.
     */
    HMODULE WINAPI ModuleLoadRoutingHookSet::LoadLibraryWDetour(LPCWSTR lpLibFileName)
    {
        const ModuleLoadRoutingHookSet* current = Current();
        if (current != nullptr)
        {
            current->LogRequest(L"LoadLibraryW", lpLibFileName != nullptr ? std::wstring_view(lpLibFileName) : std::wstring_view(L"<null>"));
        }

        return CallRealLoadLibraryW(lpLibFileName);
    }

    /**
     * @brief IAT detour for LoadLibraryExA that logs the request and forwards the original load.
     * @param lpLibFileName ANSI module text passed to LoadLibraryExA.
     * @param hFile Optional module file handle passed to LoadLibraryExA.
     * @param dwFlags Loader flags passed to LoadLibraryExA.
     * @return The original LoadLibraryExA result after logging the routing decision.
     */
    HMODULE WINAPI ModuleLoadRoutingHookSet::LoadLibraryExADetour(LPCSTR lpLibFileName, HANDLE hFile, DWORD dwFlags)
    {
        const ModuleLoadRoutingHookSet* current = Current();
        if (current != nullptr)
        {
            current->LogRequest(L"LoadLibraryExA", ConvertAnsiRequest(lpLibFileName));
        }

        return CallRealLoadLibraryExA(lpLibFileName, hFile, dwFlags);
    }

    /**
     * @brief IAT detour for LoadLibraryExW that logs the request and forwards the original load.
     * @param lpLibFileName Wide module text passed to LoadLibraryExW.
     * @param hFile Optional module file handle passed to LoadLibraryExW.
     * @param dwFlags Loader flags passed to LoadLibraryExW.
     * @return The original LoadLibraryExW result after logging the routing decision.
     */
    HMODULE WINAPI ModuleLoadRoutingHookSet::LoadLibraryExWDetour(LPCWSTR lpLibFileName, HANDLE hFile, DWORD dwFlags)
    {
        const ModuleLoadRoutingHookSet* current = Current();
        if (current != nullptr)
        {
            current->LogRequest(L"LoadLibraryExW", lpLibFileName != nullptr ? std::wstring_view(lpLibFileName) : std::wstring_view(L"<null>"));
        }

        return CallRealLoadLibraryExW(lpLibFileName, hFile, dwFlags);
    }

    /**
     * @brief Inline detour for ntdll's LdrLoadDll export that logs direct loader activity before forwarding the call.
     * @param search_path Optional search-path override passed to LdrLoadDll.
     * @param load_flags Optional flag word passed to LdrLoadDll.
     * @param module_file_name Module name or path passed to LdrLoadDll.
     * @param module_handle Receives the loaded module handle when the original export succeeds.
     * @return The original LdrLoadDll status when the export can be reached; otherwise a failure status.
     */
    NTSTATUS WINAPI ModuleLoadRoutingHookSet::LdrLoadDllDetour(
        PWSTR search_path,
        PULONG load_flags,
        PUNICODE_STRING module_file_name,
        PHANDLE module_handle)
    {
        const ModuleLoadRoutingHookSet* current = Current();
        if (current != nullptr)
        {
            current->LogRequest(L"LdrLoadDll", ConvertUnicodeRequest(module_file_name));
        }

        return CallRealLdrLoadDll(search_path, load_flags, module_file_name, module_handle);
    }
}
