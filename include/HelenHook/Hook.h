#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <windows.h>

#include <HelenHook/Memory.h>

namespace helen
{
    void** FindImportAddress(const ModuleView& module, std::string_view imported_dll, std::string_view imported_name);

    class InlineHook
    {
    public:
        InlineHook() = default;
        ~InlineHook();

        InlineHook(const InlineHook&) = delete;
        InlineHook& operator=(const InlineHook&) = delete;

        bool Install(void* target, void* detour, std::size_t patch_size);
        void Remove();
        bool IsInstalled() const noexcept;

        template <typename T>
        T Original() const
        {
            return reinterpret_cast<T>(trampoline_);
        }

    private:
        void* target_{};
        void* detour_{};
        void* trampoline_{};
        std::size_t patch_size_{};
        std::uint8_t original_bytes_[16]{};
    };

    class IatHook
    {
    public:
        IatHook() = default;
        ~IatHook();

        IatHook(const IatHook&) = delete;
        IatHook& operator=(const IatHook&) = delete;

        bool Install(const ModuleView& module, std::string_view imported_dll, std::string_view imported_name, void* replacement);
        void Remove();
        bool IsInstalled() const noexcept;

        template <typename T>
        T Original() const
        {
            return reinterpret_cast<T>(original_);
        }

    private:
        void** slot_{};
        void* original_{};
    };
}
