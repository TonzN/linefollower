#include <cassert>
#include <iostream>
#include "Arduino.h"
#include "hardware.h"
#include "protocol.h"
uint32_t fakeMicros = 0;

namespace fake
{
    bool sensor = true;
    bool motor = true;
    bool readOk = true;
    uint32_t readUs = 200;
    uint16_t value = 100;
} // namespace fake

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
        return fake::sensor;
    }

    bool motorReady()
    {
        return fake::motor;
    }

    bool readSensors(uint16_t* raw)
    {
        fakeMicros += fake::readUs;
        for (uint8_t i = 0; i < cfg::SensorCount; i++)
        {
            raw[i] = fake::value;
        }
        return fake::readOk;
    }

    uint16_t rawMaximum()
    {
        return 2000;
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

// Test den faktiske ESP32-kontrollflyten med falsk maskinvare.
#include "../src/control_runtime.cpp"
using namespace control_runtime;

void reset()
{
    fakeMicros = 0;
    fake::sensor = fake::motor = fake::readOk = true;
    fake::value = 100;
    fake::readUs = 200;
    mode = Mode::Idle;
    fault = Fault::None;
    armed = calibratedOk = complete = sampleValid = false;
    pending = heartbeat = stopRequested = dropped = false;
    sampleNumber = sampleAt = lastCycleAt = 0;
    missedDeadlines = 0;
    lastHeartbeat = 0;
    minimumCycle = minimumPeriod = 65535;
    maximumCycle = maximumPeriod = 0;
    controller = LineController();
    scheduler = runtime::Scheduler();
    hw::stop();
    memset(raw, 0, sizeof(raw));
    memset(calibrated, 0, sizeof(calibrated));
    memset(calibrationMin, 0, sizeof(calibrationMin));
    memset(calibrationMax, 0, sizeof(calibrationMax));
    control_runtime::begin();
}

Result invoke(Command op, const uint8_t* p = nullptr, uint8_t size = 0)
{
    uint8_t packet[32];
    proto::frame(packet, op, 42, p, size);
    return command(packet);
}

Result motor(int16_t left, int16_t right, uint32_t ms = 1000)
{
    uint8_t p[8];
    proto::put16(p, left);
    proto::put16(p + 2, right);
    proto::put32(p + 4, ms);
    return invoke(Command::Motor, p, 8);
}

int main()
{
    reset();
    assert(motor(20, 20) == Result::NotArmed);
    assert(invoke(Command::Arm) == Result::Ok);
    assert(motor(-100, -100) == Result::Ok);
    cycle(0);
    assert(hw::appliedLeft == -100);
    reset();
    invoke(Command::Arm);
    assert(motor(-32768, 0) == Result::BadArgument);
    assert(motor(121, 0) == Result::BadArgument);
    reset();
    fake::motor = false;
    assert(invoke(Command::Arm) == Result::NotConfigured);
    reset();
    invoke(Command::Arm);
    motor(20, 20);
    fakeMicros = 2000;
    cycle(fakeMicros);
    assert(fault == Fault::LateStart && hw::appliedLeft == 0 && !armed);
    reset();
    mode = Mode::Sensors;
    testDuration = 10000;
    fake::readUs = cfg::MaxCycleUs + 1;
    cycle(0);
    assert(fault == Fault::CycleBudget && hw::appliedLeft == 0);
    reset();
    mode = Mode::Sensors;
    fake::readOk = false;
    cycle(0);
    assert(fault == Fault::SensorRead);
    reset();
    invoke(Command::Arm);
    motor(20, 20);
    fakeMicros = (cfg::HostLeaseMs + 1) * 1000;
    control_runtime::tick();
    assert(fault == Fault::LinkLost && !armed && hw::appliedLeft == 0);
    reset();
    invoke(Command::Arm);
    motor(20, 20);
    cycle(0);
    control_runtime::requestStop(99);
    control_runtime::tick();
    assert(mode == Mode::Idle && !armed && hw::appliedLeft == 0 && lastSequence == 99);
    reset();
    mode = Mode::Sensors;
    testDuration = 1000;
    cycle(0);
    publish(70);
    uint8_t saved[sizeof(masterSnapshot)];
    memcpy(saved, masterSnapshot, sizeof(saved));
    fake::value = 900;
    fakeMicros = 8000;
    cycle(8000);
    assert(memcmp(saved, masterSnapshot, sizeof(saved)) == 0);
    publish(71);
    assert(proto::get16(masterSnapshot + 64) == 900);
    reset();
    uint8_t duration[4];
    proto::put32(duration, 16);
    assert(invoke(Command::Calibrate, duration, 4) == Result::Ok);
    cycle(0);
    fake::value = 900;
    fakeMicros = 8000;
    cycle(8000);
    fakeMicros = 16000;
    cycle(16000);
    assert(mode == Mode::Idle && complete && calibratedOk && calibrated[0] == 1000);
    reset();
    proto::put32(duration, 8);
    invoke(Command::Calibrate, duration, 4);
    cycle(0);
    fakeMicros = 8000;
    cycle(8000);
    assert(complete && !calibratedOk);
    reset();
    platform_runtime::watchdogOk = false;
    control_runtime::begin();
    assert(fault == Fault::Watchdog);
    invoke(Command::Clear);
    assert(invoke(Command::Arm) == Result::NotConfigured);
    platform_runtime::watchdogOk = true;
    reset();
    uint8_t invalid[32] = {};
    assert(!enqueueCommand(Command::Sensors, 1, invalid, 26));
    std::cout
        << "Actual ESP32 runtime: motor gate, reverse bounds, deadlines, link lease, priority "
           "STOP, snapshot cache, calibration OK\n";
}
