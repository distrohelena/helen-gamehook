#pragma once

#include <optional>
#include <string_view>

#include <HelenHook\JsonValue.h>

namespace helen
{
    /**
     * @brief Parses JSON text into a lightweight tree for pack loading.
     */
    class JsonParser
    {
    public:
        /** @brief Parses the given text and returns the root value on success. */
        static std::optional<JsonValue> Parse(std::string_view text);
    };
}
