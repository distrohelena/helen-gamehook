#include <HelenHook/Pattern.h>

#include <cctype>
#include <optional>
#include <string_view>

namespace
{
    /**
     * @brief Converts one hexadecimal character into its numeric nibble value.
     * @param character ASCII character that should be interpreted as hexadecimal.
     * @return Nibble value when the character is hexadecimal; otherwise no value.
     */
    std::optional<std::uint8_t> TryParseHexDigit(char character)
    {
        if (character >= '0' && character <= '9')
        {
            return static_cast<std::uint8_t>(character - '0');
        }

        const char upper_character = static_cast<char>(std::toupper(static_cast<unsigned char>(character)));
        if (upper_character >= 'A' && upper_character <= 'F')
        {
            return static_cast<std::uint8_t>(10 + (upper_character - 'A'));
        }

        return std::nullopt;
    }

    /**
     * @brief Parses one two-digit hexadecimal byte token.
     * @param token Token text that should contain exactly two hexadecimal digits.
     * @param value Receives the parsed byte value on success.
     * @return True when the token is valid hexadecimal; otherwise false.
     */
    bool TryParseByteToken(std::string_view token, std::uint8_t& value)
    {
        if (token.size() != 2)
        {
            return false;
        }

        const std::optional<std::uint8_t> high_nibble = TryParseHexDigit(token[0]);
        const std::optional<std::uint8_t> low_nibble = TryParseHexDigit(token[1]);
        if (!high_nibble.has_value() || !low_nibble.has_value())
        {
            return false;
        }

        value = static_cast<std::uint8_t>((*high_nibble << 4) | *low_nibble);
        return true;
    }
}

namespace helen
{
    /**
     * @brief Creates one parsed byte pattern from the supplied textual pattern expression.
     * @param pattern_text Pattern text containing hexadecimal byte tokens and `?` wildcards.
     * @return Parsed byte pattern when the text is valid; otherwise no value.
     */
    std::optional<BytePattern> BytePattern::Parse(std::string_view pattern_text)
    {
        std::vector<PatternToken> tokens;

        std::size_t cursor = 0;
        while (cursor < pattern_text.size())
        {
            while (cursor < pattern_text.size() && std::isspace(static_cast<unsigned char>(pattern_text[cursor])) != 0)
            {
                ++cursor;
            }

            if (cursor >= pattern_text.size())
            {
                break;
            }

            std::size_t token_end = cursor;
            while (token_end < pattern_text.size() && std::isspace(static_cast<unsigned char>(pattern_text[token_end])) == 0)
            {
                ++token_end;
            }

            const std::string_view token_text = pattern_text.substr(cursor, token_end - cursor);
            PatternToken token;
            if (token_text == "?" || token_text == "??")
            {
                token.wildcard = true;
                tokens.push_back(token);
                cursor = token_end;
                continue;
            }

            std::uint8_t byte_value = 0;
            if (!TryParseByteToken(token_text, byte_value))
            {
                return std::nullopt;
            }

            token.value = byte_value;
            token.wildcard = false;
            tokens.push_back(token);
            cursor = token_end;
        }

        if (tokens.empty())
        {
            return std::nullopt;
        }

        return BytePattern(std::move(tokens));
    }

    /**
     * @brief Returns the parsed token sequence used by this byte pattern.
     * @return Immutable token vector in parse order.
     */
    const std::vector<PatternToken>& BytePattern::Tokens() const noexcept
    {
        return tokens_;
    }

    /**
     * @brief Searches one contiguous memory block for the first occurrence of the pattern.
     * @param data Range start that should be searched.
     * @param size Byte length of the searched range.
     * @return Pointer to the first matching byte sequence when found; otherwise nullptr.
     */
    const std::uint8_t* BytePattern::FindFirst(const std::uint8_t* data, std::size_t size) const
    {
        if (data == nullptr || tokens_.empty() || size < tokens_.size())
        {
            return nullptr;
        }

        const std::size_t last_start = size - tokens_.size();
        for (std::size_t start = 0; start <= last_start; ++start)
        {
            bool matches = true;
            for (std::size_t token_index = 0; token_index < tokens_.size(); ++token_index)
            {
                const PatternToken& token = tokens_[token_index];
                if (!token.wildcard && data[start + token_index] != token.value)
                {
                    matches = false;
                    break;
                }
            }

            if (matches)
            {
                return data + start;
            }
        }

        return nullptr;
    }

    /**
     * @brief Creates one byte pattern from a pre-parsed token vector.
     * @param tokens Parsed pattern tokens stored by value.
     */
    BytePattern::BytePattern(std::vector<PatternToken> tokens)
        : tokens_(std::move(tokens))
    {
    }

    /**
     * @brief Resolves the first match for one textual byte pattern inside the requested module section.
     * @param module Loaded module that should be searched.
     * @param pattern_text Pattern text containing hexadecimal byte tokens and wildcards.
     * @param section_name Section name that bounds the search range.
     * @return Absolute match address when the pattern resolves successfully; otherwise no value.
     */
    std::optional<std::uintptr_t> FindPattern(const ModuleView& module, std::string_view pattern_text, std::string_view section_name)
    {
        const std::optional<BytePattern> pattern = BytePattern::Parse(pattern_text);
        if (!pattern.has_value())
        {
            return std::nullopt;
        }

        const std::optional<SectionView> section = FindSection(module, section_name);
        if (!section.has_value())
        {
            return std::nullopt;
        }

        const auto* section_data = reinterpret_cast<const std::uint8_t*>(section->address);
        const std::uint8_t* const match = pattern->FindFirst(section_data, section->size);
        if (match == nullptr)
        {
            return std::nullopt;
        }

        return reinterpret_cast<std::uintptr_t>(match);
    }
}
