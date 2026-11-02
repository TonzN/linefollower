#include "motor.h"
#include "conf.h"
#include "helper.h"

float rampMotor(float value, float target, float dt)
{
    if (value * target < 0)
    {
        target = 0;
    }
    float rate = magnitude(target) > magnitude(value) ? MOTOR_ACCEL : MOTOR_DECEL;
    return approach(value, target, rate * dt);
}
