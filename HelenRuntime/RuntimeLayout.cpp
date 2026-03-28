#include <HelenHook/RuntimeLayout.h>

namespace helen
{
    /**
     * @brief Builds the standard runtime directory layout from the module's parent directory.
     */
    RuntimeLayout RuntimeLayout::FromRuntimeModulePath(const std::filesystem::path& runtime_module_path)
    {
        RuntimeLayout layout{};
        layout.GameRoot = runtime_module_path.parent_path();
        layout.HelenRoot = layout.GameRoot / "helengamehook";
        layout.PacksDirectory = layout.HelenRoot / "packs";
        layout.ConfigDirectory = layout.HelenRoot / "config";
        layout.LogsDirectory = layout.HelenRoot / "logs";
        layout.AssetsDirectory = layout.HelenRoot / "assets";
        layout.CacheDirectory = layout.HelenRoot / "cache";
        return layout;
    }
}