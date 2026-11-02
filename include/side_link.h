#pragma once
#include "side_protocol.h"

namespace side_link
{
    // Maksimalt én I2C-operasjon; kalles bare når kontroll-runtime har tid.
    void service(uint32_t now);
    // Forrige komplette sett, også ved midlertidig overføringsfeil.
    const sideproto::Sample& latest();
    // now er micros(). Alderen inkluderer konservativt overføringstiden.
    uint16_t ageMs(uint32_t now);
    // true bare når settet er gyldig og yngre enn MaxAgeMs.
    bool fresh(uint32_t now);
} // namespace side_link
