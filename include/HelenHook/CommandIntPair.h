#pragma once

namespace helen
{
    /**
     * @brief Captures two related integer dispatcher values from one synchronized read.
     *
     * The values are intentionally named rather than positional-only so callers can preserve the
     * relationship between a first key and second key without observing a torn pair update.
     */
    struct CommandIntPair
    {
        /** @brief Value read for the first key supplied to the dispatcher snapshot operation. */
        int FirstValue;
        /** @brief Value read for the second key supplied to the dispatcher snapshot operation. */
        int SecondValue;
    };
}
