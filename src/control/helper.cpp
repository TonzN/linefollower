//
// Created by tonzg on 05/10/2026.
//
#include "helper.h"
#include "conf.h"

float limitValue(float value, float low, float high)
{
    if (value < low)
    {
        return low;
    }

    if (value > high)
    {
        return high;
    }

    return value;
}

float magnitude(float value)
{
    return value < 0 ? -value : value;
}

float approach(float value, float target, float step)
{
    // Gå høyst step mot målet. Rampene bruker dette til å begrense PWM-endring.
    return value + limitValue(target - value, -step, step);
}
