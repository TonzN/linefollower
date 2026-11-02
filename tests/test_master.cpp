#include <iostream>
#include "Arduino.h"
#include "Wire.h"
#include "hardware.h"
#include "side_link.h"
uint32_t fakeMicros = 0;
FakeWire Wire;
FakeSerial Serial;

namespace hw
{
    int16_t appliedLeft = 0;
    int16_t appliedRight = 0;
    int16_t currentMa = 0;
    int16_t leftRpm10 = 0;
    int16_t rightRpm10 = 0;
    uint16_t measurementFlags = 0;
    uint16_t voltageMv = 0;
    uint16_t encoderAgeMs = 0;

    bool begin()
    {
        return true;
    }

    bool sensorReady()
    {
        return true;
    }

    bool motorReady()
    {
        return true;
    }

    bool readSensors(uint16_t* raw)
    {
        for (uint8_t i = 0; i < cfg::SensorCount; i++)
        {
            raw[i] = 100;
        }
        return true;
    }

    uint16_t rawMaximum()
    {
        return 1023;
    }

    void motors(int16_t l, int16_t r)
    {
        appliedLeft = l;
        appliedRight = r;
    }

    void stop()
    {
        motors(0, 0);
    }

    void updateMeasurements(uint32_t)
    {
    }
} // namespace hw

namespace side_link
{
    sideproto::Sample value;
    bool isFresh = false;

    void service(uint32_t)
    {
    }

    const sideproto::Sample& latest()
    {
        return value;
    }

    uint16_t ageMs(uint32_t)
    {
        return isFresh ? 4 : 65535;
    }

    bool fresh(uint32_t)
    {
        return isFresh;
    }
} // namespace side_link

#define LINEFOLLOW_MASTER
#include "../src/control_runtime.cpp"
#include "../src/master.cpp"
using namespace control_runtime;

void dispatch(const char* text)
{
    char line[96];
    strcpy(line, text);
    host::dispatch(line);
}

void drain()
{
    for (unsigned i = 0; i < 500; i++)
    {
        Serial.space = 64;
        host::output();
    }
}

int main()
{
    setup();
    fakeMicros = 0xF1234567;
    sampleAt = fakeMicros;
    dispatch("POLL");
    Serial.space = 0;
    host::output();
    assert(Serial.outgoing.empty());
    drain();
    std::cout << Serial.outgoing;
    assert(Serial.outgoing.back() == '\n');
    Serial.outgoing.clear();
    side_link::isFresh = true;
    side_link::value.valid = true;
    side_link::value.raw[0] = 0;
    side_link::value.raw[1] = 950;
    dispatch("POLL");
    // STOP må virke selv om en stor rapport fortsatt venter på UART-plass.
    armed = true;
    mode = Mode::Motor;
    hw::motors(30, 30);
    dispatch("STOP");
    control_runtime::tick();
    assert(!armed && hw::appliedLeft == 0);
    drain();
    std::cout << Serial.outgoing;
    Serial.outgoing.clear();
    dispatch("MOTOR rubbish");
    drain();
    assert(Serial.outgoing.find("error") != std::string::npos);
    std::cout << "Master: nonblocking UART, unsigned micros, side telemetry and priority STOP OK\n";
}
