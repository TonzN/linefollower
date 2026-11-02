#pragma once
#include <stdint.h>

// Fyll ut disse verdiene før opplasting til faktisk robot.
// -1 betyr ikke tilkoblet. Ingen gamle pinnetildelinger gjenbrukes automatisk.
namespace cfg {
constexpr uint8_t NanoAddress = 0x21;
constexpr uint32_t I2cClockHz = 100000;
constexpr uint32_t I2cTimeoutUs = 4000;
constexpr uint32_t SerialBaud = 115200;
constexpr uint32_t HeartbeatMs = 100;
constexpr uint32_t HostLeaseMs = 1500;
constexpr uint32_t NanoLeaseMs = 1500;
constexpr uint32_t ControlPeriodUs = 8000;
constexpr uint32_t MaxCycleUs = 6000;
constexpr uint32_t MaxStartLatenessUs = 1000;
constexpr uint32_t CommandSlackUs = 1500;
constexpr uint32_t MaxTestMs = 30000;
constexpr int16_t MaxTestPwm = 120;
constexpr uint16_t CalibrationMinimumSpan = 30;
// Hovedkortet leser seks sentrale kanaler direkte. Ytterkort: side_config.h.
constexpr uint8_t SensorCount = 6;
constexpr int SensorPositionSpan = 5000;

enum class SensorBackend : uint8_t { Unconfigured, QtrRC, QtrAnalog, Custom };
constexpr SensorBackend Sensors = SensorBackend::Unconfigured;
// Direkte QTR-avlesning trenger én unik pinne per leser. Ved en sensor med
// annen grensesnittype brukes adapteren i src/custom_sensor.cpp i stedet.
constexpr int8_t SensorPins[] = {
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1
};
constexpr int8_t EmitterPin = -1;
constexpr uint16_t QtrTimeoutUs = 2000;
constexpr uint8_t AnalogSamplesPerSensor = 1;

struct MotorPins { int8_t in1, in2, pwm; bool inverted; };
constexpr bool MotorsEnabled = false;
constexpr MotorPins LeftMotor{-1,-1,-1,false};
constexpr MotorPins RightMotor{-1,-1,-1,false};
constexpr int8_t DriverEnablePin = -1;
constexpr bool DriverEnableActiveHigh = true;

// Valgfritt måleutstyr. Strømmåling gjelder plasseringen dere selv kobler inn.
constexpr int8_t VoltagePin = -1;
constexpr int8_t CurrentPin = -1;
constexpr float AdcReferenceV = 5.0f;
constexpr float VoltageDividerRatio = 1.0f;
constexpr float CurrentZeroV = 2.5f;
constexpr float CurrentVoltsPerAmp = 0.185f;
// Enkeltkanals enkoder: kun RPM-magnitude, ikke fysisk retning.
constexpr int8_t LeftEncoderPin = -1;
constexpr int8_t RightEncoderPin = -1;
constexpr uint16_t EncoderPulsesPerWheelRevolution = 0;
constexpr uint32_t EncoderWindowUs = 100000;

static_assert(SensorCount >= 2 && SensorCount <= 31, "SensorCount: 2..31");
static_assert(SensorCount <= sizeof(SensorPins), "Sensor pin array too short");
static_assert(SensorPositionSpan >= 1 && SensorPositionSpan <= 30000, "Invalid position scale");
static_assert(MaxCycleUs < ControlPeriodUs, "Cycle budget must be below period");
static_assert(MaxCycleUs>0 && ControlPeriodUs%1000==0, "Use positive budget and whole ms control periods");
static_assert(NanoAddress>=8 && NanoAddress<=119, "Use a non-reserved 7-bit I2C address");
static_assert(QtrTimeoutUs>0 && QtrTimeoutUs<=32767, "Invalid RC timeout");
static_assert(AnalogSamplesPerSensor>=1 && AnalogSamplesPerSensor<=64, "Invalid analog averaging");
static_assert(EncoderWindowUs>0 && AdcReferenceV>0 && VoltageDividerRatio>0, "Invalid measurement configuration");
static_assert(ControlPeriodUs < 60000, "Telemetry period field is uint16");
static_assert(HeartbeatMs < NanoLeaseMs, "Heartbeat must arrive before lease expiry");
static_assert(MaxTestPwm <= 255, "PWM range is 0..255");
}
