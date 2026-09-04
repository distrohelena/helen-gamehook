#include <HelenHook/BatmanDisplayMode.h>

namespace helen
{
    /**
     * @brief Constructs one display-mode pair without applying catalog-source validation.
     * @param width Horizontal display dimension.
     * @param height Vertical display dimension.
     */
    BatmanDisplayMode::BatmanDisplayMode(int width, int height)
        : width_(width),
          height_(height)
    {
    }

    /**
     * @brief Returns the horizontal dimension stored by this display mode.
     * @return Horizontal display dimension.
     */
    int BatmanDisplayMode::GetWidth() const noexcept
    {
        return width_;
    }

    /**
     * @brief Returns the vertical dimension stored by this display mode.
     * @return Vertical display dimension.
     */
    int BatmanDisplayMode::GetHeight() const noexcept
    {
        return height_;
    }

    /**
     * @brief Compares two display modes by exact dimensions.
     * @param other Display mode that should be compared with this mode.
     * @return True when both dimensions match exactly; otherwise false.
     */
    bool BatmanDisplayMode::operator==(const BatmanDisplayMode& other) const noexcept
    {
        return width_ == other.width_ && height_ == other.height_;
    }
}
