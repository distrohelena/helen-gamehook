#include <HelenHook/Memory.h>

#include <algorithm>
#include <array>
#include <cstring>

#include <winnt.h>

namespace
{
    std::wstring QueryModulePath(HMODULE module)
    {
        std::wstring buffer(MAX_PATH, L'\0');

        while (true)
        {
            const DWORD length = GetModuleFileNameW(module, buffer.data(), static_cast<DWORD>(buffer.size()));
            if (length == 0)
            {
                return {};
            }

            if (length < buffer.size() - 1)
            {
                buffer.resize(length);
                return buffer;
            }

            buffer.resize(buffer.size() * 2);
        }
    }

    const IMAGE_NT_HEADERS32* QueryNtHeaders(std::uintptr_t base_address)
    {
        const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base_address);
        if (!dos || dos->e_magic != IMAGE_DOS_SIGNATURE)
        {
            return nullptr;
        }

        const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS32*>(base_address + static_cast<std::uintptr_t>(dos->e_lfanew));
        if (!nt || nt->Signature != IMAGE_NT_SIGNATURE)
        {
            return nullptr;
        }

        return nt;
    }

    std::string NormalizeSectionName(const IMAGE_SECTION_HEADER& header)
    {
        std::array<char, IMAGE_SIZEOF_SHORT_NAME + 1> buffer{};
        std::memcpy(buffer.data(), header.Name, IMAGE_SIZEOF_SHORT_NAME);
        return buffer.data();
    }
}

namespace helen
{
    std::optional<ModuleView> QueryModule(HMODULE module)
    {
        if (!module)
        {
            return std::nullopt;
        }

        const auto path_text = QueryModulePath(module);
        if (path_text.empty())
        {
            return std::nullopt;
        }

        const auto base_address = reinterpret_cast<std::uintptr_t>(module);
        const auto* nt = QueryNtHeaders(base_address);
        if (!nt)
        {
            return std::nullopt;
        }

        ModuleView view{};
        view.handle = module;
        view.path = path_text;
        view.name = view.path.filename().wstring();
        view.base_address = base_address;
        view.image_size = nt->OptionalHeader.SizeOfImage;
        return view;
    }

    std::optional<ModuleView> QueryMainModule()
    {
        return QueryModule(GetModuleHandleW(nullptr));
    }

    std::optional<SectionView> FindSection(const ModuleView& module, std::string_view section_name)
    {
        const auto* nt = QueryNtHeaders(module.base_address);
        if (!nt)
        {
            return std::nullopt;
        }

        const auto* first_section = IMAGE_FIRST_SECTION(const_cast<IMAGE_NT_HEADERS32*>(nt));
        for (WORD i = 0; i < nt->FileHeader.NumberOfSections; ++i)
        {
            const auto& section = first_section[i];
            const std::string name = NormalizeSectionName(section);
            if (name != section_name)
            {
                continue;
            }

            SectionView view{};
            view.name = name;
            view.address = module.base_address + section.VirtualAddress;
            view.size = std::max<std::size_t>(section.SizeOfRawData, section.Misc.VirtualSize);
            return view;
        }

        return std::nullopt;
    }

    bool ProtectMemory(void* address, std::size_t size, DWORD new_protect, DWORD& old_protect)
    {
        return VirtualProtect(address, size, new_protect, &old_protect) == TRUE;
    }

    bool WriteMemory(void* address, const void* data, std::size_t size)
    {
        DWORD old_protect{};
        if (!ProtectMemory(address, size, PAGE_EXECUTE_READWRITE, old_protect))
        {
            return false;
        }

        std::memcpy(address, data, size);
        FlushInstructionCache(GetCurrentProcess(), address, size);

        DWORD ignored{};
        ProtectMemory(address, size, old_protect, ignored);
        return true;
    }

    bool FillMemoryBytes(void* address, std::uint8_t value, std::size_t size)
    {
        DWORD old_protect{};
        if (!ProtectMemory(address, size, PAGE_EXECUTE_READWRITE, old_protect))
        {
            return false;
        }

        std::memset(address, value, size);
        FlushInstructionCache(GetCurrentProcess(), address, size);

        DWORD ignored{};
        ProtectMemory(address, size, old_protect, ignored);
        return true;
    }

    void* AllocateExecutable(std::size_t size)
    {
        return VirtualAlloc(nullptr, size, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    }
}

