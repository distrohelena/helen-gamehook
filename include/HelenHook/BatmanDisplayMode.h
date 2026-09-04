#pragma once

namespace helen
{
    /**
     * @brief Stores one positive-width and positive-height display-mode pair exposed by Batman's graphics menu.
     *
     * The pair is intentionally kept as integers because the game protocol exchanges dimensions as scalar
     * values. Validation that a pair came from a supported display enumeration belongs to the catalog service.
     */
    class BatmanDisplayMode
    {
    public:
        /**
         * @brief Constructs a display-mode pair from its raw horizontal and vertical dimensions.
         * @param width Horizontal display dimension supplied by the enumeration source.
         * @param height Vertical display dimension supplied by the enumeration source.
         */
        BatmanDisplayMode(int width, int height);

        /**
         * @brief Returns the horizontal dimension stored by this display mode.
         * @return Horizontal display dimension.
         */
        int GetWidth() const noexcept;

        /**
         * @brief Returns the vertical dimension stored by this display mode.
         * @return Vertical display dimension.
         */
        int GetHeight() const noexcept;

        /**
         * @brief Compares two display modes by exact horizontal and vertical dimensions.
         * @param other Display mode that should be compared with this mode.
         * @return True when both dimensions match exactly; otherwise false.
         */
        bool operator==(const BatmanDisplayMode& other) const noexcept;

    private:
        /** @brief Horizontal display dimension retained for catalog sorting and scalar exposure. */
        int width_;
        /** @brief Vertical display dimension retained for catalog sorting and scalar exposure. */
        int height_;
    };
}
