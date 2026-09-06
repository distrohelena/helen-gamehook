#include "FullscreenProbe.h"
#include <windows.h>
#include <array>
#include <cstring>
#include <filesystem>
#include <stdexcept>
#include <utility>

FullscreenProbe::FullscreenProbe(std::wstring iniPath) : IniPath(std::move(iniPath)) {
    if (IniPath.empty() || !std::filesystem::path(IniPath).is_absolute()) {
        throw std::invalid_argument("Probe requires an absolute launcher INI path.");
    }
}

bool FullscreenProbe::TryHandle(const char* methodName, unsigned argumentCount, PrimitiveResult& result) const {
    if (methodName == nullptr || std::strcmp(methodName, "Helen_ProbeGetFullscreen") != 0) {
        return false;
    }
    if (result.Type != 0) {
        throw std::logic_error("Probe requires Batman's pre-cleared result slot.");
    }
    if (argumentCount != 0) { return true; }

    std::array<wchar_t, 32> text{};
    const DWORD length = GetPrivateProfileStringW(L"SystemSettings", L"Fullscreen", L"",
        text.data(), static_cast<DWORD>(text.size()), IniPath.c_str());
    if (length == 0 || length >= text.size() - 1) { return true; }

    if (_wcsicmp(text.data(), L"True") == 0) {
        result.Payload[0] = 1;
    } else if (_wcsicmp(text.data(), L"False") == 0) {
        result.Payload[0] = 0;
    } else {
        return true;
    }
    result.Type = 2;
    return true;
}
