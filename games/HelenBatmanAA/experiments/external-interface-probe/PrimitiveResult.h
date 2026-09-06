#pragma once
#include <cstdint>

/** Borrowed GAS primitive result layout observed in the pinned Batman executable; not a general GFx value owner. */
struct PrimitiveResult {
    /** GAS type tag: zero is undefined and two is boolean in the inspected copy routine. */
    std::uint8_t Type;
    /** Opaque bytes preceding the payload; the probe must preserve them. */
    std::uint8_t Reserved[3];
    /** Opaque primitive storage; a boolean occupies its first byte. */
    std::uint8_t Payload[12];
};
static_assert(sizeof(PrimitiveResult) == 16);
