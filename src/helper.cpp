//
// Created by tonzg on 05/10/2026.
//
#include "helper.h"
#include "conf.h"

float limitValue(float value, float low, float high) {
    return value < low ? low : (value > high ? high : value);
}

float magnitude(float value) {
    return value < 0 ? -value : value;
}

float approach(float value, float target, float step) {
    return value + limitValue(target - value, -step, step);
}

