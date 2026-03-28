#include <HelenHook/JsonParser.h>

#include <cctype>
#include <cstdlib>
#include <charconv>
#include <string>

namespace
{
    /**
     * @brief Implements a minimal recursive-descent JSON parser for pack manifests.
     */
    class Parser final
    {
    public:
        /**
         * @brief Initializes the parser with the input text.
         * @param text JSON source text.
         */
        explicit Parser(std::string_view text)
            : text_(text)
        {
        }

        /**
         * @brief Parses the full document and rejects trailing non-whitespace data.
         * @return Parsed root value on success.
         */
        std::optional<helen::JsonValue> ParseDocument()
        {
            auto value = ParseValue();
            if (!value)
            {
                return std::nullopt;
            }

            SkipWhitespace();
            if (position_ != text_.size())
            {
                return std::nullopt;
            }

            return value;
        }

    private:
        /**
         * @brief Advances over ASCII whitespace characters.
         */
        void SkipWhitespace()
        {
            while (position_ < text_.size() && std::isspace(static_cast<unsigned char>(text_[position_])) != 0)
            {
                ++position_;
            }
        }

        /**
         * @brief Returns the current character when available.
         * @return Current character or null terminator.
         */
        char Peek() const
        {
            return position_ < text_.size() ? text_[position_] : '\0';
        }

        /**
         * @brief Consumes a required character.
         * @param expected Character to consume.
         * @return True when the expected character was present.
         */
        bool Consume(char expected)
        {
            SkipWhitespace();
            if (Peek() != expected)
            {
                return false;
            }

            ++position_;
            return true;
        }

        /**
         * @brief Parses one arbitrary JSON value.
         * @return Parsed value on success.
         */
        std::optional<helen::JsonValue> ParseValue()
        {
            SkipWhitespace();
            switch (Peek())
            {
            case '{':
                return ParseObject();
            case '[':
                return ParseArray();
            case '"':
                return ParseStringValue();
            case 't':
                return ParseTrue();
            case 'f':
                return ParseFalse();
            case 'n':
                return ParseNull();
            default:
                return ParseNumber();
            }
        }

        /**
         * @brief Parses a JSON object.
         * @return Parsed object value on success.
         */
        std::optional<helen::JsonValue> ParseObject()
        {
            if (!Consume('{'))
            {
                return std::nullopt;
            }

            helen::JsonValue::Object object;
            SkipWhitespace();
            if (Consume('}'))
            {
                return helen::JsonValue(std::move(object));
            }

            while (true)
            {
                auto key = ParseString();
                if (!key || !Consume(':'))
                {
                    return std::nullopt;
                }

                auto value = ParseValue();
                if (!value)
                {
                    return std::nullopt;
                }

                object.emplace(*key, std::move(*value));
                if (Consume('}'))
                {
                    return helen::JsonValue(std::move(object));
                }

                if (!Consume(','))
                {
                    return std::nullopt;
                }
            }
        }

        /**
         * @brief Parses a JSON array.
         * @return Parsed array value on success.
         */
        std::optional<helen::JsonValue> ParseArray()
        {
            if (!Consume('['))
            {
                return std::nullopt;
            }

            helen::JsonValue::Array array;
            SkipWhitespace();
            if (Consume(']'))
            {
                return helen::JsonValue(std::move(array));
            }

            while (true)
            {
                auto value = ParseValue();
                if (!value)
                {
                    return std::nullopt;
                }

                array.push_back(std::move(*value));
                if (Consume(']'))
                {
                    return helen::JsonValue(std::move(array));
                }

                if (!Consume(','))
                {
                    return std::nullopt;
                }
            }
        }

        /**
         * @brief Parses a JSON string value.
         * @return Parsed string wrapped as a JsonValue.
         */
        std::optional<helen::JsonValue> ParseStringValue()
        {
            auto value = ParseString();
            return value ? std::optional<helen::JsonValue>(helen::JsonValue(std::move(*value))) : std::nullopt;
        }

        /**
         * @brief Parses a JSON string.
         * @return Parsed string without quotes.
         */
        std::optional<std::string> ParseString()
        {
            SkipWhitespace();
            if (Peek() != '"')
            {
                return std::nullopt;
            }

            ++position_;
            std::string result;
            while (position_ < text_.size())
            {
                const char current = text_[position_++];
                if (current == '"')
                {
                    return result;
                }

                if (current != '\\')
                {
                    result.push_back(current);
                    continue;
                }

                if (position_ >= text_.size())
                {
                    return std::nullopt;
                }

                const char escaped = text_[position_++];
                switch (escaped)
                {
                case '"':
                case '\\':
                case '/':
                    result.push_back(escaped);
                    break;
                case 'b':
                    result.push_back('\b');
                    break;
                case 'f':
                    result.push_back('\f');
                    break;
                case 'n':
                    result.push_back('\n');
                    break;
                case 'r':
                    result.push_back('\r');
                    break;
                case 't':
                    result.push_back('\t');
                    break;
                default:
                    return std::nullopt;
                }
            }

            return std::nullopt;
        }

        /**
         * @brief Parses a JSON number.
         * @return Parsed numeric value on success.
         */
        std::optional<helen::JsonValue> ParseNumber()
        {
            SkipWhitespace();
            const std::size_t start = position_;

            if (Peek() == '-')
            {
                ++position_;
            }

            while (std::isdigit(static_cast<unsigned char>(Peek())) != 0)
            {
                ++position_;
            }

            if (Peek() == '.')
            {
                ++position_;
                while (std::isdigit(static_cast<unsigned char>(Peek())) != 0)
                {
                    ++position_;
                }
            }

            if (Peek() == 'e' || Peek() == 'E')
            {
                ++position_;
                if (Peek() == '+' || Peek() == '-')
                {
                    ++position_;
                }

                while (std::isdigit(static_cast<unsigned char>(Peek())) != 0)
                {
                    ++position_;
                }
            }

            if (position_ == start)
            {
                return std::nullopt;
            }

            const std::string token(text_.substr(start, position_ - start));
            char* end = nullptr;
            const double value = std::strtod(token.c_str(), &end);
            if (end == nullptr || *end != '\0')
            {
                return std::nullopt;
            }

            return helen::JsonValue(value);
        }

        /**
         * @brief Parses the literal true.
         * @return Parsed boolean value on success.
         */
        std::optional<helen::JsonValue> ParseTrue()
        {
            return ParseLiteral("true", helen::JsonValue(true));
        }

        /**
         * @brief Parses the literal false.
         * @return Parsed boolean value on success.
         */
        std::optional<helen::JsonValue> ParseFalse()
        {
            return ParseLiteral("false", helen::JsonValue(false));
        }

        /**
         * @brief Parses the literal null.
         * @return Parsed null value on success.
         */
        std::optional<helen::JsonValue> ParseNull()
        {
            return ParseLiteral("null", helen::JsonValue());
        }

        /**
         * @brief Parses one fixed literal token.
         * @param literal Expected token text.
         * @param value Resulting JSON value.
         * @return Parsed value on success.
         */
        std::optional<helen::JsonValue> ParseLiteral(std::string_view literal, helen::JsonValue value)
        {
            SkipWhitespace();
            if (text_.substr(position_, literal.size()) != literal)
            {
                return std::nullopt;
            }

            position_ += literal.size();
            return value;
        }

        /** @brief Source text being parsed. */
        std::string_view text_;
        /** @brief Current byte position inside the source text. */
        std::size_t position_{};
    };
}

namespace helen
{
    std::optional<JsonValue> JsonParser::Parse(std::string_view text)
    {
        Parser parser(text);
        return parser.ParseDocument();
    }
}