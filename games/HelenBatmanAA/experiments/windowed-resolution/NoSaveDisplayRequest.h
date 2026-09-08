#pragma once

#include <HelenHook/BatmanDisplayMode.h>
#include <span>
#include <stdexcept>

namespace helen {
    /** Validates one experimental display transition before the process's single engine attempt is consumed. */
    class NoSaveDisplayRequest {
    private:
        /** Positive horizontal dimension passed unchanged to the engine. */
        unsigned Width;
        /** Positive vertical dimension passed unchanged to the engine. */
        unsigned Height;
        /** Exact engine mode argument: zero windowed, one exclusive fullscreen. */
        int Fullscreen;

    public:
        /** Rejects invalid/no-op requests and requires an exact driver-reported pair for fullscreen.
         * Windowed requests deliberately do not require a fullscreen mode match.
         */
        NoSaveDisplayRequest(int width, int height, int fullscreen, unsigned currentWidth,
            unsigned currentHeight, bool currentFullscreen, std::span<const BatmanDisplayMode> supportedModes)
            : Width(static_cast<unsigned>(width)), Height(static_cast<unsigned>(height)), Fullscreen(fullscreen) {
            if (width <= 0 || height <= 0 || currentWidth == 0 || currentHeight == 0) {
                throw std::invalid_argument("Display transition requires positive requested and current dimensions");
            }
            if (fullscreen != 0 && fullscreen != 1) {
                throw std::invalid_argument("Display transition requires fullscreen zero or one");
            }
            if (Width == currentWidth && Height == currentHeight && (Fullscreen == 1) == currentFullscreen) {
                throw std::invalid_argument("Display transition requires a size or mode change");
            }
            if (Fullscreen == 1) {
                bool supported = false;
                for (const BatmanDisplayMode& mode : supportedModes) {
                    if (mode.GetWidth() == width && mode.GetHeight() == height) {
                        supported = true;
                        break;
                    }
                }
                if (!supported) {
                    throw std::invalid_argument("Fullscreen resolution is not supported by the active adapter");
                }
            }
        }

        /** Returns the validated engine width. */
        unsigned GetWidth() const noexcept { return Width; }
        /** Returns the validated engine height. */
        unsigned GetHeight() const noexcept { return Height; }
        /** Returns the validated engine fullscreen flag without boolean coercion. */
        int GetFullscreen() const noexcept { return Fullscreen; }
    };
}
