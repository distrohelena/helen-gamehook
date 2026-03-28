#pragma once

#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace helen
{
    /**
     * @brief Represents a parsed JSON value used by pack manifests.
     */
    class JsonValue
    {
    public:
        /** @brief Enumerates the JSON value kinds supported by the parser. */
        enum class Kind
        {
            Null,
            Boolean,
            Number,
            String,
            Array,
            Object
        };

        /** @brief Represents a JSON array. */
        using Array = std::vector<JsonValue>;
        /** @brief Represents a JSON object. */
        using Object = std::map<std::string, JsonValue>;

        /** @brief Creates a null JSON value. */
        JsonValue();
        /** @brief Creates a boolean JSON value. */
        explicit JsonValue(bool value);
        /** @brief Creates a numeric JSON value. */
        explicit JsonValue(double value);
        /** @brief Creates a string JSON value. */
        explicit JsonValue(std::string value);
        /** @brief Creates an array JSON value. */
        explicit JsonValue(Array value);
        /** @brief Creates an object JSON value. */
        explicit JsonValue(Object value);

        /** @brief Returns the stored JSON kind. */
        Kind GetKind() const noexcept;
        /** @brief Returns true when the value is null. */
        bool IsNull() const noexcept;
        /** @brief Returns true when the value is a boolean. */
        bool IsBoolean() const noexcept;
        /** @brief Returns true when the value is numeric. */
        bool IsNumber() const noexcept;
        /** @brief Returns true when the value is a string. */
        bool IsString() const noexcept;
        /** @brief Returns true when the value is an array. */
        bool IsArray() const noexcept;
        /** @brief Returns true when the value is an object. */
        bool IsObject() const noexcept;

        /** @brief Returns the boolean value when the kind matches. */
        std::optional<bool> AsBoolean() const;
        /** @brief Returns the numeric value when the kind matches. */
        std::optional<double> AsNumber() const;
        /** @brief Returns the string value when the kind matches. */
        const std::string* AsString() const;
        /** @brief Returns the array value when the kind matches. */
        const Array* AsArray() const;
        /** @brief Returns the object value when the kind matches. */
        const Object* AsObject() const;
        /** @brief Returns the named child from an object value. */
        const JsonValue* FindMember(std::string_view key) const;

    private:
        /** @brief Stores the concrete value payload. */
        std::variant<std::monostate, bool, double, std::string, Array, Object> value_;
    };
}
