#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <windows.h>

namespace
{
    /**
     * @brief Throws when a required condition is false so the shared test harness stops at the first broken export contract.
     * @param condition Boolean condition under test.
     * @param message Failure text reported by the shared test harness.
     */
    void Expect(bool condition, const char* message)
    {
        if (!condition)
        {
            throw std::runtime_error(message);
        }
    }

    /**
     * @brief Reads the entire text content of a repository file so export aliases can be validated as a contract.
     * @param path Absolute file path that should be loaded from disk.
     * @return Full file content as a single string.
     */
    std::string ReadAllText(const std::filesystem::path& path)
    {
        std::ifstream stream(path, std::ios::binary);
        if (!stream)
        {
            throw std::runtime_error("Failed to open HelenGameHook.def.");
        }

        return std::string(
            std::istreambuf_iterator<char>(stream),
            std::istreambuf_iterator<char>());
    }

    /**
     * @brief Resolves the repository root from the active test process path so the export contract test can locate source files reliably.
     * @return Absolute repository root path derived from the test executable location.
     */
    std::filesystem::path GetRepositoryRoot()
    {
        std::filesystem::path root = std::filesystem::current_path();
        if (std::filesystem::exists(root / "HelenGameHook" / "HelenGameHook.def"))
        {
            return root;
        }

        root = std::filesystem::absolute(root);
        for (int index = 0; index < 6; ++index)
        {
            root = root.parent_path();
            if (std::filesystem::exists(root / "HelenGameHook" / "HelenGameHook.def"))
            {
                return root;
            }
        }

        throw std::runtime_error("Failed to locate the repository root for HelenGameHook.def.");
    }

    /**
     * @brief Resolves the expected build output path for the Win32 debug Helen runtime DLL that Batman loads.
     * @return Absolute path to the built `HelenGameHook.dll` binary.
     */
    std::filesystem::path GetBuiltModulePath()
    {
        return GetRepositoryRoot() / "bin" / "Win32" / "Debug" / "HelenGameHook.dll";
    }

    /**
     * @brief Returns true when the requested export definition line is present in the module definition file.
     * @param definition_text Complete `HelenGameHook.def` content.
     * @param export_line Exact export mapping that the gameplay callback ABI requires.
     * @return True when the export mapping is present in the definition file.
     */
    bool ContainsExportLine(const std::string& definition_text, std::string_view export_line)
    {
        return definition_text.find(std::string(export_line)) != std::string::npos;
    }

    /**
     * @brief Loads the built Helen runtime DLL so the test can validate the real export table that Batman resolves at runtime.
     * @param module_path Absolute path to the built `HelenGameHook.dll` binary.
     * @return Loaded Win32 module handle for the test process.
     */
    HMODULE LoadModule(const std::filesystem::path& module_path)
    {
        const HMODULE module = LoadLibraryW(module_path.c_str());
        Expect(module != nullptr, "Failed to load the built HelenGameHook.dll.");
        return module;
    }

    /**
     * @brief Resolves one required exported callback from the loaded Helen runtime DLL.
     * @param module Loaded `HelenGameHook.dll` module handle.
     * @param export_name Exact export name that must be published by the runtime DLL.
     * @return Raw function address reported by `GetProcAddress`.
     */
    FARPROC GetRequiredExport(HMODULE module, const char* export_name)
    {
        const FARPROC export_address = GetProcAddress(module, export_name);
        Expect(export_address != nullptr, export_name);
        return export_address;
    }
}

/**
 * @brief Verifies that the runtime DLL exports the generic callback and direct config entry points consumed by Batman and future native blobs.
 */
void RunHelenGameHookExportTests()
{
    const std::filesystem::path definition_path = GetRepositoryRoot() / "HelenGameHook" / "HelenGameHook.def";
    const std::filesystem::path module_path = GetBuiltModulePath();
    const std::string definition_text = ReadAllText(definition_path);

    Expect(
        ContainsExportLine(definition_text, "Helen_GetInt=_HelenGetIntA@8"),
        "HelenGameHook.def does not export Helen_GetInt.");
    Expect(
        ContainsExportLine(definition_text, "Helen_SetInt=_HelenSetIntA@8"),
        "HelenGameHook.def does not export Helen_SetInt.");
    Expect(
        ContainsExportLine(definition_text, "Helen_RunCommand=_HelenRunCommandA@4"),
        "HelenGameHook.def does not export Helen_RunCommand.");
    Expect(
        ContainsExportLine(definition_text, "HelenSetConfigIntA=_HelenSetConfigIntA@8"),
        "HelenGameHook.def does not export HelenSetConfigIntA.");

    const HMODULE module = LoadModule(module_path);

    const FARPROC get_int_export = GetRequiredExport(module, "Helen_GetInt");
    const FARPROC get_int_a_export = GetRequiredExport(module, "HelenGetIntA");
    const FARPROC set_int_export = GetRequiredExport(module, "Helen_SetInt");
    const FARPROC set_int_a_export = GetRequiredExport(module, "HelenSetIntA");
    const FARPROC set_config_int_a_export = GetRequiredExport(module, "HelenSetConfigIntA");
    const FARPROC run_command_export = GetRequiredExport(module, "Helen_RunCommand");
    const FARPROC run_command_a_export = GetRequiredExport(module, "HelenRunCommandA");

    Expect(get_int_export == get_int_a_export, "Helen_GetInt does not alias HelenGetIntA in the built DLL.");
    Expect(set_int_export == set_int_a_export, "Helen_SetInt does not alias HelenSetIntA in the built DLL.");
    Expect(set_config_int_a_export != nullptr, "HelenSetConfigIntA is missing from the built DLL.");
    Expect(run_command_export == run_command_a_export, "Helen_RunCommand does not alias HelenRunCommandA in the built DLL.");

    FreeLibrary(module);
}
