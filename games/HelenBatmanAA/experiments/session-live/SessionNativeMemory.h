#pragma once
#include <windows.h>
#include <HelenHook/BatmanGraphicsField.h>
#include <filesystem>
#include <array>
#include <cstdint>
#include <cstring>
#include <stdexcept>

namespace helen {
    /** @brief Restricts pinned native reads to checked copies and rejects incompatible executable entry instructions. */
    class SessionNativeMemory {
    public:
        /** @brief Copies a required engine field; inaccessible or partial reads fail before callers use its value. */
        template<typename T> static T Read(std::uintptr_t address) {
            T value;
            SIZE_T copied = 0;
            if (!ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<const void*>(address),&value,sizeof(value),&copied) || copied != sizeof(value)) {
                throw std::runtime_error("Session native read failed");
            }
            return value;
        }
        /** @brief Validates the loaded function entry instead of trusting only an on-disk fingerprint. */
        template<std::size_t N> static void RequireBytes(std::uintptr_t address, const unsigned char (&expected)[N]) {
            const auto actual = Read<std::array<unsigned char,N>>(address);
            if (std::memcmp(actual.data(),expected,N) != 0) { throw std::runtime_error("Session native instruction contract mismatch"); }
        }
        /** @brief Verifies the supported on-disk executable once and its required preferred load address. */
        static void RequireExecutable();
        /** @brief Reads the exact cached logical engine INI path; no filesystem scanning is permitted. */
        static std::wstring EngineIniPath();
        /** @brief Reads the verified initialized file-manager translation root used by Batman. */
        static std::filesystem::path EngineUserRoot();
        /** @brief Uses Batman's actual config getter and rejects a missing or malformed scalar. */
        static int Cached(helen::BatmanGraphicsField field, const std::wstring& filename);
        /** @brief Reads the directional-lightmap guard through the stock Boolean getter. */
        static int CachedDirectionalLightmaps(const std::wstring& filename);
        /** @brief Replaces the existing cached FConfigFile using its exact logical filename and routed engine reader. */
        static void Reload(const std::wstring& filename);
        /** @brief Synchronously drains rendering commands using the stock settings Apply boundary. */
        static void Flush();
    };
}
