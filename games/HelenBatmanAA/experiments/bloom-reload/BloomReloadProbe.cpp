#include "BloomReloadProbe.h"
#include "BloomDraft.h"
#include "BloomOverlayEdit.h"
#include "BloomIniBinding.h"
#include <HelenHook/ExecutableFingerprint.h>
#include <HelenHook/Log.h>
#include <array>
#include <cstring>
#include <stdexcept>

namespace {
    /** @brief Required session owner retained from direct graphics initialization; absence refuses this experiment. */
    std::shared_ptr<helen::FileWriteRoutingService> Routing;
    /** @brief Original user INI selected by the existing graphics runtime, never chosen by scanning scratch directories. */
    std::filesystem::path Ini;

    /** @brief Copies checked engine data without retaining pointers into mutable engine containers. */
    template<typename T> T Read(std::uintptr_t address) {
        T value;
        SIZE_T copied = 0;
        if (!ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<const void*>(address), &value, sizeof(value), &copied) ||
            copied != sizeof(value)) { throw std::runtime_error("Bloom probe engine read failed"); }
        return value;
    }
    /** @brief Refuses modified loaded entry instructions before calling a pinned native binding. */
    template<std::size_t N> void RequireBytes(std::uintptr_t address, const unsigned char (&expected)[N]) {
        const std::array<unsigned char, N> actual = Read<std::array<unsigned char, N>>(address);
        if (std::memcmp(actual.data(), expected, N) != 0) { throw std::runtime_error("Bloom probe entry bytes mismatch"); }
    }
    /** @brief Reads a required NUL-terminated engine path from the pinned global TCHAR buffer. */
    std::wstring EngineIniPath() {
        std::wstring result;
        for (unsigned index = 0; index < 1024; ++index) {
            const wchar_t character = Read<wchar_t>(0x266F080 + index * sizeof(wchar_t));
            if (character == 0) {
                if (result.empty()) { throw std::runtime_error("Engine INI path is empty"); }
                return result;
            }
            result.push_back(character);
        }
        throw std::runtime_error("Engine INI path is unterminated");
    }
    /** @brief Reads through Batman's own Boolean getter so cache observation does not use Helen's INI parser. */
    int CachedBool(std::uintptr_t cache, const wchar_t* key, const wchar_t* filename) {
        /** @brief Verified GetBool thiscall: section, key, output int, filename; callee removes 16 bytes. */
        using GetBool = int(__thiscall*)(void*, const wchar_t*, const wchar_t*, int*, const wchar_t*);
        int value = -1;
        if (!reinterpret_cast<GetBool>(0x6244D0)(reinterpret_cast<void*>(cache), L"SystemSettings", key, &value, filename) ||
            (value != 0 && value != 1)) { throw std::runtime_error("Engine configuration Boolean unavailable"); }
        return value;
    }
    /** @brief Reads the initialized user root used by the pinned file manager's install-to-user filename translation. */
    std::filesystem::path EngineUserRoot() {
        const std::uintptr_t manager = Read<std::uintptr_t>(0x26667C0);
        if (manager == 0) { throw std::runtime_error("Engine file manager unavailable"); }
        const std::uintptr_t table = Read<std::uintptr_t>(manager);
        if (table != 0x22D37C0 || Read<std::uintptr_t>(table + 4) != 0x5504F0 ||
            Read<std::uintptr_t>(table + 0x50) != 0x5503F0 || Read<std::uintptr_t>(table + 0x54) != 0x554130) {
            throw std::runtime_error("Unsupported engine filename translation bindings");
        }
        const std::uintptr_t data = Read<std::uintptr_t>(manager + 8);
        const int count = Read<int>(manager + 0xC);
        const int capacity = Read<int>(manager + 0x10);
        if (data == 0 || count < 2 || count > 32768 || capacity < count) {
            throw std::runtime_error("Engine user root is uninitialized or invalid");
        }
        std::wstring root;
        for (int index = 0; index < count; ++index) {
            const wchar_t character = Read<wchar_t>(data + index * sizeof(wchar_t));
            if (index == count - 1) {
                if (character != 0) { throw std::runtime_error("Engine user root is unterminated"); }
            } else if (character == 0) {
                throw std::runtime_error("Engine user root contains premature terminator");
            } else {
                root.push_back(character);
            }
        }
        return root;
    }
}

namespace helen {
    void BloomReloadProbe::Bind(const std::filesystem::path& ini, std::shared_ptr<FileWriteRoutingService> routing) {
        if (!Ini.empty()) { throw std::logic_error("Bloom probe already bound"); }
        Ini = ini;
        Routing = std::move(routing);
    }

    BatmanGraphicsApplyResult BloomReloadProbe::Apply(const BatmanGraphicsDraftState& baseline, const BatmanGraphicsDraftState& draft) {
        const BatmanGraphicsField field = baseline.Get(BatmanGraphicsField::DynamicShadows) != draft.Get(BatmanGraphicsField::DynamicShadows)
            ? BatmanGraphicsField::DynamicShadows : BatmanGraphicsField::Bloom;
        const int selected = BloomDraft::Select(baseline, draft, field);
        const wchar_t* const key = field == BatmanGraphicsField::Bloom ? L"Bloom" : L"DynamicShadows";
        // Pinned C20130 key table and C24B50/C20B30 renderer transfer; offsets are relative to owner+4.
        const std::size_t valueIndex = (field == BatmanGraphicsField::Bloom ? 0x30 : 0x24C) / sizeof(std::uint32_t);
        const std::uintptr_t renderAddress = field == BatmanGraphicsField::Bloom ? 0x26C0DEC : 0x26C0DF0;
        if (!Routing || Ini.empty()) { throw std::runtime_error("Bloom probe requires initialized session routing"); }
        const std::vector<FileWriteRoutingService::RouteDiagnostics> routes = Routing->GetRouteDiagnostics();
        std::filesystem::path overlay;
        for (const FileWriteRoutingService::RouteDiagnostics& route : routes) {
            if (std::filesystem::equivalent(route.OriginalPath, Ini)) {
                if (!overlay.empty() || route.WritePolicy != FileWritePolicy::Redirect || route.ReadPolicy != FileReadPolicy::Redirected ||
                    route.OverlayPath.empty()) { throw std::runtime_error("Bloom probe requires one redirected read/write route"); }
                overlay = route.OverlayPath;
            }
        }
        if (overlay.empty()) { throw std::runtime_error("Bloom session route unavailable"); }
        const std::wstring filename = EngineIniPath();
        const std::filesystem::path userRoot = EngineUserRoot();
        BloomIniBinding::Require(filename, userRoot, Ini);
        Logf(L"[bloom-reload] INI-BINDING logical=%ls user-root=%ls original=%ls session=%ls",
            filename.c_str(), userRoot.c_str(), Ini.c_str(), overlay.c_str());
        const unsigned char getBool[] = {0x6A,0xFF,0x68,0x88,0xC9,0xC6,0x01};
        const unsigned char find[] = {0x6A,0xFF,0x68,0x08,0xC4,0xC6,0x01};
        const unsigned char read[] = {0x6A,0xFF,0x68,0x58,0xC0,0xC6,0x01};
        const unsigned char apply[] = {0x83,0xEC,0x28,0x83,0x3D,0xDC,0x3C,0x6C,0x02,0x00};
        RequireBytes(0x6244D0, getBool);
        RequireBytes(0x622120, find);
        RequireBytes(0x6204A0, read);
        RequireBytes(0xC40090, apply);
        const std::uintptr_t cache = Read<std::uintptr_t>(0x26667B0);
        if (cache == 0 || Read<unsigned>(0x26C0B38 + 0x2D4) != 0) { throw std::runtime_error("Bloom probe requires game config owner"); }
        /** @brief Stock synchronous rendering-command flush, called without arguments by the settings apply path. */
        using Flush = void(__cdecl*)();
        const Flush flush = reinterpret_cast<Flush>(0x729290);
        flush();
        /** @brief Mirrors the complete 0xAB-dword value payload copied by the stock settings apply function. */
        using Settings = std::array<std::uint32_t, 0xAB>;
        const Settings before = Read<Settings>(0x26C0B3C);
        if (before[valueIndex] > 1 || static_cast<int>(before[valueIndex]) == selected) {
            throw std::runtime_error("Reload probe live value is invalid or already selected");
        }
        const int cachedBefore = CachedBool(cache, key, filename.c_str());
        const ExecutableFingerprint originalBefore = ExecutableFingerprint::FromPath(Ini);
        Logf(L"[ini-reload] BEFORE key=%ls live=%u render=%u cache=%d selected=%d session=%ls",
            key, before[valueIndex], Read<unsigned>(renderAddress), cachedBefore, selected, overlay.c_str());
        BloomOverlayEdit::Stage(Ini, overlay, selected, field);
        const int cachedAfterFile = CachedBool(cache, key, filename.c_str());
        Logf(L"[ini-reload] FILE-STAGED key=%ls selected=%d cache-before-explicit-read=%d", key, selected, cachedAfterFile);
        /** @brief Verified cache lookup with filename and create-if-missing flag; returns its owned FConfigFile. */
        using Find = void*(__thiscall*)(void*, const wchar_t*, int);
        void* const configFile = reinterpret_cast<Find>(0x622120)(reinterpret_cast<void*>(cache), filename.c_str(), 0);
        if (configFile == nullptr) { throw std::runtime_error("Cached engine file unavailable"); }
        /** @brief Stock FConfigFile read replaces cached sections from the filename using the game's routed file APIs. */
        using ReadFile = void(__thiscall*)(void*, const wchar_t*);
        reinterpret_cast<ReadFile>(0x6204A0)(configFile, filename.c_str());
        const int reloaded = CachedBool(cache, key, filename.c_str());
        Logf(L"[ini-reload] CACHE-RELOADED key=%ls value=%d", key, reloaded);
        if (reloaded != selected) { throw std::runtime_error("Explicit engine file read did not observe the edited session setting"); }
        if (CachedBool(cache, L"DirectionalLightmaps", filename.c_str()) != static_cast<int>(before[0x24 / 4])) {
            throw std::runtime_error("Reloaded lighting differs from live state; refusing broad Apply");
        }
        Settings incoming = before;
        incoming[valueIndex] = static_cast<unsigned>(reloaded);
        /** @brief Stock settings apply consumes a complete value payload and explicit save=false. */
        using ApplySettings = void(__thiscall*)(void*, const void*, int);
        reinterpret_cast<ApplySettings>(0xC40090)(reinterpret_cast<void*>(0x26C0B38), incoming.data(), 0);
        flush();
        const Settings after = Read<Settings>(0x26C0B3C);
        const unsigned rendered = Read<unsigned>(renderAddress);
        const ExecutableFingerprint originalAfter = ExecutableFingerprint::FromPath(Ini);
        Logf(L"[ini-reload] AFTER key=%ls live=%u render=%u cache=%d original-unchanged=%d",
            key, after[valueIndex], rendered, CachedBool(cache, key, filename.c_str()), originalAfter.Sha256 == originalBefore.Sha256);
        if (originalAfter.Sha256 != originalBefore.Sha256) { throw std::runtime_error("Original INI changed during reload experiment"); }
        if (after != incoming || rendered != static_cast<unsigned>(selected)) {
            throw std::runtime_error("Reload readback or unrelated settings preservation failed");
        }
        Logf(L"[ini-reload] PASS key=%ls: edited session INI reached live and render settings; unrelated value payload preserved; no Helen save.", key);
        return BatmanGraphicsApplyResult(BatmanGraphicsApplyOutcome::NotApplied, {});
    }
}
