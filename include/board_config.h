#pragma once

#include <stdint.h>

// NodeMCU-32S / klassisk ESP32. Bruk GPIO-numre, ikke fysisk pinnummer.
// Standardoppsettet har ingen aktiverte sensorer eller motorer.
namespace cfg
{
    constexpr uint32_t SerialBaud = 115200;
    constexpr uint32_t HostLeaseMs = 1500;
    constexpr uint32_t WatchdogTimeoutMs = 1000;
    constexpr uint32_t ControlPeriodUs = 8000;
    constexpr uint32_t MaxCycleUs = 6000;
    constexpr uint32_t MaxStartLatenessUs = 1000;
    constexpr uint32_t CommandSlackUs = 1500;
    constexpr uint32_t MaxTestMs = 30000;
    constexpr int16_t MaxTestPwm = 120;
    constexpr uint16_t CalibrationMinimumSpan = 30;

    // Sensorens totale antall er foreløpig identifisert som 25.
    // Utvalget dekker bredden, med tettere målepunkter nær midten.
    constexpr uint8_t PhysicalSensorCount = 25;
    constexpr uint8_t SensorCount = 12;
    constexpr int SensorPositionSpan = 5000;

    enum class SensorBackend : uint8_t
    {
        Unconfigured,
        Analog
    };

    struct SensorInput
    {
        uint8_t physicalIndex; // Nullbasert indeks på selve sensorrekken.
        int8_t pin;            // Direkte ADC-pin, -1 hvis ikke tilkoblet / via mux.
        int8_t muxChannel;     // -1 betyr direkte. 0..15 velger kanal på én 16:1-mux.
    };

#if defined(LINEFOLLOW_BUILD_FIXTURE)
    constexpr SensorBackend Sensors = SensorBackend::Analog;
#if defined(LINEFOLLOW_MUX_FIXTURE)
    constexpr SensorInput SensorInputs[] = {{0, 25, -1},
                                            {3, 26, -1},
                                            {6, 27, -1},
                                            {9, 32, -1},
                                            {10, 33, -1},
                                            {11, 34, -1},
                                            {12, 35, -1},
                                            {13, 36, -1},
                                            {14, -1, 0},
                                            {17, -1, 1},
                                            {21, -1, 2},
                                            {24, -1, 3}};
#else
    constexpr SensorInput SensorInputs[] = {{0, 4, -1},
                                            {3, 13, -1},
                                            {6, 14, -1},
                                            {9, 25, -1},
                                            {10, 26, -1},
                                            {11, 27, -1},
                                            {12, 32, -1},
                                            {13, 33, -1},
                                            {14, 34, -1},
                                            {17, 35, -1},
                                            {21, 36, -1},
                                            {24, 39, -1}};
#endif
#else
    constexpr SensorBackend Sensors = SensorBackend::Unconfigured;
    constexpr SensorInput SensorInputs[] = {{0, -1, -1},
                                            {3, -1, -1},
                                            {6, -1, -1},
                                            {9, -1, -1},
                                            {10, -1, -1},
                                            {11, -1, -1},
                                            {12, -1, -1},
                                            {13, -1, -1},
                                            {14, -1, -1},
                                            {17, -1, -1},
                                            {21, -1, -1},
                                            {24, -1, -1}};
#endif

    // Oppstarts-/strapping-pinner tillates bare etter eksplisitt vurdering av kablingen.
    constexpr bool AllowBootPins = false;
#if defined(LINEFOLLOW_MUX_FIXTURE)
    constexpr bool MuxEnabled = true;
    constexpr int8_t MuxSignalPin = 39;
    constexpr int8_t MuxSelectPins[] = {23, 4, 13, 14};
#else
    constexpr bool MuxEnabled = false;
    constexpr int8_t MuxSignalPin = -1;
    constexpr int8_t MuxSelectPins[] = {-1, -1, -1, -1};
#endif
    constexpr uint16_t MuxSettleUs = 20;
    constexpr bool DiscardFirstAdc = true;
    constexpr uint8_t AnalogSamplesPerSensor = 1;
    constexpr uint8_t AdcBits = 12;
    constexpr uint16_t AdcMaximum = (1U << AdcBits) - 1;
    constexpr int8_t EmitterPin = -1;

    struct MotorPins
    {
        int8_t in1;
        int8_t in2;
        int8_t pwm;
        bool inverted;
    };

#if defined(LINEFOLLOW_BUILD_FIXTURE)
    constexpr bool MotorsEnabled = true;
    constexpr MotorPins LeftMotor{16, 17, 18, false};
    constexpr MotorPins RightMotor{19, 21, 22, false};
#else
    constexpr bool MotorsEnabled = false;
    constexpr MotorPins LeftMotor{-1, -1, -1, false};
    constexpr MotorPins RightMotor{-1, -1, -1, false};
#endif
    constexpr uint32_t MotorPwmHz = 20000;
    constexpr uint8_t MotorPwmBits = 8; // Regulator og testkommandoer bruker PWM 0..255.
    constexpr int8_t DriverEnablePin = -1;
    constexpr bool DriverEnableActiveHigh = true;

    // Målekretser er valgfrie og må bruke egne ledige ADC-innganger.
    constexpr int8_t VoltagePin = -1;
    constexpr int8_t CurrentPin = -1;
    constexpr float VoltageDividerRatio = 1.0f;
    constexpr float CurrentZeroV = 1.65f;
    constexpr float CurrentVoltsPerAmp = 0.185f;
    constexpr int8_t LeftEncoderPin = -1;
    constexpr int8_t RightEncoderPin = -1;
    constexpr bool EncoderInternalPullups = true;
    constexpr uint16_t EncoderPulsesPerWheelRevolution = 0;
    constexpr uint32_t EncoderWindowUs = 100000;

    inline int sensorPosition(uint8_t selectedIndex)
    {
        return int32_t(SensorInputs[selectedIndex].physicalIndex) * SensorPositionSpan /
               (PhysicalSensorCount - 1);
    }

    static_assert(SensorCount >= 2 && SensorCount <= 31, "SensorCount: 2..31");
    static_assert(sizeof(SensorInputs) / sizeof(SensorInputs[0]) == SensorCount,
                  "Input count mismatch");
    static_assert(PhysicalSensorCount >= SensorCount && PhysicalSensorCount <= 31,
                  "Invalid physical count");
    static_assert(SensorPositionSpan > 0 && SensorPositionSpan <= 30000, "Invalid position scale");
    static_assert(MaxCycleUs > 0 && MaxCycleUs < ControlPeriodUs, "Invalid cycle budget");
    static_assert(ControlPeriodUs % 1000 == 0 && ControlPeriodUs < 60000,
                  "Use whole ms control period");
    static_assert(AnalogSamplesPerSensor >= 1 && AnalogSamplesPerSensor <= 64, "Invalid averaging");
    static_assert(AdcBits >= 9 && AdcBits <= 12, "ESP32 ADC resolution: 9..12");
    static_assert(MotorPwmBits == 8 && MotorPwmHz > 0, "PWM uses 8-bit duty");
    static_assert(MaxTestPwm >= 0 && MaxTestPwm <= 255, "Invalid PWM limit");
    static_assert(WatchdogTimeoutMs >= 1000 && HostLeaseMs > 0, "Invalid watchdog/lease");
} // namespace cfg
