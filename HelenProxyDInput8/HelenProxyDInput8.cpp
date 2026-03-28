#include <windows.h>

#include <filesystem>
#include <mutex>
#include <optional>
#include <string>

namespace
{
    /**
     * @brief Signature used to call the Helen runtime bootstrap export.
     */
    using HelenInitializeFunction = BOOL(__stdcall*)();

    /**
     * @brief Signature used to call the Helen runtime shutdown export.
     */
    using HelenShutdownFunction = void(__stdcall*)();

    /**
     * @brief Signature used to forward DirectInput8Create into the real system library.
     */
    using DirectInput8CreateFunction = HRESULT(__stdcall*)(
        HINSTANCE instance_handle,
        DWORD version,
        REFIID interface_identifier,
        LPVOID* output_interface,
        LPVOID outer_unknown);

    /**
     * @brief Signature used to forward DllCanUnloadNow into the real system library.
     */
    using DllCanUnloadNowFunction = HRESULT(__stdcall*)();

    /**
     * @brief Signature used to forward DllGetClassObject into the real system library.
     */
    using DllGetClassObjectFunction = HRESULT(__stdcall*)(REFCLSID class_identifier, REFIID interface_identifier, LPVOID* output_object);

    /**
     * @brief Signature used to forward DllRegisterServer into the real system library.
     */
    using DllRegisterServerFunction = HRESULT(__stdcall*)();

    /**
     * @brief Signature used to forward DllUnregisterServer into the real system library.
     */
    using DllUnregisterServerFunction = HRESULT(__stdcall*)();

    /**
     * @brief Signature used to forward GetdfDIJoystick into the real system library.
     */
    using GetdfDIJoystickFunction = void* (__stdcall*)();

    /**
     * @brief Initialization mutex that serializes first-use loading of the real proxy targets.
     */
    std::mutex InitializationMutex;

    /**
     * @brief Module handle for the current proxy DLL instance.
     */
    HMODULE ProxyModule = nullptr;

    /**
     * @brief Module handle for the real system dinput8 library.
     */
    HMODULE RealDInputModule = nullptr;

    /**
     * @brief Module handle for the Helen runtime library loaded beside the game executable.
     */
    HMODULE HelenRuntimeModule = nullptr;

    /**
     * @brief Export pointer used to bootstrap the Helen runtime once.
     */
    HelenInitializeFunction HelenInitialize = nullptr;

    /**
     * @brief Export pointer used to request Helen shutdown during process detach when available.
     */
    HelenShutdownFunction HelenShutdown = nullptr;

    /**
     * @brief Returns whether this proxy instance has already completed one successful Helen bootstrap.
     */
    bool HelenBootstrapCompleted = false;

    /**
     * @brief Forward target for DirectInput8Create.
     */
    DirectInput8CreateFunction RealDirectInput8Create = nullptr;

    /**
     * @brief Forward target for DllCanUnloadNow.
     */
    DllCanUnloadNowFunction RealDllCanUnloadNow = nullptr;

    /**
     * @brief Forward target for DllGetClassObject.
     */
    DllGetClassObjectFunction RealDllGetClassObject = nullptr;

    /**
     * @brief Forward target for DllRegisterServer.
     */
    DllRegisterServerFunction RealDllRegisterServer = nullptr;

    /**
     * @brief Forward target for DllUnregisterServer.
     */
    DllUnregisterServerFunction RealDllUnregisterServer = nullptr;

    /**
     * @brief Forward target for GetdfDIJoystick.
     */
    GetdfDIJoystickFunction RealGetdfDIJoystick = nullptr;

    /**
     * @brief Returns the full path to the proxy DLL module.
     * @return Absolute proxy module path when it can be queried; otherwise no value.
     */
    std::optional<std::filesystem::path> TryGetProxyModulePath()
    {
        if (ProxyModule == nullptr)
        {
            return std::nullopt;
        }

        std::wstring buffer(MAX_PATH, L'\0');
        while (true)
        {
            const DWORD length = GetModuleFileNameW(ProxyModule, buffer.data(), static_cast<DWORD>(buffer.size()));
            if (length == 0)
            {
                return std::nullopt;
            }

            if (length < buffer.size() - 1)
            {
                buffer.resize(length);
                return std::filesystem::path(buffer);
            }

            buffer.resize(buffer.size() * 2);
        }
    }

    /**
     * @brief Returns the absolute path to the system dinput8 DLL.
     * @return Absolute system-library path when it can be resolved; otherwise no value.
     */
    std::optional<std::filesystem::path> TryGetSystemDInputPath()
    {
        std::wstring buffer(MAX_PATH, L'\0');
        const UINT length = GetSystemDirectoryW(buffer.data(), static_cast<UINT>(buffer.size()));
        if (length == 0 || length >= buffer.size())
        {
            return std::nullopt;
        }

        buffer.resize(length);
        return std::filesystem::path(buffer) / "dinput8.dll";
    }

    /**
     * @brief Loads the real system dinput8 library and resolves its forwarded exports.
     * @return True when the real proxy target library and its required exports are available; otherwise false.
     */
    bool EnsureRealDInputLoaded()
    {
        if (RealDInputModule != nullptr)
        {
            return true;
        }

        const std::optional<std::filesystem::path> system_dinput_path = TryGetSystemDInputPath();
        if (!system_dinput_path.has_value())
        {
            return false;
        }

        RealDInputModule = LoadLibraryW(system_dinput_path->c_str());
        if (RealDInputModule == nullptr)
        {
            return false;
        }

        RealDirectInput8Create = reinterpret_cast<DirectInput8CreateFunction>(
            GetProcAddress(RealDInputModule, "DirectInput8Create"));
        RealDllCanUnloadNow = reinterpret_cast<DllCanUnloadNowFunction>(
            GetProcAddress(RealDInputModule, "DllCanUnloadNow"));
        RealDllGetClassObject = reinterpret_cast<DllGetClassObjectFunction>(
            GetProcAddress(RealDInputModule, "DllGetClassObject"));
        RealDllRegisterServer = reinterpret_cast<DllRegisterServerFunction>(
            GetProcAddress(RealDInputModule, "DllRegisterServer"));
        RealDllUnregisterServer = reinterpret_cast<DllUnregisterServerFunction>(
            GetProcAddress(RealDInputModule, "DllUnregisterServer"));
        RealGetdfDIJoystick = reinterpret_cast<GetdfDIJoystickFunction>(
            GetProcAddress(RealDInputModule, "GetdfDIJoystick"));

        return RealDirectInput8Create != nullptr &&
            RealDllCanUnloadNow != nullptr &&
            RealDllGetClassObject != nullptr &&
            RealDllRegisterServer != nullptr &&
            RealDllUnregisterServer != nullptr &&
            RealGetdfDIJoystick != nullptr;
    }

    /**
     * @brief Loads the Helen runtime DLL beside the proxy and resolves its bootstrap exports.
     * @return True when Helen loads successfully or is already loaded; otherwise false.
     */
    bool EnsureHelenRuntimeLoaded()
    {
        if (HelenRuntimeModule != nullptr)
        {
            return true;
        }

        const std::optional<std::filesystem::path> proxy_path = TryGetProxyModulePath();
        if (!proxy_path.has_value())
        {
            return false;
        }

        const std::filesystem::path runtime_path = proxy_path->parent_path() / "HelenGameHook.dll";
        HelenRuntimeModule = LoadLibraryW(runtime_path.c_str());
        if (HelenRuntimeModule == nullptr)
        {
            return false;
        }

        HelenInitialize = reinterpret_cast<HelenInitializeFunction>(
            GetProcAddress(HelenRuntimeModule, "HelenInitialize"));
        HelenShutdown = reinterpret_cast<HelenShutdownFunction>(
            GetProcAddress(HelenRuntimeModule, "HelenShutdown"));
        if (HelenInitialize == nullptr)
        {
            FreeLibrary(HelenRuntimeModule);
            HelenRuntimeModule = nullptr;
            return false;
        }

        return true;
    }

    /**
     * @brief Ensures both the real system library and the Helen runtime are loaded before forwarding calls.
     * @return True when the proxy is fully initialized; otherwise false.
     */
    bool EnsureInitialized()
    {
        std::lock_guard<std::mutex> lock(InitializationMutex);
        if (!EnsureRealDInputLoaded())
        {
            return false;
        }

        if (!EnsureHelenRuntimeLoaded())
        {
            return false;
        }

        if (HelenBootstrapCompleted)
        {
            return true;
        }

        if (HelenInitialize == nullptr || HelenInitialize() != TRUE)
        {
            return false;
        }

        HelenBootstrapCompleted = true;
        return true;
    }
}

/**
 * @brief Captures the proxy DLL module handle used for later bootstrap path resolution.
 * @param module_handle Current proxy DLL module handle supplied by the loader.
 * @param reason Process or thread notification code supplied by the loader.
 * @param reserved Reserved loader parameter.
 * @return Always returns TRUE so the loader keeps the proxy attached.
 */
extern "C" BOOL APIENTRY DllMain(HMODULE module_handle, DWORD reason, LPVOID reserved)
{
    (void)reserved;

    if (reason == DLL_PROCESS_ATTACH)
    {
        ProxyModule = module_handle;
        DisableThreadLibraryCalls(module_handle);
    }
    else if (reason == DLL_PROCESS_DETACH)
    {
        if (HelenShutdown != nullptr)
        {
            HelenShutdown();
        }

        if (HelenRuntimeModule != nullptr)
        {
            FreeLibrary(HelenRuntimeModule);
            HelenRuntimeModule = nullptr;
        }

        if (RealDInputModule != nullptr)
        {
            FreeLibrary(RealDInputModule);
            RealDInputModule = nullptr;
        }
    }

    return TRUE;
}

/**
 * @brief Bootstraps Helen and forwards DirectInput8Create into the real system library.
 * @param instance_handle Original caller module handle.
 * @param version Requested DirectInput version.
 * @param interface_identifier Requested DirectInput interface identifier.
 * @param output_interface Receives the created DirectInput interface on success.
 * @param outer_unknown Optional outer COM object used for aggregation.
 * @return Result returned by the real system DirectInput8Create export, or a failure HRESULT when initialization fails.
 */
extern "C" __declspec(dllexport) HRESULT __stdcall DirectInput8Create(
    HINSTANCE instance_handle,
    DWORD version,
    REFIID interface_identifier,
    LPVOID* output_interface,
    LPVOID outer_unknown)
{
    if (!EnsureInitialized() || RealDirectInput8Create == nullptr)
    {
        return E_FAIL;
    }

    return RealDirectInput8Create(instance_handle, version, interface_identifier, output_interface, outer_unknown);
}

/**
 * @brief Forwards DllCanUnloadNow into the real system dinput8 library.
 * @return Result returned by the real system library, or a failure HRESULT when initialization fails.
 */
extern "C" __declspec(dllexport) HRESULT __stdcall DllCanUnloadNow()
{
    if (!EnsureInitialized() || RealDllCanUnloadNow == nullptr)
    {
        return E_FAIL;
    }

    return RealDllCanUnloadNow();
}

/**
 * @brief Forwards DllGetClassObject into the real system dinput8 library.
 * @param class_identifier Requested COM class identifier.
 * @param interface_identifier Requested COM interface identifier.
 * @param output_object Receives the created class object on success.
 * @return Result returned by the real system library, or a failure HRESULT when initialization fails.
 */
extern "C" __declspec(dllexport) HRESULT __stdcall DllGetClassObject(
    REFCLSID class_identifier,
    REFIID interface_identifier,
    LPVOID* output_object)
{
    if (!EnsureInitialized() || RealDllGetClassObject == nullptr)
    {
        return E_FAIL;
    }

    return RealDllGetClassObject(class_identifier, interface_identifier, output_object);
}

/**
 * @brief Forwards DllRegisterServer into the real system dinput8 library.
 * @return Result returned by the real system library, or a failure HRESULT when initialization fails.
 */
extern "C" __declspec(dllexport) HRESULT __stdcall DllRegisterServer()
{
    if (!EnsureInitialized() || RealDllRegisterServer == nullptr)
    {
        return E_FAIL;
    }

    return RealDllRegisterServer();
}

/**
 * @brief Forwards DllUnregisterServer into the real system dinput8 library.
 * @return Result returned by the real system library, or a failure HRESULT when initialization fails.
 */
extern "C" __declspec(dllexport) HRESULT __stdcall DllUnregisterServer()
{
    if (!EnsureInitialized() || RealDllUnregisterServer == nullptr)
    {
        return E_FAIL;
    }

    return RealDllUnregisterServer();
}

/**
 * @brief Forwards GetdfDIJoystick into the real system dinput8 library.
 * @return Result returned by the real system library, or nullptr when initialization fails.
 */
extern "C" __declspec(dllexport) void* __stdcall GetdfDIJoystick()
{
    if (!EnsureInitialized() || RealGetdfDIJoystick == nullptr)
    {
        return nullptr;
    }

    return RealGetdfDIJoystick();
}
