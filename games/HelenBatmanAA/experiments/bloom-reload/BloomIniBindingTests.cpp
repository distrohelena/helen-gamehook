#include "BloomIniBinding.h"
#include <fstream>
#include <iostream>
#include <stdexcept>

/** @brief Requires an invalid mapping to fail before any session edit is authorized. */
void RequireRejected(const std::wstring& logicalName, const std::filesystem::path& root, const std::filesystem::path& original) {
    try { helen::BloomIniBinding::Require(logicalName, root, original); }
    catch (const std::exception&) { return; }
    throw std::runtime_error("Invalid INI binding accepted");
}

/** @brief Exercises the captured cache key using a real file and rejects wrong roots and unsupported logical names. */
int main(int argc, char** argv) {
    try {
        if (argc != 2) { throw std::runtime_error("Fixture root required"); }
        const std::filesystem::path root = std::filesystem::absolute(argv[1]);
        const std::filesystem::path original = root / "BmGame/Config/BmEngine.ini";
        std::filesystem::create_directories(original.parent_path());
        std::ofstream(original) << "[SystemSettings]\nBloom=True\n";
        helen::BloomIniBinding::Require(L"..\\BmGame\\Config\\BmEngine.ini", root, original);
        RequireRejected(L"..\\BmGame\\Config\\BmEngine.ini", root / "wrong", original);
        RequireRejected(L"..\\BmGame\\Config\\Other.ini", root, original);
        RequireRejected(original.wstring(), root, original);
        RequireRejected(L"..\\BmGame\\Config\\BmEngine.ini", {}, original);
        RequireRejected(L"..\\BmGame\\Config\\BmEngine.ini", L"relative", original);
        std::cout << "PASS: logical INI maps to protected user file; unsupported mappings refused\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
