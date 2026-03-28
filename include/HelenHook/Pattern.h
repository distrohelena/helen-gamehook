#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

#include <HelenHook/Memory.h>

namespace helen
{
    struct PatternToken
    {
        std::uint8_t value{};
        bool wildcard{};
    };

    class BytePattern
    {
    public:
        static std::optional<BytePattern> Parse(std::string_view pattern_text);

        const std::vector<PatternToken>& Tokens() const noexcept;
        const std::uint8_t* FindFirst(const std::uint8_t* data, std::size_t size) const;

    private:
        explicit BytePattern(std::vector<PatternToken> tokens);

        std::vector<PatternToken> tokens_;
    };

    std::optional<std::uintptr_t> FindPattern(const ModuleView& module, std::string_view pattern_text, std::string_view section_name = ".text");
}
