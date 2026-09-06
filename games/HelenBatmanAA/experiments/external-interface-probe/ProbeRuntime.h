#pragma once
#include <filesystem>

/** Binds the experimental reader before any probe hook is installed; throws on duplicate initialization. */
void InitializeBatmanDirectProbe(const std::filesystem::path& launcherIniPath);
