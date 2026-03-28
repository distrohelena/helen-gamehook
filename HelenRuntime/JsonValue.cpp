#include <HelenHook/JsonValue.h>

namespace helen
{
    JsonValue::JsonValue() = default;

    JsonValue::JsonValue(bool value)
        : value_(value)
    {
    }

    JsonValue::JsonValue(double value)
        : value_(value)
    {
    }

    JsonValue::JsonValue(std::string value)
        : value_(std::move(value))
    {
    }

    JsonValue::JsonValue(Array value)
        : value_(std::move(value))
    {
    }

    JsonValue::JsonValue(Object value)
        : value_(std::move(value))
    {
    }

    JsonValue::Kind JsonValue::GetKind() const noexcept
    {
        switch (value_.index())
        {
        case 0:
            return Kind::Null;
        case 1:
            return Kind::Boolean;
        case 2:
            return Kind::Number;
        case 3:
            return Kind::String;
        case 4:
            return Kind::Array;
        case 5:
            return Kind::Object;
        default:
            return Kind::Null;
        }
    }

    bool JsonValue::IsNull() const noexcept
    {
        return std::holds_alternative<std::monostate>(value_);
    }

    bool JsonValue::IsBoolean() const noexcept
    {
        return std::holds_alternative<bool>(value_);
    }

    bool JsonValue::IsNumber() const noexcept
    {
        return std::holds_alternative<double>(value_);
    }

    bool JsonValue::IsString() const noexcept
    {
        return std::holds_alternative<std::string>(value_);
    }

    bool JsonValue::IsArray() const noexcept
    {
        return std::holds_alternative<Array>(value_);
    }

    bool JsonValue::IsObject() const noexcept
    {
        return std::holds_alternative<Object>(value_);
    }

    std::optional<bool> JsonValue::AsBoolean() const
    {
        if (!IsBoolean())
        {
            return std::nullopt;
        }

        return std::get<bool>(value_);
    }

    std::optional<double> JsonValue::AsNumber() const
    {
        if (!IsNumber())
        {
            return std::nullopt;
        }

        return std::get<double>(value_);
    }

    const std::string* JsonValue::AsString() const
    {
        return IsString() ? &std::get<std::string>(value_) : nullptr;
    }

    const JsonValue::Array* JsonValue::AsArray() const
    {
        return IsArray() ? &std::get<Array>(value_) : nullptr;
    }

    const JsonValue::Object* JsonValue::AsObject() const
    {
        return IsObject() ? &std::get<Object>(value_) : nullptr;
    }

    const JsonValue* JsonValue::FindMember(std::string_view key) const
    {
        const auto* object = AsObject();
        if (!object)
        {
            return nullptr;
        }

        const auto found = object->find(std::string(key));
        return found != object->end() ? &found->second : nullptr;
    }
}
