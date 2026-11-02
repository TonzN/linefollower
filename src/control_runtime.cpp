#include <Arduino.h>
#include "platform_runtime.h"
#include "board_config.h"
#include "control_runtime.h"
#include "protocol.h"
#include "runtime_core.h"
#include "hardware.h"
#include "linefollow.h"
#include "conf.h"

namespace control_runtime
{
    using proto::Command;
    using proto::Fault;
    using proto::Mode;
    using proto::Result;
    bool pending = false;
    bool heartbeat = false;
    bool stopRequested = false;
    bool dropped = false;
    bool watchdogReady = false;
    uint16_t stopSequence = 0;
    uint8_t mailbox[proto::FrameSize];
    uint8_t masterSnapshot[proto::HeaderSize + 4 * cfg::SensorCount];

    uint16_t raw[cfg::SensorCount] = {};
    uint16_t calibrated[cfg::SensorCount] = {};
    uint16_t calibrationMin[cfg::SensorCount];
    uint16_t calibrationMax[cfg::SensorCount];
    bool calibratedOk = false;
    bool armed = false;
    bool complete = false;
    bool sampleValid = false;
    Mode mode = Mode::Idle;
    Fault fault = Fault::None;
    LineController controller;
    LineReading reading;
    runtime::Scheduler scheduler;
    runtime::Sweep sweep;
    uint32_t sampleNumber = 0;
    uint32_t sampleAt = 0;
    uint32_t sensorUs = 0;
    uint32_t cycleUs = 0;
    uint32_t lateUs = 0;
    uint32_t lastCycleAt = 0;
    uint32_t minimumCycle = 65535;
    uint32_t maximumCycle = 0;
    uint32_t minimumPeriod = 65535;
    uint32_t maximumPeriod = 0;
    uint32_t missedDeadlines = 0;
    uint32_t lastHeartbeat = 0;
    uint32_t testAt = 0;
    uint32_t testDuration = 0;
    uint16_t lastSequence = 0;
    Result lastResult = Result::Ok;
    int16_t motorLeft = 0;
    int16_t motorRight = 0;

    bool motion()
    {
        return mode == Mode::Motor || mode == Mode::Sweep || mode == Mode::Follow;
    }

    void idle(bool finished = false)
    {
        hw::stop();
        mode = Mode::Idle;
        complete = finished;
    }

    // Feilen låses til CLEAR. Armering oppheves, og begge motorer stoppes.
    void fail(Fault reason)
    {
        fault = reason;
        armed = false;
        idle();
    }

    // Fast byteformat gjør at ESP32 og PC tolker de samme feltene likt.
    // Feltenes offset og enheter er dokumentert i docs/PROTOKOLL.md.
    void publish(uint16_t sequence)
    {
        uint8_t snapshot[sizeof(masterSnapshot)] = {};

        proto::put32(snapshot, sampleNumber);
        proto::put32(snapshot + 4, sampleAt);
        proto::put16(snapshot + 8, proto::saturated16(sensorUs));
        proto::put16(snapshot + 10, proto::saturated16(cycleUs));
        proto::put16(
            snapshot + 12,
            proto::saturated16(sampleNumber > 1 ? sampleAt - lastCycleAt : cfg::ControlPeriodUs));
        proto::put16(snapshot + 14, proto::saturated16(lateUs));
        proto::put32(snapshot + 16, missedDeadlines);
        proto::put32(snapshot + 20, scheduler.skipped);
        proto::put16(snapshot + 24, minimumCycle == 65535 ? 0 : minimumCycle);
        proto::put16(snapshot + 26, proto::saturated16(maximumCycle));
        proto::put16(snapshot + 28, uint16_t(reading.error));
        proto::put16(snapshot + 30, uint16_t(hw::appliedLeft));
        proto::put16(snapshot + 32, uint16_t(hw::appliedRight));
        proto::put16(snapshot + 34, hw::voltageMv);
        proto::put16(snapshot + 36, uint16_t(hw::currentMa));
        proto::put16(snapshot + 38, uint16_t(hw::leftRpm10));
        proto::put16(snapshot + 40, uint16_t(hw::rightRpm10));
        uint16_t flags =
            hw::measurementFlags | (hw::sensorReady() ? proto::SensorReady : 0) |
            (calibratedOk ? proto::Calibrated : 0) | (hw::motorReady() ? proto::MotorReady : 0) |
            (armed ? proto::Armed : 0) | (complete ? proto::TestComplete : 0) |
            (dropped ? proto::DroppedCommand : 0) | (sampleValid ? proto::SampleValid : 0);
        proto::put16(snapshot + 42, flags);
        snapshot[44] = uint8_t(mode);
        snapshot[45] = uint8_t(fault);
        snapshot[46] = cfg::SensorCount;
        snapshot[47] = reading.quality;
        proto::put16(snapshot + 48, lastSequence);
        snapshot[50] = uint8_t(lastResult);
        snapshot[51] = controller.mode();
        proto::put16(snapshot + 52, minimumPeriod == 65535 ? 0 : minimumPeriod);
        proto::put16(snapshot + 54, proto::saturated16(maximumPeriod));
        proto::put16(snapshot + 56, proto::saturated16((micros() - lastHeartbeat) / 1000));
        proto::put16(snapshot + 58, hw::rawMaximum());
        proto::put16(snapshot + 60, sweep.index);
        proto::put16(snapshot + 62, hw::encoderAgeMs);
        for (uint8_t i = 0; i < cfg::SensorCount; i++)
        {
            proto::put16(snapshot + 64 + 2 * i, raw[i]);
            proto::put16(snapshot + 64 + 2 * cfg::SensorCount + 2 * i, calibrated[i]);
        }
        (void)sequence;
        memcpy(masterSnapshot, snapshot, sizeof(snapshot));
    }

    // Valider først, endre modus etterpå. Et avvist argument starter ikke et forsøk.
    Result command(const uint8_t* frame)
    {
        Command op = Command(frame[2]);
        const uint8_t* p = frame + 6;
        uint8_t size = frame[5];
        uint32_t now = micros();
        if (op == Command::Latch)
        {
            if (size)
            {
                return Result::BadArgument;
            }
            publish(proto::get16(frame + 3));
            return Result::Ok;
        }
        if (op == Command::Stop)
        {
            armed = false;
            idle();
            return Result::Ok;
        }
        if (op == Command::Clear)
        {
            if (motion())
            {
                return Result::Busy;
            }
            fault = Fault::None;
            armed = false;
            idle();
            return Result::Ok;
        }
        if (fault != Fault::None)
        {
            return Result::FaultLatched;
        }
        if (op == Command::Arm)
        {
            if (!watchdogReady || !hw::motorReady())
            {
                return Result::NotConfigured;
            }
            armed = true;
            lastHeartbeat = now;
            return Result::Ok;
        }
        if (mode != Mode::Idle)
        {
            return Result::Busy;
        }
        uint32_t duration = 0;
        if (op == Command::Calibrate || op == Command::Sensors || op == Command::Timing ||
            op == Command::Follow)
        {
            if (size != 4)
            {
                return Result::BadArgument;
            }
            duration = proto::get32(p);
            if (!duration || duration > cfg::MaxTestMs)
            {
                return Result::BadArgument;
            }
            if (op != Command::Timing && !hw::sensorReady())
            {
                return Result::NotConfigured;
            }
            if (op == Command::Follow && !calibratedOk)
            {
                return Result::NotCalibrated;
            }
            if (op == Command::Follow && (!armed || !hw::motorReady()))
            {
                return Result::NotArmed;
            }
            if (op == Command::Calibrate)
            {
                calibratedOk = false;
                for (uint8_t i = 0; i < cfg::SensorCount; i++)
                {
                    calibrationMin[i] = 65535;
                    calibrationMax[i] = 0;
                }
                mode = Mode::Calibrating;
            }
            else if (op == Command::Sensors)
            {
                mode = Mode::Sensors;
            }
            else if (op == Command::Timing)
            {
                mode = Mode::Timing;
                minimumPeriod = 65535;
                minimumCycle = 65535;
                missedDeadlines = 0;
                maximumPeriod = 0;
                maximumCycle = 0;
                scheduler.skipped = 0;
            }
            else
            {
                mode = Mode::Follow;
                controller = LineController();
            }
        }
        else if (op == Command::Motor)
        {
            if (size != 8)
            {
                return Result::BadArgument;
            }
            if (!hw::motorReady())
            {
                return Result::NotConfigured;
            }
            if (!armed)
            {
                return Result::NotArmed;
            }
            motorLeft = int16_t(proto::get16(p));
            motorRight = int16_t(proto::get16(p + 2));
            duration = proto::get32(p + 4);
            if (motorLeft > cfg::MaxTestPwm || motorLeft < -int16_t(cfg::MaxTestPwm) ||
                motorRight > cfg::MaxTestPwm || motorRight < -int16_t(cfg::MaxTestPwm) ||
                !duration || duration > cfg::MaxTestMs)
            {
                return Result::BadArgument;
            }
            mode = Mode::Motor;
        }
        else if (op == Command::Sweep)
        {
            if (size != 9)
            {
                return Result::BadArgument;
            }
            if (!hw::motorReady())
            {
                return Result::NotConfigured;
            }
            if (!armed)
            {
                return Result::NotArmed;
            }
            sweep.start = int16_t(proto::get16(p));
            sweep.end = int16_t(proto::get16(p + 2));
            sweep.step = int16_t(proto::get16(p + 4));
            sweep.dwell = proto::get16(p + 6);
            sweep.side = p[8];
            sweep.index = 0;
            if (!sweep.valid(cfg::MaxTestPwm, cfg::MaxTestMs))
            {
                return Result::BadArgument;
            }
            duration = sweep.totalMs();
            mode = Mode::Sweep;
        }
        else
        {
            return Result::BadCommand;
        }
        testAt = now;
        testDuration = duration;
        complete = false;
        return Result::Ok;
    }

    // Én råavlesning brukes både til kalibrering og linjeanalyse i samme kontrollrunde.
    void readControlSensors()
    {
        sensorUs = 0;
        sampleValid = false;
        reading = LineReading();
        if (mode != Mode::Idle && hw::sensorReady())
        {
            uint32_t before = micros();
            bool ok = hw::readSensors(raw);
            sensorUs = micros() - before;
            if (!ok)
            {
                fail(Fault::SensorRead);
            }
            else
            {
                sampleValid = true;
                for (uint8_t i = 0; i < cfg::SensorCount; i++)
                {
                    if (mode == Mode::Calibrating)
                    {
                        calibrationMin[i] = min(calibrationMin[i], raw[i]);
                        calibrationMax[i] = max(calibrationMax[i], raw[i]);
                    }
                    uint16_t span = calibrationMax[i] - calibrationMin[i];
                    int32_t value = span ? 1000L * (int32_t(raw[i]) - calibrationMin[i]) / span : 0;
                    calibrated[i] = calibratedOk ? constrain(value, 0L, 1000L) : 0;
                }
                if (calibratedOk)
                {
                    reading = analyzeLine(calibrated);
                }
            }
        }
    }

    // Avslutt tidsbegrensede forsøk før vi eventuelt skriver nye motorutganger.
    void finishExpiredTest(uint32_t cycleStartedUs)
    {
        if (mode != Mode::Idle && (cycleStartedUs - testAt) / 1000 >= testDuration)
        {
            if (mode == Mode::Calibrating)
            {
                calibratedOk = true;
                for (uint8_t i = 0; i < cfg::SensorCount; i++)
                {
                    if (calibrationMax[i] - calibrationMin[i] < cfg::CalibrationMinimumSpan)
                    {
                        calibratedOk = false;
                    }
                }
                if (calibratedOk)
                {
                    for (uint8_t i = 0; i < cfg::SensorCount; i++)
                    {
                        calibrated[i] = constrain(1000L * (int32_t(raw[i]) - calibrationMin[i]) /
                                                      (calibrationMax[i] - calibrationMin[i]),
                                                  0L,
                                                  1000L);
                    }
                    reading = analyzeLine(calibrated);
                }
            }
            idle(true);
        }
    }

    // Regulator og manuelle forsøk deler samme maskinvareutgang.
    // Bare én modus får bestemme pådraget i en gitt kontrollrunde.
    void applyMotorOutput(uint32_t cycleStartedUs)
    {
        if (mode == Mode::Follow)
        {
            controller.update(reading, millis());
            hw::motors(int16_t(controller.left), int16_t(controller.right));
            if (controller.stopped)
            {
                idle(true);
            }
        }
        else if (mode == Mode::Motor)
        {
            hw::motors(motorLeft, motorRight);
        }
        else if (mode == Mode::Sweep)
        {
            sweep.index = (cycleStartedUs - testAt) / 1000 / sweep.dwell;
            int16_t value = sweep.value();
            hw::motors(sweep.side & 1 ? value : 0, sweep.side & 2 ? value : 0);
        }
        else
        {
            hw::stop();
        }
    }

    // Fast rekkefølge: tidskontroll, sensorer, testslutt, motorer og tidsmåling.
    void cycle(uint32_t now)
    {
        const uint32_t begun = micros();
        lastCycleAt = sampleAt;
        sampleAt = begun;
        ++sampleNumber;
        if (sampleNumber > 1)
        {
            uint32_t period = sampleAt - lastCycleAt;
            minimumPeriod = min(minimumPeriod, period);
            maximumPeriod = max(maximumPeriod, period);
        }
        lateUs = scheduler.take(now, cfg::ControlPeriodUs);
        if (lateUs > cfg::MaxStartLatenessUs)
        {
            ++missedDeadlines;
            if (motion())
            {
                fail(Fault::LateStart);
            }
        }
        readControlSensors();
        finishExpiredTest(begun);
        applyMotorOutput(begun);

        hw::updateMeasurements(micros());
        cycleUs = micros() - begun;
        minimumCycle = min(minimumCycle, cycleUs);
        maximumCycle = max(maximumCycle, cycleUs);
        if (cycleUs > cfg::MaxCycleUs)
        {
            ++missedDeadlines;
            fail(Fault::CycleBudget);
        }
    }
} // namespace control_runtime

namespace control_runtime
{
    void begin()
    {
        bool hardwareConfigured = hw::begin();
        hw::stop();

        scheduler.next = micros();
        lastHeartbeat = micros();
        publish(0);
        if (!hardwareConfigured)
        {
            fail(Fault::Configuration);
        }

        watchdogReady = platform_runtime::beginWatchdog();
        if (!watchdogReady)
        {
            fail(Fault::Watchdog);
        }
    }

    void tick()
    {
        platform_runtime::feedWatchdog();
        uint32_t now = micros();
        if (heartbeat)
        {
            heartbeat = false;
            lastHeartbeat = now;
        }
        if (stopRequested)
        {
            stopRequested = false;
            lastSequence = stopSequence;
            lastResult = Result::Ok;
            armed = false;
            idle();
        }
        if (armed && (now - lastHeartbeat) / 1000 > cfg::HostLeaseMs)
        {
            fail(Fault::LinkLost);
        }
        if (scheduler.due(now))
        {
            cycle(now);
            return;
        }
        // Kommandoer behandles i ledig tid. Encoder-avbruddene håndteres i hardware.cpp.
        if (pending && int32_t(scheduler.next - micros()) > int32_t(cfg::CommandSlackUs))
        {
            uint8_t message[proto::FrameSize];
            memcpy(message, mailbox, sizeof(message));
            pending = false;
            if (Command(message[2]) == Command::Latch)
            {
                command(message);
            }
            else
            {
                lastSequence = proto::get16(message + 3);
                lastResult = command(message);
            }
        }
    }

    bool hasTimeFor(uint32_t requiredUs)
    {
        return int32_t(scheduler.next - micros()) > int32_t(requiredUs);
    }

    bool enqueueCommand(proto::Command operation,
                        uint16_t sequence,
                        const uint8_t* payload,
                        uint8_t payloadSize)
    {
        if (pending || payloadSize > proto::PayloadSize)
        {
            return false;
        }

        proto::frame(mailbox, operation, sequence, payload, payloadSize);
        pending = true;
        return true;
    }

    void keepAlive()
    {
        heartbeat = true;
    }

    void requestStop(uint16_t sequence)
    {
        // Kalles fra samme Arduino-oppgave som tick(), ikke fra encoder-avbrudd.
        stopSequence = sequence;
        stopRequested = true;
        pending = false;
    }

    const uint8_t* captureSnapshot()
    {
        publish(0);
        return masterSnapshot;
    }
} // namespace control_runtime
