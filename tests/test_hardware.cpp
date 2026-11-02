#include <cassert>
#include <iostream>
#include "Arduino.h"
#include "board_config.h"
uint32_t fakeMicros = 0;
int fakePinValues[40] = {};
uint32_t fakePwmDuty[2] = {};
int adcCalls = 0;

int analogRead(int pin)
{
    ++adcCalls;
    fakeMicros += 10;

    if (cfg::MuxEnabled && pin == cfg::MuxSignalPin)
    {
        int channel = 0;
        for (uint8_t bit = 0; bit < 4; ++bit)
        {
            channel |= fakePinValues[cfg::MuxSelectPins[bit]] << bit;
        }
        return 200 * channel;
    }

    return pin * 30;
}

uint32_t analogReadMilliVolts(int)
{
    return 1500;
}

#include "../src/hardware.cpp"

int main()
{
    assert(hw::begin());
    assert(hw::sensorReady() && hw::motorReady());
    assert(hw::rawMaximum() == 4095);
    uint16_t values[cfg::SensorCount] = {};
    assert(hw::readSensors(values));
    assert(adcCalls == cfg::SensorCount * 2);

    for (uint8_t i = 0; i < cfg::SensorCount; ++i)
    {
        const auto& input = cfg::SensorInputs[i];
        int expected = input.muxChannel < 0 ? input.pin * 30 : input.muxChannel * 200;
        assert(values[i] == expected);
    }

    hw::motors(-50, 500);
    assert(hw::appliedLeft == -50 && hw::appliedRight == 255);
    assert(fakePwmDuty[0] == 50 && fakePwmDuty[1] == 255);
    hw::stop();
    assert(fakePwmDuty[0] == 0 && fakePwmDuty[1] == 0);

    bool used[40] = {};
    assert(!takePin(-1, used, PinRole::Analog));
    assert(!takePin(37, used, PinRole::Analog));
    assert(!takePin(0, used, PinRole::Analog));
    assert(!takePin(34, used, PinRole::Output));
    assert(takePin(34, used, PinRole::Analog));
    assert(!takePin(34, used, PinRole::Analog));
    std::cout << "ESP32 hardware: pin validation, ADC, mux, LEDC and stop OK\n";
}
