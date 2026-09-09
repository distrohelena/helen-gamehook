#include "BloomIniBinding.h"
#include <stdexcept>

namespace helen {
    void BloomIniBinding::Require(const std::wstring& logicalName, const std::filesystem::path& userRoot, const std::filesystem::path& original) {
        if (logicalName != L"..\\BmGame\\Config\\BmEngine.ini" || !userRoot.is_absolute()) {
            throw std::runtime_error("Unsupported engine INI logical name or user root");
        }
        if (!std::filesystem::equivalent(userRoot / L"BmGame/Config/BmEngine.ini", original)) {
            throw std::runtime_error("Engine reads a different INI than the protected user file");
        }
    }
}
