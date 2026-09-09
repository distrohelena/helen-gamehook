#include "BloomOverlayEdit.h"
#include <windows.h>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>

namespace {
    /** @brief Reports a failed real-file contract through the console. */
    void Expect(bool condition, const char* message) { if (!condition) { throw std::runtime_error(message); } }
    /** @brief Reads exact persisted bytes rather than the Windows profile cache. */
    std::string Bytes(const std::filesystem::path& path) {
        std::ifstream stream(path, std::ios::binary);
        return std::string(std::istreambuf_iterator<char>(stream), {});
    }
}
/** @brief Uses fresh real INIs to catch original-file writes, no-op staging and malformed-value acceptance. */
int wmain(int argc, wchar_t** argv) {
    SetErrorMode(0x8003);
    try {
        Expect(argc == 2, "Pass fresh fixture directory");
        const std::filesystem::path root(argv[1]);
        Expect(std::filesystem::create_directory(root), "Fixture exists");
        const std::filesystem::path original = root / L"original.ini";
        const std::filesystem::path overlay = root / L"overlay.ini";
        const std::string initial = "[SystemSettings]\r\nBloom=True\r\nDirectionalLightmaps=True\r\nResX=1280\r\n";
        { std::ofstream stream(original, std::ios::binary); stream << initial; }
        std::filesystem::copy_file(original, overlay);
        helen::BloomOverlayEdit::Stage(original, overlay, 0);
        Expect(Bytes(original) == initial, "Original INI changed");
        Expect(Bytes(overlay).find("Bloom=False") != std::string::npos, "Overlay Bloom was not written to disk");
        Expect(Bytes(overlay).find("DirectionalLightmaps=True") != std::string::npos &&
            Bytes(overlay).find("ResX=1280") != std::string::npos, "Unrelated overlay settings changed");
        helen::BloomOverlayEdit::Stage(original, overlay, 1);
        Expect(Bytes(overlay).find("Bloom=True") != std::string::npos, "Overlay on staging failed");
        bool refused = false;
        try { helen::BloomOverlayEdit::Stage(original, original, 0); }
        catch (const std::exception&) { refused = true; }
        Expect(refused && Bytes(original) == initial, "Original target accepted");
        refused = false;
        try { helen::BloomOverlayEdit::Stage(original, overlay, 2); }
        catch (const std::exception&) { refused = true; }
        Expect(refused, "Invalid Boolean accepted");
        const std::filesystem::path unicode = root / L"unicode.ini";
        const std::wstring wide = L"\xFEFF[SystemSettings]\r\nBloom=True\r\nDirectionalLightmaps=True\r\nResX=1280\r\n";
        { std::ofstream stream(unicode, std::ios::binary); stream.write(reinterpret_cast<const char*>(wide.data()), wide.size() * sizeof(wchar_t)); }
        helen::BloomOverlayEdit::Stage(original, unicode, 0);
        const std::string rawUnicode = Bytes(unicode);
        const std::wstring expected = L"Bloom=False";
        Expect(rawUnicode.size() >= 2 && static_cast<unsigned char>(rawUnicode[0]) == 0xFF &&
            static_cast<unsigned char>(rawUnicode[1]) == 0xFE && rawUnicode.find(std::string(
                reinterpret_cast<const char*>(expected.data()), expected.size() * sizeof(wchar_t))) != std::string::npos,
            "UTF-16 overlay encoding or Bloom edit incorrect");
        const std::filesystem::path alias = root / L"alias.ini";
        std::filesystem::create_hard_link(original, alias);
        refused = false;
        try { helen::BloomOverlayEdit::Stage(original, alias, 0); }
        catch (const std::exception&) { refused = true; }
        Expect(refused && Bytes(original) == initial, "Original hard-link alias accepted");
        const std::filesystem::path malformed = root / L"malformed.ini";
        const std::string missing = "[SystemSettings]\r\nResX=1280\r\n";
        { std::ofstream stream(malformed, std::ios::binary); stream << missing; }
        refused = false;
        try { helen::BloomOverlayEdit::Stage(original, malformed, 0); }
        catch (const std::exception&) { refused = true; }
        Expect(refused && Bytes(malformed) == missing, "Missing Bloom key changed a file");
        const std::filesystem::path shadows = root / L"shadows.ini";
        const std::string shadowInitial = "[SystemSettings]\r\nDynamicShadows=True\r\nBloom=True\r\nResX=1280\r\n";
        { std::ofstream stream(shadows, std::ios::binary); stream << shadowInitial; }
        helen::BloomOverlayEdit::Stage(original, shadows, 0, helen::BatmanGraphicsField::DynamicShadows);
        Expect(Bytes(shadows).find("DynamicShadows=False") != std::string::npos &&
            Bytes(shadows).find("Bloom=True") != std::string::npos && Bytes(shadows).find("ResX=1280") != std::string::npos,
            "Shadows staging changed wrong key or did not reach disk");
        helen::BloomOverlayEdit::Stage(original, shadows, 1, helen::BatmanGraphicsField::DynamicShadows);
        Expect(Bytes(shadows).find("DynamicShadows=True") != std::string::npos, "Shadows enable failed");
        refused = false;
        try { helen::BloomOverlayEdit::Stage(original, shadows, 0, helen::BatmanGraphicsField::MotionBlur); }
        catch (const std::exception&) { refused = true; }
        Expect(refused && Bytes(shadows) == shadowInitial, "Unsupported key mutated session");
        refused = false;
        try { helen::BloomOverlayEdit::Stage(original, overlay, 0, helen::BatmanGraphicsField::DynamicShadows); }
        catch (const std::exception&) { refused = true; }
        Expect(refused && Bytes(original) == initial, "Missing shadow key accepted or original changed");
        std::cout << "BLOOM_AND_SHADOWS_OVERLAY_EDIT_PASS\n";
        return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
