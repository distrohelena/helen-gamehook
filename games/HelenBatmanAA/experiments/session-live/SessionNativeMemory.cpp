#include "SessionNativeMemory.h"
#include "SessionGraphicsDelta.h"
#include <HelenHook/ExecutableFingerprint.h>

namespace {
    /** @brief Reads one required value through the pinned GetBool/GetInt native ABI, not Helen's profile cache. */
    int CachedValue(const wchar_t* section, const wchar_t* key, const std::wstring& filename, bool boolean) {
        const std::uintptr_t cache = helen::SessionNativeMemory::Read<std::uintptr_t>(0x26667B0);
        if (cache == 0) { throw std::runtime_error("Engine config cache unavailable"); }
        /** @brief Both verified getters use section, key, output integer and filename, returning presence. */
        using Getter = int(__thiscall*)(void*,const wchar_t*,const wchar_t*,int*,const wchar_t*);
        int value = -1;
        if (!reinterpret_cast<Getter>(boolean ? 0x6244D0 : 0x624200)(reinterpret_cast<void*>(cache),section,key,&value,filename.c_str()) ||
            (boolean && value != 0 && value != 1)) { throw std::runtime_error("Required engine cache scalar missing or invalid"); }
        return value;
    }
}
namespace helen {
    void SessionNativeMemory::RequireExecutable() {
        wchar_t path[32768];
        const DWORD count = GetModuleFileNameW(nullptr,path,32768);
        if (count == 0 || count >= 32768 || reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr)) != 0x400000) {
            throw std::runtime_error("Session binding requires pinned executable at preferred base");
        }
        // A function-local immutable fingerprint avoids rehashing the executable on every menu click.
        static const ExecutableFingerprint fingerprint = ExecutableFingerprint::FromPath(path);
        if (fingerprint.FileSize != 38758728 || fingerprint.Sha256 != "4dac1f5e2ac6710b7378fdce74601f616f4753e3756cb5fda63c7519cc2eb028") {
            throw std::runtime_error("Session binding rejects foreign executable");
        }
        const unsigned char resize[] = {0x83,0xEC,0x28,0x55,0x56,0x8B,0xF1,0x33,0xED,0x39,0xAE,0x80,0,0,0};
        const unsigned char thread[] = {0x83,0x3D,0xE8,0x60,0x67,0x02,0x00,0x74,0x11,0xFF,0x15,0xB4,0x92,0xE2,0x01};
        const unsigned char getBool[] = {0x6A,0xFF,0x68,0x88,0xC9,0xC6,0x01};
        const unsigned char find[] = {0x6A,0xFF,0x68,0x08,0xC4,0xC6,0x01};
        const unsigned char read[] = {0x6A,0xFF,0x68,0x58,0xC0,0xC6,0x01};
        const unsigned char apply[] = {0x83,0xEC,0x28,0x83,0x3D,0xDC,0x3C,0x6C,0x02,0x00};
        RequireBytes(0xEB91D0,resize);
        RequireBytes(0x715880,thread);
        RequireBytes(0x6244D0,getBool);
        RequireBytes(0x622120,find);
        RequireBytes(0x6204A0,read);
        RequireBytes(0xC40090,apply);
    }
    std::wstring SessionNativeMemory::EngineIniPath() {
        std::wstring result;
        for (unsigned index=0; index<1024; ++index) {
            const wchar_t character = Read<wchar_t>(0x266F080 + index*sizeof(wchar_t));
            if (character == 0) {
                if (result.empty()) { throw std::runtime_error("Engine INI path empty"); }
                return result;
            }
            result.push_back(character);
        }
        throw std::runtime_error("Engine INI path unterminated");
    }
    std::filesystem::path SessionNativeMemory::EngineUserRoot() {
        const std::uintptr_t manager = Read<std::uintptr_t>(0x26667C0);
        if (manager == 0) { throw std::runtime_error("Engine file manager unavailable"); }
        const std::uintptr_t table = Read<std::uintptr_t>(manager);
        if (table != 0x22D37C0 || Read<std::uintptr_t>(table+4) != 0x5504F0 ||
            Read<std::uintptr_t>(table+0x50) != 0x5503F0 || Read<std::uintptr_t>(table+0x54) != 0x554130) {
            throw std::runtime_error("Engine filename translation binding mismatch");
        }
        const std::uintptr_t data = Read<std::uintptr_t>(manager+8);
        const int count = Read<int>(manager+0xC);
        const int capacity = Read<int>(manager+0x10);
        if (data == 0 || count < 2 || count > 32768 || capacity < count) { throw std::runtime_error("Engine user root unavailable"); }
        std::wstring root;
        for (int index=0; index<count; ++index) {
            const wchar_t character = Read<wchar_t>(data+index*sizeof(wchar_t));
            if (index == count-1) {
                if (character != 0) { throw std::runtime_error("Engine root unterminated"); }
            } else if (character == 0) {
                throw std::runtime_error("Engine root contains premature terminator");
            } else { root.push_back(character); }
        }
        return root;
    }
    int SessionNativeMemory::Cached(BatmanGraphicsField field, const std::wstring& filename) {
        const std::string key(SessionGraphicsDelta::Key(field));
        const std::string section(SessionGraphicsDelta::Section(field));
        const bool boolean = field != BatmanGraphicsField::Msaa && field != BatmanGraphicsField::Physx &&
            field != BatmanGraphicsField::PersistedWidth && field != BatmanGraphicsField::PersistedHeight;
        return CachedValue(std::wstring(section.begin(),section.end()).c_str(),std::wstring(key.begin(),key.end()).c_str(),filename,boolean);
    }
    int SessionNativeMemory::CachedDirectionalLightmaps(const std::wstring& filename) {
        return CachedValue(L"SystemSettings",L"DirectionalLightmaps",filename,true);
    }
    void SessionNativeMemory::Reload(const std::wstring& filename) {
        const std::uintptr_t cache = Read<std::uintptr_t>(0x26667B0);
        if (cache == 0) { throw std::runtime_error("Engine cache unavailable during reload"); }
        /** @brief Finds an already owned FConfigFile; create-if-missing is deliberately zero. */
        using Find = void*(__thiscall*)(void*,const wchar_t*,int);
        void* const file = reinterpret_cast<Find>(0x622120)(reinterpret_cast<void*>(cache),filename.c_str(),0);
        if (file == nullptr) { throw std::runtime_error("Required cached engine file missing"); }
        /** @brief Replaces cached sections by reading through Batman's routed file manager. */
        using ReadFile = void(__thiscall*)(void*,const wchar_t*);
        reinterpret_cast<ReadFile>(0x6204A0)(file,filename.c_str());
    }
    void SessionNativeMemory::Flush() {
        /** @brief Stock synchronous rendering-command flush, called with no parameters. */
        using FlushRendering = void(__cdecl*)();
        reinterpret_cast<FlushRendering>(0x729290)();
    }
}
