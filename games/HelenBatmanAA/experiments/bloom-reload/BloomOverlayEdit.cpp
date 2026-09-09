#include "BloomOverlayEdit.h"
#include <windows.h>
#include <stdexcept>
namespace helen {
    void BloomOverlayEdit::Stage(const std::filesystem::path& original, const std::filesystem::path& overlay, int selected, BatmanGraphicsField field) {
        if (field != BatmanGraphicsField::Bloom && field != BatmanGraphicsField::DynamicShadows) {
            throw std::invalid_argument("Reload staging supports only Bloom or DynamicShadows");
        }
        const wchar_t* const key = field == BatmanGraphicsField::Bloom ? L"Bloom" : L"DynamicShadows";
        if ((selected != 0 && selected != 1) || !std::filesystem::is_regular_file(original) ||
            !std::filesystem::is_regular_file(overlay) || std::filesystem::equivalent(original, overlay)) {
            throw std::invalid_argument("Reload staging requires distinct existing original/session files and a Boolean");
        }
        wchar_t before[16]{};
        const DWORD count = GetPrivateProfileStringW(L"SystemSettings", key, L"", before, 16, overlay.c_str());
        if (count == 0 || count >= 15 || (_wcsicmp(before, L"True") != 0 && _wcsicmp(before, L"False") != 0)) {
            throw std::runtime_error("Selected session INI key is missing or invalid");
        }
        const wchar_t* const value = selected == 1 ? L"True" : L"False";
        if (!WritePrivateProfileStringW(L"SystemSettings", key, value, overlay.c_str())) {
            throw std::runtime_error("Session INI setting write failed");
        }
        wchar_t after[16]{};
        if (GetPrivateProfileStringW(L"SystemSettings", key, L"", after, 16, overlay.c_str()) == 0 ||
            _wcsicmp(after, value) != 0) { throw std::runtime_error("Session INI setting verification failed"); }
    }
}
